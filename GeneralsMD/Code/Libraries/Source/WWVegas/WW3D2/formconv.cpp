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
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/formconv.cpp                           $*
 *                                                                                             *
 *              Original Author:: Nathaniel Hoffman                                            *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               * 
 *                                                                                             * 
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 3                                                           $*
 *                                                                                             *
 * 06/27/02 KM Z Format support																						*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include "formconv.h"
#include <vulkan/vulkan.h>
#include <unordered_map>

VkFormat WW3DFormatToD3DFormatConversionArray[WW3D_FORMAT_COUNT] = {
	VK_FORMAT_UNDEFINED,
	VK_FORMAT_B8G8R8_UNORM,//D3DFMT_R8G8B8,
	VK_FORMAT_B8G8R8A8_UNORM, //D3DFMT_A8R8G8B8
	VK_FORMAT_B8G8R8A8_SRGB, //X8R8G8B8
	VK_FORMAT_R5G6B5_UNORM_PACK16,
	VK_FORMAT_R5G5B5A1_UNORM_PACK16,
	VK_FORMAT_A1R5G5B5_UNORM_PACK16, //X1R5G5B5
	VK_FORMAT_A4R4G4B4_UNORM_PACK16,
	VK_FORMAT_R32G32B32A32_SFLOAT,
	VK_FORMAT_R8_UINT,
	VK_FORMAT_UNDEFINED,//VK_FORMAT_A8R3G3B2, //Not sure if this one is even used, will need to assert on it or convert it
	VK_FORMAT_A4R4G4B4_UNORM_PACK16, //VK_FORMAT_X4R4G4B4,
	VK_FORMAT_R8G8_UINT,//VK_FORMAT_A8P8,
	VK_FORMAT_R8_UINT,//VK_FORMAT_P8,
	VK_FORMAT_R8_UINT,//VK_FORMAT_L8,
	VK_FORMAT_R8G8_UINT,//VK_FORMAT_A8L8,
	VK_FORMAT_R4G4_UNORM_PACK8,//VK_FORMAT_A4L4,
	VK_FORMAT_R8G8_UINT,//VK_FORMAT_V8U8,		// Bumpmap
	VK_FORMAT_R5G6B5_UNORM_PACK16,//VK_FORMAT_L6V5U5,		// Bumpmap
	VK_FORMAT_R8G8B8A8_UINT,//VK_FORMAT_X8L8V8U8,	// Bumpmap
	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,//VK_FORMAT_DXT1,
	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,//DXt2
	VK_FORMAT_BC2_UNORM_BLOCK,// VK_FORMAT_DXT3,
	VK_FORMAT_BC2_UNORM_BLOCK,//DXT4
	VK_FORMAT_BC3_UNORM_BLOCK//VK_FORMAT_DXT5
};

// adding depth stencil format conversion
VkFormat WW3DZFormatToD3DFormatConversionArray[WW3D_ZFORMAT_COUNT] = 
{
#ifndef _XBOX
	VK_FORMAT_UNDEFINED,//D3DFMT_UNKNOWN,
	VK_FORMAT_D16_UNORM,//D3DFMT_D16_LOCKABLE, // 16-bit z-buffer bit depth. This is an application-lockable surface format. 
	VK_FORMAT_D32_SFLOAT,//D3DFMT_D32, // 32-bit z-buffer bit depth. 
	VK_FORMAT_UNDEFINED,//D3DFMT_D15S1, // 16-bit z-buffer bit depth where 15 bits are reserved for the depth channel and 1 bit is reserved for the stencil channel. 
	VK_FORMAT_D24_UNORM_S8_UINT,//D3DFMT_D24S8, // 32-bit z-buffer bit depth using 24 bits for the depth channel and 8 bits for the stencil channel. 
	VK_FORMAT_D16_UNORM,//D3DFMT_D16, // 16-bit z-buffer bit depth. 
	VK_FORMAT_D24_UNORM_S8_UINT,//D3DFMT_D24X8, // 32-bit z-buffer bit depth using 24 bits for the depth channel. 
	VK_FORMAT_D24_UNORM_S8_UINT,//D3DFMT_D24X4S4, // 32-bit z-buffer bit depth using 24 bits for the depth channel and 4 bits for the stencil channel. 
#else
	D3DFMT_UNKNOWN,
	D3DFMT_D16_LOCKABLE, // 16-bit z-buffer bit depth. This is an application-lockable surface format. 
	D3DFMT_D32, // 32-bit z-buffer bit depth. 
	D3DFMT_D15S1, // 16-bit z-buffer bit depth where 15 bits are reserved for the depth channel and 1 bit is reserved for the stencil channel. 
	D3DFMT_D24S8, // 32-bit z-buffer bit depth using 24 bits for the depth channel and 8 bits for the stencil channel. 
	D3DFMT_D16, // 16-bit z-buffer bit depth. 
	D3DFMT_D24X8, // 32-bit z-buffer bit depth using 24 bits for the depth channel. 
	D3DFMT_D24X4S4, // 32-bit z-buffer bit depth using 24 bits for the depth channel and 4 bits for the stencil channel. 

	D3DFMT_LIN_D24S8,
	D3DFMT_LIN_F24S8,
	D3DFMT_LIN_D16,
	D3DFMT_LIN_F16
#endif
};


