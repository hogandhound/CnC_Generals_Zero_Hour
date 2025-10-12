#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex1;
layout( push_constant ) uniform AlphaRef {
  layout(offset=64) float ref;
};

layout(location = 0) in vec3 fragNorm;
layout(location = 1) in vec4 fragDiffuse;
layout(location = 2) in vec3 viewDir;
layout(location = 3) in vec2 fragUv1;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 baseColor = texture(tex1, fragUv1) * fragDiffuse;
	if (baseColor.a < ref)
		discard;
	finalColor = baseColor;
    finalColor.a = 1.0;
}
