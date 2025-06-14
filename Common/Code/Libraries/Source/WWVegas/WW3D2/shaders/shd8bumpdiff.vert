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

// DX8 bump diffuse vertex shader
// Kenny Mitchell - Westwood Studios EA 2002

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
layout(set = 0, binding = 2) uniform SSBumpDiff{
	mat4 m;
	vec4 lightDir[4];
	vec4 lightColor[4];
	vec4 diffuse;
	vec4 ambient;
	//vec4 constVal; 0.0f, 1.0f, 0.5f, 2.0f
	vec4 bumpiness;
} bump;


layout(location = 0) in vec3 vert;
layout(location = 1) in vec3 vS;
layout(location = 2) in vec3 vT;
layout(location = 3) in vec3 vSxT;
layout(location = 4) in vec3 norm;
layout(location = 5) in vec4 diffuse;
layout(location = 6) in vec2 uv;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec4 fragSpec;
layout(location = 2) out vec4 fragDiff;

void main() {
	vec4 eye = proj.m * view.m * push.world * vec4(vert,1.0);
    gl_Position = eye;
	vec3 wS = (push.world * vec4(vS,0)).xyz;
	vec3 wT = (push.world * vec4(vT,0)).xyz;
	vec3 wSxT = (push.world * vec4(vSxT,0)).xyz;
	
	vec4 lightLocal = vec4(
		dot(wS,bump.lightDir[0].xyz), 
		dot(wT,bump.lightDir[0].xyz), 
		dot(wSxT,bump.lightDir[0].xyz), 0.0);
	lightLocal.w = inversesqrt(dot(lightLocal.xyz,lightLocal.xyz));
	lightLocal = lightLocal * lightLocal.w;
	
	vec4 worldNorm = push.world * vec4(norm,0.0);
	worldNorm.w = inversesqrt(dot(worldNorm.xyz,worldNorm.xyz));
	worldNorm = worldNorm * worldNorm.w;
	
	vec4 light = vec4(0,0,0,0);
	light.x = clamp(dot(worldNorm.xyz,bump.lightDir[0].xyz),0,1) * bump.lightColor[0].w;
	light.y = clamp(dot(worldNorm.xyz,bump.lightDir[1].xyz),0,1) * bump.lightColor[1].w;
	light.z = clamp(dot(worldNorm.xyz,bump.lightDir[2].xyz),0,1) * bump.lightColor[2].w;
	light.w = clamp(dot(worldNorm.xyz,bump.lightDir[3].xyz),0,1) * bump.lightColor[3].w;
	
	vec4 col = bump.lightColor[0] * light.x + bump.lightColor[1] * light.y + bump.lightColor[2] * light.z + bump.lightColor[3] * light.w;
	
	col = col * diffuse * bump.diffuse;
	fragSpec = col + bump.ambient;
	
	vec4 constVal = vec4(0.0, 1.0, 0.5, 2.0);
	lightLocal.xyz = (lightLocal.xyz + constVal.yyy) * constVal.zzz * bump.bumpiness.xxx;
	fragDiff.xyz = vec3(lightLocal.xyz + bump.bumpiness.yyy);
	
	light.x = (constVal.y - light.x) * bump.lightColor[0].x;
	light.y = (constVal.y - light.y) * bump.lightColor[0].y;
	fragDiff.w = light.x + light.y;
	
	fragUv = uv;
}


//Converted to GLSL
#if 0
vs.1.1

#include "shdhw_constants.h"

#include "shd8bumpdiff_constants.h"

// In:
//	v0 - object space vertex position
//	v1 - object space normal
//	v2 - color
//	v3 - texture coords
//	v4 - S basis
//	v5 - T basis
//	v6 - SxT

// object space vertex position -> screen (early as possible for view clipping)
dp4 EYE_VECTOR.x, V_POSITION, c[CV_WORLD_VIEW_PROJECTION_0]
dp4 EYE_VECTOR.y, V_POSITION, c[CV_WORLD_VIEW_PROJECTION_1]
dp4 EYE_VECTOR.z, V_POSITION, c[CV_WORLD_VIEW_PROJECTION_2]
dp4 EYE_VECTOR.w, V_POSITION, c[CV_WORLD_VIEW_PROJECTION_3]

mov oPos, EYE_VECTOR

// Transform basis vectors to world space
dp3 S_WORLD.x, V_S, c[CV_WORLD_0]
dp3 S_WORLD.y, V_S, c[CV_WORLD_1]
dp3 S_WORLD.z, V_S, c[CV_WORLD_2]

dp3 T_WORLD.x, V_T, c[CV_WORLD_0]
dp3 T_WORLD.y, V_T, c[CV_WORLD_1]
dp3 T_WORLD.z, V_T, c[CV_WORLD_2]

