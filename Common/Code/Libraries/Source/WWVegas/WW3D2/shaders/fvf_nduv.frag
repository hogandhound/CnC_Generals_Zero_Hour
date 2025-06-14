#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "lightCommon.frag.inc"

layout(set = 0, binding = 2) uniform LightCollectionBlock {LightCollection lights;};

layout(set = 0, binding = 3) uniform MaterialBlock { DX8Material material;};

layout(binding = 4) uniform sampler2D tex;

layout(location = 0) in vec3 fragNorm;
layout(location = 1) in vec4 fragDiffuse;
layout(location = 3) in vec2 fragUv;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 baseColor = texture(tex, fragUv) * fragDiffuse;
	finalColor = CalculateLights(lights, material, fragNorm, fragPos, viewPos,
 baseColor.rgb, baseColor.rgb, baseColor.rgb);
    finalColor.a = baseColor.a;
    if (baseColor.a < 0.01) discard;
}
