#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex1; //uv1
layout(binding = 5) uniform sampler2D tex2; //uv1
layout(binding = 6) uniform sampler2D tex3; //uvcam
layout(binding = 7) uniform sampler2D tex4; //uv2

layout(location = 0) in vec4 fragDiffuse;
layout(location = 1) in vec2 fragUv1;
layout(location = 2) in vec2 fragUv2;
layout(location = 3) in vec2 fragCamUv1;
layout(location = 4) in vec2 fragCamUv2;

layout(location = 0) out vec4 finalColor;

void main() {
    vec4 r0 = fragDiffuse * texture(tex1, fragCamUv1) * texture(tex2, fragUv1) * texture(tex3, fragCamUv2) * texture(tex4, fragUv2);
	finalColor = r0;
}
