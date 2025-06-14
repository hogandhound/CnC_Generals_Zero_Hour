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

// DX8 self shadowed bump specular with gloss shader
// Kenny Mitchell

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 4) uniform sampler2D texNorm;
layout(binding = 5) uniform sampler2D texDecal;

layout(location = 0) in vec4 fragUv;
layout(location = 1) in vec4 fragSpec;
layout(location = 2) in vec4 fragDiff;
layout(location = 3) in vec4 fragSpecUv;

layout(location = 0) out vec4 finalColor;

void main() {
	vec4 normal = texture(texNorm,fragUv.xy);
    finalColor = normal;
}

// pixel shader version 1.1
#if 0
#include "shd8bumpspec_constants.h"
ps.1.1

tex		TEX_NORMALMAP			// normal map texture
//tex		TEX_DECAL				// decal texture

mov r0,TEX_NORMALMAP

//texcoord TEX_SPECULAR			// specular term
//texcoord t3

// bumped normal
//dp3_sat r1, TEX_NORMALMAP_bx2, COL_LIGHT_bx2

// fill light normal
//add r1, COL_LIGHT.a, r1

// modulate texture and light color
//mul r0, COL_DIFFUSE, TEX_DECAL

// apply light intensity
//mul r0, r0, r1

// specular section
//dp3_sat r1, TEX_NORMALMAP_bx2, t3_bx2

//mul r1, r1, r1
//mul r1, r1, r1

// apply gloss map
//mul r1, TEX_DECAL.a, r1

//mad r0, TEX_SPECULAR, r1, r0

#endif
