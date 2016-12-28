uniform mat4 uModelMatrix;
uniform mat4 uProjMatrix;
uniform mat4 uNormMatrix;
uniform vec4 uMeshColor;

attribute vec4 aPosition;
attribute vec3 aNormal;

varying vec4 vColor;

void main()
{
    vec3 vPosition = vec3(uModelMatrix * aPosition);
    vec3 vNormal = vec3(uNormMatrix * vec4(aNormal, 0.0));
    const vec3 lightDir = vec3(-0.371391, -0.557086, 0.742781);
    float diffuse = dot(vNormal, lightDir);
    diffuse = diffuse * 0.75 + 0.25;
    vColor = vec4(uMeshColor.xyz * diffuse, uMeshColor.w);

    gl_Position = uProjMatrix * uModelMatrix * aPosition;
}
