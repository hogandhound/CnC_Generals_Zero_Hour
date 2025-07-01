#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex;

layout(location = 0) in vec3 fragNorm;
layout(location = 1) in vec4 fragDiffuse;
layout(location = 2) in vec3 viewDir;
layout(location = 3) in vec2 fragUv;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 baseColor = texture(tex, fragUv) * fragDiffuse;
	finalColor = baseColor;
}