/*
#define HIGHEST_SUPPORTED_D3DFORMAT D3DFMT_X8L8V8U8	//A4L4
WW3DFormat D3DFormatToWW3DFormatConversionArray[HIGHEST_SUPPORTED_D3DFORMAT + 1] = {
	WW3D_FORMAT_UNKNOWN,		// 0
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_R8G8B8,		// 20
	WW3D_FORMAT_A8R8G8B8,
	WW3D_FORMAT_X8R8G8B8,
	WW3D_FORMAT_R5G6B5,
	WW3D_FORMAT_X1R5G5B5,
	WW3D_FORMAT_A1R5G5B5,
	WW3D_FORMAT_A4R4G4B4,
	WW3D_FORMAT_R3G3B2,
	WW3D_FORMAT_A8,
	WW3D_FORMAT_A8R3G3B2,
	WW3D_FORMAT_X4R4G4B4,	// 30
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_A8P8,			// 40
	WW3D_FORMAT_P8,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,	WW3D_FORMAT_UNKNOWN,
	WW3D_FORMAT_L8,			// 50
	WW3D_FORMAT_A8L8,
	WW3D_FORMAT_A4L4
};
*/

std::unordered_map<VkFormat, WW3DFormat> VK_to_WW3DFormat = {
{VK_FORMAT_UNDEFINED,WW3D_FORMAT_UNKNOWN},
{VK_FORMAT_B8G8R8_UNORM,WW3D_FORMAT_R8G8B8},
{VK_FORMAT_B8G8R8A8_SRGB, WW3D_FORMAT_X8R8G8B8},
{VK_FORMAT_B8G8R8A8_UNORM,WW3D_FORMAT_A8R8G8B8},
{VK_FORMAT_R5G6B5_UNORM_PACK16,WW3D_FORMAT_R5G6B5},
{VK_FORMAT_R5G5B5A1_UNORM_PACK16,WW3D_FORMAT_X1R5G5B5},
{VK_FORMAT_A1R5G5B5_UNORM_PACK16,WW3D_FORMAT_A1R5G5B5},
{VK_FORMAT_A4R4G4B4_UNORM_PACK16,WW3D_FORMAT_A4R4G4B4},
{VK_FORMAT_R32G32B32A32_SFLOAT,WW3D_FORMAT_R3G3B2},
{VK_FORMAT_R8_UINT,WW3D_FORMAT_A8},
{VK_FORMAT_UNDEFINED,WW3D_FORMAT_A8R3G3B2},
{VK_FORMAT_A4R4G4B4_UNORM_PACK16,WW3D_FORMAT_X4R4G4B4},
{VK_FORMAT_R8G8_UINT,WW3D_FORMAT_A8P8},
{VK_FORMAT_R8_UINT,WW3D_FORMAT_P8},
{VK_FORMAT_R8_UINT,WW3D_FORMAT_L8},
{VK_FORMAT_R8G8_UINT,WW3D_FORMAT_A8L8},
{VK_FORMAT_R4G4_UNORM_PACK8,WW3D_FORMAT_A4L4},
{VK_FORMAT_R8G8_UINT,WW3D_FORMAT_U8V8},
{VK_FORMAT_R5G6B5_UNORM_PACK16,WW3D_FORMAT_L6V5U5},
{VK_FORMAT_R8G8B8A8_UINT,WW3D_FORMAT_X8L8V8U8},
{VK_FORMAT_BC1_RGBA_UNORM_BLOCK,WW3D_FORMAT_DXT1},
{VK_FORMAT_BC1_RGBA_UNORM_BLOCK,WW3D_FORMAT_DXT2},
{VK_FORMAT_BC2_UNORM_BLOCK,WW3D_FORMAT_DXT3},
{VK_FORMAT_BC2_UNORM_BLOCK,WW3D_FORMAT_DXT4},
{VK_FORMAT_BC3_UNORM_BLOCK,WW3D_FORMAT_DXT5}
};
std::unordered_map<VkFormat, WW3DZFormat> VK_to_WW3DZFormat = {
{VK_FORMAT_UNDEFINED,WW3D_ZFORMAT_UNKNOWN},
{VK_FORMAT_D16_UNORM,WW3D_ZFORMAT_D16_LOCKABLE},
{VK_FORMAT_D32_SFLOAT,WW3D_ZFORMAT_D32},
{VK_FORMAT_UNDEFINED,WW3D_ZFORMAT_D15S1},
{VK_FORMAT_D24_UNORM_S8_UINT,WW3D_ZFORMAT_D24S8},
{VK_FORMAT_D16_UNORM,WW3D_ZFORMAT_D16},
{VK_FORMAT_D24_UNORM_S8_UINT,WW3D_ZFORMAT_D24X8},
{VK_FORMAT_D24_UNORM_S8_UINT,WW3D_ZFORMAT_D24X4S4},
};

