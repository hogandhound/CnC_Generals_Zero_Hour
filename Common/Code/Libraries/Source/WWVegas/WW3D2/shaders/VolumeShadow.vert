#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vert;

void main() {
    gl_Position = vec4(vert, 1);
}
