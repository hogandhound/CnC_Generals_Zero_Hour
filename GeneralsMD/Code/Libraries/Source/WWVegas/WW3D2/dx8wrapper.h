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
 *                     $Archive:: /Commando/Code/ww3d2/dx8wrapper.h                           $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               * 
 *                                                                                             * 
 *                     $Modtime:: 08/05/02 2:40p                                              $*
 *                                                                                             *
 *                    $Revision:: 92                                                          $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Render to shadow buffer texture support														*
 * 08/05/02 KM Texture class redesign 
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef DX8_WRAPPER_H
#define DX8_WRAPPER_H

#include "always.h"
#include "dllist.h"
#include "matrix4.h"
#include "statistics.h"
#include "wwstring.h"
#include "lightenvironment.h"
#include "shader.h"
#include "vector4.h"
#include "cpudetect.h"
#include "dx8caps.h"

#include "texture.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "vertmaterial.h"
#include <DirectXMath.h>
#include <vulkan/vulkan.h>
#include <VkRenderTarget.h>
#include <shaders/WWVK_shaderdef.h>
#include <VkBufferTools.h>
#include <VkTexture.h>

/*
** Registry value names
*/
#define	VALUE_NAME_RENDER_DEVICE_NAME					"RenderDeviceName"
#define	VALUE_NAME_RENDER_DEVICE_WIDTH				"RenderDeviceWidth"
#define	VALUE_NAME_RENDER_DEVICE_HEIGHT				"RenderDeviceHeight"
#define	VALUE_NAME_RENDER_DEVICE_DEPTH				"RenderDeviceDepth"
#define	VALUE_NAME_RENDER_DEVICE_WINDOWED			"RenderDeviceWindowed"
#define	VALUE_NAME_RENDER_DEVICE_TEXTURE_DEPTH		"RenderDeviceTextureDepth"

const unsigned MAX_TEXTURE_STAGES=8;
const unsigned MAX_VERTEX_STREAMS=2;
const unsigned MAX_VERTEX_SHADER_CONSTANTS=96;
const unsigned MAX_PIXEL_SHADER_CONSTANTS=8;
const unsigned MAX_SHADOW_MAPS=1;

#define prevVer
#define nextVer
//#define __volatile unsigned


enum {
	BUFFER_TYPE_DX8,
	BUFFER_TYPE_SORTING,
	BUFFER_TYPE_DYNAMIC_DX8,
	BUFFER_TYPE_DYNAMIC_SORTING,
	BUFFER_TYPE_INVALID
};

typedef enum class VkTransformState {
	VIEW = 1,
	PROJECTION = 2,
	TEXTURE0 = 3,
	TEXTURE1 = 4,
	TEXTURE2 = 5,
	TEXTURE3 = 6,
	TEXTURE4 = 7,
	TEXTURE5 = 8,
	TEXTURE6 = 9,
	TEXTURE7 = 10,
	WORLD = 11,
	WORLD0 = 12,
	WORLD1 = 13,
	WORLD2 = 14,
	FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} VkTS;
enum VKTEXTURESTAGESTATETYPE
{
	VKTSS_COLOROP, /* D3DTEXTUREOP - per-stage blending controls for color channels */
	VKTSS_COLORARG1, /* VKTA_* (texture arg) */
	VKTSS_COLORARG2, /* VKTA_* (texture arg) */
	VKTSS_ALPHAOP, /* D3DTEXTUREOP - per-stage blending controls for alpha channel */
	VKTSS_ALPHAARG1, /* VKTA_* (texture arg) */
	VKTSS_ALPHAARG2, /* VKTA_* (texture arg) */
	VKTSS_TEXCOORDINDEX, /* identifies which set of texture coordinates index this texture */
	VKTSS_TEXTURETRANSFORMFLAGS, /* D3DTEXTURETRANSFORMFLAGS controls texture transform */
	VKTSS_BUMPENVMAT00, /* float (bump mapping matrix) */
	VKTSS_BUMPENVMAT01, /* float (bump mapping matrix) */
	VKTSS_BUMPENVMAT10, /* float (bump mapping matrix) */
	VKTSS_BUMPENVMAT11, /* float (bump mapping matrix) */
	VKTSS_MAX,
	VKTSS_SHADER_COMPARE_MAX = VKTSS_BUMPENVMAT00,

	VKTSS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
};

/*
 * State enumerants for per-sampler texture processing.
 */
enum VKSAMPLERSTATETYPE
{
	VKSAMP_ADDRESSU,  /* D3DTEXTUREADDRESS for U coordinate */
	VKSAMP_ADDRESSV,  /* D3DTEXTUREADDRESS for V coordinate */
	VKSAMP_MAGFILTER,  /* D3DTEXTUREFILTER filter to use for magnification */
	VKSAMP_MINFILTER,  /* D3DTEXTUREFILTER filter to use for minification */
	VKSAMP_MIPFILTER,  /* D3DTEXTUREFILTER filter to use between mipmaps during minification */
	VKSAMP_MAXANISOTROPY, /* DWORD maximum anisotropy */
	VKSAMP_MAX,
	VKSAMP_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
};
enum VKRENDERSTATETYPE {
	VKRS_FILLMODE,    /* D3DFILLMODE */
	VKRS_ZWRITEENABLE,   /* TRUE to enable z writes */
	VKRS_ALPHATESTENABLE,   /* TRUE to enable alpha tests */
	VKRS_SRCBLEND,   /* D3DBLEND */
	VKRS_DESTBLEND,   /* D3DBLEND */
	VKRS_CULLMODE,   /* D3DCULL */
	VKRS_ZFUNC,   /* D3DCMPFUNC */
	VKRS_ALPHAREF,   /* D3DFIXED */
	VKRS_ALPHAFUNC,   /* D3DCMPFUNC */
	VKRS_ALPHABLENDENABLE,   /* TRUE to enable alpha blending */
	VKRS_FOGENABLE,   /* TRUE to enable fog blending */
	VKRS_SPECULARENABLE,   /* TRUE to enable specular */
	VKRS_STENCILENABLE,   /* BOOL enable/disable stenciling */
	VKRS_STENCILFAIL,   /* D3DSTENCILOP to do if stencil test fails */
	VKRS_STENCILZFAIL,   /* D3DSTENCILOP to do if stencil test passes and Z test fails */
	VKRS_STENCILPASS,   /* D3DSTENCILOP to do if both stencil and Z tests pass */
	VKRS_STENCILFUNC,   /* D3DCMPFUNC fn.  Stencil Test passes if ((ref & mask) stencilfn (stencil & mask)) is true */
	VKRS_STENCILREF,   /* Reference value used in stencil test */
	VKRS_STENCILMASK,   /* Mask value used in stencil test */
	VKRS_STENCILWRITEMASK,   /* Write mask applied to values written to stencil buffer */
	VKRS_LIGHTING,
	VKRS_DIFFUSEMATERIALSOURCE,
	VKRS_SPECULARMATERIALSOURCE,
	VKRS_AMBIENTMATERIALSOURCE,
	VKRS_EMISSIVEMATERIALSOURCE,
	VKRS_COLORWRITEENABLE,  // per-channel write enable
	VKRS_BLENDOP,   // D3DBLENDOP setting
	VKRS_DEPTHBIAS,
	VKRS_POINTSIZE,   /* float point size */
	VKRS_POINTSIZE_MIN,   /* float point size min threshold */
	VKRS_POINTSPRITEENABLE,   /* BOOL point texture coord control */
	VKRS_POINTSCALEENABLE,   /* BOOL point size scale enable */
	VKRS_POINTSIZE_MAX,   /* float point size max threshold */
	VKRS_AMBIENT,
	VKRS_FOGCOLOR,   /* D3DCOLOR */
	VKRS_FOGSTART,   /* Fog start (for both vertex and pixel fog) */
	VKRS_FOGEND,   /* Fog end      */
	VKRS_TEXTUREFACTOR,   /* D3DCOLOR used for multi-texture blend */
	//VKRS_WRAP0,  /* wrap for 1st texture coord. set */
	VKRS_NORMALIZENORMALS,
	VKRS_MAX,
	VKRS_SHADER_COMPARE_MAX = VKRS_POINTSIZE,

	VKRS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
};
enum VKTEXTUREOP
{
	// Control
	VKTOP_DISABLE,      // disables stage
	VKTOP_SELECTARG1,      // the default
	VKTOP_SELECTARG2,

	// Modulate
	VKTOP_MODULATE,      // multiply args together
	VKTOP_MODULATE2X,      // multiply and  1 bit

	// Add
	VKTOP_ADD,   // add arguments together
	VKTOP_ADDSIGNED,   // add with -0.5 bias
	VKTOP_ADDSIGNED2X,   // as above but left  1 bit
	VKTOP_SUBTRACT,   // Arg1 - Arg2, with no saturation
	VKTOP_ADDSMOOTH,   // add 2 args, subtract product
	// Arg1 + Arg2 - Arg1*Arg2
	//Arg1 + (1-Arg1)*Arg2

	// Linear alpha blend: Arg1*(Alpha) + Arg2*(1-Alpha)
	VKTOP_BLENDTEXTUREALPHA, // texture alpha

	// Linear alpha blend with pre-multiplied arg1 input: Arg1 + Arg2*(1-Alpha)
	VKTOP_BLENDCURRENTALPHA, // by alpha of current color

