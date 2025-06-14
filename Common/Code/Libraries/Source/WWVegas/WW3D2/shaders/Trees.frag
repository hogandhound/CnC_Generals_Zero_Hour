//
//	Command & Conquer Generals Zero Hour(tm)
//	Copyright 2025 Electronic Arts Inc.
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

//////////////////////////////////////////////////////////////////////////////////
////																																						 //
////  (c) 2001-2003 Electronic Arts Inc.																				 //
////																																						 //
//////////////////////////////////////////////////////////////////////////////////

// Trees pixel shader.  Not actually used at this time. jba.
// Created:   John Ahlquist, June 2003
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D tex;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragDiffuse;

layout(location = 0) out vec4 finalColor;
layout(set = 0, binding = 2) uniform Trees{
	vec4 c1;
	vec4 tilt[24];
	vec4 shroudOffset;
	vec4 shroudScale;
} tree;

void main() {
	vec4 t0 = texture(tex, fragUv);
	vec4 r0 = t0 * fragDiffuse;
	r0.rgb = 2 * r0.rgb * tree.c1.rgb;
	
    finalColor = vec4(r0.rgb, t0.a);
}

#if 0
// Declare pixel shader version 1.1
ps.1.1

tex t0	; get texture 0

mov r0, v0
mul r0, r0, t0
mul_x2 r0.rgb, r0, c1					; modulate 2 * c1
mov r0.a, t0.a
#endif
