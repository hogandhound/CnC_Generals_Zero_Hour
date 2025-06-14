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

// DX8 bump diffuse shader
// Kenny Mitchell
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D texNorm;
layout(binding = 5) uniform sampler2D texDecal;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragSpec;
layout(location = 2) in vec4 fragDiff;

layout(location = 0) out vec4 finalColor;

void main() {
	vec3 normal = 2.0 * (texture(texNorm,fragUv).rgb - 0.5) * fragSpec.xyz; 
	vec4 decal = texture(texDecal,fragUv);
	decal.rgb = fragDiff.xyz * decal.rgb;
	
	vec4 r1 = vec4(clamp(decal.rgb * -normal, 0.0, 1.0),0.0);
	vec4 r0 = vec4(clamp(decal.rgb * normal, 0.0, 1.0),0.0);
	r0 = r0 + r1;
	r0 = decal * fragSpec.a + r0;
    finalColor = r0;
}

#if 0

#include "shd8bumpdiff_constants.h"

// pixel shader version 1.1
ps.1.1

tex		TEX_NORMALMAP			// normal map texture
tex		TEX_DECAL				// decal texture

// bumped normal
dp3 TEX_NORMALMAP, TEX_NORMALMAP_bx2, COL_LIGHT_bx2

// modulate texture and light color
mul TEX_DECAL.rgb, COL_DIFFUSE, TEX_DECAL

// apply bump color in reverse direction with saturation
mul_sat r1, TEX_DECAL, -TEX_NORMALMAP

// apply bump color in primary direction with saturation
mul_sat r0, TEX_DECAL, TEX_NORMALMAP

// add bump light contribution
add r0, r0, r1

// add key light contribution
mad r0, TEX_DECAL, COL_LIGHT.a, r0
#endif
