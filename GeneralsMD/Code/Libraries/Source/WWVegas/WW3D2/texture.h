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
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/texture.h                              $*
 *                                                                                             *
 *                  $Org Author:: Jani_p                                                      $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               * 
 *                                                                                             * 
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 46                                                          $*
 *                                                                                             *
 * 05/16/02 KM Base texture class to abstract major texture types, e.g. 3d, z, cube, etc.
 * 06/27/02 KM Texture class abstraction																			*
 * 08/05/02 KM Texture class redesign (revisited)
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#if defined(_MSC_VER)
#pragma once
#endif

#ifndef TEXTURE_H
#define TEXTURE_H

#include "always.h"
#include "refcount.h"
#include "chunkio.h"
#include "surfaceclass.h"
#include "ww3dformat.h"
#include "wwstring.h"
#include "vector3.h"
#include "texturefilter.h"
#include <VkRenderTarget.h>

struct IDirect3DBaseTexture9;
struct IDirect3DTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DVolumeTexture9;

class DX8Wrapper;
class TextureLoader;
class LoaderThreadClass;
class TextureLoadTaskClass;
class CubeTextureClass;
class VolumeTextureClass;

class TextureClass;
class TextureBaseClass : public RefCountClass
{
	friend class TextureLoader;
	friend class LoaderThreadClass;
	friend class DX8TextureTrackerClass;  //(gth) so it can call Poke_Texture, 
	friend class DX8ZTextureTrackerClass;

public:
	enum TexAssetType
	{
		TEX_REGULAR
	};

	// base constructor for derived classes
	TextureBaseClass
	(
		unsigned width, 
		unsigned height, 
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		bool rendertarget=false,
		bool reducible=true
	);

	virtual ~TextureBaseClass();

	virtual TexAssetType Get_Asset_Type() const=0;

	// Names
	void	Set_Texture_Name(const char * name);
	void	Set_Full_Path(const char * path)			{ FullPath = path; }
	const StringClass& Get_Texture_Name(void) const		{ return Name; }
	const StringClass& Get_Full_Path(void) const			{ if (FullPath.Is_Empty ()) return Name; return FullPath; }

	unsigned Get_ID() const { return texture_id; }	// Each textrure has a unique id

	// The number of Mip levels in the texture
	unsigned int Get_Mip_Level_Count(void) const
	{
		return MipLevelCount;
	}

	// Note! Width and Height may be zero and may change if texture uses mipmaps
	int Get_Width() const
	{
		return Width;
	}
	int Get_Height() const
	{
		return Height; 
	}

	// Time, after which the texture is invalidated if not used. Set to zero to indicate infinite.
	// Time is in milliseconds.
	void Set_Inactivation_Time(unsigned time) { InactivationTime=time; }
	int Get_Inactivation_Time() const { return InactivationTime; }

	// Texture priority affects texture management and caching.
	unsigned int Get_Priority(void);
	unsigned int Set_Priority(unsigned int priority);	// Returns previous priority

	// Debug utility functions for returning the texture memory usage
	virtual unsigned Get_Texture_Memory_Usage() const=0;

	bool Is_Initialized() const { return Initialized; }
	bool Is_Lightmap() const { return IsLightmap; }
	bool Is_Procedural() const { return IsProcedural; }
	bool Is_Reducible() const { return IsReducible; } //can texture be reduced in resolution for LOD purposes?

	static int _Get_Total_Locked_Surface_Size();
	static int _Get_Total_Texture_Size();
	static int _Get_Total_Lightmap_Texture_Size();
	static int _Get_Total_Procedural_Texture_Size();
	static int _Get_Total_Locked_Surface_Count();
	static int _Get_Total_Texture_Count();
	static int _Get_Total_Lightmap_Texture_Count();
	static int _Get_Total_Procedural_Texture_Count();

	virtual void Init()=0;

	// This utility function processes the texture reduction (used during rendering)
	void Invalidate();

	// texture accessors (dx8)
	const VK::Texture& Peek_D3D_Base_Texture() const;
	void Set_D3D_Base_Texture(VK::Texture tex);

	// Support for self managed textures
	bool Is_Dirty() { return Dirty; };
	void Set_Dirty() { Dirty=true; }
	void Clean() { Dirty=false; };

	void Set_HSV_Shift(const Vector3 &hsv_shift);
	const Vector3& Get_HSV_Shift() { return HSVShift; }

	bool Is_Compression_Allowed() const { return IsCompressionAllowed; }

	unsigned Get_Reduction() const;

	// Background texture loader will call this when texture has been loaded
	virtual void Apply_New_Surface(VK::Texture tex, bool initialized, VK::Surface* surface, bool disable_auto_invalidation = false)=0;	// If the parameter is true, the texture will be flagged as initialised

	MipCountType MipLevelCount;

	// Inactivate textures that haven't been used in a while. Pass zero to use textures'
	// own inactive times (default). In urgent need to free up texture memory, try
	// calling with relatively small (just few seconds) time override to free up everything
	// but the currently used textures.
	static void Invalidate_Old_Unused_Textures(unsigned inactive_time_override);

	// Apply this texture's settings into D3D
	virtual void Apply(unsigned int stage)=0;

	// Apply a Null texture's settings into D3D
	static void Apply_Null(unsigned int stage);

	virtual TextureClass* As_TextureClass() { return NULL; }
	virtual CubeTextureClass* As_CubeTextureClass() { return NULL; }
	virtual VolumeTextureClass* As_VolumeTextureClass() { return NULL; }

