#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform Water {
  vec4 bumpenv;
  vec4 waterReduce;
} push;

layout(binding = 4) uniform sampler2D tex1; //uv1
layout(binding = 5) uniform sampler2D tex2; //uv1
layout(binding = 6) uniform sampler2D tex3; //uv1

layout(location = 0) in vec4 fragDiffuse;
layout(location = 1) in vec2 fragUv1;
layout(location = 2) in vec2 fragUv2;

layout(location = 0) out vec4 finalColor;

void main() {

	vec4 t1 = texture(tex2, fragUv2);
	vec2 uv = fragUv2 + push.bumpenv.rg * t1.rr + push.bumpenv.ba * t1.gg;
	vec4 t2 = texture(tex3, uv);
	vec4 r0 = fragDiffuse * texture(tex1, fragUv1);
	vec4 r1 = t2 * push.waterReduce; 
	r0.rgb += r1.rgb;

	finalColor = r0;
}
