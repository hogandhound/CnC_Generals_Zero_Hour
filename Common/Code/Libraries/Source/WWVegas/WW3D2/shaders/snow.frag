#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex;

layout(location = 0) out vec4 finalColor;

void main() {
    finalColor = texture(tex, gl_PointCoord);
}
