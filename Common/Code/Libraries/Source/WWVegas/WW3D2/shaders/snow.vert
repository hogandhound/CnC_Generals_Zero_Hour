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

void main() {
    // Pass the tex coord straight through to the fragment shader
	gl_PointSize = 1.0;
    gl_Position = proj.m*view.m*push.world*vec4(vert, 1);
}
