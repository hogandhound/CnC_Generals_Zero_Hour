#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform WorldMatrixUVT {
  mat4 world;
  mat4 uvt;
} push;
layout(set = 0, binding = 0) uniform Projection{
	mat4 m;
} proj;
layout(set = 0, binding = 1) uniform ViewMatrix{
	mat4 m;
} view;

layout(location = 0) in vec3 vert;
layout(location = 1) in vec3 norm;
layout(location = 2) in uint diffuse;
layout(location = 3) in vec2 uv1;
layout(location = 4) in vec2 uv2;

layout(location = 0) out vec3 fragNorm;
layout(location = 1) out vec4 fragDiffuse;
layout(location = 3) out vec2 fragUv1;
layout(location = 4) out vec2 fragUv2;
layout(location = 9) out vec3 fragPos;
layout(location = 10) out vec3 viewPos;

void main() {
	mat3 normalMat = transpose(inverse(mat3(push.world)));
	fragNorm = normalize(normalMat * norm);
	fragPos = (push.world * vec4(vert,1)).xyz;
	viewPos = (inverse(view.m)*vec4(0,0,0,1)).xyz;
	
    // Pass the tex coord straight through to the fragment shader
    fragUv1 = (push.uvt*vec4(uv1,0,0)).xy;
	fragUv2 = uv2;
	fragDiffuse = unpackUnorm4x8(diffuse).bgra;
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
}