	// Specular mapping
	VKTOP_MODULATEALPHA_ADDCOLOR,     // Arg1.RGB + Arg1.A*Arg2.RGB
	// COLOROP only

	// Bump mapping
	VKTOP_BUMPENVMAP, // per pixel env map perturbation
	VKTOP_BUMPENVMAPLUMINANCE, // with luminance channel

	// This can do either diffuse or specular bump mapping with correct input.
	// Performs the function (Arg1.R*Arg2.R + Arg1.G*Arg2.G + Arg1.B*Arg2.B)
	// where each component has been scaled and offset to make it signed.
	// The result is replicated into all four (including alpha) channels.
	// This is a valid COLOROP only.
	VKTOP_DOTPRODUCT3,

	// Triadic ops
	VKTOP_MULTIPLYADD, // Arg0 + Arg1*Arg2

	VKTOP_FORCE_DWORD = 0x7fffffff,
};
enum VKTEXTURETRANSFORMFLAGS {
	VKTTFF_DISABLE = 0,    // texture coordinates are passed directly
	VKTTFF_COUNT2 = 2,    // rasterizer should expect 2-D texture coords
	VKTTFF_COUNT3 = 3,
	VKTTFF_PROJECTED = 4,
	VKTTFF_FORCE_DWORD = 0x7fffffff,
};
#define VKTA_DIFFUSE           0x00000000  // select diffuse color (read only)
#define VKTA_CURRENT           0x00000001  // select stage destination register (read/write)
#define VKTA_TEXTURE           0x00000002  // select texture color (read only)
#define VKTA_TFACTOR           0x00000003  // select D3DRS_TEXTUREFACTOR (read only)
#define VKTA_COMPLEMENT        0x00000010  // take 1.0 - x (read modifier)
#define VKTA_ALPHAREPLICATE    0x00000020  // replicate alpha to color components (read modifier)

#define VKTSS_TCI_PASSTHRU                             0x00000000
#define VKTSS_TCI_CAMERASPACENORMAL                    0x00010000
#define VKTSS_TCI_CAMERASPACEPOSITION                  0x00020000
#define VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR          0x00040000


class VertexMaterialClass;
class CameraClass;
class LightEnvironmentClass;
class RenderDeviceDescClass;
class VertexBufferClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class DynamicIBAccessClass;
class TextureClass;
class ZTextureClass;
class LightClass;
class SurfaceClass;
class DX8Caps;

#define DX8_RECORD_MATRIX_CHANGE()				matrix_changes++
#define DX8_RECORD_MATERIAL_CHANGE()			material_changes++
#define DX8_RECORD_VERTEX_BUFFER_CHANGE()		vertex_buffer_changes++
#define DX8_RECORD_INDEX_BUFFER_CHANGE()		index_buffer_changes++
#define DX8_RECORD_LIGHT_CHANGE()				light_changes++
#define DX8_RECORD_TEXTURE_CHANGE()				texture_changes++
#define DX8_RECORD_RENDER_STATE_CHANGE()		render_state_changes++
#define DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE() texture_stage_state_changes++
#define DX8_RECORD_DRAW_CALLS()					draw_calls++

extern unsigned number_of_DX8_calls;
extern bool _DX8SingleThreaded;

void DX8_Assert();
void Log_DX8_ErrorCode(unsigned res);

WWINLINE void DX8_ErrorCode(unsigned res)
{
#ifdef INFO_VULKAN
	if (res==D3D_OK) return;
	Log_DX8_ErrorCode(res);
#endif
}

#ifdef WWDEBUG
#define DX8CALL_HRES(x,res) DX8_Assert(); res = DX8Wrapper::_Get_D3D_Device8()->x; DX8_ErrorCode(res); number_of_DX8_calls++;
#define DX8CALL(x) DX8_Assert(); DX8_ErrorCode(DX8Wrapper::_Get_D3D_Device8()->x); number_of_DX8_calls++;
#define DX8CALL_D3D(x) DX8_Assert(); DX8_ErrorCode(DX8Wrapper::_Get_D3D8()->x); number_of_DX8_calls++;
#define DX8_THREAD_ASSERT() if (_DX8SingleThreaded) { WWASSERT_PRINT(DX8Wrapper::_Get_Main_Thread_ID()==ThreadClass::_Get_Current_Thread_ID(),"DX8Wrapper::DX8 calls must be called from the main thread!"); }
#else
#define DX8CALL_HRES(x,res) res = DX8Wrapper::_Get_D3D_Device8()->x; number_of_DX8_calls++;
#define DX8CALL(x) DX8Wrapper::_Get_D3D_Device8()->x; number_of_DX8_calls++;
#define DX8CALL_D3D(x) DX8Wrapper::_Get_D3D8()->x; number_of_DX8_calls++;
#define DX8_THREAD_ASSERT() ;
#endif


#define no_EXTENDED_STATS
// EXTENDED_STATS collects additional timing statistics by turning off parts
// of the 3D drawing system (terrain, objects, etc.)
#ifdef EXTENDED_STATS
class DX8_Stats
{
public:
	bool m_showingStats;
	bool m_disableTerrain;
	bool m_disableWater;
	bool m_disableObjects;
	bool m_disableOverhead;
	bool m_disableConsole;
	int  m_debugLinesToShow;
	int	 m_sleepTime;
public:
	DX8_Stats::DX8_Stats(void) {
		m_disableConsole = m_showingStats = m_disableTerrain = m_disableWater = m_disableOverhead = m_disableObjects = false;
		m_sleepTime = 0;
		m_debugLinesToShow = -1; // -1 means show all expected lines of output
	}
};
#endif


// This virtual interface was added for the Generals RTS.
// It is called before resetting the dx8 device to ensure
// that all dx8 resources are released.  Otherwise reset fails. jba.
class DX8_CleanupHook
{
public:
	virtual void ReleaseResources(void)=0;
	virtual void ReAcquireResources(void)=0;
};


struct RenderStateStruct
{
	ShaderClass shader;
	VertexMaterialClass* material;
	TextureBaseClass * Textures[MAX_TEXTURE_STAGES];
#ifdef INFO_VULKAN
	D3DLIGHT9 Lights[4];
#endif
	LightCollection Lights;
	bool LightEnable[4];
  //unsigned lightsHash;
	Matrix4x4 world;
	Matrix4x4 view;
	unsigned vertex_buffer_types[MAX_VERTEX_STREAMS];
	unsigned index_buffer_type;
	unsigned short vba_offset;
	unsigned short vba_count;
	unsigned short iba_offset;
	VertexBufferClass* vertex_buffers[MAX_VERTEX_STREAMS];
	IndexBufferClass* index_buffer;
	unsigned short index_base_offset;

	RenderStateStruct();
	~RenderStateStruct();

	RenderStateStruct& operator= (const RenderStateStruct& src);
};

#define WWVKRENDER DX8Wrapper::_GetRenderTarget()
#define WWVKPIPES DX8Wrapper::_GetPipelineCol()
#define WWVKDSV std::vector<VkDescriptorSet> sets

/**
** DX8Wrapper
**
** DX8 interface wrapper class.  This encapsulates the DX8 interface; adding redundant state
** detection, stat tracking, etc etc.  In general, we will wrap all DX8 calls with at least
** an WWINLINE function so that we can add stat tracking, etc if needed.  Direct access to the
** D3D device will require "friend" status and should be granted only in extreme circumstances :-)
*/
class DX8Wrapper
{
	enum ChangedStates {
		WORLD_CHANGED	=	1<<0,
		VIEW_CHANGED	=	1<<1,
		LIGHT0_CHANGED	=	1<<2,
		LIGHT1_CHANGED	=	1<<3,
		LIGHT2_CHANGED	=	1<<4,
		LIGHT3_CHANGED	=	1<<5,
		TEXTURE0_CHANGED=	1<<6,
		TEXTURE1_CHANGED=	1<<7,
		TEXTURE2_CHANGED=	1<<8,
		TEXTURE3_CHANGED=	1<<9,
		MATERIAL_CHANGED=	1<<14,
		SHADER_CHANGED	=	1<<15,
		VERTEX_BUFFER_CHANGED = 1<<16,
		INDEX_BUFFER_CHANGED = 1 << 17,
		WORLD_IDENTITY=	1<<18,
		VIEW_IDENTITY=		1<<19,

		TEXTURES_CHANGED=
			TEXTURE0_CHANGED|TEXTURE1_CHANGED|TEXTURE2_CHANGED|TEXTURE3_CHANGED,
		LIGHTS_CHANGED=
			LIGHT0_CHANGED|LIGHT1_CHANGED|LIGHT2_CHANGED|LIGHT3_CHANGED,
	};

	static void Draw_Sorting_IB_VB(
		unsigned primitive_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);

public:
	static void Convert_Sorting_IB_VB(SortingVertexBufferClass* sortingV, SortingIndexBufferClass* sortingI, VK::Buffer& vbo, VK::Buffer& ibo);

#ifdef EXTENDED_STATS
	static DX8_Stats stats;
#endif

