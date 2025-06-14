#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "lightCommon.frag.inc"

layout(set = 0, binding = 2) uniform LightCollectionBlock {LightCollection lights;};

layout(set = 0, binding = 3) uniform MaterialBlock { DX8Material material;};
layout( push_constant ) uniform AlphaRef {
  layout(offset=64) float ref;
};

layout(binding = 4) uniform sampler2D tex1;
layout(binding = 5) uniform sampler2D tex2;

layout(location = 0) in vec3 fragNorm;
layout(location = 2) in vec2 fragUv;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 baseColor = texture(tex1, fragUv) * texture(tex2, fragUv);
	if (baseColor.a < ref)
		discard;
	vec4 tempColor = CalculateLights(lights, material, fragNorm, fragPos, viewPos,
		baseColor.rgb, baseColor.rgb, baseColor.rgb);
	finalColor = tempColor;
    tempColor.a = 1.0;
}
