#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 fragNorm;
layout(location = 1) in vec4 fragDiffuse;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 baseColor = fragDiffuse;
}