	static std::vector<WWVK_Pipeline_Entry> FindClosestPipelines(unsigned FVF, VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	static bool Init(void * hwnd, bool lite = false);
	static void Shutdown(void);

	static void SetCleanupHook(DX8_CleanupHook *pCleanupHook) {m_pCleanupHook = pCleanupHook;};
	/*
	** Some WW3D sub-systems need to be initialized after the device is created and shutdown
	** before the device is released.
	*/
	static void	Do_Onetime_Device_Dependent_Inits(void);
	static void Do_Onetime_Device_Dependent_Shutdowns(void);

	static bool Is_Device_Lost() { return IsDeviceLost; }
	static bool Is_Initted(void) { return IsInitted; }

	static bool Has_Stencil (void);
	static void Get_Format_Name(unsigned int format, StringClass *tex_format);

	/*
	** Rendering
	*/
	static void Begin_Scene(void);
	static void End_Scene(bool flip_frame = true);

	static void Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha=0.0f, float z=1.0f, unsigned int stencil=0);

	static void	Set_Viewport(CONST VkViewport* pViewport);

	static void Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream=0);
	static void Set_Vertex_Buffer(const DynamicVBAccessClass& vba);
	static VertexBufferClass* Get_Vertex_Buffer();
	static void Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset);
	static void Set_Index_Buffer(const DynamicIBAccessClass& iba,unsigned short index_base_offset);
	static IndexBufferClass* Set_Index_Buffer();
	static void Set_Index_Buffer_Index_Offset(unsigned offset);

	static void Get_Render_State(RenderStateStruct& state);
	static RenderStateStruct& Get_Render_State() { return render_state; };
	static void Set_Render_State(const RenderStateStruct& state);
	static void Release_Render_State();

	static void Set_DX8_Material(const DX8Material* mat);

	static void Set_Gamma(float gamma,float bright,float contrast,bool calibrate=true,bool uselimit=true);

	// Set_ and Get_Transform() functions take the matrix in Westwood convention format.

	static void Set_DX8_ZBias(int zbias);
	static void Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix,float znear, float zfar);	// pointer to 16 matrices

	static void Set_Transform(VkTransformState transform,const Matrix4x4& m);
	static void Set_Transform(VkTransformState transform,const Matrix3D& m);
	static void Get_Transform(VkTransformState transform, Matrix4x4& m);
	static void	Set_World_Identity();
	static void Set_View_Identity();
	static bool	Is_World_Identity();
	static bool Is_View_Identity();

	static VK::Buffer UboProj();
	static VK::Buffer UboView();
	static VK::Buffer UboIdentProj();
	static VK::Buffer UboIdent();

	// Note that *_DX8_Transform() functions take the matrix in DX8 format - transposed from Westwood convention.

	static void _Set_DX8_Transform(VkTransformState transform,const Matrix4x4& m);
	static void _Set_DX8_Transform(VkTransformState transform,const Matrix3D& m);
	static void _Get_DX8_Transform(VkTransformState transform, Matrix4x4& m);/*
 * State enumerants for per-stage processing of fixed function pixel processing
 * Two of these affect fixed function vertex processing as well: TEXTURETRANSFORMFLAGS and TEXCOORDINDEX.
 */
	static void Set_DX8_Render_State(VKRENDERSTATETYPE state, unsigned value);
	static void Set_DX8_Texture_Stage_State(unsigned stage, VKTEXTURESTAGESTATETYPE state, unsigned value);
	static void Set_DX8_Sampler_Stage_State(unsigned stage, VKSAMPLERSTATETYPE state, unsigned value);

	static void Set_DX8_Texture(unsigned int stage, VK::Texture texture);
	static VK::Texture Get_DX8_Texture(unsigned int stage);
	static void Set_Light_Environment(LightEnvironmentClass* light_env);
	static LightEnvironmentClass* Get_Light_Environment() { return Light_Environment; }
	static void Set_Fog(bool enable, const Vector3 &color, float start, float end);

	static WWINLINE const LightGeneric& Peek_Light(unsigned index);
	static WWINLINE bool Is_Light_Enabled(unsigned index);

	static bool Validate_Device(void);

	// Deferred

	static void Set_Shader(const ShaderClass& shader);
	static void Get_Shader(ShaderClass& shader);
	static void Set_Texture(unsigned stage,TextureBaseClass* texture);
	static TextureBaseClass* Get_Texture(unsigned stage);
	static void Set_Material(const VertexMaterialClass* material);
	static void Set_Light(unsigned index,const LightGeneric* light);
	static void Set_Light(unsigned index,const LightClass &light);

	static VK::Buffer UboLight();
	static VK::Buffer UboMaterial();

	static void Apply_Render_State_Changes();	// Apply deferred render state changes (will be called automatically by Draw...)

