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
////																																					   //
////  (c) 2001-2003 Electronic Arts Inc.																				 //
////																																						 //
//////////////////////////////////////////////////////////////////////////////////

// Waves vertex shader
// Kenny Mitchell April 2001
// Modified July 2001 Mark Wilczynski

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform Wave {
  mat4 world;
  vec4 texProj;
  vec4 bumpenv;
} push;
layout(set = 0, binding = 0) uniform Projection{
	mat4 m;
} proj;
layout(set = 0, binding = 1) uniform ViewMatrix{
	mat4 m;
} view;

layout(location = 0) in vec3 vert;
layout(location = 1) in vec2 uv;
layout(location = 2) in uint diffuse;

layout(location = 1) out vec2 fragUv0;
layout(location = 2) out vec2 fragUv1;
layout(location = 0) out vec4 fragDiffuse;

void main() {
    // Pass the tex coord straight through to the fragment shader
    fragUv0 = uv;
	fragDiffuse = unpackUnorm4x8(diffuse).bgra;
    
	vec4 r0 = proj.m*view.m*push.world*vec4(vert, 1);
    gl_Position = r0;
	
	vec4 r1 = vec4(0,0,0,1.0/r0.w);
	r1.xy = r0.xz * r1.w;
	
	fragUv1 = r1.xy * push.texProj.xy + push.texProj.zw;
}

#if 0
#define CV_ZERO 0
#define CV_ONE 1

#define CV_WORLDVIEWPROJ_0 2
#define CV_WORLDVIEWPROJ_1 3
#define CV_WORLDVIEWPROJ_2 4
#define CV_WORLDVIEWPROJ_3 5

#define CV_TEXPROJ_0 6
#define CV_TEXPROJ_1 7
#define CV_TEXPROJ_2 8
#define CV_TEXPROJ_3 9
#define CV_PATCH_SCALE_OFFSET 10

#define V_POSITION v0
#define V_DIFFUSE v1
#define V_TEXTURE v2
#define V_TEXTURE2 v3
vs.1.1

// Below is Kenny's new optimized version
// Transform position to clip space and output it
dp4 r0.x, V_POSITION, c[CV_WORLDVIEWPROJ_0]
dp4 r0.y, V_POSITION, c[CV_WORLDVIEWPROJ_1]
dp4 r0.z, V_POSITION, c[CV_WORLDVIEWPROJ_2]
dp4 r0.w, V_POSITION, c[CV_WORLDVIEWPROJ_3]

mov oPos, r0

// get 1/w and multiply it onto x and y
rcp r1.w, r0.w
mul r1.xy, r0.xy, r1.w

// scale/flip/offset tex coords to screen
mad oT1.xy, r1.xy, c[CV_TEXPROJ_0].xy,c[CV_TEXPROJ_0].zw

mov oT0, V_TEXTURE
mov oD0, V_DIFFUSE

#ifdef DO_WATER_ALPHA_TEXTURE
//generate uv coordinates for a third texture (alpha channel)
mad oT2.xy,v0.xz,c[CV_PATCH_SCALE_OFFSET].zw,c[CV_PATCH_SCALE_OFFSET].xy
#endif
#endif
