#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(binding = 1) uniform UProj {
    mat4 proj;
} uproj;

layout(location = 0) out vec2 fragTexCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
    gl_Position = uproj.proj * vec4(inPosition, 1.0, 1.0);
    fragTexCoord = inTexCoord;
}
