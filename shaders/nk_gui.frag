#version 450

layout(set = 1, binding = 0) uniform sampler2D font_textures[8];

layout(location = 0) in vec4 v_fragcolor;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 outcolor;

void main() {
    vec4 texcolor = texture(font_textures[0], v_uv);
    outcolor = v_fragcolor * texcolor;
}
