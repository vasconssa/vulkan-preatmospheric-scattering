#version 450

layout(set = 1, binding = 0) uniform sampler2D fontTexture[8];
//layout(set=0, binding=0) uniform sampler2D fontTexture;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(fontTexture[0], fragUv);
    outColor = fragColor * texColor;
}
