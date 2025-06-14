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
layout(location = 1) in uint diffuse;

layout(location = 0) out vec4 fragDiffuse;

void main() {
    // Pass the tex coord straight through to the fragment shader
	fragDiffuse = unpackUnorm4x8(diffuse).bgra;
    
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
}
