#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex;

layout(location = 1) in vec2 fragUv;
layout(location = 0) in vec4 fragDiffuse;

layout(location = 0) out vec4 finalColor;

void main() {
    vec4 color = texture(tex, fragUv) * fragDiffuse;
	float val = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
	finalColor = vec4(val,val,val,color.a);
}
