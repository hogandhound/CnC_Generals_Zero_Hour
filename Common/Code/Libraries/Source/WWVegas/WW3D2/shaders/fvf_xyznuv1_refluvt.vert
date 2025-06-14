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
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 fragNorm;
layout(location = 2) out vec2 fragUv;
layout(location = 3) out vec2 reflUv;
layout(location = 9) out vec3 fragPos;
layout(location = 10) out vec3 viewPos;

void main() {
	mat3 normalMat = transpose(inverse(mat3(push.world)));
	fragNorm = normalize(normalMat * norm);
	fragPos = (push.world * vec4(vert,1)).xyz;
	viewPos = (inverse(view.m)*vec4(0,0,0,1)).xyz;
    fragUv = uv;
	
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
	vec3 refl = vert - 2 * dot(vert,norm) * norm;
	reflUv = (push.uvt*view.m*push.world*vec4(refl, 1)).xy;
}