VkFormat WW3DFormat_To_D3DFormat(WW3DFormat ww3d_format) {
	if (ww3d_format >= WW3D_FORMAT_COUNT) {
		return VK_FORMAT_UNDEFINED;
	} else {
		return WW3DFormatToD3DFormatConversionArray[(unsigned int)ww3d_format];
	}
}

WW3DFormat D3DFormat_To_WW3DFormat(VkFormat d3d_format)
{
	if (VK_to_WW3DFormat.find(d3d_format) != VK_to_WW3DFormat.end())
		return VK_to_WW3DFormat[d3d_format];
	return WW3D_FORMAT_UNKNOWN;
}

//**********************************************************************************************
//! Depth Stencil W3D to D3D format conversion
/*! KJM
*/
VkFormat WW3DZFormat_To_D3DFormat(WW3DZFormat ww3d_zformat) 
{
	if (ww3d_zformat >= WW3D_ZFORMAT_COUNT) 
	{
		return VK_FORMAT_UNDEFINED;
	}
	else 
	{
		return WW3DZFormatToD3DFormatConversionArray[(unsigned int)ww3d_zformat];
	}
}

//**********************************************************************************************
//! D3D to Depth Stencil W3D format conversion
/*! KJM
*/
WW3DZFormat D3DFormat_To_WW3DZFormat(VkFormat d3d_format)
{
	if (VK_to_WW3DZFormat.find(d3d_format) != VK_to_WW3DZFormat.end())
		return VK_to_WW3DZFormat[d3d_format];
	return WW3D_ZFORMAT_UNKNOWN;
}

//**********************************************************************************************
//! Init format conversion tables
/*!
 * 06/27/02 KM Z Format support																						*
*/
void Init_D3D_To_WW3_Conversion()
{
#ifdef INFO_VULKAN
	int i = 0;
	for (;i<HIGHEST_SUPPORTED_D3DFORMAT;++i) {
		D3DFormatToWW3DFormatConversionArray[i]=WW3D_FORMAT_UNKNOWN;
	}

	D3DFormatToWW3DFormatConversionArray[D3DFMT_R8G8B8]=WW3D_FORMAT_R8G8B8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8R8G8B8]=WW3D_FORMAT_A8R8G8B8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X8R8G8B8]=WW3D_FORMAT_X8R8G8B8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_R5G6B5]=WW3D_FORMAT_R5G6B5;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X1R5G5B5]=WW3D_FORMAT_X1R5G5B5;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A1R5G5B5]=WW3D_FORMAT_A1R5G5B5;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A4R4G4B4]=WW3D_FORMAT_A4R4G4B4;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_R3G3B2]=WW3D_FORMAT_R3G3B2;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8]=WW3D_FORMAT_A8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8R3G3B2]=WW3D_FORMAT_A8R3G3B2;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X4R4G4B4]=WW3D_FORMAT_X4R4G4B4;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8P8]=WW3D_FORMAT_A8P8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_P8]=WW3D_FORMAT_P8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_L8]=WW3D_FORMAT_L8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A8L8]=WW3D_FORMAT_A8L8;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_A4L4]=WW3D_FORMAT_A4L4;
	D3DFormatToWW3DFormatConversionArray[D3DFMT_V8U8]=WW3D_FORMAT_U8V8;				// Bumpmap
	D3DFormatToWW3DFormatConversionArray[D3DFMT_L6V5U5]=WW3D_FORMAT_L6V5U5;		// Bumpmap
	D3DFormatToWW3DFormatConversionArray[D3DFMT_X8L8V8U8]=WW3D_FORMAT_X8L8V8U8;	// Bumpmap

	// init depth stencil conversion
	for (i=0; i<HIGHEST_SUPPORTED_D3DZFORMAT; i++) 
	{
		D3DFormatToWW3DZFormatConversionArray[i]=WW3D_ZFORMAT_UNKNOWN;
	}

	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D16_LOCKABLE]=WW3D_ZFORMAT_D16_LOCKABLE;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D32]=WW3D_ZFORMAT_D32;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D15S1]=WW3D_ZFORMAT_D15S1;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D24S8]=WW3D_ZFORMAT_D24S8;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D16]=WW3D_ZFORMAT_D16;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D24X8]=WW3D_ZFORMAT_D24X8;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_D24X4S4]=WW3D_ZFORMAT_D24X4S4;
#ifdef _XBOX
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_LIN_D24S8]=WW3D_ZFORMAT_LIN_D24S8;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_LIN_F24S8]=WW3D_ZFORMAT_LIN_F24S8;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_LIN_D16]=WW3D_ZFORMAT_LIN_D16;
	D3DFormatToWW3DZFormatConversionArray[D3DFMT_LIN_F16]=WW3D_ZFORMAT_LIN_F16;
#endif
#endif
};
