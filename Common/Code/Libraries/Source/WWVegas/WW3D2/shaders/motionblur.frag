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

// Motion blur pixel shader
// Created:   John Ahlquist, April 2002
// Currently unused prototype code.  jba.

#version 450
#extension GL_ARB_separate_shader_objects : enable
layout( push_constant ) uniform BumpEnv {
  layout ( offset = 64) vec4 bumpenv;
  vec4 c0;
} push;
layout(binding = 4) uniform sampler2D tex0;
layout(binding = 5) uniform sampler2D tex1;
layout(binding = 6) uniform sampler2D tex2;
layout(binding = 7) uniform sampler2D tex3;

layout(location = 1) in vec2 fragUv;
layout(location = 0) in vec4 fragDiffuse;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 t0 = texture(tex0, fragUv);
	vec4 t1 = texture(tex1, fragUv);
	vec4 t3 = texture(tex3, fragUv);
	vec2 uv = fragUv + push.bumpenv.rg * t1.rr + push.bumpenv.ba * t1.gg;
	vec4 t2 = texture(tex2, uv);
	
	vec4 r0 = vec4(0,0,0,0);
	r0.rgb = fragDiffuse.xyz * t0.rgb;
	r0.a = fragDiffuse.a + t0.a;
	vec4 r1 = t2 * push.c0;
	r0.rgb = r0.rgb + r1.rgb;
	r0.a = r0.a + r0.a * t3.a;
	
    finalColor = r0;
}

// Declare pixel shader version 1.1
#if 0
ps.1.1

tex t0
// Define t0 as a standard 3-vector from bumpmap
tex t1

// Perform EMBM to get a local normal bump reflection.
texbem t2, t1       ; compute new (u,v) values
tex t3
// result goes in output color multiplied by diffuse
//mul r0, t2, v0

mul r0.rgb, v0, t0
+add r0.a, v0, t0
mul r1, t2, c0
add r0.rgb, r0, r1
+mul r0.a, r0, t3

#endif
