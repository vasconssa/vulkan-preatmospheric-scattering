#version 450


layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in uvec4 color;

layout(push_constant) uniform PushConstants {
    vec2 scale;
    vec2 translate;
} push_constants;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUv;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec2 pos = position;
    pos.x = pos.x * push_constants.scale.x + push_constants.translate.x;
    pos.y = pos.y * push_constants.scale.y + push_constants.translate.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    fragColor = vec4(float(color.x)/255, float(color.y)/255, float(color.z)/255, float(color.w)/255);
    fragUv = uv;
}