#ifdef INFO_VULKAN
	static void Draw_Triangles(
		unsigned buffer_type,
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	static void Draw_Triangles(
		unsigned short start_index,
		unsigned short polygon_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
	static void Draw_Strip(
		unsigned short start_index,
		unsigned short index_count,
		unsigned short min_vertex_index,
		unsigned short vertex_count);
#endif

	/*
	** Resources
	*/

#ifdef INFO_VULKAN
	static IDirect3DVolumeTexture9* _Create_DX8_Volume_Texture
	(
		unsigned int width,
		unsigned int height,
		unsigned int depth,
		WW3DFormat format,
		MipCountType mip_level_count
	);

	static IDirect3DCubeTexture9* _Create_DX8_Cube_Texture
	(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count,
		bool rendertarget=false
	);


	static VK::Texture _Create_DX8_ZTexture
	(
		unsigned int width,
		unsigned int height,
		WW3DZFormat zformat,
		MipCountType mip_level_count
	);


	static VK::Texture _Create_DX8_Texture
	(
		unsigned int width,
		unsigned int height,
		WW3DFormat format,
		MipCountType mip_level_count
	);
	static IDirect3DTexture9 * _Create_DX8_Texture(const char *filename, MipCountType mip_level_count);
	static IDirect3DTexture9 * _Create_DX8_Texture(IDirect3DSurface9 *surface, MipCountType mip_level_count);

	static IDirect3DSurface9 * _Create_DX8_Surface(unsigned int width, unsigned int height, WW3DFormat format);
	static IDirect3DSurface9 * _Create_DX8_Surface(const char *filename);
	static IDirect3DSurface9 * _Get_DX8_Front_Buffer();
	static SurfaceClass * _Get_DX8_Back_Buffer(unsigned int num=0);

	static void _Copy_DX8_Rects(
			IDirect3DSurface9* pSourceSurface,
			CONST RECT* pSourceRectsArray,
			UINT cRects,
			IDirect3DSurface9* pDestinationSurface,
			CONST POINT* pDestPointsArray
	);
#endif
	static VK::Surface _Create_DX8_Surface(unsigned int width, unsigned int height, WW3DFormat format);
	static VK::Surface _Create_DX8_Surface(const char* filename);

	static void _Update_Texture(TextureClass *system, TextureClass *video);
	static void Flush_DX8_Resource_Manager(unsigned int bytes=0);
	static unsigned int Get_Free_Texture_RAM();

	static unsigned _Get_Main_Thread_ID() { return _MainThreadID; }
#ifdef INFO_VULKAN
	static const D3DADAPTER_IDENTIFIER9& Get_Current_Adapter_Identifier() { return CurrentAdapterIdentifier; }
#endif

	/*
	** Statistics
	*/
	static void Begin_Statistics();
	static void End_Statistics();
	static unsigned Get_Last_Frame_Matrix_Changes();
	static unsigned Get_Last_Frame_Material_Changes();
	static unsigned Get_Last_Frame_Vertex_Buffer_Changes();
	static unsigned Get_Last_Frame_Index_Buffer_Changes();
	static unsigned Get_Last_Frame_Light_Changes();
	static unsigned Get_Last_Frame_Texture_Changes();
	static unsigned Get_Last_Frame_Render_State_Changes();
	static unsigned Get_Last_Frame_Texture_Stage_State_Changes();
	static unsigned Get_Last_Frame_DX8_Calls();
	static unsigned Get_Last_Frame_Draw_Calls();

	static unsigned long Get_FrameCount(void);

	// Needed by shader class
	static bool						Get_Fog_Enable() { return FogEnable; }
	static uint32_t				Get_Fog_Color() { return FogColor; }

	// Utilities
	static Vector4 Convert_Color(unsigned color);
	static unsigned int Convert_Color(const Vector4& color);
	static unsigned int Convert_Color(const Vector3& color, const float alpha);
	static void Clamp_Color(Vector4& color);
	static unsigned int Convert_Color_Clamp(const Vector4& color);

	static void			  Set_Alpha (const float alpha, unsigned int &color);

	static void _Enable_Triangle_Draw(bool enable) { _EnableTriangleDraw=enable; }
	static bool _Is_Triangle_Draw_Enabled() { return _EnableTriangleDraw; }


	/*
	** Render target interface. If render target format is WW3D_FORMAT_UNKNOWN, current display format is used.
	*/
	static TextureClass *	Create_Render_Target (int width, int height, WW3DFormat format = WW3D_FORMAT_UNKNOWN);

	static void					Set_Render_Target(); //Clear to base

	static bool					Is_Render_To_Texture(void) { return IsRenderToTexture; }

	// for depth map support KJM V
	static void Create_Render_Target
	(
		int width, 
		int height, 
		WW3DFormat format,
		WW3DZFormat zformat,
		TextureClass** target,
		ZTextureClass** depth_buffer
	);
	static void Set_Render_Target_With_Z (TextureClass * texture, ZTextureClass* ztexture=NULL);

	static void Set_Shadow_Map(int idx, ZTextureClass* ztex) { Shadow_Map[idx]=ztex; }
	static ZTextureClass* Get_Shadow_Map(int idx) { return Shadow_Map[idx]; }
	// for depth map support KJM ^

	// shader system udpates KJM v
	static void Apply_Default_State();

#ifdef INFO_VULKAN
	static void Set_Vertex_Shader(DWORD vertex_shader);
	static void Set_Vertex_Shader(IDirect3DVertexShader9* vertex_shader);
	static void Set_Pixel_Shader(IDirect3DPixelShader9* pixel_shader);
#endif
	static void Set_Pipeline(WWVK_Pipeline_Entry pipeline) { pipeline_ = pipeline; }
	static WWVK_Pipeline_Entry Get_Pipeline() { return pipeline_; }

	static void Set_Vertex_Shader_Constant(int reg, const void* data, int count);
	static void Set_Pixel_Shader_Constant(int reg, const void* data, int count);

	static DWORD Get_Vertex_Processing_Behavior() { return Vertex_Processing_Behavior; }

	// Needed by scene lighting class
	static void						Set_Ambient(const Vector3& color);
	static const Vector3&		Get_Ambient() { return Ambient_Color; }
	// shader system updates KJM ^




#ifdef INFO_VULKAN
	static IDirect3DDevice9* _Get_D3D_Device8() { return D3DDevice; }
	static IDirect3D9* _Get_D3D8() { return D3DInterface; }
#endif
	static VkRenderTarget& _GetRenderTarget() { return target; }
	static WWVK_Pipeline_Collection& _GetPipelineCol() { return pipelineCol_; }

	/// Returns the display format - added by TR for video playback - not part of W3D
	static WW3DFormat	getBackBufferFormat( void );
	static bool Reset_Device(bool reload_assets=true);

#ifdef INFO_VULKAN
	static const DX8Caps*	Get_Current_Caps() { 
		WWASSERT(CurrentCaps); return CurrentCaps; 
	}
#endif

#ifdef INFO_VULKAN
	static const char* Get_DX8_Render_State_Name(D3DRENDERSTATETYPE state);
	static const char* Get_DX8_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state);
	static const char* Get_DX8_Sampler_Stage_State_Name(D3DSAMPLERSTATETYPE state);

	// Names of the specific values of render states and texture stage states
	static void Get_DX8_Texture_Stage_State_Value_Name(StringClass& name, D3DTEXTURESTAGESTATETYPE state, unsigned value);
	static void Get_DX8_Sampler_Stage_State_Value_Name(StringClass& name, D3DSAMPLERSTATETYPE state, unsigned value);
	static void Get_DX8_Render_State_Value_Name(StringClass& name, D3DRENDERSTATETYPE state, unsigned value);
#endif
	static unsigned Get_DX8_Render_State(VKRENDERSTATETYPE state) 
	{ return RenderStates[state] == 0x12345678 ? RenderStates[VKRS_MAX + state] : RenderStates[state]; }

	static const char* Get_DX8_Texture_Address_Name(unsigned value);
	static const char* Get_DX8_Texture_Filter_Name(unsigned value);
	static const char* Get_DX8_Texture_Arg_Name(unsigned value);
	static const char* Get_DX8_Texture_Op_Name(unsigned value);
	static const char* Get_DX8_Texture_Transform_Flag_Name(unsigned value);
	static const char* Get_DX8_ZBuffer_Type_Name(unsigned value);
	static const char* Get_DX8_Fill_Mode_Name(unsigned value);
	static const char* Get_DX8_Shade_Mode_Name(unsigned value);
	static const char* Get_DX8_Blend_Name(unsigned value);
	static const char* Get_DX8_Cull_Mode_Name(unsigned value);
	static const char* Get_DX8_Cmp_Func_Name(unsigned value);
	static const char* Get_DX8_Fog_Mode_Name(unsigned value);
	static const char* Get_DX8_Stencil_Op_Name(unsigned value);
	static const char* Get_DX8_Material_Source_Name(unsigned value);
	static const char* Get_DX8_Vertex_Blend_Flag_Name(unsigned value);
	static const char* Get_DX8_Patch_Edge_Style_Name(unsigned value);
	static const char* Get_DX8_Debug_Monitor_Token_Name(unsigned value);
	static const char* Get_DX8_Blend_Op_Name(unsigned value);

	static void Invalidate_Cached_Render_States(void);

	static void Set_Draw_Polygon_Low_Bound_Limit(unsigned n) { DrawPolygonLowBoundLimit=n; }

	struct WWVK_Pipeline_State
	{
		unsigned FVF;
		uint64_t isDynamic;
		VkPrimitiveTopology topo;
		unsigned RenderStates[VKRS_SHADER_COMPARE_MAX];
		unsigned TextureStageStates[4][VKTSS_MAX];
		//unsigned SamplerStates[MAX_TEXTURE_STAGES][VKSAMP_MAX];//I don't think I need to track this for shaders
	};
protected:

	static bool	Create_Device(void);
	static void Release_Device(void);

	static void Reset_Statistics();
	static void Enumerate_Devices();
	static void Set_Default_Global_Render_States(void);

	/*
	** Device Selection Code.
	** For backward compatibility, the public interface for these functions is in the ww3d.
	** header file.  These functions are protected so that we aren't exposing two interfaces.
	*/
	static bool Set_Any_Render_Device(void);
	static bool	Set_Render_Device(const char * dev_name,int width=-1,int height=-1,int bits=-1,int windowed=-1,bool resize_window=false);
	static bool	Set_Render_Device(int dev=-1,int resx=-1,int resy=-1,int bits=-1,int windowed=-1,bool resize_window = false, bool reset_device = false, bool restore_assets=true);
	static bool Toggle_Windowed(void);

	static int	Get_Render_Device_Count(void);
	static int	Get_Render_Device(void);
	static const RenderDeviceDescClass & Get_Render_Device_Desc(int deviceidx);
	static const char * Get_Render_Device_Name(int device_index);
	static bool Set_Device_Resolution(int width=-1,int height=-1,int bits=-1,int windowed=-1, bool resize_window=false);
	static void Get_Device_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed);
	static void Get_Render_Target_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed);
	static int	Get_Device_Resolution_Width(void) { return ResolutionWidth; }
	static int	Get_Device_Resolution_Height(void) { return ResolutionHeight; }

#ifdef INFO_VULKAN
	static bool Registry_Save_Render_Device( const char *sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth);
	static bool Registry_Load_Render_Device( const char * sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int &texture_depth);
#endif
	static bool Is_Windowed(void) { return IsWindowed; }

	static void	Set_Texture_Bitdepth(int depth)	{ WWASSERT(depth==16 || depth==32); TextureBitDepth = depth; }
	static int	Get_Texture_Bitdepth(void)			{ return TextureBitDepth; }

	static void	Set_Swap_Interval(int swap);
	static int	Get_Swap_Interval(void);
	static void Set_Polygon_Mode(int mode);

	/*
	** Internal functions
	*/
#ifdef INFO_VULKAN
	static bool Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,D3DFORMAT * set_colorbuffer,D3DFORMAT * set_backbuffer, D3DFORMAT * set_zmode);
	static bool Find_Color_Mode(D3DFORMAT colorbuffer, int resx, int resy, UINT *mode);
	static bool Find_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT *zmode);
	static bool Test_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT zmode);
#endif
	static void Compute_Caps(WW3DFormat display_format);

	/*
	** Protected Member Variables
	*/

	static DX8_CleanupHook *m_pCleanupHook;

	static RenderStateStruct			render_state;
	static unsigned						render_state_changed;
	static Matrix4x4						DX8Transforms[((int)VkTS::WORLD) + 1];
	static VK::Buffer DX8TransformsUbos[((int)VkTS::WORLD) + 1];
	static VK::Buffer ProjUbo, ProjIdentityUbo, IdentityUbo;
	static VK::Buffer LightUbo;
	static VK::Texture WhiteTexture;

	static bool								IsInitted;
	static bool								IsDeviceLost;
	static void *							Hwnd;
	static unsigned						_MainThreadID;

	static bool								_EnableTriangleDraw;

	static int								CurRenderDevice;
	static int								ResolutionWidth;
	static int								ResolutionHeight;
	static int								BitDepth;
	static int								TextureBitDepth;
	static bool								IsWindowed;
	
	static DirectX::XMMATRIX						old_world;
	static DirectX::XMMATRIX						old_view;
	static DirectX::XMMATRIX						old_prj;
#ifdef INFO_VULKAN
	static D3DFORMAT					DisplayFormat;

	// shader system updates KJM v
	static DWORD							Vertex_Shader_FVF;
	static IDirect3DVertexShader9* Vertex_Shader_Ptr;
	static IDirect3DPixelShader9* Pixel_Shader;
#endif
	static WWVK_Pipeline_Entry pipeline_;
	static WWVK_Pipeline_State pipelineStates_[PIPELINE_WWVK_MAX];

	static Vector4							Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
	static Vector4							Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

	static LightEnvironmentClass*		Light_Environment;
	static RenderInfoClass*				Render_Info;
	static DX8Material Material;
	static VK::Buffer MaterialUbo;

	static DWORD							Vertex_Processing_Behavior;

	static ZTextureClass*				Shadow_Map[MAX_SHADOW_MAPS];

	static Vector3							Ambient_Color;
	// shader system updates KJM ^

	static bool world_identity;
	static unsigned RenderStates[VKRS_MAX * 2];
	static unsigned TextureStageStates[MAX_TEXTURE_STAGES][VKTSS_MAX];
	static unsigned SamplerStates[MAX_TEXTURE_STAGES][VKSAMP_MAX];
	static VK::Texture Textures[MAX_TEXTURE_STAGES];

	// These fog settings are constant for all objects in a given scene,
	// unlike the matching renderstates which vary based on shader settings.
	static bool								FogEnable;
	static uint32_t						FogColor;

	static unsigned						matrix_changes;
	static unsigned						material_changes;
	static unsigned						vertex_buffer_changes;
	static unsigned						index_buffer_changes;
	static unsigned						light_changes;
	static unsigned						texture_changes;
	static unsigned						render_state_changes;
	static unsigned						texture_stage_state_changes;
	static unsigned						draw_calls;
	static bool								CurrentDX8LightEnables[4];

	static unsigned long FrameCount;


