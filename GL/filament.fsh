#ifdef GL_ES
  precision mediump float;
/*#else
    #define USE_Z
#endif

#extension EXT_frag_depth: enable
#ifdef GL_EXT_frag_depth
    #define USE_Z
#endif

#ifdef USE_Z
varying float vZOff;*/
#endif

varying vec4 vColor;

void main()
{
    gl_FragColor = vColor;

/*#ifndef GL_ES
    gl_FragDepth = gl_FragCoord.z + abs(vZOff);
#endif
#ifdef GL_EXT_frag_depth
    gl_FragDepthEXT = gl_FragCoord.z + abs(vZOff);
#endif*/
}

// TODO: the depths are not calculated correctly for intersecting points
