#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform WorldMatrix {
  mat4 world;
} push;
layout(set = 0, binding = 0) uniform Projection{
	mat4 m;
} proj;
layout(set = 0, binding = 1) uniform ViewMatrix{
	mat4 m;
} view;
layout(set = 0, binding = 2) uniform UVT2{
	mat4 uvt1;
	mat4 uvt2;
} uvt;

layout(location = 0) in vec3 vert;
layout(location = 1) in uint diffuse;
layout(location = 2) in vec2 uv1;
layout(location = 3) in vec2 uv2;

layout(location = 0) out vec4 fragDiffuse;
layout(location = 1) out vec2 fragUv1;
layout(location = 2) out vec2 fragUv2;
layout(location = 3) out vec2 camuv1;
layout(location = 4) out vec2 camuv2;

void main() {
    // Pass the tex coord straight through to the fragment shader
    fragUv1 = uv1;
    fragUv2 = uv2;
	fragDiffuse = unpackUnorm4x8(diffuse).bgra;
    
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
	vec4 camuv = view.m*push.world*vec4(vert, 1);
	camuv1 = (uvt.uvt1*camuv).xy;
	camuv2 = (uvt.uvt2*camuv).xy;
}