#ifdef INFO_VULKAN
	static DX8Caps*						CurrentCaps;
	static D3DADAPTER_IDENTIFIER9		CurrentAdapterIdentifier;

	static IDirect3D9 *					D3DInterface;			//d3d8;
	static IDirect3DDevice9 *			D3DDevice;				//d3ddevice8;
#endif
	static VkRenderTarget target;
	static WWVK_Pipeline_Collection pipelineCol_;

#ifdef INFO_VULKAN
	static IDirect3DSurface9 *			CurrentRenderTarget;
	static IDirect3DSurface9 *			CurrentDepthBuffer;
	static IDirect3DSurface9 *			DefaultRenderTarget;
	static IDirect3DSurface9 *			DefaultDepthBuffer;
#endif

	static unsigned							DrawPolygonLowBoundLimit;

	static bool								IsRenderToTexture;

	static int								ZBias;
	static float							ZNear;
	static float							ZFar;
	static Matrix4x4						ProjectionMatrix;

	friend void DX8_Assert();
	friend class WW3D;
	friend class DX8IndexBufferClass;
	friend class DX8VertexBufferClass;
};

// shader system updates KJM v
#ifdef INFO_VULKAN
WWINLINE void DX8Wrapper::Set_Vertex_Shader(DWORD vertex_shader)
{
#if 0 //(gth) some code is bypassing this acessor function so we can't count on this variable...
	// may be incorrect if shaders are created and destroyed dynamically
	if (Vertex_Shader == vertex_shader) return;
#endif

	Vertex_Shader_Ptr = 0;
	Vertex_Shader_FVF = vertex_shader;
	DX8CALL(SetFVF(Vertex_Shader_FVF));
}
WWINLINE void DX8Wrapper::Set_Vertex_Shader(IDirect3DVertexShader9* vertex_shader)
{
#if 0 //(gth) some code is bypassing this acessor function so we can't count on this variable...
	// may be incorrect if shaders are created and destroyed dynamically
	if (Vertex_Shader == vertex_shader) return;
#endif

	Vertex_Shader_Ptr = vertex_shader;
	Vertex_Shader_FVF = 0;
	DX8CALL(SetVertexShader(vertex_shader));
}

WWINLINE void DX8Wrapper::Set_Pixel_Shader(IDirect3DPixelShader9* pixel_shader)
{
	// may be incorrect if shaders are created and destroyed dynamically
	if (Pixel_Shader==pixel_shader) return;

	Pixel_Shader=pixel_shader;
	DX8CALL(SetPixelShader(Pixel_Shader));
}
#endif
WWINLINE void DX8Wrapper::Set_Vertex_Shader_Constant(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Vertex_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Vertex_Shader_Constants[reg],data,memsize);
#ifdef INFO_VULKAN
	DX8CALL(SetVertexShaderConstantF(reg,(const float*)data,count));
#endif
}
WWINLINE void DX8Wrapper::Set_Pixel_Shader_Constant(int reg, const void* data, int count)
{
	int memsize=sizeof(Vector4)*count;

	// may be incorrect if shaders are created and destroyed dynamically
	if (memcmp(data, &Pixel_Shader_Constants[reg],memsize)==0) return;

	memcpy(&Pixel_Shader_Constants[reg],data,memsize);
#ifdef INFO_VULKAN
	DX8CALL(SetPixelShaderConstantF(reg,(float*)data,count));
#endif
}
#endif
// shader system updates KJM ^


WWINLINE void DX8Wrapper::_Set_DX8_Transform(VkTransformState transform,const Matrix4x4& m)
{
	WWASSERT(transform<=VkTS::WORLD);
#if 0 // (gth) this optimization is breaking generals because they set the transform behind our backs.
	if (m!=DX8Transforms[transform]) 
#endif
	if (transform == VkTS::WORLD)
	{
		render_state.world = m;
	}
	{
		target.PushSingleFrameBuffer(DX8TransformsUbos[(uintptr_t)transform]);
		VkBufferTools::CreateUniformBuffer(&target, sizeof(float) * 16, (uint8_t*) & m, DX8TransformsUbos[(uintptr_t)transform]);
		DX8Transforms[(uintptr_t)transform]=m;
		SNAPSHOT_SAY(("DX8 - SetTransform %d [%f,%f,%f,%f][%f,%f,%f,%f][%f,%f,%f,%f][%f,%f,%f,%f]\n",transform,m[0][0],m[0][1],m[0][2],m[0][3],m[1][0],m[1][1],m[1][2],m[1][3],m[2][0],m[2][1],m[2][2],m[2][3],m[3][0],m[3][1],m[3][2],m[3][3]));
		DX8_RECORD_MATRIX_CHANGE();
#ifdef INFO_VULKAN
		DX8CALL(SetTransform(transform,(DirectX::XMMATRIX*)&m));
#endif
	}
}


WWINLINE void DX8Wrapper::_Set_DX8_Transform(VkTransformState transform,const Matrix3D& m)
{
	WWASSERT(transform<=VkTS::WORLD);
	Matrix4x4 mtx(m);
	{
		DX8Transforms[(uintptr_t)transform]=mtx;
		SNAPSHOT_SAY(("DX8 - SetTransform %d [%f,%f,%f,%f][%f,%f,%f,%f][%f,%f,%f,%f]\n",transform,m[0][0],m[0][1],m[0][2],m[0][3],m[1][0],m[1][1],m[1][2],m[1][3],m[2][0],m[2][1],m[2][2],m[2][3]));
		DX8_RECORD_MATRIX_CHANGE();
#ifdef INFO_VULKAN
		DX8CALL(SetTransform(transform,(DirectX::XMMATRIX*)&m));
#endif
	}
}

WWINLINE void DX8Wrapper::_Get_DX8_Transform(VkTransformState transform, Matrix4x4& m)
{
	switch (transform) {
	case VkTS::WORLD:
		m = render_state.world;
		break;
	case VkTS::VIEW:
		m = render_state.view;
		break;
	default:
		m = DX8Transforms[(uint32_t)transform];
		break;
	}
#ifdef INFO_VULKAN
	DX8CALL(GetTransform(transform,(DirectX::XMMATRIX*)&m));
#endif
}

