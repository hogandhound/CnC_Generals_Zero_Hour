#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex1; //uv1
layout(binding = 5) uniform sampler2D tex2; //uv1
layout(binding = 6) uniform sampler2D tex3; //uvcam
layout(binding = 7) uniform sampler2D tex4; //uv2

layout(location = 0) in vec4 fragDiffuse;
layout(location = 1) in vec2 fragUv1;
layout(location = 3) in vec2 fragUv2; //Modified Cam
layout(location = 4) in vec2 fragUv3; //Modified Cam 2

layout(location = 0) out vec4 finalColor;

void main() {
    vec4 r0 = fragDiffuse * texture(tex1, fragUv1);
	r0.rgb = (texture(tex2, fragUv1) * texture(tex3, fragUv2) + r0).rgb * texture(tex4, fragUv3).rgb;
	r0.a = 0.1;
	finalColor = r0;
}
#if 0

			"ps.1.1\n \
			tex t0 ;get water texture\n\
			tex t1 ;get white highlights on black background\n\
			tex t2 ;get white highlights with more tiling\n\
			tex t3	; get black shroud \n\
			mul r0,v0,t0 ; blend vertex color and alpha into base texture. \n\
			mad r0.rgb, t1, t2, r0	; blend sparkles and noise \n\
			mul r0.rgb, r0, t3 ; blend in black shroud \n\
			;\n";
#endif
