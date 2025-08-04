#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform AlphaRef {
  layout(offset=64) float ref;
};
layout(binding = 4) uniform sampler2D tex;

layout(location = 1) in vec2 fragUv;
layout(location = 0) in vec4 fragDiffuse;

layout(location = 0) out vec4 finalColor;

void main() {
    finalColor = texture(tex, fragUv) * fragDiffuse;
	if (finalColor.a > ref)
		discard;
}
