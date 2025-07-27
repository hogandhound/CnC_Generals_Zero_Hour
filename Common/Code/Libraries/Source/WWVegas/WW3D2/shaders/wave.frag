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

// Waves pixel shader
// Kenny Mitchell April 2001
// Modified July 2001 Mark Wilczynski

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( push_constant ) uniform Wave {
  mat4 world;
  vec4 texProj;
  vec4 bumpenv;
} push;
layout(binding = 4) uniform sampler2D tex;
layout(binding = 5) uniform sampler2D tex1;

layout(location = 1) in vec2 fragUv0;
layout(location = 2) in vec2 fragUv1;
layout(location = 0) in vec4 fragDiffuse;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 t0 = texture(tex, fragUv0);
	vec2 uv = fragUv1 + push.bumpenv.rg * t0.rr + push.bumpenv.ba * t0.gg;
	vec4 t1 = texture(tex1, uv);
    finalColor = t1 * fragDiffuse;
}


// Declare pixel shader version 1.1
#if 0
ps.1.1

// Define t0 as a standard 3-vector from bumpmap
tex t0

// Perform EMBM to get a local normal bump reflection.
texbem t1, t0       ; compute new (u,v) values

#ifndef DO_WATER_ALPHA_TEXTURE

// result goes in output color multiplied by diffuse
mul r0, t1, v0

#else

//alternate version which uses a third texture which provides per-pixel alpha.
// result goes in output color multiplied by diffuse
tex t2	; get alpha texture
mul r0.rgb, t1, v0
+mul r0.a, t2, v0

#endif



#endif
