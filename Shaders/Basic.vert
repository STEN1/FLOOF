#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUv;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 imodel;
} pushConstants;

void main() {
    gl_Position = pushConstants.mvp * vec4(pos, 1.0);
    fragNormal = normalize(mat3(transpose(pushConstants.imodel)) * normal);
    fragUv = uv;
}