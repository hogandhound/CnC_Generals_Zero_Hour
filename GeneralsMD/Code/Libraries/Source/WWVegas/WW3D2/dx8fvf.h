/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8fvf.h                               $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               * 
 *                                                                                             * 
 *                     $Modtime:: 06/26/02 5:06p                                             $*
 *                                                                                             *
 *                    $Revision:: 7                                                          $*
 *                                                                                             *
 * 06/26/02 KM VB Vertex format update for shaders                                       *
 * 07/17/02 KM VB Vertex format update for displacement mapping                               *
 * 08/01/02 KM VB Vertex format update for cube mapping                               *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#if defined(_MSC_VER)
#pragma once
#endif

#ifndef DX8_FVF_H
#define DX8_FVF_H

#include "always.h"
#ifdef WWDEBUG
#include "wwdebug.h"
#endif

class StringClass;

#define VKFVF_RESERVED0        0x001
#define VKFVF_POSITION_MASK    0x400E
#define VKFVF_XYZ              0x002
#define VKFVF_XYZRHW           0x004
#define VKFVF_XYZB1            0x006
#define VKFVF_XYZB2            0x008
#define VKFVF_XYZB3            0x00a
#define VKFVF_XYZB4            0x00c
#define VKFVF_XYZB5            0x00e
#define VKFVF_XYZW             0x4002

#define VKFVF_NORMAL           0x010
#define VKFVF_PSIZE            0x020
#define VKFVF_DIFFUSE          0x040
#define VKFVF_SPECULAR         0x080

#define VKFVF_TEXCOUNT_MASK    0xf00
#define VKFVF_TEXCOUNT_SHIFT   8
#define VKFVF_TEX0             0x000
#define VKFVF_TEX1             0x100
#define VKFVF_TEX2             0x200
#define VKFVF_TEX3             0x300
#define VKFVF_TEX4             0x400
#define VKFVF_TEX5             0x500
#define VKFVF_TEX6             0x600
#define VKFVF_TEX7             0x700
#define VKFVF_TEX8             0x800

#define VKFVF_LASTBETA_UBYTE4   0x1000
#define VKFVF_LASTBETA_D3DCOLOR 0x8000

#define VKFVF_TEXTUREFORMAT2 0         // Two floating point values
#define VKFVF_TEXTUREFORMAT1 3         // One floating point value
#define VKFVF_TEXTUREFORMAT3 1         // Three floating point values
#define VKFVF_TEXTUREFORMAT4 2         // Four floating point values
#define VKFVF_TEXCOORDSIZE3(CoordIndex) (VKFVF_TEXTUREFORMAT3 << (CoordIndex*2 + 16))
#define VKFVF_TEXCOORDSIZE2(CoordIndex) (VKFVF_TEXTUREFORMAT2)
#define VKFVF_TEXCOORDSIZE4(CoordIndex) (VKFVF_TEXTUREFORMAT4 << (CoordIndex*2 + 16))
#define VKFVF_TEXCOORDSIZE1(CoordIndex) (VKFVF_TEXTUREFORMAT1 << (CoordIndex*2 + 16))

#define VKFVF_RESERVED2         0x6000  // 2 reserved bits
enum {
	//DX8_FVF_XYZ				= VKFVF_XYZ,
	//DX8_FVF_XYZN			= VKFVF_XYZ|VKFVF_NORMAL,
	DX8_FVF_XYZD			= VKFVF_XYZ|VKFVF_DIFFUSE,
	DX8_FVF_XYZNUV1		= VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX1,
	DX8_FVF_XYZNUV2		= VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX2,
	DX8_FVF_XYZNDUV1		= VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX1|VKFVF_DIFFUSE,
	DX8_FVF_XYZNDUV2		= VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX2|VKFVF_DIFFUSE,
	DX8_FVF_XYZDUV1		= VKFVF_XYZ|VKFVF_TEX1|VKFVF_DIFFUSE,
	DX8_FVF_XYZDUV2		= VKFVF_XYZ|VKFVF_TEX2|VKFVF_DIFFUSE,
	//DX8_FVF_XYZUV1			= VKFVF_XYZ|VKFVF_TEX1,
	//DX8_FVF_XYZUV2			= VKFVF_XYZ|VKFVF_TEX2,
 	//DX8_FVF_XYZNDUV1TG3	= (VKFVF_XYZ|VKFVF_NORMAL|VKFVF_DIFFUSE|VKFVF_TEX4|VKFVF_TEXCOORDSIZE2(0)|VKFVF_TEXCOORDSIZE3(1)|VKFVF_TEXCOORDSIZE3(2)|VKFVF_TEXCOORDSIZE3(3)),
 	//DX8_FVF_XYZNUV2DMAP	= (VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX3 | VKFVF_TEXCOORDSIZE1(0) | VKFVF_TEXCOORDSIZE4(1) | VKFVF_TEXCOORDSIZE2(2) ),
	DX8_FVF_XYZNDCUBEMAP	= VKFVF_XYZ|VKFVF_NORMAL|VKFVF_DIFFUSE //|VKFVF_TEX1|VKFVF_TEXCOORDSIZE3(0)
};

