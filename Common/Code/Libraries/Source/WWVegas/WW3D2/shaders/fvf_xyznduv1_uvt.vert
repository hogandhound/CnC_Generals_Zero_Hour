#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform WorldMatrixUvt {
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
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragNorm;
layout(location = 1) out vec4 fragDiffuse;
layout(location = 2) out vec3 viewDir;
layout(location = 3) out vec2 fragUv;

void main() {
    // Pass the tex coord straight through to the fragment shader
    fragUv = uv;
	fragDiffuse = unpackUnorm4x8(diffuse).bgra;
    
	fragNorm = normalize(push.world * vec4(norm,0)).xyz;
	viewDir = normalize(vec3(view.m[0].z, view.m[1].z, view.m[2].z)); //view.m[2].rgb
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
}
