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

layout(location = 0) in vec3 vert;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 fragNorm;
layout(location = 2) out vec2 fragUv;
layout(location = 9) out vec3 fragPos;
layout(location = 10) out vec3 viewPos;

void main() {
	mat3 normalMat = transpose(inverse(mat3(push.world)));
	fragNorm = normalize(normalMat * norm);
	fragPos = (push.world * vec4(vert,1)).xyz;
	viewPos = (inverse(view.m)*vec4(0,0,0,1)).xyz;
    // Pass the tex coord straight through to the fragment shader
    fragUv = uv;
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
}
