#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex1;
layout(binding = 5) uniform sampler2D tex2;

layout(location = 0) in vec4 fragDiffuse;
layout(location = 1) in vec2 fragUv1;
layout(location = 2) in vec2 fragUv2;
layout(location = 3) in vec2 camuv1;

layout(location = 0) out vec4 finalColor;

void main() {
    finalColor = texture(tex1, camuv1) * texture(tex2, fragUv2) * fragDiffuse;
}