dp3 SxT_WORLD.x, V_SxT, c[CV_WORLD_0]
dp3 SxT_WORLD.y, V_SxT, c[CV_WORLD_1]
dp3 SxT_WORLD.z, V_SxT, c[CV_WORLD_2]

// transform light 0 by basis vectors to put it into texture space
dp3 LIGHT_LOCAL.x, S_WORLD.xyz, c[CV_LIGHT_DIRECTION_0]
dp3 LIGHT_LOCAL.y, T_WORLD.xyz, c[CV_LIGHT_DIRECTION_0]
dp3 LIGHT_LOCAL.z, SxT_WORLD.xyz, c[CV_LIGHT_DIRECTION_0]

// Normalize the light vector
dp3 LIGHT_LOCAL.w, LIGHT_LOCAL, LIGHT_LOCAL
rsq LIGHT_LOCAL.w, LIGHT_LOCAL.w
mul LIGHT_LOCAL, LIGHT_LOCAL, LIGHT_LOCAL.w

dp3 WORLD_NORMAL.x, V_NORMAL, c[CV_WORLD_0]
dp3 WORLD_NORMAL.y, V_NORMAL, c[CV_WORLD_1]
dp3 WORLD_NORMAL.z, V_NORMAL, c[CV_WORLD_2]

// Normalize the world normal vector
dp3 WORLD_NORMAL.w, WORLD_NORMAL, WORLD_NORMAL
rsq WORLD_NORMAL.w, WORLD_NORMAL.w
mul WORLD_NORMAL, WORLD_NORMAL, WORLD_NORMAL.w
// calculate light 0 factor
dp3 LIGHT_0.w, WORLD_NORMAL, c[CV_LIGHT_DIRECTION_0]	// L.N
max LIGHT_0.w, LIGHT_0.w, c[CV_CONST].x				// clamp 0-1
mul LIGHT_0.w, LIGHT_0.w, c[CV_LIGHT_COLOR_0].w		// light attentuation factor

// calculate light 1 factor
dp3 LIGHT_1.w, WORLD_NORMAL, c[CV_LIGHT_DIRECTION_1]	// L.N
max LIGHT_1.w, LIGHT_1.w, c[CV_CONST].x				// clamp 0-1
mul LIGHT_1.w, LIGHT_1.w, c[CV_LIGHT_COLOR_1].w		// light attentuation factor

// calculate light 2 factor
dp3 LIGHT_2.w, WORLD_NORMAL, c[CV_LIGHT_DIRECTION_2]	// L.N
max LIGHT_2.w, LIGHT_2.w, c[CV_CONST].x				// clamp 0-1
mul LIGHT_2.w, LIGHT_2.w, c[CV_LIGHT_COLOR_2].w		// light attentuation factor

// calculate light 3 factor
dp3 LIGHT_3.w, WORLD_NORMAL, c[CV_LIGHT_DIRECTION_3]	// L.N
max LIGHT_3.w, LIGHT_3.w, c[CV_CONST].x				// clamp 0-1
mul LIGHT_3.w, LIGHT_3.w, c[CV_LIGHT_COLOR_3].w		// light attentuation factor

// accumulate light colors
mul COL, c[CV_LIGHT_COLOR_0], LIGHT_0.w
mad COL, c[CV_LIGHT_COLOR_1], LIGHT_1.w, COL
mad COL, c[CV_LIGHT_COLOR_2], LIGHT_2.w, COL
mad COL, c[CV_LIGHT_COLOR_3], LIGHT_3.w, COL

// apply vertex color and diffuse and ambient material terms
mul COL, COL, V_DIFFUSE
mul COL, COL, c[CV_DIFFUSE]
add oD1, COL, c[CV_AMBIENT]
// Scale to 0-1
add LIGHT_LOCAL, LIGHT_LOCAL, c[CV_CONST].yyy
mul LIGHT_LOCAL, LIGHT_LOCAL, c[CV_CONST].zzz

// apply bump scale and bias controls
mul LIGHT_LOCAL, LIGHT_LOCAL, c[CV_BUMPINESS].xxx
add oD0, LIGHT_LOCAL, c[CV_BUMPINESS].yyy


// calc light factor excluding bumped lights
add LIGHT_0.w, c[CV_CONST].y, -LIGHT_0.w
add LIGHT_1.w, c[CV_CONST].y, -LIGHT_1.w

mul LIGHT_0.w, c[CV_LIGHT_COLOR_0], LIGHT_0.w
mul LIGHT_1.w, c[CV_LIGHT_COLOR_1], LIGHT_1.w
add oD0.w, LIGHT_0.w, LIGHT_1.w

mov oT0, V_TEXTURE
mov oT1, V_TEXTURE
#endif
