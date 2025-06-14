#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex1;

layout(location = 0) in vec3 fragNorm;
layout(location = 2) in vec2 fragUv;
layout(location = 3) in vec2 camuv1;
layout(location = 9) in vec3 fragPos;
layout(location = 10) in vec3 viewPos;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 baseColor = texture(tex1, camuv1);
    if (baseColor.a < 0.01) discard;
	finalColor = baseColor;
}
