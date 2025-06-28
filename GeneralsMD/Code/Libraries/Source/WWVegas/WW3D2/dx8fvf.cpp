/*
**	Command & Conquer Generals Zero Hour(tm)
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

#include "dx8fvf.h"
#include "wwstring.h"

static unsigned Get_FVF_Vertex_Size(unsigned FVF)
{
#ifdef TODO_VULKAN
	return D3DXGetFVFVertexSize(FVF);
	#else
	return 0;
#endif
}

FVFInfoClass::FVFInfoClass(unsigned FVF_, unsigned vertex_size) 
	:
	FVF(FVF_),
	fvf_size(FVF!=0 ? Get_FVF_Vertex_Size(FVF) : vertex_size)
{
	location_offset=0;
	blend_offset=location_offset;
	
	if ((FVF&VKFVF_XYZ)==VKFVF_XYZ) blend_offset+=3*sizeof(float);
	normal_offset=blend_offset;

	if ( ((FVF&VKFVF_XYZB4)==VKFVF_XYZB4) &&
		  ((FVF&VKFVF_LASTBETA_UBYTE4)==VKFVF_LASTBETA_UBYTE4) ) normal_offset+=3*sizeof(float)+sizeof(DWORD);
	diffuse_offset=normal_offset;

	if ((FVF&VKFVF_NORMAL)==VKFVF_NORMAL) diffuse_offset+=3*sizeof(float);
	specular_offset=diffuse_offset;

	if ((FVF&VKFVF_DIFFUSE)==VKFVF_DIFFUSE) specular_offset+=sizeof(DWORD);
	texcoord_offset[0]=specular_offset;

	if ((FVF&VKFVF_SPECULAR)==VKFVF_SPECULAR) texcoord_offset[0]+=sizeof(DWORD);	

#ifdef TODO_VULKAN
	for (unsigned int i=1; i<D3DDP_MAXTEXCOORD; i++)
	{
		texcoord_offset[i]=texcoord_offset[i-1];

		if ((int(FVF)&VKFVF_TEXCOORDSIZE1(i-1))==VKFVF_TEXCOORDSIZE1(i-1)) texcoord_offset[i]+=sizeof(float);
		else if ((int(FVF)&VKFVF_TEXCOORDSIZE2(i-1))==VKFVF_TEXCOORDSIZE2(i-1)) texcoord_offset[i]+=2*sizeof(float);
		else if ((int(FVF)&VKFVF_TEXCOORDSIZE3(i-1))==VKFVF_TEXCOORDSIZE3(i-1)) texcoord_offset[i]+=3*sizeof(float);
		else if ((int(FVF)&VKFVF_TEXCOORDSIZE4(i-1))==VKFVF_TEXCOORDSIZE4(i-1)) texcoord_offset[i]+=4*sizeof(float);
	}
#endif
}

void FVFInfoClass::Get_FVF_Name(StringClass& fvfname) const
{
	switch (Get_FVF()) {
	//case DX8_FVF_XYZ: fvfname="VKFVF_XYZ"; break;
	//case DX8_FVF_XYZN: fvfname="VKFVF_XYZ|VKFVF_NORMAL"; break;
	case DX8_FVF_XYZNUV1: fvfname="VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX1"; break;
	case DX8_FVF_XYZNUV2: fvfname="VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX2"; break;
	case DX8_FVF_XYZNDUV1: fvfname="VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX1|VKFVF_DIFFUSE"; break;
	case DX8_FVF_XYZNDUV2: fvfname="VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX2|VKFVF_DIFFUSE"; break;
	case DX8_FVF_XYZDUV1: fvfname="VKFVF_XYZ|VKFVF_TEX1|VKFVF_DIFFUSE"; break;
	case DX8_FVF_XYZDUV2: fvfname="VKFVF_XYZ|VKFVF_TEX2|VKFVF_DIFFUSE"; break;
	//case DX8_FVF_XYZUV1: fvfname="VKFVF_XYZ|VKFVF_TEX1"; break;
	//case DX8_FVF_XYZUV2: fvfname="VKFVF_XYZ|VKFVF_TEX2"; break;
	//case DX8_FVF_XYZNDUV1TG3 : fvfname="(VKFVF_XYZ|VKFVF_NORMAL|VKFVF_DIFFUSE|VKFVF_TEX4|VKFVF_TEXCOORDSIZE2(0)|VKFVF_TEXCOORDSIZE3(1)|VKFVF_TEXCOORDSIZE3(2)|VKFVF_TEXCOORDSIZE3(3))"; break;
	//case DX8_FVF_XYZNUV2DMAP :	fvfname="(VKFVF_XYZ|VKFVF_NORMAL|VKFVF_TEX3|VKFVF_TEXCOORDSIZE1(0)|VKFVF_TEXCOORDSIZE4(1)|VKFVF_TEXCOORDSIZE2(2))"; break;
	case DX8_FVF_XYZNDCUBEMAP : fvfname="(VKFVF_XYZ|VKFVF_NORMAL|VKFVF_DIFFUSE|VKFVF_TEX1|D3DFVFTEXCOORDSIZE3(0)"; break;
	default: fvfname="Unknown!";
	}
}
