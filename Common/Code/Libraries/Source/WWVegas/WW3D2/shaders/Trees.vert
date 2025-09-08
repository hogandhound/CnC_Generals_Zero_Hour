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

//vertex shader for grass..
// v0   - Vertex Position
// v1   - Normal vector - used for other fun stuff.
//      - v1.x = index for the motion wave data 
//      - v1.y = color scale factor for darkening.
//      - v1.z = z coordinate value of the base of the tree/grass/bush.
// v2   - Diffuse color
// v7   - Vertex Texture Data u,v 
// c4-7  - Composite World-View-Projection Matrix
// c8  - Tilt vector.
// c32  - offset for shroud texture mapping
// c33  - scale for shroud texture mapping
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
layout(set = 0, binding = 10) uniform Trees{
	vec4 c1;
	vec4 tilt[24];
	vec4 shroudOffset;
	vec4 shroudScale;
} tree;

layout(location = 0) in vec3 vert;
layout(location = 1) in vec3 norm;
layout(location = 2) in uint diffuse;
layout(location = 3) in vec2 uv;

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
    fragUv1 = uv;
	
	vec4 localDiffuse = unpackUnorm4x8(diffuse).bgra;
	
	vec4 r2 = vec4(vert,0.0) - vec4(0,0,norm.z,0);
	int offset = int(norm.x);
    vec4 r0 = tree.tilt[offset];
	vec4 r1 = r2.zzzw * r0 + vec4(vert,1.0);
    gl_Position = proj.m*view.m*push.world*r1;
	
	r2 = vec4(norm.yyy,1.0);
	fragDiffuse = localDiffuse * r2;
	fragUv2 = ((vec4(vert,0) + tree.shroudOffset) * tree.shroudScale).xy;
}

#if 0
vs.1.1


mov  r2, v1.wwzw    ; move the z value for the tree into r2
sub  r2, v0, r2     ; subtract the z from the base coordinate.
mov  a0.x, v1       ; get the index for the wave value.
mov  r0, c[a0.x+8]  ; load the correct wave entry for this tree/grass
mad  r1, r2.zzzw,r0,v0 ; multiply the z value by wave value to get the xy skew value for the wave motion
                    ; and add it to the coordinate value.
m4x4 oPos, r1, c4   ; Transform vertex to screen coords.
mov  r2, v1.yyyw    ; Get the color scale from v1.y
mul  oD0, v2,r2 	  ; Scale and move diffuse color into color register.
mov  oT0, v7			  ; Move texture coords into texture register.

add  r1, v0, c32    ; offset coordinate
mul  oT1, r1, c33    ; 
#endif