	VK::Texture& Peek_D3D_Texture() const { return (VK::Texture&)Peek_D3D_Base_Texture(); }
	VK::Texture& Peek_D3D_VolumeTexture() const { return (VK::Texture&)Peek_D3D_Base_Texture(); }
	VK::Texture& Peek_D3D_CubeTexture() const { return (VK::Texture&)Peek_D3D_Base_Texture(); }

protected:

	void Load_Locked_Surface();
	void Poke_Texture(VK::Texture tex) { D3DTexture = tex; }

	bool Initialized;

	// For debug purposes the texture sets this true if it is a lightmap texture
	bool IsLightmap;
	bool IsCompressionAllowed;
	bool IsProcedural;
	bool IsReducible;


	unsigned InactivationTime;	// In milliseconds
	unsigned ExtendedInactivationTime;	// This is set by the engine, if needed
	unsigned LastInactivationSyncTime;
	mutable unsigned LastAccessed;

	// If this is non-zero, the texture will have a hue shift done at the next init (this
	// value should only be changed by Set_HSV_Shift() function, which also invalidates the
	// texture).
	Vector3 HSVShift;	

	int Width;
	int Height;

private:

	// Direct3D texture object
	VK::Texture D3DTexture = {};

	// Name
	StringClass Name;
	StringClass	FullPath;

	// Unique id
	unsigned texture_id;

	// Support for self-managed textures
	bool Dirty;

	friend class TextureLoadTaskClass;
	friend class VolumeTextureLoadTaskClass;
	TextureLoadTaskClass* TextureLoadTask;
	TextureLoadTaskClass* ThumbnailLoadTask;

};


/*************************************************************************
**                             TextureClass
**
** This is our regular texture class. For legacy reasons it contains some
** information beyond the D3D texture itself, such as texture addressing
** modes.
**
*************************************************************************/
class TextureClass : public TextureBaseClass
{
	W3DMPO_GLUE(TextureClass)
//	friend DX8Wrapper;

public:

	// Create texture with desired height, width and format.
	TextureClass
	(
		unsigned width, 
		unsigned height, 
		WW3DFormat format,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		bool allow_reduction=true
	);

	// Create texture from a file. If format is specified the texture is converted to that format.
	// Note that the format must be supported by the current device and that a texture can't exist
	// in the system with the same name in multiple formats.
	TextureClass
	(
		const char *name,
		const char *full_path=NULL,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		WW3DFormat texture_format=WW3D_FORMAT_UNKNOWN,
		bool allow_compression=true,
		bool allow_reduction=true
	);

	// Create texture from a surface.
	TextureClass
	(
		SurfaceClass *surface, 
		MipCountType mip_level_count=MIP_LEVELS_ALL
	);		

	TextureClass(const VK::Texture& d3d_texture);

	// defualt constructors for derived classes (cube & vol)
	TextureClass
	(
		unsigned width,
		unsigned height,
		MipCountType mip_level_count=MIP_LEVELS_ALL,
		bool rendertarget=false,
		WW3DFormat format=WW3D_FORMAT_UNKNOWN,
		bool allow_reduction=true
	)
	: TextureBaseClass(width,height,mip_level_count,rendertarget,allow_reduction), TextureFormat(format), Filter(mip_level_count) { }

	virtual TexAssetType Get_Asset_Type() const { return TEX_REGULAR; }

	virtual void Init();

	// Background texture loader will call this when texture has been loaded
	virtual void Apply_New_Surface(VK::Texture tex, bool initialized, VK::Surface* surface, bool disable_auto_invalidation = false);	// If the parameter is true, the texture will be flagged as initialised

	// Get the surface of one of the mipmap levels (defaults to highest-resolution one)
	SurfaceClass *Get_Surface_Level(unsigned int level = 0);
	void Upload();
#ifdef INFO_VULKAN
	IDirect3DSurface9 *Get_D3D_Surface_Level(unsigned int level = 0);
#endif
	void Get_Level_Description( SurfaceClass::SurfaceDescription & desc, unsigned int level = 0 );
	
	TextureFilterClass& Get_Filter() { return Filter; }

	WW3DFormat Get_Texture_Format() const { return TextureFormat; }

	virtual void Apply(unsigned int stage);

	virtual unsigned Get_Texture_Memory_Usage() const;

	virtual TextureClass* As_TextureClass() { return this; }

protected:
	SurfaceClass* surface;
	WW3DFormat				TextureFormat;

	// legacy
	TextureFilterClass	Filter;
};

class ZTextureClass : public TextureBaseClass
{
public:
	// Create a z texture with desired height, width and format
	ZTextureClass
	(
		unsigned width,
		unsigned height,
		WW3DZFormat zformat,
		MipCountType mip_level_count=MIP_LEVELS_ALL
	);

	WW3DZFormat Get_Texture_Format() const { return DepthStencilTextureFormat; }

	virtual TexAssetType Get_Asset_Type() const { return TEX_REGULAR; }

	virtual void Init() {}

	// Background texture loader will call this when texture has been loaded
	virtual void Apply_New_Surface(VK::Texture tex, bool initialized, VK::Surface* surface, bool disable_auto_invalidation = false);	// If the parameter is true, the texture will be flagged as initialised

	virtual void Apply(unsigned int stage);

#ifdef INFO_VULKAN
	IDirect3DSurface9 *Get_D3D_Surface_Level(unsigned int level = 0);
#endif
	virtual unsigned Get_Texture_Memory_Usage() const;

private:

	WW3DZFormat DepthStencilTextureFormat;
};

// Utility functions for loading and saving texture descriptions from/to W3D files
TextureClass *Load_Texture(ChunkLoadClass & cload);
void Save_Texture(TextureClass * texture, ChunkSaveClass & csave);

#endif //TEXTURE_H
