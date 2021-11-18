#version 450

layout (location = 0) out vec2 v_uv;
layout (location = 1) out vec3 camera_ray;

layout(set=0, binding=0) uniform u_global_ubo {
    mat4 projection;
    mat4 view;
    mat4 projection_view;
    mat4 inverse_view;
    mat4 inverse_projection;
    vec4 light_position[4];
    vec4 camera_position;
    vec4 exposure_gama;
} global_ubo;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	v_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vec4 cp = vec4(v_uv * 2.0f - 1.0f, 0.0f, 1.0f);
    vec4 p = global_ubo.inverse_projection * cp;
    camera_ray = mat3(global_ubo.inverse_view) * p.xyz;

    gl_Position = cp;
}