// ----------------------------------------------------------------------------
//
// Set the index offset for the current index buffer
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_Index_Buffer_Index_Offset(unsigned offset)
{
	if (render_state.index_base_offset==offset) return;
	render_state.index_base_offset=offset;
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
// Set the fog settings. This function should be used, rather than setting the
// appropriate renderstates directly, because the shader sets some of the
// renderstates on a per-mesh / per-pass basis depending on global fog states
// (stored in the wrapper) as well as the shader settings.
// This function should be called rarely - once per scene would be appropriate.
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_Fog(bool enable, const Vector3 &color, float start, float end)
{
	// Set global states
	FogEnable = enable;
	FogColor = Convert_Color(color,0.0f);

	// Invalidate the current shader (since the renderstates set by the shader
	// depend on the global fog settings as well as the actual shader settings)
	ShaderClass::Invalidate();

	// Set renderstates which are not affected by the shader
	Set_DX8_Render_State(VKRS_FOGSTART, *(DWORD *)(&start));
	Set_DX8_Render_State(VKRS_FOGEND,   *(DWORD *)(&end));
}


WWINLINE void DX8Wrapper::Set_Ambient(const Vector3& color)
{
	Ambient_Color=color;
	Set_DX8_Render_State(VKRS_AMBIENT, DX8Wrapper::Convert_Color(color,0.0f));
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer to be used in the subsequent render calls. If there was
// a vertex buffer being used earlier, release the reference to it. Passing
// NULL just will release the vertex buffer.
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Set_DX8_Material(const DX8Material* mat)
{
	DX8_RECORD_MATERIAL_CHANGE();
	WWASSERT(mat);
	SNAPSHOT_SAY(("DX8 - SetMaterial\n"));
#ifdef INFO_VULKAN
	DX8CALL(SetMaterial(mat));
#endif
	Material = *mat;
	target.PushSingleFrameBuffer(MaterialUbo);
	VkBufferTools::CreateUniformBuffer(&target, sizeof(Material), &Material, MaterialUbo);
}

WWINLINE void DX8Wrapper::Set_DX8_Render_State(VKRENDERSTATETYPE state, unsigned value)
{
#ifdef INFO_VULKAN
	// Can't monitor state changes because setShader call to GERD may change the states!
	if (RenderStates[state]==value) return;

#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0,true);
		Get_DX8_Render_State_Value_Name(value_name,state,value);
		SNAPSHOT_SAY(("DX8 - SetRenderState(state: %s, value: %s)\n",
			Get_DX8_Render_State_Name(state),
			value_name));
	}
#endif

	RenderStates[state]=value;
	DX8CALL(SetRenderState( state, value ));
	DX8_RECORD_RENDER_STATE_CHANGE();
#endif
	if (RenderStates[state] == value) return;
	RenderStates[state] = value;
	if (!target.currentCmd)
		return;
#if 0
	if (state == VKRS_COLORWRITEENABLE)
	{
		VkColorComponentFlags flags[4] = {};
		unsigned int flagCount = 0;
		if (0x1 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_R_BIT; }
		if (0x2 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_G_BIT; }
		if (0x4 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_B_BIT; }
		if (0x8 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_A_BIT; }
		flagCount = 1;
		if (RenderStates[VKRS_COLORWRITEENABLE] == 0x12345678)
		{
			flags[0] = VK_COLOR_COMPONENT_R_BIT| VK_COLOR_COMPONENT_G_BIT| VK_COLOR_COMPONENT_B_BIT| VK_COLOR_COMPONENT_A_BIT;
			flagCount = 1;
		}
		vkCmdSetColorWriteMaskEXT(target.currentCmd, 0, flagCount, flags);
	}
	else if (state == VKRS_DEPTHBIAS)
	{
		vkCmdSetDepthBias(target.currentCmd, RenderStates[VKRS_DEPTHBIAS] * -0.000005f, 0, 0);
	}
	else if (state == VKRS_ZWRITEENABLE)
	{
		vkCmdSetDepthWriteEnable(target.currentCmd, RenderStates[VKRS_ZWRITEENABLE]);
	}
	else if (state == VKRS_ZFUNC)
	{
		vkCmdSetDepthCompareOp(target.currentCmd, (VkCompareOp)RenderStates[VKRS_ZFUNC]);
	}
	else if (state == VKRS_CULLMODE)
	{
		VkCullModeFlags cull = {};
		switch (RenderStates[VKRS_CULLMODE])
		{
		case VK_FRONT_FACE_MAX_ENUM: cull = VK_CULL_MODE_NONE; break;
		default:
		case VK_FRONT_FACE_CLOCKWISE: cull = VK_CULL_MODE_BACK_BIT; break;
		case VK_FRONT_FACE_COUNTER_CLOCKWISE: cull = VK_CULL_MODE_FRONT_BIT; break;
		}
		vkCmdSetCullMode(target.currentCmd, cull);
	}
	else if (state == VKRS_STENCILMASK)
	{
		vkCmdSetStencilCompareMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILMASK]);
	}
	else if (state == VKRS_STENCILWRITEMASK)
	{
		vkCmdSetStencilWriteMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILWRITEMASK]);
	}
	else if (state == VKRS_STENCILREF)
	{
		vkCmdSetStencilReference(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILREF]);
	}
	else if (state == VKRS_STENCILENABLE)
	{
		vkCmdSetStencilTestEnable(WWVKRENDER.currentCmd, RenderStates[VKRS_STENCILENABLE]);
	}
#endif
}

WWINLINE void DX8Wrapper::Set_DX8_Texture_Stage_State(unsigned stage, VKTEXTURESTAGESTATETYPE state, unsigned value)
{
#ifdef INFO_VULKAN
  	if (stage >= MAX_TEXTURE_STAGES)
  	{	DX8CALL(SetTextureStageState( stage, state, value ));
  		return;
  	}

	// Can't monitor state changes because setShader call to GERD may change the states!
	if (TextureStageStates[stage][(unsigned int)state]==value) return;
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0,true);
		Get_DX8_Texture_Stage_State_Value_Name(value_name,state,value);
		SNAPSHOT_SAY(("DX8 - SetTextureStageState(stage: %d, state: %s, value: %s)\n",
			stage,
			Get_DX8_Texture_Stage_State_Name(state),
			value_name));
	}
#endif

	TextureStageStates[stage][(unsigned int)state]=value;
	DX8CALL(SetTextureStageState( stage, state, value ));
	DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
#endif
	if (stage >= MAX_TEXTURE_STAGES)
	{
		return;
	}
	TextureStageStates[stage][(unsigned int)state] = value;
}

WWINLINE void DX8Wrapper::Set_DX8_Sampler_Stage_State(unsigned stage, VKSAMPLERSTATETYPE state, unsigned value)
{
	if (stage >= MAX_TEXTURE_STAGES)
	{
#ifdef INFO_VULKAN
		DX8CALL(SetSamplerState(stage, state, value));
#endif
		return;
	}

	// Can't monitor state changes because setShader call to GERD may change the states!
#ifdef INFO_VULKAN
	if (SamplerStates[stage][(unsigned int)state] == value) return;
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0, true);
		Get_DX8_Sampler_Stage_State_Value_Name(value_name, state, value);
		SNAPSHOT_SAY(("DX8 - SetTextureStageState(stage: %d, state: %s, value: %s)\n",
			stage,
			Get_DX8_Sampler_Stage_State_Name(state),
			value_name));
	}
#endif

	SamplerStates[stage][(unsigned int)state] = value;
	DX8CALL(SetSamplerState(stage, state, value));
	DX8_RECORD_TEXTURE_STAGE_STATE_CHANGE();
#else
	SamplerStates[stage][(unsigned int)state] = value;
#endif
}

#ifdef INFO_VULKAN
WWINLINE void DX8Wrapper::_Copy_DX8_Rects(
  IDirect3DSurface9* pSourceSurface,
  CONST RECT* pSourceRectsArray,
  UINT cRects,
  IDirect3DSurface9* pDestinationSurface,
  CONST POINT* pDestPointsArray
)
{
#if 0
	DX8CALL(CopyRects(
  pSourceSurface,
  pSourceRectsArray,
  cRects,
  pDestinationSurface,
  pDestPointsArray));
#else

	if (cRects == 0)
		cRects = 1;
	if (pDestPointsArray)
	{
		for (UINT i = 0; i < cRects; ++i)
		{
			//auto hr = 
				DX8Wrapper::_Get_D3D_Device8()->UpdateSurface(pSourceSurface, pSourceRectsArray + i, pDestinationSurface, pDestPointsArray + i); //DXS::G().number_of_DX8_calls++;;
			//DX8_ErrorCode(hr);
		}
	}
	else
	{
		for (UINT i = 0; i < cRects; ++i)
		{
			//auto hr = 
				DX8Wrapper::_Get_D3D_Device8()->UpdateSurface(pSourceSurface, pSourceRectsArray + i, pDestinationSurface, nullptr); //DXS::G().number_of_DX8_calls++;;
			//DX8_ErrorCode(hr);
		}
	}
#endif
}
#endif

WWINLINE void DX8Wrapper::Set_DX8_Texture(unsigned int stage, VK::Texture texture)
{
	if (stage >= MAX_TEXTURE_STAGES)
	{
		return;
	}

	if (Textures[stage].image == texture.image) return;

	SNAPSHOT_SAY(("DX8 - SetTexture(%x) \n", texture));

	Textures[stage] = texture;
	//if (Textures[stage]) Textures[stage]->AddRef();
	//DX8CALL(SetTexture(stage, texture));
	DX8_RECORD_TEXTURE_CHANGE();
}
WWINLINE VK::Texture DX8Wrapper::Get_DX8_Texture(unsigned int stage)
{
	if (Textures[stage].image)
	{
		VK::SamplerSettings settings = {};
		settings.minF = SamplerStates[stage][VKSAMP_MINFILTER];
		settings.maxF = SamplerStates[stage][VKSAMP_MAGFILTER];
		settings.mipF = SamplerStates[stage][VKSAMP_MIPFILTER];
		settings.addU = SamplerStates[stage][VKSAMP_ADDRESSU];
		settings.addV = SamplerStates[stage][VKSAMP_ADDRESSV];
		if (Textures[stage].sampSettings != settings)
		{
			VK::Texture dump = {};
			dump.sampler = Textures[stage].sampler;
			target.PushSingleTexture(dump);
			VK::CreateTextureSampler(&target, Textures[stage], settings, Textures[stage].mips);
			if (render_state.Textures[stage]->Peek_D3D_Texture().image == Textures[stage].image)
			{
				render_state.Textures[stage]->Peek_D3D_Texture() = Textures[stage];
			}
		}

		return Textures[stage];
	}
	if (!WhiteTexture.image)
	{
		uint32_t white = 0xffffffff;
		VK::CreateTexture(&target, WhiteTexture, 1, 1, (uint8_t*) & white);
	}
	return WhiteTexture;
}

WWINLINE Vector4 DX8Wrapper::Convert_Color(unsigned color)
{
	Vector4 col;
	col[3]=((color&0xff000000)>>24)/255.0f;
	col[0]=((color&0xff0000)>>16)/255.0f;
	col[1]=((color&0xff00)>>8)/255.0f;
	col[2]=((color&0xff)>>0)/255.0f;
//	col=Vector4(1.0f,1.0f,1.0f,1.0f);
	return col;
}

#if 0
WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector3& color, const float alpha)
{
	WWASSERT(color.X<=1.0f);
	WWASSERT(color.Y<=1.0f);
	WWASSERT(color.Z<=1.0f);
	WWASSERT(alpha<=1.0f);
	WWASSERT(color.X>=0.0f);
	WWASSERT(color.Y>=0.0f);
	WWASSERT(color.Z>=0.0f);
	WWASSERT(alpha>=0.0f);

	return D3DCOLOR_COLORVALUE(color.X,color.Y,color.Z,alpha);
}
WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector4& color)
{
	WWASSERT(color.X<=1.0f);
	WWASSERT(color.Y<=1.0f);
	WWASSERT(color.Z<=1.0f);
	WWASSERT(color.W<=1.0f);
	WWASSERT(color.X>=0.0f);
	WWASSERT(color.Y>=0.0f);
	WWASSERT(color.Z>=0.0f);
	WWASSERT(color.W>=0.0f);

	return D3DCOLOR_COLORVALUE(color.X,color.Y,color.Z,color.W);
}
#else

