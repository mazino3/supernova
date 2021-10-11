#version 450

uniform u_vs_pointsParams {
    mat4 mvpMatrix;
} pointsParams;

in vec3 a_position;

#ifdef HAS_VERTEX_COLOR_VEC3
    in vec3 a_color;
    out vec3 v_color;
#endif

#ifdef HAS_VERTEX_COLOR_VEC4
    in vec4 a_color;
    out vec4 v_color;
#endif

#ifdef HAS_TEXTURE
    in vec2 a_texcoord1;
    out vec2 v_uv1;
#endif


void main() {

    #if defined(HAS_VERTEX_COLOR_VEC3) || defined(HAS_VERTEX_COLOR_VEC4)
        v_color = a_color;
    #endif

    #ifdef HAS_TEXTURE
        v_uv1 = a_texcoord1;
    #endif

    gl_Position = pointsParams.mvpMatrix * vec4(a_position, 1.0);
    gl_PointSize = 30.0;
}