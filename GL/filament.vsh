#ifdef GL_ES
  precision mediump float;
#endif

uniform mat4 uModelMatrix;
uniform mat4 uProjMatrix;
uniform float uFilamentRadius;
uniform bool uLineOnly;
uniform vec4 uColor;

attribute vec3 aCurPos;
attribute vec2 aNextPos;
attribute vec2 aPrevPos;
attribute float aSide;

varying vec4 vColor;

vec2 GetNormal(vec2 a, vec2 b, bool outside)
{
    float dX = b.x - a.x;
    float dY = b.y - a.y;
    if (outside)
        dY *= -1.0;
    else
        dX *= -1.0;
    vec2 norm = vec2(dY, dX);
    return normalize(norm) * uFilamentRadius;
}

struct InterPoint
{
    vec2 point;
    bool does;
};

InterPoint Intersection(float x1, float x2, float x3, float x4,
                 float y1, float y2, float y3, float y4)
{
    float x12 = x1 - x2;
    float x34 = x3 - x4;
    float y12 = y1 - y2;
    float y34 = y3 - y4;

    float c = x12 * y34 - y12 * x34;

    InterPoint inter;

    if (abs(c) < 0.01)
    {
        // No intersection
        inter.does = false;
    }
    else
    {
        // Intersection
        float a = x1 * y2 - y1 * x2;
        float b = x3 * y4 - y3 * x4;

        inter.point.x = (a * x34 - b * x12) / c;
        inter.point.y = (a * y34 - b * y12) / c;
        inter.does = true;
    }

    return inter;
}

void main(void)
{
    // Detect lightweight rendering
    if (uLineOnly)
    {
        vColor = uColor;
        gl_Position = uProjMatrix * uModelMatrix * vec4(aCurPos, 1.0);
        return;
    }

    // Project the positions into view space
    vec4 curPos = uModelMatrix * vec4(aCurPos, 1.0);
    vec4 prevPos = uModelMatrix * vec4(aPrevPos, aCurPos.z, 1.0);
    vec4 nextPos = uModelMatrix * vec4(aNextPos, aCurPos.z, 1.0);
    vec3 newPos;

    bool outsidePoint = (aSide > 0.0);

    // Calculate the normals for the line segments
    vec2 normNext = GetNormal(curPos.xy, nextPos.xy, outsidePoint);
    vec2 normPrev = GetNormal(prevPos.xy, curPos.xy, outsidePoint);

    // If the normals have a positive cross then the direction will
    // be on the outside
    vec3 crossed = cross(vec3(normPrev, 0.0), vec3(normNext, 0.0));
    const vec3 ref = vec3(0.0, 0.0, 1.0);
    bool interOut = (dot(crossed, ref) > 0.0);

    float absSide = abs(aSide);
    bool sndPoint, cnrPoint, fstPoint, fstOnly, sndOnly;
    sndPoint = cnrPoint = fstPoint = fstOnly = sndOnly = false;
    if (absSide < 15.0)
        sndPoint = true;
    else if (absSide < 25.0)
        cnrPoint = true;
    else if (absSide < 35.0)
        fstPoint = true;
    else if (absSide < 45.0)
        fstPoint = fstOnly = true;
    else
        sndPoint = sndOnly = true;
    // TODO: combine fstonly & sndonly

    // Invert the normals for a corner point if it is on the wrong side
    if (cnrPoint && outsidePoint && !interOut)
    {
        normNext *= -1.0;
        normPrev *= -1.0;
    }

    // Get the 4 points of the two lines
    vec2 a1 = prevPos.xy + normPrev;
    vec2 a2 = curPos.xy + normPrev;
    vec2 b1 = nextPos.xy + normNext;
    vec2 b2 = curPos.xy + normNext;

    // Calculate the intersection if any
    // We can use avergae x 2 for comparison
    float prevZ = (prevPos.z + curPos.z);
    float nextZ = (curPos.z + nextPos.z);
    bool prevFront = (prevZ > nextZ);

    // We should only check for an intersection if it can be possible like when the interscetion is
    // on the current side
    if (!fstOnly && !sndOnly && (cnrPoint || outsidePoint == interOut))
    {
        // If the intersection is not on a horizontal edge then it should be on a vertical edge
        InterPoint inter = Intersection(a1.x, a2.x, b1.x, b2.x, a1.y, a2.y, b1.y, b2.y);
        // TODO: optimize below
        if (inter.does && distance(a1, inter.point) < distance(a1, a2) && distance(a2, inter.point) < distance(a1, a2))
        {
            // We need the z pos of the intersection
            float totalPrevD = distance(a1, a2);
            float totalNextD = distance(b1, b2);
            float z = curPos.z;

            // The z of a 2nd point should lay on the previous line whilst the z of a 1st point should be on the next line
            // the z of the should be on whichever one is in front
            if ((sndPoint || (cnrPoint && prevFront)) && totalPrevD != 0.0 && prevPos.z != curPos.z)
            {
                float deltaD = distance(a1, inter.point);
                float deltaZ = curPos.z - prevPos.z;
                z = prevPos.z + (deltaD / totalPrevD * deltaZ);
            }
            else if (!sndPoint && totalNextD != 0.0 && nextPos.z != curPos.z)
            {
                float deltaD = distance(b1, inter.point);
                float deltaZ = curPos.z - nextPos.z;
                z = nextPos.z + (deltaD / totalNextD * deltaZ);
            }

            newPos = vec3(inter.point, z);
        }
        else
        {
            if (prevFront)
                newPos = vec3(a2, curPos.z);
            else
                newPos = vec3(b2, curPos.z);
        }
    }
    else
    {
        if (sndPoint)
            newPos = vec3(a2, curPos.z);
        else
            newPos = vec3(b2, curPos.z);
    }

    // Add the radius to the z
    newPos.z += uFilamentRadius;
    gl_Position = uProjMatrix * vec4(newPos, 1.0);

    // Get the lighting normal which is the same as the next or prev normal
    // but raised slightly out of the screen
    vec2 temp;
    const float lift = 0.5;
    if (cnrPoint)
    {
        if (abs(prevZ - nextZ) < 1.0)
            temp = (normPrev + normNext) / 2.0;
        else if (nextZ > prevZ)
            temp = normNext;
        else
            temp = normPrev;
    }
    else if (sndPoint)
        temp = normPrev;
    else
        temp = normNext;

    vec3 lightNorm = normalize(vec3(temp, lift));

    // Calculate the lighting and colour
    const vec3 lightDir = vec3(-0.371391, -0.557086, 0.742781); // normalize(vec3(-1.0, -1.5, 2.0));
    float diffuse = dot(lightNorm, lightDir);
    diffuse = diffuse * 0.75 + 0.25;
    vColor = vec4(uColor.xyz * diffuse, uColor.w);
}