// ----------------------------------------------------------------------------
//
// Convert RGBA color from float vector to 32 bit integer
// Note: Color vector needs to be clamped to [0...1] range!
//
// ----------------------------------------------------------------------------

WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector3& color,float alpha)
{
	//const float scale = 255.0;
	unsigned int col;

	// Multiply r, g, b and a components (0.0,...,1.0) by 255 and convert to integer. Or the integer values togerher
	// such that 32 bit ingeger has AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB.
#if 0
	__asm
	{
		sub	esp,20					// space for a, r, g and b float plus fpu rounding mode

		// Store the fpu rounding mode

		fwait
		fstcw		[esp+16]				// store control word to stack
		mov		eax,[esp+16]		// load it to eax
		mov		edi,eax				// take copy
		and		eax,~(1024|2048)	// mask out certain bits
		or			eax,(1024|2048)	// or with precision control value "truncate"
		sub		edi,eax				// did it change?
		jz			skip					// .. if not, skip
		mov		[esp],eax			// .. change control word
		fldcw		[esp]
skip:

		// Convert the color

		mov	esi,dword ptr color
		fld	dword ptr[scale]

		fld	dword ptr[esi]			// r
		fld	dword ptr[esi+4]		// g
		fld	dword ptr[esi+8]		// b
		fld	dword ptr[alpha]		// a
		fld	st(4)
		fmul	st(4),st
		fmul	st(3),st
		fmul	st(2),st
		fmulp	st(1),st
		fistp	dword ptr[esp+0]		// a
		fistp	dword ptr[esp+4]		// b
		fistp	dword ptr[esp+8]		// g
		fistp	dword ptr[esp+12]		// r
		mov	ecx,[esp]				// a
		mov	eax,[esp+4]				// b
		mov	edx,[esp+8]				// g
		mov	ebx,[esp+12]			// r
		shl	ecx,24					// a << 24
		shl	ebx,16					// r << 16
		shl	edx,8						//	g << 8
		or		eax,ecx					// (a << 24) | b
		or		eax,ebx					// (a << 24) | (r << 16) | b
		or		eax,edx					// (a << 24) | (r << 16) | (g << 8) | b

		fstp	st(0)

		// Restore fpu rounding mode

		cmp	edi,0					// did we change the value?
		je		not_changed			// nope... skip now...
		fwait
		fldcw	[esp+16];
not_changed:
		add	esp,20

		mov	col,eax
	}
#else
	unsigned char r, g, b, a;
	r = (unsigned char)(color.X * 255.0);
	g = (unsigned char)(color.Y * 255.0);
	b = (unsigned char)(color.Z * 255.0);
	a = (unsigned char)(alpha * 255.0);
	col = (a << 24) | (r << 16) | (g << 8) | b;
#endif
	return col;
}

// ----------------------------------------------------------------------------
//
// Clamp color vertor to [0...1] range
//
// ----------------------------------------------------------------------------

WWINLINE void DX8Wrapper::Clamp_Color(Vector4& color)
{
	//if (!CPUDetectClass::Has_CMOV_Instruction())
	{
		for (int i=0;i<4;++i) {
			float f=(color[i]<0.0f) ? 0.0f : color[i];
			color[i]=(f>1.0f) ? 1.0f : f;
		}
		return;
	}
#if 0
	__asm
	{
		mov	esi,dword ptr color

		mov edx,0x3f800000

		mov edi,dword ptr[esi]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi],edi

		mov edi,dword ptr[esi+4]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+4],edi

		mov edi,dword ptr[esi+8]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+8],edi

		mov edi,dword ptr[esi+12]
		mov ebx,edi
		sar edi,31
		not edi			// mask is now zero if negative value
		and edi,ebx
		cmp edi,edx		// if no less than 1.0 set to 1.0
		cmovnb edi,edx
		mov dword ptr[esi+12],edi
	}
#endif
}

// ----------------------------------------------------------------------------
//
// Convert RGBA color from float vector to 32 bit integer
//
// ----------------------------------------------------------------------------

WWINLINE unsigned int DX8Wrapper::Convert_Color(const Vector4& color)
{
	return Convert_Color(reinterpret_cast<const Vector3&>(color),color[3]);
}

WWINLINE unsigned int DX8Wrapper::Convert_Color_Clamp(const Vector4& color)
{
	Vector4 clamped_color=color;
	DX8Wrapper::Clamp_Color(clamped_color);
	return Convert_Color(reinterpret_cast<const Vector3&>(clamped_color),clamped_color[3]);
}

#endif


WWINLINE void DX8Wrapper::Set_Alpha (const float alpha, unsigned int &color)
{
	unsigned char *component = (unsigned char*) &color;

	component [3] = 255.0f * alpha;
}

WWINLINE void DX8Wrapper::Get_Render_State(RenderStateStruct& state)
{
	state=render_state;
}

WWINLINE void DX8Wrapper::Get_Shader(ShaderClass& shader)
{
	shader=render_state.shader;
}

WWINLINE void DX8Wrapper::Set_Texture(unsigned stage,TextureBaseClass* texture)
{
	//WWASSERT(stage<(unsigned int)CurrentCaps->Get_Max_Textures_Per_Pass());
	if (texture==render_state.Textures[stage]) return;
	REF_PTR_SET(render_state.Textures[stage],texture);
	render_state_changed|=(TEXTURE0_CHANGED<<stage);
}

WWINLINE TextureBaseClass* DX8Wrapper::Get_Texture(unsigned stage)
{
	//WWASSERT(stage < (unsigned int)CurrentCaps->Get_Max_Textures_Per_Pass());
	return render_state.Textures[stage];
}

WWINLINE void DX8Wrapper::Set_Material(const VertexMaterialClass* material)
{
/*	if (material && render_state.material &&
		// !_stricmp(material->Get_Name(),render_state.material->Get_Name())) {
		material->Get_CRC()!=render_state.material->Get_CRC()) {
		return;
	}
*/
//	if (material==render_state.material) {
//		return;
//	}
	REF_PTR_SET(render_state.material,const_cast<VertexMaterialClass*>(material));
	render_state_changed|=MATERIAL_CHANGED;
	SNAPSHOT_SAY(("DX8Wrapper::Set_Material(%s)\n",material ? material->Get_Name() : "NULL"));
}

WWINLINE void DX8Wrapper::Set_Shader(const ShaderClass& shader)
{
	if (!ShaderClass::ShaderDirty && ((unsigned&)shader==(unsigned&)render_state.shader)) {
		return;
	}
	render_state.shader=shader;
	render_state_changed|=SHADER_CHANGED;
#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	StringClass str;
#endif
	SNAPSHOT_SAY(("DX8Wrapper::Set_Shader(%s)\n",shader.Get_Description(str)));
}

WWINLINE void DX8Wrapper::Set_Projection_Transform_With_Z_Bias(const Matrix4x4& matrix, float znear, float zfar)
{
	ZFar=zfar;
	ZNear=znear;
	ProjectionMatrix = matrix.Transpose();

	auto flip = DirectX::XMMatrixScaling(1, -1, 1);
	DirectX::XMMATRIX set, base = DirectX::XMMATRIX((float*)&ProjectionMatrix);
	if (
#ifdef INFO_VULKAN
		!Get_Current_Caps()->Support_ZBias() && 
#endif
		ZNear!=ZFar) {
		float tmp_zbias=ZBias;
		tmp_zbias*=(1.0f/16.0f);
		tmp_zbias*=1.0f / (ZFar - ZNear);
		base.r[2].m128_f32[2]-=tmp_zbias* base.r[3].m128_f32[2];
	}
	set = DirectX::XMMatrixMultiply(flip, base);
	WWVKRENDER.PushSingleFrameBuffer(ProjUbo);
	VkBufferTools::CreateUniformBuffer(&WWVKRENDER, sizeof(Matrix4x4), (uint8_t*)&set, ProjUbo);
}

WWINLINE void DX8Wrapper::Set_DX8_ZBias(int zbias)
{
	if (zbias==ZBias) return;
	if (zbias>15) zbias=15;
	if (zbias<0) zbias=0;
	ZBias=zbias;

#ifdef INFO_VULKAN
	if (!Get_Current_Caps()->Support_ZBias() && ZNear!=ZFar) {
		Matrix4x4 tmp=ProjectionMatrix;
		float tmp_zbias=ZBias;
		tmp_zbias*=(1.0f/16.0f);
		tmp_zbias*=1.0f / (ZFar - ZNear);
		tmp[2][2]-=tmp_zbias*tmp[3][2];
		DX8CALL(SetTransform(VkTS::PROJECTION,(DirectX::XMMATRIX*)&tmp));
	}
	else {
		Set_DX8_Render_State (VKRS_DEPTHBIAS,ZBias * -0.000005f);
	}
#else
	//No idea if this is correct
	vkCmdSetDepthBias(target.currentCmd, ZBias * -0.000005f, 0.f, 1.f);
#endif
}

