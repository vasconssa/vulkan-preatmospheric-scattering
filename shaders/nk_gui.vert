#version 450

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in uvec4 a_color;

layout(push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
} push_constants;

layout(location = 0) out vec4 v_fragcolor;
layout(location = 1) out vec2 v_uv;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec2 pos = a_pos;
    pos = pos * push_constants.scale + push_constants.translate;
    v_fragcolor = vec4(float(a_color.x) / 255.0, float(a_color.y) / 255.0, float(a_color.z) / 255.0, float(a_color.w) / 255.0);
    v_uv = a_uv;

    gl_Position = vec4(pos, 0.0, 1.0);
}
