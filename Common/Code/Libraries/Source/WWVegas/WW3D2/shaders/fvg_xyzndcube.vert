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
layout(location = 2) in vec3 uv;
layout(location = 3) in uint diffuse;

layout(location = 0) out vec3 fragNorm;
layout(location = 1) out vec3 fragUv;
layout(location = 2) out vec4 fragDiffuse;

void main() {
    // Pass the tex coord straight through to the fragment shader
    fragUv = uv;
	fragNorm = normalize(push.world * vec4(norm,0)).xyz;
	//viewDir = normalize(vec3(view.m[0].z, view.m[1].z, view.m[2].z)); //view.m[2].rgb
	fragDiffuse = unpackUnorm4x8(diffuse).bgra;
    
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
}