WWINLINE void DX8Wrapper::Set_Transform(VkTransformState transform,const Matrix4x4& m)
{
	switch (transform) {
	case VkTS::WORLD:
		render_state.world = m.Transpose();
		render_state_changed|=(unsigned)WORLD_CHANGED;
		render_state_changed&=~(unsigned)WORLD_IDENTITY;
		break;
	case VkTS::VIEW:
		render_state.view = m.Transpose();
		render_state_changed|=(unsigned)VIEW_CHANGED;
		render_state_changed&=~(unsigned)VIEW_IDENTITY;
		break;
	case VkTS::PROJECTION:
		{
		Matrix4x4 ProjectionMatrix = m.Transpose();
			ZFar=0.0f;
			ZNear=0.0f;
			WWVKRENDER.PushSingleFrameBuffer(ProjUbo);
			VkBufferTools::CreateUniformBuffer(&target, sizeof(float) * 16, &ProjectionMatrix, ProjUbo);
		}
		break;
	default:
		DX8_RECORD_MATRIX_CHANGE();
		Matrix4x4 m2 = m.Transpose();
		WWVKRENDER.PushSingleFrameBuffer(DX8TransformsUbos[(int)transform]);
		VkBufferTools::CreateUniformBuffer(&target, sizeof(float) * 16, &m2, DX8TransformsUbos[(int)transform]);
		break;
	}
}

WWINLINE void DX8Wrapper::Set_Transform(VkTransformState transform,const Matrix3D& m)
{
	Matrix4x4 m2(m);
	switch (transform) {
	case VkTS::WORLD:
		render_state.world = m2.Transpose();
		render_state_changed|=(unsigned)WORLD_CHANGED;
		render_state_changed&=~(unsigned)WORLD_IDENTITY;
		break;
	case VkTS::VIEW:
		render_state.view = m2.Transpose();
		render_state_changed|=(unsigned)VIEW_CHANGED;
		render_state_changed&=~(unsigned)VIEW_IDENTITY;
		break;
	default:
		DX8_RECORD_MATRIX_CHANGE();
		m2 = m2.Transpose();
		target.PushSingleFrameBuffer(DX8TransformsUbos[(int)transform]);
		VkBufferTools::CreateUniformBuffer(&target, sizeof(float) * 16, &m2, DX8TransformsUbos[(int)transform]);
		break;
	}
}

WWINLINE void DX8Wrapper::Set_World_Identity()
{
	if (render_state_changed&(unsigned)WORLD_IDENTITY) return;
	render_state.world.Make_Identity();
	render_state_changed|=(unsigned)WORLD_CHANGED|(unsigned)WORLD_IDENTITY;
}

WWINLINE void DX8Wrapper::Set_View_Identity()
{
	if (render_state_changed&(unsigned)VIEW_IDENTITY) return;
	render_state.view.Make_Identity();
	render_state_changed|=(unsigned)VIEW_CHANGED|(unsigned)VIEW_IDENTITY;
}

WWINLINE bool DX8Wrapper::Is_World_Identity()
{
	return !!(render_state_changed&(unsigned)WORLD_IDENTITY);
}

WWINLINE bool DX8Wrapper::Is_View_Identity()
{
	return !!(render_state_changed&(unsigned)VIEW_IDENTITY);
}
WWINLINE VK::Buffer DX8Wrapper::UboProj()
{
	return ProjUbo;
}
WWINLINE VK::Buffer DX8Wrapper::UboIdentProj()
{
	if (!ProjIdentityUbo.buffer)
	{
		Matrix4x4 ident(true);
		ident[1][1] = -1;
		VkBufferTools::CreateUniformBuffer(&target, 16 * sizeof(float), &ident, ProjIdentityUbo);
	}
	return ProjIdentityUbo;
}
WWINLINE VK::Buffer DX8Wrapper::UboView()
{
	if ((render_state_changed & ((unsigned)VIEW_CHANGED | (unsigned)VIEW_IDENTITY)) == VIEW_CHANGED)
	{
		target.PushSingleFrameBuffer(DX8TransformsUbos[(int)VkTS::VIEW]);
		VkBufferTools::CreateUniformBuffer(&target, sizeof(float) * 16, &render_state.view, DX8TransformsUbos[(int)VkTS::VIEW]);
	}
	return Is_View_Identity() ? IdentityUbo : DX8TransformsUbos[(int)VkTS::VIEW];
}

WWINLINE VK::Buffer DX8Wrapper::UboIdent()
{
	if (!IdentityUbo.buffer)
	{
		Matrix4x4 ident(true);
		VkBufferTools::CreateUniformBuffer(&target, sizeof(Matrix4x4), &ident, IdentityUbo);
	}
	return IdentityUbo;
}

WWINLINE void DX8Wrapper::Get_Transform(VkTransformState transform, Matrix4x4& m)
{
	switch (transform) {
	case VkTS::WORLD:
		if (render_state_changed&WORLD_IDENTITY) m.Make_Identity();
		else m=render_state.world.Transpose();
		break;
	case VkTS::VIEW:
		if (render_state_changed&VIEW_IDENTITY) m.Make_Identity();
		else m=render_state.view.Transpose();
		break;
	default:
		m=*(Matrix4x4*)&DX8Transforms[(int)transform];
		m=m.Transpose();
		break;
	}
}

WWINLINE const LightGeneric& DX8Wrapper::Peek_Light(unsigned index)
{
	return render_state.Lights.lights[index];;
}

WWINLINE bool DX8Wrapper::Is_Light_Enabled(unsigned index)
{
	return render_state.LightEnable[index];
}


WWINLINE void DX8Wrapper::Set_Render_State(const RenderStateStruct& state)
{
	int i;

	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) 
	{
		if (render_state.vertex_buffers[i]) 
		{
			render_state.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	render_state=state;
	render_state_changed=0xffffffff;

	if (render_state.index_buffer) {
		render_state.index_buffer->Add_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) 
	{
		if (render_state.vertex_buffers[i]) 
		{
			render_state.vertex_buffers[i]->Add_Engine_Ref();
		}
	}
}

WWINLINE void DX8Wrapper::Release_Render_State()
{
	int i;

	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		if (render_state.vertex_buffers[i]) {
			render_state.vertex_buffers[i]->Release_Engine_Ref();
		}
	}

	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(render_state.vertex_buffers[i]);
	}
	REF_PTR_RELEASE(render_state.index_buffer);
	REF_PTR_RELEASE(render_state.material);

	
	for (i=0;i<MAX_TEXTURE_STAGES;++i) 
	{
		REF_PTR_RELEASE(render_state.Textures[i]);
	}
}


WWINLINE RenderStateStruct::RenderStateStruct()
	:
	material(0),
	index_buffer(0)
{
	unsigned i;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) vertex_buffers[i]=0;
	for (i=0;i<MAX_TEXTURE_STAGES;++i) Textures[i]=0;
  //lightsHash = (unsigned)this;
}

WWINLINE RenderStateStruct::~RenderStateStruct()
{
	unsigned i;
	REF_PTR_RELEASE(material);
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_RELEASE(vertex_buffers[i]);
	}
	REF_PTR_RELEASE(index_buffer);

	for (i=0;i<MAX_TEXTURE_STAGES;++i) 
	{
		REF_PTR_RELEASE(Textures[i]);
	}
}


WWINLINE unsigned flimby( char* name, unsigned crib )
{
  unsigned lnt prevVer = 0x00000000;  
  unsigned D3D2_BASE_VEC nextVer = 0;
  for( unsigned t = 0; t < crib; ++t )
  {
    (D3D2_BASE_VEC)nextVer += name[t];
    (D3D2_BASE_VEC)nextVer %= 32;
    (D3D2_BASE_VEC)nextVer-- ;
    (lnt) prevVer ^=  ( 1 << (D3D2_BASE_VEC)prevVer ); 
  }
  return (lnt) prevVer;
}

WWINLINE RenderStateStruct& RenderStateStruct::operator= (const RenderStateStruct& src)
{
	unsigned i;
	REF_PTR_SET(material,src.material);
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		REF_PTR_SET(vertex_buffers[i],src.vertex_buffers[i]);
	}
	REF_PTR_SET(index_buffer,src.index_buffer);

	for (i=0;i<MAX_TEXTURE_STAGES;++i) 
	{
		REF_PTR_SET(Textures[i],src.Textures[i]);
	}

	LightEnable[0]=src.LightEnable[0];
	LightEnable[1]=src.LightEnable[1];
	LightEnable[2]=src.LightEnable[2];
	LightEnable[3]=src.LightEnable[3];
	if (LightEnable[0]) {
		Lights.lights[0]=src.Lights.lights[0];
		if (LightEnable[1]) {
			Lights.lights[1]=src.Lights.lights[1];
			if (LightEnable[2]) {
				Lights.lights[2]=src.Lights.lights[2];
				if (LightEnable[3]) {
					Lights.lights[3]=src.Lights.lights[3];
				}
			}
		}


    //lightsHash = flimby((char*)(&Lights[0]), sizeof(D3DLIGHT9)-1 );

	}

	shader=src.shader;
	world=src.world;
	view=src.view;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		vertex_buffer_types[i]=src.vertex_buffer_types[i];
	}
	index_buffer_type=src.index_buffer_type;
	vba_offset=src.vba_offset;
	vba_count=src.vba_count;
	iba_offset=src.iba_offset;
	index_base_offset=src.index_base_offset;

	return *this;
}