// ----------------------------------------------------------------------------
//
// Util structures for vertex buffer handling. Cast the void pointer returned
// by the vertex buffer to one of these structures.
//
// ----------------------------------------------------------------------------

struct VertexFormatXYZ
{
	float x;
	float y;
	float z;
};

struct VertexFormatXYZNUV1
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u1;
	float v1;
};

struct VertexFormatXYZNUV2
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZN
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
};

struct VertexFormatXYZNDUV1
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
};

struct VertexFormatXYZNDUV2
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZDUV1
{
	float x;
	float y;
	float z;
	unsigned diffuse;
	float u1;
	float v1;
};

struct VertexFormatXYZD
{
	float x;
	float y;
	float z;
	unsigned diffuse;
};

struct VertexFormatXYZDUV2
{
	float x;
	float y;
	float z;
	unsigned diffuse;
	float u1;
	float v1;
	float u2;
	float v2;
};

struct VertexFormatXYZUV1
{
	float x;
	float y;
	float z;
	float u1;
	float v1;
};

struct VertexFormatXYZUV2
{
	float x;
	float y;
	float z;
	float u1;
	float v1;
	float u2;
	float v2;
};

// todo KJM compress
struct VertexFormatXYZNDUV1TG3
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
	float u1;
	float v1;
	float Sx;
	float Sy;
	float Sz;
	float Tx;
	float Ty;
	float Tz;
	float SxTx;
	float SxTy;
	float SxTz;
};


// displacement mapping format
struct VertexFormatXYZNUV2DMAP
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float T1x;
	float T1y;
	float T1z;
	float T1w;
	float T2x;
	float T2y;
};

// cube map format (texcoords are normally generated)
struct VertexFormatXYZNDCUBEMAP
{
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	unsigned diffuse;
//	float u1;
//	float v1;
//	float w1;
};

// FVF info class can be created for any legal FVF. It constructs information
// of offsets to various elements in the vertex buffer.

class FVFInfoClass : public W3DMPO
{
	W3DMPO_GLUE(FVFInfoClass)

	mutable unsigned						FVF;
	mutable unsigned						fvf_size;

	unsigned							location_offset;
	unsigned							normal_offset;
	unsigned							blend_offset;
#ifdef INFO_VULKAN
	unsigned							texcoord_offset[D3DDP_MAXTEXCOORD];	
#else
	unsigned							texcoord_offset[8];	
#endif
	unsigned							diffuse_offset;
	unsigned							specular_offset;
public:
	FVFInfoClass(unsigned FVF, unsigned vertex_size=0);

	inline unsigned Get_Location_Offset() const { return location_offset; }
	inline unsigned Get_Normal_Offset() const { return normal_offset; }
#ifdef WWDEBUG
	inline unsigned Get_Tex_Offset(unsigned int n) const {
#ifdef INFO_VULKAN
		WWASSERT(n<D3DDP_MAXTEXCOORD); 
#endif
		return texcoord_offset[n]; }	
#else
	inline unsigned Get_Tex_Offset(unsigned int n) const { return texcoord_offset[n]; }	
#endif

	inline unsigned Get_Diffuse_Offset() const { return diffuse_offset; }
	inline unsigned Get_Specular_Offset() const { return specular_offset; }
	inline unsigned Get_FVF() const { return FVF; }
	inline unsigned Get_FVF_Size() const { return fvf_size; }

	void Get_FVF_Name(StringClass& fvfname) const;	// For debug purposes

	// for enabling vertex shaders
	inline void Set_FVF(unsigned fvf) const { FVF=fvf; }
	inline void Set_FVF_Size(unsigned size) const { fvf_size=size; }
};


#endif
