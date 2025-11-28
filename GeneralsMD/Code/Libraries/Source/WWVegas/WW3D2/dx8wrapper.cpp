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
 *                     $Archive:: /Commando/Code/ww3d2/dx8wrapper.cpp                         $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               * 
 *                                                                                             * 
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 170                                                         $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 * 06/27/02 KM Render to shadow buffer texture support														*
 * 06/27/02 KM Shader system updates																				*
 * 08/05/02 KM Texture class redesign 
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   DX8Wrapper::_Update_Texture -- Copies a texture from system memory to video memory        *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

//#define CREATE_DX8_MULTI_THREADED
//#define CREATE_DX8_FPU_PRESERVE
#define WW3D_DEVTYPE D3DDEVTYPE_HAL

#include "dx8wrapper.h"
#include "dx8fvf.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "dx8renderer.h"
#include "ww3d.h"
#include "camera.h"
#include "wwstring.h"
#include "matrix4.h"
#include "vertmaterial.h"
#include "rddesc.h"
#include "lightenvironment.h"
#include "statistics.h"
#include "registry.h"
#include "boxrobj.h"
#include "pointgr.h"
#include "render2d.h"
#include "sortingrenderer.h"
#include "shattersystem.h"
#include "light.h"
#include "assetmgr.h"
#include "textureloader.h"
#include "missingtexture.h"
#include "thread.h"
#include <stdio.h>
#include "pot.h"
#include "wwprofile.h"
#include "ffactory.h"
#include "dx8caps.h"
#include "formconv.h"
#include "dx8texman.h"
#include "bound.h"
#include <DxErr.h>

#include "VkTexture.h"

#include "shdlib.h"

const int DEFAULT_RESOLUTION_WIDTH = 640;
const int DEFAULT_RESOLUTION_HEIGHT = 480;
const int DEFAULT_BIT_DEPTH = 32;
const int DEFAULT_TEXTURE_BIT_DEPTH = 16;

bool DX8Wrapper_IsWindowed = true;

// FPU_PRESERVE
int DX8Wrapper_PreserveFPU = 0;

/***********************************************************************************
**
** DX8Wrapper Static Variables
**
***********************************************************************************/

static HWND						_Hwnd															= NULL;
bool								DX8Wrapper::IsInitted									= false;
bool								DX8Wrapper::_EnableTriangleDraw						= true;

int								DX8Wrapper::CurRenderDevice							= -1;
int								DX8Wrapper::ResolutionWidth							= DEFAULT_RESOLUTION_WIDTH;
int								DX8Wrapper::ResolutionHeight							= DEFAULT_RESOLUTION_HEIGHT;
int								DX8Wrapper::BitDepth										= DEFAULT_BIT_DEPTH;
int								DX8Wrapper::TextureBitDepth							= DEFAULT_TEXTURE_BIT_DEPTH;
bool								DX8Wrapper::IsWindowed									= false;

DirectX::XMMATRIX						DX8Wrapper::old_world;
DirectX::XMMATRIX						DX8Wrapper::old_view;
DirectX::XMMATRIX						DX8Wrapper::old_prj;
#ifdef INFO_VULKAN
D3DFORMAT					DX8Wrapper::DisplayFormat	= D3DFMT_UNKNOWN;

// shader system additions KJM v
DWORD								DX8Wrapper::Vertex_Shader_FVF = 0;
IDirect3DVertexShader9* DX8Wrapper::Vertex_Shader_Ptr = 0;
IDirect3DPixelShader9* DX8Wrapper::Pixel_Shader								= 0;
#endif
WWVK_Pipeline_Entry DX8Wrapper::pipeline_ = PIPELINE_WWVK_MAX;
DX8Wrapper::WWVK_Pipeline_State DX8Wrapper::pipelineStates_[PIPELINE_WWVK_MAX] = {};

Vector4							DX8Wrapper::Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
Vector4							DX8Wrapper::Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

LightEnvironmentClass*		DX8Wrapper::Light_Environment							= NULL;
RenderInfoClass*				DX8Wrapper::Render_Info									= NULL;
DX8Material DX8Wrapper::Material = {};

DWORD								DX8Wrapper::Vertex_Processing_Behavior				= 0;
ZTextureClass*					DX8Wrapper::Shadow_Map[MAX_SHADOW_MAPS];

Vector3							DX8Wrapper::Ambient_Color;
// shader system additions KJM ^

bool								DX8Wrapper::world_identity;
unsigned							DX8Wrapper::RenderStates[VKRS_MAX * 2];
unsigned							DX8Wrapper::TextureStageStates[MAX_TEXTURE_STAGES][VKTSS_MAX];
unsigned							DX8Wrapper::SamplerStates[MAX_TEXTURE_STAGES][VKSAMP_MAX];
VK::Texture							DX8Wrapper::Textures[MAX_TEXTURE_STAGES];
RenderStateStruct					DX8Wrapper::render_state;
unsigned							DX8Wrapper::render_state_changed;

bool								DX8Wrapper::FogEnable									= false;
uint32_t							DX8Wrapper::FogColor										= 0;
#ifdef INFO_VULKAN

IDirect3D9 *					DX8Wrapper::D3DInterface								= NULL;
IDirect3DDevice9 *			DX8Wrapper::D3DDevice									= NULL;
IDirect3DSurface9 *			DX8Wrapper::CurrentRenderTarget						= NULL;
IDirect3DSurface9 *			DX8Wrapper::CurrentDepthBuffer						= NULL;
IDirect3DSurface9 *			DX8Wrapper::DefaultRenderTarget						= NULL;
IDirect3DSurface9 *			DX8Wrapper::DefaultDepthBuffer						= NULL;
#endif
bool								DX8Wrapper::IsRenderToTexture							= false;

unsigned							DX8Wrapper::matrix_changes								= 0;
unsigned							DX8Wrapper::material_changes							= 0;
unsigned							DX8Wrapper::vertex_buffer_changes					= 0;
unsigned							DX8Wrapper::index_buffer_changes                = 0;
unsigned							DX8Wrapper::light_changes								= 0;
unsigned							DX8Wrapper::texture_changes							= 0;
unsigned							DX8Wrapper::render_state_changes						= 0;
unsigned							DX8Wrapper::texture_stage_state_changes			= 0;
unsigned							DX8Wrapper::draw_calls									= 0;
unsigned							DX8Wrapper::_MainThreadID								= 0;
bool								DX8Wrapper::CurrentDX8LightEnables[4];
bool								DX8Wrapper::IsDeviceLost;
int								DX8Wrapper::ZBias;
float								DX8Wrapper::ZNear;
float								DX8Wrapper::ZFar;
VkRenderTarget						DX8Wrapper::target;
WWVK_Pipeline_Collection						DX8Wrapper::pipelineCol_ = {};
Matrix4x4						DX8Wrapper::ProjectionMatrix;
Matrix4x4						DX8Wrapper::DX8Transforms[((int)VkTS::WORLD) + 1];//VkTS::WORLD+1
VK::Buffer DX8Wrapper::DX8TransformsUbos[((int)VkTS::WORLD) + 1];
VK::Buffer DX8Wrapper::ProjUbo, DX8Wrapper::ProjIdentityUbo, DX8Wrapper::IdentityUbo;
VK::Buffer DX8Wrapper::LightUbo, DX8Wrapper::MaterialUbo;
VK::Texture DX8Wrapper::WhiteTexture;

// Hack test... this disables rendering of batches of too few polygons.
unsigned							DX8Wrapper::DrawPolygonLowBoundLimit=0;

#ifdef INFO_VULKAN
DX8Caps*							DX8Wrapper::CurrentCaps = 0;
D3DADAPTER_IDENTIFIER9		DX8Wrapper::CurrentAdapterIdentifier;
#endif

unsigned long DX8Wrapper::FrameCount = 0;

bool								_DX8SingleThreaded										= false;

unsigned							number_of_DX8_calls										= 0;
static unsigned				last_frame_matrix_changes								= 0;
static unsigned				last_frame_material_changes							= 0;
static unsigned				last_frame_vertex_buffer_changes						= 0;
static unsigned				last_frame_index_buffer_changes						= 0;
static unsigned				last_frame_light_changes								= 0;
static unsigned				last_frame_texture_changes								= 0;
static unsigned				last_frame_render_state_changes						= 0;
static unsigned				last_frame_texture_stage_state_changes				= 0;
static unsigned				last_frame_number_of_DX8_calls						= 0;
static unsigned				last_frame_draw_calls									= 0;

static DynamicVectorClass<StringClass>					_RenderDeviceNameTable;
static DynamicVectorClass<StringClass>					_RenderDeviceShortNameTable;
static DynamicVectorClass<RenderDeviceDescClass>	_RenderDeviceDescriptionTable;


DX8_CleanupHook	 *DX8Wrapper::m_pCleanupHook=NULL;
#ifdef EXTENDED_STATS
DX8_Stats	 DX8Wrapper::stats;
#endif
/***********************************************************************************
**
** DX8Wrapper Implementation
**
***********************************************************************************/

void Log_DX8_ErrorCode(unsigned res)
{
	const char* tmp=DXGetErrorStringA(res);

	if (tmp) {
		WWDEBUG_SAY((tmp));
	}
	else
	{
		return;
	}

	WWASSERT(0);
}

void Non_Fatal_Log_DX8_ErrorCode(unsigned res,const char * file,int line)
{
	const char* tmp = DXGetErrorStringA(res);

	if (tmp) {
		WWDEBUG_SAY(("DX8 Error: %s, File: %s, Line: %d\n",tmp,file,line));
	}
}
void PopulateShaderCompare(DX8Wrapper::WWVK_Pipeline_State* states)
{
	DX8Wrapper::WWVK_Pipeline_State def = {};
	def.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
	def.isDynamic = (1 << VKRS_COLORWRITEENABLE) | (1 << VKRS_SRCBLEND) | (1 << VKRS_DESTBLEND) | (1 << VKRS_ZWRITEENABLE)
		| (1 << VKRS_ZFUNC) | (1 << VKRS_ALPHABLENDENABLE) | (1 << VKRS_CULLMODE) | (1 << VKRS_DEPTHBIAS)
		| (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_SPECULARMATERIALSOURCE) | (1 << VKRS_AMBIENTMATERIALSOURCE) | (1 << VKRS_EMISSIVEMATERIALSOURCE)
		| (1 << VKRS_STENCILZFAIL) | (1 << VKRS_STENCILPASS) | (1 << VKRS_STENCILFUNC) | (1 << VKRS_STENCILREF)
		| (1 << VKRS_STENCILWRITEMASK) | (1 << VKRS_STENCILMASK) | (1 << VKRS_STENCILFAIL) | (1 << VKRS_STENCILENABLE);
	def.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	def.RenderStates[VKRS_FILLMODE] = VK_POLYGON_MODE_FILL;
	def.RenderStates[VKRS_ZWRITEENABLE] = VK_TRUE;
	def.RenderStates[VKRS_ALPHATESTENABLE] = 0;
	def.RenderStates[VKRS_SRCBLEND] = VK_BLEND_FACTOR_SRC_ALPHA;
	def.RenderStates[VKRS_DESTBLEND] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	def.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_CLOCKWISE;
	def.RenderStates[VKRS_ZFUNC] = VK_COMPARE_OP_LESS_OR_EQUAL;
	def.RenderStates[VKRS_ALPHAREF] = 0;
	def.RenderStates[VKRS_ALPHAFUNC] = 0;
	def.RenderStates[VKRS_ALPHABLENDENABLE] = 1;
	def.RenderStates[VKRS_FOGENABLE] = 0;
	def.RenderStates[VKRS_SPECULARENABLE] = 0;
	def.RenderStates[VKRS_STENCILENABLE] = 0;
	def.RenderStates[VKRS_STENCILFAIL] = 0;
	def.RenderStates[VKRS_STENCILZFAIL] = 0;
	def.RenderStates[VKRS_STENCILPASS] = 0;
	def.RenderStates[VKRS_STENCILFUNC] = 0;
	def.RenderStates[VKRS_STENCILREF] = 0;
	def.RenderStates[VKRS_STENCILMASK] = 0;
	def.RenderStates[VKRS_STENCILWRITEMASK] = 0;
	def.RenderStates[VKRS_LIGHTING] = 0;
	def.RenderStates[VKRS_DIFFUSEMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_SPECULARMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_AMBIENTMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_EMISSIVEMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_COLORWRITEENABLE] = 0x7;
	def.RenderStates[VKRS_BLENDOP] = VK_BLEND_OP_ADD;
	def.RenderStates[VKRS_DEPTHBIAS] = 0;
	for (int i = 0; i < 4; ++i)
	{
		def.TextureStageStates[i][VKTSS_COLOROP] = VKTOP_DISABLE;
		def.TextureStageStates[i][VKTSS_COLORARG1] = VKTA_TEXTURE;
		def.TextureStageStates[i][VKTSS_COLORARG2] = i == 0 ? VKTA_DIFFUSE : VKTA_CURRENT;
		def.TextureStageStates[i][VKTSS_ALPHAOP] = VKTOP_DISABLE;
		def.TextureStageStates[i][VKTSS_ALPHAARG1] = VKTA_TEXTURE;
		def.TextureStageStates[i][VKTSS_ALPHAARG2] = i == 0 ? VKTA_DIFFUSE : VKTA_CURRENT;
		def.TextureStageStates[i][VKTSS_TEXCOORDINDEX] = i;
		def.TextureStageStates[i][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_DISABLE;
	}
	for (int P = 0; P < PIPELINE_WWVK_MAX; ++P)
	{
		auto& p = states[P];
		p = def;
		switch (P)
		{
			case PIPELINE_WWVK_FVF_D:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE;
				break;
			case PIPELINE_WWVK_FVF_NUV_ARef_Strip:
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; //Fall Through
			case PIPELINE_WWVK_FVF_NUV_ARef:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.isDynamic |= (1 << VKRS_ALPHAREF);
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_NUV_2Tex:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHATESTENABLE] = false;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.isDynamic |= (1 << VKRS_ALPHAREF);
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NUV_2Tex_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHATESTENABLE] = false;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.isDynamic |= (1 << VKRS_ALPHAREF);
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NUV_2Tex_ARef:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.isDynamic |= (1 << VKRS_ALPHAREF);
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NUV_DROPUV_REFLUVT:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NUV_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				break;
			case PIPELINE_WWVK_FVF_NUV_CAMUVT_DROPUV_NOL_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NUV_NOL_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				break;
			case PIPELINE_WWVK_FVF_NUV_NoTex:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_NUV_UVT:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NUV_UVT_UV:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NUV_REFLUVT_UV:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NUV_UVT12:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NUV_Strip:
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; //Fall Through
			case PIPELINE_WWVK_FVF_NUV:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL | VKFVF_TEX1;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_N_Strip:
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; //Fall Through
			case PIPELINE_WWVK_FVF_N:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.RenderStates[VKRS_LIGHTING] = 1;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = 0;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_N_NOL:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = 0;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_N_NOL_CAMUVT:
				p.FVF = VKFVF_XYZ | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = 0;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_DUV:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_DUV_DropUV:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				p.RenderStates[VKRS_ZFUNC] = VK_COMPARE_OP_ALWAYS;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_DUV_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				p.RenderStates[VKRS_ZFUNC] = VK_COMPARE_OP_ALWAYS;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.RenderStates[VKRS_SRCBLEND] = VK_BLEND_FACTOR_ZERO;
				p.RenderStates[VKRS_DESTBLEND] = VK_BLEND_FACTOR_SRC_COLOR;
				break;
			case PIPELINE_WWVK_FVF_DUV_Strip:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_DUV_CAMUVT_Strip:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_DUV_DepthBias_Strip:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_DUV_CAMUVT_DepthBias_Strip:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.isDynamic = 1 << VKRS_DEPTHBIAS;
				break;
			case PIPELINE_WWVK_FVF_DUV2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_DUV2_Strip:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_DUV2_DropUV_Strip:
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			case PIPELINE_WWVK_FVF_DUV2_DropUV:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_DUV2_DropUV_ARef:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = false;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_ALPHAREF) | (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_EMISSIVEMATERIALSOURCE)
					| (1 << VKRS_AMBIENTMATERIALSOURCE);
				break;
			case PIPELINE_WWVK_FVF_DUV2_TerrainPass:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
				p.RenderStates[VKRS_ZWRITEENABLE] = VK_FALSE;
				p.RenderStates[VKRS_ZFUNC] = VK_COMPARE_OP_EQUAL;
				p.RenderStates[VKRS_SRCBLEND] = VK_BLEND_FACTOR_ZERO;
				p.RenderStates[VKRS_DESTBLEND] = VK_BLEND_FACTOR_SRC_COLOR;
				p.RenderStates[VKRS_DIFFUSEMATERIALSOURCE] = VertexMaterialClass::ColorSourceType::COLOR1;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_COLORARG1] = VKTA_TEXTURE;
				p.TextureStageStates[0][VKTSS_COLORARG2] = VKTA_CURRENT;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAARG1] = VKTA_TEXTURE;
				p.TextureStageStates[0][VKTSS_ALPHAARG2] = VKTA_CURRENT;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV_REFLUVT_DROPUV:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV_AREF_Strip:
				p.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			case PIPELINE_WWVK_FVF_NDUV_AREF:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_ALPHAREF) | ( 1 << VKRS_DIFFUSEMATERIALSOURCE) | ( 1 << VKRS_EMISSIVEMATERIALSOURCE) 
					| (1 << VKRS_AMBIENTMATERIALSOURCE);
				break;
			case PIPELINE_WWVK_FVF_NDUV_CAMUVT_NOL:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV_CAMUVT_NOL_AREF:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHATESTENABLE] = 1; 
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_ALPHAREF);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV_DropUV_CAMUVT_NoDiffuse_NOL:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				break;
			case PIPELINE_WWVK_FVF_NDUV_DropUV_CAMUVT_NoDiffuse:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV_UVT2_UVT_PLUS_UVTRGB:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_ADD;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV_UV_PLUS_UVRGB:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_ADD;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NDUV_NOL:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_NDUV_UVT_NOL:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DROPUV_UVT2_UVT_PLUS_UVTRGB:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_ADD;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DROPUV_UV_PLUS_UVRGB:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_ADD;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_UVT2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_UVT1_UV1:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_UVT1_DropTex:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_UVT2_NoAlpha2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NoAlpha2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DropUV:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DropUV_RGB1_A2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DropUV_ARef:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = false;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_ALPHAREF) | (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_EMISSIVEMATERIALSOURCE)
					| (1 << VKRS_AMBIENTMATERIALSOURCE);
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DropTex_ARef:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = 0;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = false;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_ALPHAREF) | (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_EMISSIVEMATERIALSOURCE)
					| (1 << VKRS_AMBIENTMATERIALSOURCE);
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DropTex:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DropTex_UV2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = 1;
				p.RenderStates[VKRS_LIGHTING] = true;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_LIGHTING] = false;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL_OnlyTex1:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_OnlyTex2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_LIGHTING] = true;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = 1;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_CAMUVT_NOL_OnlyTex1:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ZWRITEENABLE] = VK_FALSE;
				p.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_CLOCKWISE;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = 0;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG1;
				p.TextureStageStates[0][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[0][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL_DROPTEX:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ZWRITEENABLE] = VK_FALSE;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL_DROPTEX_NoTexAlpha:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ZWRITEENABLE] = VK_FALSE;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL_DROPTEX2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_DROPTEX2:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = VK_FALSE;
				p.RenderStates[VKRS_LIGHTING] = VK_TRUE;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_SELECTARG2;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_SELECTARG2;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL_AREF:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = false;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_MAX_ENUM;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_ALPHAREF);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FVF_NDUV2_NOL_AREF_DROPTEX:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2 | VKFVF_NORMAL;
				p.RenderStates[VKRS_ALPHABLENDENABLE] = false;
				p.RenderStates[VKRS_ALPHATESTENABLE] = true;
				p.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_MAX_ENUM;
				p.RenderStates[VKRS_ALPHAFUNC] = VK_COMPARE_OP_GREATER_OR_EQUAL;
				p.isDynamic |= (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_ALPHAREF);
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			default:
				p.FVF = 0xFFFFFFFF;
				break;
#if 0 //I don't think there's any point in these being searchable
			case PIPELINE_WWVK_FVF_DUV_Grayscale_NoDepth:
				p.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX1;
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_Halo:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_SRCBLEND] = VK_BLEND_FACTOR_ONE;
				p.RenderStates[VKRS_DESTBLEND] = VK_BLEND_FACTOR_ONE;
				p.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_MAX_ENUM;//No Cull
				break;
			case PIPELINE_WWVK_Dazzle:
				p.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_MAX_ENUM; //No Cull
				p.RenderStates[VKRS_SRCBLEND] = VK_BLEND_FACTOR_ONE;
				p.RenderStates[VKRS_DESTBLEND] = VK_BLEND_FACTOR_ONE;
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				p.RenderStates[VKRS_ZFUNC] = VK_COMPARE_OP_ALWAYS;
				break;
			case PIPELINE_WWVK_Snow: 
				p.RenderStates[VKRS_FILLMODE] = VK_POLYGON_MODE_POINT;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_FTerrain:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				break;
			case PIPELINE_WWVK_FTerrainNoise:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[2][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				break;
			case PIPELINE_WWVK_FTerrainNoise2:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[2][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[3][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[3][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[3][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				break;
			case PIPELINE_WWVK_Road:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				break;
			case PIPELINE_WWVK_RoadNoise:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_RoadNoise12:
				p.RenderStates[VKRS_ZWRITEENABLE] = false;
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[1][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				p.TextureStageStates[2][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[2][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_COUNT2;
				break;
			case PIPELINE_WWVK_Monochrome:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_MotionBlur:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_BumpDiff:
				//TODO
				break;
			case PIPELINE_WWVK_BumpSpec:
				//TODO 
				break;
			case PIPELINE_WWVK_SSBumpDiff:
				//TODO
				break;
			case PIPELINE_WWVK_SSBumpSpec:
				//TODO
				break;
			case PIPELINE_WWVK_Terrain:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_TerrainNoise:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				break;
			case PIPELINE_WWVK_TerrainNoise2:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[2][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				p.TextureStageStates[3][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[3][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[3][VKTSS_TEXCOORDINDEX] = VKTSS_TCI_CAMERASPACEPOSITION;
				break;
			case PIPELINE_WWVK_Trees:
				p.TextureStageStates[0][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[0][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_COLOROP] = VKTOP_MODULATE;
				p.TextureStageStates[1][VKTSS_ALPHAOP] = VKTOP_MODULATE;
				break;
			case PIPELINE_WWVK_Wave: 
				break;
			case PIPELINE_WWVK_RiverWater: 
				break;
			case PIPELINE_WWVK_RiverWaterAdd: 
				break;
			case PIPELINE_WWVK_RiverWaterShroud: 
				break;
			case PIPELINE_WWVK_WaterTrapezoid: 
				break;
			case PIPELINE_WWVK_WaterTrapezoid_DstAlpha: 
				break;
			case PIPELINE_WWVK_WaterTrapezoid_DstAlphaInvDest: 
				break;
			case PIPELINE_WWVK_WaterTrapezoidStrip: 
				break;
			case PIPELINE_WWVK_VolumeShadow: 
				break;
			case PIPELINE_WWVK_VolumeStencilShadow: 
				break;
			case PIPELINE_WWVK_ProjShadow: 
				break;
			case PIPELINE_WWVK_StencilPlayerColor: 
				break;
			case PIPELINE_WWVK_StencilPlayerColorClear: 
				break;
#endif
		}
		if (p.RenderStates[VKRS_ALPHATESTENABLE] == 0)
		{
			p.isDynamic |= (1 << VKRS_ALPHAREF) | (1 << VKRS_ALPHAFUNC);
		}
		if (!(p.FVF & VKFVF_NORMAL))
		{
			p.isDynamic |= (1 << VKRS_LIGHTING)| (1 << VKRS_DIFFUSEMATERIALSOURCE) | (1 << VKRS_SPECULARMATERIALSOURCE)
				| (1 << VKRS_AMBIENTMATERIALSOURCE) | (1 << VKRS_EMISSIVEMATERIALSOURCE);
		}
	}
}

#include <string>
std::string TextureArgToString(uint32_t fvf, uint32_t arg, uint32_t texI, uint32_t uvI)
{
	if (arg == VKTA_CURRENT)
		return "finalColor";
	if (arg == VKTA_DIFFUSE)
		return (fvf & VKFVF_DIFFUSE) ? "fragDiff" : "vec4(1,1,1,1)";
	if (arg == VKTA_TEXTURE)
	{
		std::string uvStr;
		if (uvI == VKTSS_TCI_CAMERASPACENORMAL)
			uvStr = "viewNorm";
		else if (uvI == VKTSS_TCI_CAMERASPACEPOSITION)
			uvStr = "viewDir";
		else if (uvI == VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR)
			uvStr = "viewRefl";
		else
			uvStr = "uv" + std::to_string(uvI + 1);
		return "texture(tex" + std::to_string(texI+1) + ", " + uvStr + ")";
	}
	assert(false);
	return "";
}
std::string TextureStageToString(uint32_t fvf, int i, uint32_t TextureStageStates[8][VKTSS_MAX])
{
	if (TextureStageStates[i][0] == TextureStageStates[i][3] &&
		TextureStageStates[i][1] == TextureStageStates[i][4] &&
		TextureStageStates[i][2] == TextureStageStates[i][5])
	{
		uint32_t texI = TextureStageStates[i][VKTSS_TEXCOORDINDEX];
		if (TextureStageStates[i][0] == VKTOP_SELECTARG1)
		{
			return "finalColor = " + TextureArgToString(fvf, TextureStageStates[i][1], i, texI) + ";";
		}
		if (TextureStageStates[i][0] == VKTOP_SELECTARG2)
		{
			return "finalColor = " + TextureArgToString(fvf, TextureStageStates[i][2], i, texI) + ";";
		}
		if (TextureStageStates[i][0] == VKTOP_MODULATE)
		{
			return "finalColor = " + TextureArgToString(fvf, TextureStageStates[i][1], i, texI) + " * "
				+ TextureArgToString(fvf, TextureStageStates[i][2], i, texI)
				+ ";";
		}
		if (TextureStageStates[i][0] == VKTOP_ADD)
		{
			return "finalColor = " + TextureArgToString(fvf, TextureStageStates[i][1], i, texI) + " + "
				+ TextureArgToString(fvf, TextureStageStates[i][2], i, texI)
				+ ";";
		}
		assert(false);
		return "";
	}
	else
	{
		std::string out = "finalColor = vec4(";
		uint32_t texI = TextureStageStates[i][VKTSS_TEXCOORDINDEX];
		if (TextureStageStates[i][VKTSS_COLOROP] == VKTOP_SELECTARG1)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_COLORARG1], i, texI) + ".rgb, ";
		}
		else if (TextureStageStates[i][VKTSS_COLOROP] == VKTOP_SELECTARG2)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_COLORARG2], i, texI) + ".rgb, ";
		}
		else if (TextureStageStates[i][VKTSS_COLOROP] == VKTOP_MODULATE)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_COLORARG1], i, texI) + ".rgb * "
				+ TextureArgToString(fvf, TextureStageStates[i][VKTSS_COLORARG2], i, texI)
				+ ".rgb, ";
		}
		else if (TextureStageStates[i][VKTSS_COLOROP] == VKTOP_ADD)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_COLORARG1], i, texI) + ".rgb + "
				+ TextureArgToString(fvf, TextureStageStates[i][VKTSS_COLORARG2], i, texI)
				+ ".rgb, ";
		}
		else
		{
			assert(false);
		}
		if (TextureStageStates[i][VKTSS_ALPHAOP] == VKTOP_SELECTARG1)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_ALPHAARG1], i, texI) + ".a);";
		}
		else if (TextureStageStates[i][VKTSS_ALPHAOP] == VKTOP_SELECTARG2)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_ALPHAARG2], i, texI) + ".a);";
		}
		else if (TextureStageStates[i][VKTSS_ALPHAOP] == VKTOP_MODULATE)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_ALPHAARG1], i, texI) + ".a * "
				+ TextureArgToString(fvf, TextureStageStates[i][VKTSS_ALPHAARG2], i, texI)
				+ ".a);";
		}
		else if (TextureStageStates[i][VKTSS_ALPHAOP] == VKTOP_ADD)
		{
			out += TextureArgToString(fvf, TextureStageStates[i][VKTSS_ALPHAARG1], i, texI) + ".a + "
				+ TextureArgToString(fvf, TextureStageStates[i][VKTSS_ALPHAARG2], i, texI)
				+ ".a);";
		}
		else
		{
			assert(false);
		}
		return out;
	}
}
void EstimateFragShader(uint32_t fvf, uint32_t RenderStates[VKRS_MAX], uint32_t TextureStageStates[8][VKTSS_MAX])
{
	bool lighting = RenderStates[VKRS_LIGHTING];
	uint32_t textureCount = 0;
	uint32_t uvCount = (fvf & (VKFVF_TEX7)) >> 8, uvUsed = 0;
	bool haveCamPos = false, haveCamNorm = false, haveCamReflect = false;
	for (int i = 0; i < 4; ++i)
	{
		if (TextureStageStates[i][0] == VKTOP_DISABLE)
			break;
		textureCount++;
		if (TextureStageStates[i][VKTSS_TEXCOORDINDEX] & VKTSS_TCI_CAMERASPACENORMAL)
			haveCamNorm = true;
		else if (TextureStageStates[i][VKTSS_TEXCOORDINDEX] & VKTSS_TCI_CAMERASPACEPOSITION)
			haveCamPos = true;
		else if (TextureStageStates[i][VKTSS_TEXCOORDINDEX] & VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR)
			haveCamReflect = true;
		else
			uvUsed++;
	}

	std::string output = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable
)";
	if ((fvf & VKFVF_NORMAL) && lighting)
	{
		output += R"(#include "lightCommon.frag.inc"

layout(set = 0, binding = 2) uniform LightCollectionBlock {LightCollection lights;};
layout(set = 0, binding = 3) uniform MaterialBlock { DX8Material material;};
)";
	}
	for (uint32_t i = 0; i < textureCount; ++i)
	{
		output += "layout(binding = " + std::to_string(4 + i) + ") uniform sampler2D tex" + std::to_string(i + 1) + ";\n";
	}
	int inputI = 0;
	if (fvf & VKFVF_NORMAL)
	{
		output += "layout(location = " + std::to_string(inputI++) + ") in vec3 fragNorm;\n";
	}
	if (fvf & VKFVF_DIFFUSE)
	{
		output += "layout(location = " + std::to_string(inputI++) + ") in vec4 fragDiff;\n";
	}
	//bool haveCamPos = false, haveCamNorm = false, haveCameReflect = false;
	if (haveCamPos)
	{
		output += "layout(location = " + std::to_string(inputI++) + ") in vec3 viewDir;\n";
	}
	if (haveCamNorm)
	{
		output += "layout(location = " + std::to_string(inputI++) + ") in vec3 viewNorm;\n";
	}
	if (haveCamReflect)
	{
		output += "layout(location = " + std::to_string(inputI++) + ") in vec3 viewRefl;\n";
	}
	for (uint32_t i = 0; i < uvCount; ++i)
	{
		output += "layout(location = " + std::to_string(inputI++) + ") in vec2 uv" + std::to_string(i + 1) + ";\n";
	}
	output += R"(layout(location = 0) out vec4 finalColor;

void main() {
)";
	for (uint32_t i = 0; i < 4; ++i)
	{
		if (TextureStageStates[i][0] == VKTOP_DISABLE)
			break;
		//Don't know if this is valid. Curious if it occurs
		if (i > 0 && (TextureStageStates[i][0] == VKTOP_SELECTARG1 || TextureStageStates[i][0] == VKTOP_SELECTARG2))
			assert(false);
		output += "    " + TextureStageToString(fvf, i, TextureStageStates) + "\n";
	}
	if ((fvf & VKFVF_NORMAL) && lighting)
	{
		output += R"(    finalColor = CalculateLights(lights, material, fragNorm, gl_FragCoord.xyz, viewDir,
			finalColor.rgb, finalColor.rgb, finalColor.rgb);
)";
	}
	output += "}\n";

	std::string fragName = "fvf_";
	if (fvf & VKFVF_NORMAL)
		fragName += "n";
	if (fvf & VKFVF_DIFFUSE)
		fragName += "d";
	if (uvCount > 0)
		fragName += "uv" + std::to_string(uvCount);
	if ((fvf & VKFVF_NORMAL) && !lighting)
	{
		fragName += "_NOL";
	}
	fragName = "Frag Name:\n" + fragName + ".frag\n#####################################\n";

	OutputDebugStringA(fragName.c_str());
	OutputDebugStringA(output.c_str());
}

WWVK_Pipeline_Entry DX8Wrapper::FindClosestPipelines(unsigned FVF, VkPrimitiveTopology topo)
{
	static bool compInit = false;
	if (!compInit)
	{
		PopulateShaderCompare(pipelineStates_);
		compInit = 1;
	}
	DX8Wrapper::WWVK_Pipeline_State def = {};
	def.FVF = VKFVF_XYZ | VKFVF_DIFFUSE | VKFVF_TEX2;
	def.isDynamic = 0;
	def.topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	def.RenderStates[VKRS_FILLMODE] = VK_POLYGON_MODE_FILL;
	def.RenderStates[VKRS_ZWRITEENABLE] = VK_TRUE;
	def.RenderStates[VKRS_ALPHATESTENABLE] = 0;
	def.RenderStates[VKRS_SRCBLEND] = VK_BLEND_FACTOR_SRC_ALPHA;
	def.RenderStates[VKRS_DESTBLEND] = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	def.RenderStates[VKRS_CULLMODE] = VK_FRONT_FACE_CLOCKWISE;
	def.RenderStates[VKRS_ZFUNC] = VK_COMPARE_OP_LESS_OR_EQUAL;
	def.RenderStates[VKRS_ALPHAREF] = 0;
	def.RenderStates[VKRS_ALPHAFUNC] = 0;
	def.RenderStates[VKRS_ALPHABLENDENABLE] = TRUE;
	def.RenderStates[VKRS_FOGENABLE] = 0;
	def.RenderStates[VKRS_SPECULARENABLE] = 0;
	def.RenderStates[VKRS_STENCILENABLE] = 0;
	def.RenderStates[VKRS_STENCILFAIL] = 0;
	def.RenderStates[VKRS_STENCILZFAIL] = 0;
	def.RenderStates[VKRS_STENCILPASS] = 0;
	def.RenderStates[VKRS_STENCILFUNC] = 0;
	def.RenderStates[VKRS_STENCILREF] = 0;
	def.RenderStates[VKRS_STENCILMASK] = 0;
	def.RenderStates[VKRS_STENCILWRITEMASK] = 0;
	def.RenderStates[VKRS_LIGHTING] = 0;
	def.RenderStates[VKRS_DIFFUSEMATERIALSOURCE] = VertexMaterialClass::ColorSourceType::COLOR1;
	def.RenderStates[VKRS_SPECULARMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_AMBIENTMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_EMISSIVEMATERIALSOURCE] = 0;
	def.RenderStates[VKRS_COLORWRITEENABLE] = 0xF;
	def.RenderStates[VKRS_BLENDOP] = VK_BLEND_OP_ADD;
	def.RenderStates[VKRS_DEPTHBIAS] = 0;
	for (int i = 0; i < 4; ++i)
	{
		def.TextureStageStates[i][VKTSS_COLOROP] = VKTOP_DISABLE;
		def.TextureStageStates[i][VKTSS_COLORARG1] = VKTA_TEXTURE;
		def.TextureStageStates[i][VKTSS_COLORARG2] = VKTA_DIFFUSE;
		def.TextureStageStates[i][VKTSS_ALPHAOP] = VKTOP_DISABLE;
		def.TextureStageStates[i][VKTSS_ALPHAARG1] = VKTA_TEXTURE;
		def.TextureStageStates[i][VKTSS_ALPHAARG2] = VKTA_DIFFUSE;
		def.TextureStageStates[i][VKTSS_TEXCOORDINDEX] = i;
		def.TextureStageStates[i][VKTSS_TEXTURETRANSFORMFLAGS] = VKTTFF_DISABLE;
	}

	if (pipeline_ != PIPELINE_WWVK_MAX)
	{
		return { pipeline_ };
	}

	WWVK_Pipeline_Entry ret = PIPELINE_WWVK_MAX;
	int pi = 0;
	for (auto p : pipelineStates_)
	{
		bool valid = true;
		if (p.FVF != FVF || p.topo != topo)
		{
			pi++;
			continue;
		}
		for (int rs = 0; rs < VKRS_SHADER_COMPARE_MAX; ++rs)
		{
			if ((p.isDynamic & (1ULL << rs)) != 0)
			{
				//Still valid
			}
			else if (RenderStates[rs] == 0x12345678 && p.RenderStates[rs] == def.RenderStates[rs])
			{
				//If state is invalid, check against default
			}
			else if (p.RenderStates[rs] == RenderStates[rs])
			{
				//Valid
			}
			else
			{
				valid = false;
				break;
			}
		}
		if (!valid)
		{
			pi++;
			continue;
		}
		unsigned int uvCount = 0;
		if (FVF & VKFVF_TEX2)
			uvCount = 2;
		else if (FVF & VKFVF_TEX1)
			uvCount = 1;
		for (unsigned int ti = 0; valid && ti < 4; ++ti)
		{
			if (p.TextureStageStates[ti][VKTSS_COLOROP] == VKTOP_DISABLE && 
				(TextureStageStates[ti][VKTSS_COLOROP] == VKTOP_DISABLE || TextureStageStates[ti][VKTSS_COLOROP] == 0x12345678))
				break;
			//If we have bound texture beyond our UVs, ignore them
			if (ti + 1 > uvCount && TextureStageStates[ti][VKTSS_TEXCOORDINDEX] < 8 && 
				TextureStageStates[ti][VKTSS_TEXCOORDINDEX] >= uvCount)
				break;
			int ts = 0;
			for (int cs = 0; cs < 2; ++cs, ts += 3)
			{
				if (p.TextureStageStates[ti][ts] != TextureStageStates[ti][ts])
				{
					valid = false;
					break;
				}
				switch (p.TextureStageStates[ti][ts])
				{
				case VKTOP_ADD: //Order independent operations
				case VKTOP_MODULATE:
				case VKTOP_MODULATE2X:
				{
					if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1] &&
						p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
					{
						//valid
					}
					else if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 2] &&
						p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 2])
					{
						//valid
					}
					else
					{
						valid = false;
					}
					break;
				}
				case VKTOP_SELECTARG1:
					if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1])
					{
						//valid
					}
					else
					{
						valid = false;
					}
					break;
				case VKTOP_SELECTARG2:
					if (p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
					{
						//valid
					}
					else
					{
						valid = false;
					}
					break;
				default:
					if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1] &&
						p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
					{
						//valid
					}
					else
					{
						valid = false;
					}
					break;
				}
			}
			for (; valid && ts < VKTSS_SHADER_COMPARE_MAX; ++ts)
			{
				if (p.TextureStageStates[ti][ts] == TextureStageStates[ti][ts])
				{

				}
				else if (TextureStageStates[ti][ts] == 0x12345678 && p.TextureStageStates[ti][ts] == def.TextureStageStates[ti][ts])
				{
					//If state is invalid, check against default
				}
				else
				{
					valid = false;
					break;
				}
			}
		}
		if (valid)
		{
			ret = (WWVK_Pipeline_Entry)pi;
		}
		pi++;
	}
	if (ret == PIPELINE_WWVK_MAX)
	{
		pi = 0;
		std::vector<std::pair<WWVK_Pipeline_Entry, int>> matches;
		for (auto p : pipelineStates_)
		{
			if (p.FVF == 0xFFFFFFFF)
			{
				pi++;
				continue;
			}
			std::pair<WWVK_Pipeline_Entry, int> valid = {(WWVK_Pipeline_Entry)pi,0};
			if (p.FVF == FVF)
				valid.second+=100;
			if (p.topo == topo)
				valid.second++;
			for (int rs = 0; rs < VKRS_SHADER_COMPARE_MAX; ++rs)
			{
				if ((p.isDynamic & (1ULL << rs)) != 0)
				{
					//Still valid
					valid.second++;
				}
				else if (RenderStates[rs] == 0x12345678 && p.RenderStates[rs] == def.RenderStates[rs])
				{
					//If state is invalid, check against default
					valid.second++;
				}
				else if (p.RenderStates[rs] == RenderStates[rs])
				{
					valid.second++;
				}
			}
			for (int ti = 0; ti < 4; ++ti)
			{
				if (p.TextureStageStates[ti][VKTSS_COLOROP] == VKTOP_DISABLE &&
					(TextureStageStates[ti][VKTSS_COLOROP] == VKTOP_DISABLE || TextureStageStates[ti][VKTSS_COLOROP] == 0x12345678))
				{
					valid.second += VKTSS_SHADER_COMPARE_MAX * (4 - ti);
					break;
				}
				int ts = 0;
				for (int cs = 0; cs < 2; ++cs, ts += 3)
				{
					if (p.TextureStageStates[ti][ts] != TextureStageStates[ti][ts])
					{
						continue;
					}
					valid.second++;
					switch (p.TextureStageStates[ti][ts])
					{
					case VKTOP_ADD: //Order independent operations
					case VKTOP_MODULATE:
					case VKTOP_MODULATE2X:
					{
						if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1] &&
							p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
						{
							valid.second+=2;
						}
						else if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 2] &&
							p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 2])
						{
							valid.second += 2;
						}
						else if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1])
						{
							valid.second++;
						}
						else if (p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
						{
							valid.second++;
						}

						break;
					}
					case VKTOP_SELECTARG1:
						if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1])
						{
							valid.second += 2;
						}
						break;
					case VKTOP_SELECTARG2:
						if (p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
						{
							valid.second += 2;
						}
						break;
					default:
						if (p.TextureStageStates[ti][ts + 1] == TextureStageStates[ti][ts + 1])
						{
							valid.second += 1;
						}
						if (p.TextureStageStates[ti][ts + 2] == TextureStageStates[ti][ts + 2])
						{
							valid.second += 1;
						}
						break;
					}
				}
				for (; ts < VKTSS_SHADER_COMPARE_MAX; ++ts)
				{
					if (p.TextureStageStates[ti][ts] == TextureStageStates[ti][ts])
					{
						valid.second++;
					}
					else if (TextureStageStates[ti][ts] == 0x12345678 && p.TextureStageStates[ti][ts] == def.TextureStageStates[ti][ts])
					{
						//If state is invalid, check against default
						valid.second++;
					}
				}
			}
			matches.push_back(valid);
			pi++;
		}
		qsort(matches.data(), matches.size(), sizeof(std::pair<int, int>),
			[](const void* A, const void* B) {
				const std::pair<int, int>* a = (std::pair<int, int>*)A, * b = (std::pair<int, int>*)B;
				if (a->second < b->second)
					return 1;
				if (a->second > b->second)
					return -1;
				return 0;
			});
		WWDEBUG_SAY(("Closest Matches\n"));
		for (auto& m : matches)
		{
			if (m.second <= 2)
				break;
			WWDEBUG_SAY(("%d: %d\n", m.first, m.second));
		}

		auto& bps = pipelineStates_[matches[0].first];
		std::pair<uint32_t, uint32_t> rsCompare[VKRS_SHADER_COMPARE_MAX];
		for (int i = 0; i < VKRS_SHADER_COMPARE_MAX; ++i)
		{
			if ((bps.isDynamic & (uint64_t)(1ULL << i)) != 0)
			{
				rsCompare[i].first = rsCompare[i].second = 0x1234;
			}
			else
			{
				rsCompare[i].first = bps.RenderStates[i];
				rsCompare[i].second = RenderStates[i];
				if (rsCompare[i].second == 0x12345678)
					rsCompare[i].second = def.RenderStates[i];
			}
		}
		std::pair<uint32_t, uint32_t> tsCompare[4][VKTSS_MAX];
		for (int t = 0; t < 4; ++t)
		{
			for (int i = 0; i < VKTSS_MAX; ++i)
			{
				tsCompare[t][i].first = bps.TextureStageStates[t][i];
				tsCompare[t][i].second = TextureStageStates[t][i];
				if (tsCompare[t][i].second == 0x12345678)
					tsCompare[t][i].second = def.TextureStageStates[t][i];
			}
		}
		printf("");
		EstimateFragShader(FVF, RenderStates, TextureStageStates);
	}

	return ret;
}

bool DX8Wrapper::Init(void * hwnd, bool lite)
{
	WWASSERT(!IsInitted);

	// zero memory
	memset(Textures,0,sizeof(IDirect3DBaseTexture9*)*MAX_TEXTURE_STAGES);
	memset(RenderStates,0,sizeof(RenderStates));
	memset(TextureStageStates,0,sizeof(TextureStageStates));
	memset(Vertex_Shader_Constants,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
	memset(Pixel_Shader_Constants,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
	memset(&render_state,0,sizeof(RenderStateStruct));
	memset(Shadow_Map,0,sizeof(ZTextureClass*)*MAX_SHADOW_MAPS);

	/*
	** Initialize all variables!
	*/
	_Hwnd = (HWND)hwnd;
	_MainThreadID=ThreadClass::_Get_Current_Thread_ID();
	WWDEBUG_SAY(("DX8Wrapper main thread: 0x%x\n",_MainThreadID));
	CurRenderDevice = -1;
	ResolutionWidth = DEFAULT_RESOLUTION_WIDTH;
	ResolutionHeight = DEFAULT_RESOLUTION_HEIGHT;
	// Initialize Render2DClass Screen Resolution
	Render2DClass::Set_Screen_Resolution( RectClass( 0, 0, ResolutionWidth, ResolutionHeight ) );
	BitDepth = DEFAULT_BIT_DEPTH;
	IsWindowed = false;	
	DX8Wrapper_IsWindowed = false;

	for (int light=0;light<4;++light) CurrentDX8LightEnables[light]=false;

	::ZeroMemory(&old_world, sizeof(DirectX::XMMATRIX));
	::ZeroMemory(&old_view, sizeof(DirectX::XMMATRIX));
	::ZeroMemory(&old_prj, sizeof(DirectX::XMMATRIX));

	//old_vertex_shader; TODO
	//old_sr_shader;
	//current_shader;

	//world_identity;
	//CurrentFogColor;

	target.InitVulkan(hwnd);
	WWVK_PopulatePipeline(&target, pipelineCol_);
	IsInitted = true;
	Enumerate_Devices();

	Apply_Default_State();

#ifdef INFO_VULKAN
	D3DInterface = NULL;
	D3DDevice = NULL;

	WWDEBUG_SAY(("Reset DX8Wrapper statistics\n"));
	Reset_Statistics();

	Invalidate_Cached_Render_States();

	if (!lite) {
		D3D8Lib = LoadLibrary("D3D9.DLL");

		if (D3D8Lib == NULL) return false;	// Return false at this point if init failed

		Direct3DCreate8Ptr = (Direct3DCreate8Type) GetProcAddress(D3D8Lib, "Direct3DCreate9");
		if (Direct3DCreate8Ptr == NULL) return false;

		/*
		** Create the D3D interface object
		*/
		WWDEBUG_SAY(("Create Direct3D8\n"));
		D3DInterface = Direct3DCreate8Ptr(D3D_SDK_VERSION);		// TODO: handle failure cases...
		if (D3DInterface == NULL) {
			return(false);
		}
		IsInitted = true;

		/*
		** Enumerate the available devices
		*/
		WWDEBUG_SAY(("Enumerate devices\n"));
		Enumerate_Devices();
		WWDEBUG_SAY(("DX8Wrapper Init completed\n"));
	}
#endif

	return(true);
}

void DX8Wrapper::Shutdown(void)
{
	target.~VkRenderTarget();
#ifdef INFO_VULKAN
	if (D3DDevice) {

		Set_Render_Target ();
		Release_Device();
	}

	if (D3DInterface) {
		D3DInterface->Release();
		D3DInterface=NULL;

	}
#endif

	//if (CurrentCaps)
	{
		//int max=CurrentCaps->Get_Max_Textures_Per_Pass();
		for (int i = 0; i < MAX_TEXTURE_STAGES; i++)
		{
			if (Textures[i].image) 
			{
				target.PushSingleTexture(Textures[i]);
				Textures[i] = {};
			}
		}
	}

#ifdef INFO_VULKAN
	if (D3DInterface) {
		UINT newRefCount=D3DInterface->Release();
		D3DInterface=NULL;
	}

	if (D3D8Lib) {
		FreeLibrary(D3D8Lib);
		D3D8Lib = NULL;
	}
#endif
	_RenderDeviceNameTable.Clear();		 // note - Delete_All() resizes the vector, causing a reallocation.  Clear is better. jba.
	_RenderDeviceShortNameTable.Clear();
	_RenderDeviceDescriptionTable.Clear();	

	DX8Caps::Shutdown();
	IsInitted = false;		// 010803 srj
}

void DX8Wrapper::Do_Onetime_Device_Dependent_Inits(void)
{
	/*
	** Set Global render states (some of which depend on caps)
	*/
#ifdef INFO_VULKAN
	Compute_Caps(D3DFormat_To_WW3DFormat(DisplayFormat));
#endif

   /*
	** Initalize any other subsystems inside of WW3D
	*/
	MissingTexture::_Init();
	TextureFilterClass::_Init_Filters((TextureFilterClass::TextureFilterMode)WW3D::Get_Texture_Filter());
	TheDX8MeshRenderer.Init();
	SHD_INIT;
	BoxRenderObjClass::Init();
	VertexMaterialClass::Init();
	PointGroupClass::_Init(); // This needs the VertexMaterialClass to be initted
	ShatterSystem::Init();
	TextureLoader::Init();

	Set_Default_Global_Render_States();
}
#if 0
//WWINLINE 
void DX8Wrapper::Set_DX8_Render_State(VKRENDERSTATETYPE state, unsigned value)
{
#ifdef INFO_VULKAN
	// Can't monitor state changes because setShader call to GERD may change the states!
	if (RenderStates[state] == value) return;

#ifdef MESH_RENDER_SNAPSHOT_ENABLED
	if (WW3D::Is_Snapshot_Activated()) {
		StringClass value_name(0, true);
		Get_DX8_Render_State_Value_Name(value_name, state, value);
		SNAPSHOT_SAY(("DX8 - SetRenderState(state: %s, value: %s)\n",
			Get_DX8_Render_State_Name(state),
			value_name));
	}
#endif

	RenderStates[state] = value;
	DX8CALL(SetRenderState(state, value));
	DX8_RECORD_RENDER_STATE_CHANGE();
#endif
	RenderStates[state] = value;
#if 1
	if (!target.currentCmd)
		return;
	if (state == VKRS_COLORWRITEENABLE)
	{
		VkColorComponentFlags flags[4] = {};
		unsigned int flagCount = 0;
		if (0x1 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_R_BIT; }
		if (0x2 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_G_BIT; }
		if (0x4 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_B_BIT; }
		if (0x8 & RenderStates[state]) { flags[flagCount] |= VK_COLOR_COMPONENT_A_BIT; }
		flagCount = 1;
		if (state == 0x12345678)
		{
			flags[0] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			flagCount = 1;
		}
		vkCmdSetColorWriteMaskEXT(target.currentCmd, 0, flagCount, flags);
	}
	else if (state == VKRS_DEPTHBIAS)
	{
		vkCmdSetDepthBias(target.currentCmd, RenderStates[VKRS_DEPTHBIAS] * -0.000005f, -1, 0);
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
#endif
}
#endif

inline DWORD F2DW(float f) { return *((unsigned*)&f); }
void DX8Wrapper::Set_Default_Global_Render_States(void)
{
	DX8_THREAD_ASSERT();
#ifdef INFO_VULKAN
	const D3DCAPS9 &caps = Get_Current_Caps()->Get_DX8_Caps();
	
	Set_DX8_Render_State(VKRS_RANGEFOGENABLE, (caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) ? TRUE : FALSE);
	Set_DX8_Render_State(VKRS_FOGTABLEMODE, D3DFOG_NONE);
	Set_DX8_Render_State(VKRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	Set_DX8_Render_State(VKRS_COLORVERTEX, TRUE);
	Set_DX8_Texture_Stage_State(1, VKTSS_BUMPENVLSCALE, F2DW(1.0f));
	Set_DX8_Texture_Stage_State(1, VKTSS_BUMPENVLOFFSET, F2DW(0.0f));
#endif
	Set_DX8_Render_State(VKRS_SPECULARMATERIALSOURCE, VertexMaterialClass::ColorSourceType::MATERIAL);
	Set_DX8_Render_State(VKRS_DEPTHBIAS,0);
	Set_DX8_Texture_Stage_State(0, VKTSS_BUMPENVMAT00,F2DW(1.0f));
	Set_DX8_Texture_Stage_State(0, VKTSS_BUMPENVMAT01,F2DW(0.0f));
	Set_DX8_Texture_Stage_State(0, VKTSS_BUMPENVMAT10,F2DW(0.0f));
	Set_DX8_Texture_Stage_State(0, VKTSS_BUMPENVMAT11,F2DW(1.0f));

//	Set_DX8_Render_State(VKRS_CULLMODE, VK_FRONT_FACE_CLOCKWISE);
	// Set dither mode here?
}

//MW: I added this for 'Generals'.
bool DX8Wrapper::Validate_Device(void)
{	DWORD numPasses=0;
	HRESULT hRes = 1;

#ifdef INFO_VULKAN
	hRes=_Get_D3D_Device8()->ValidateDevice(&numPasses);
#endif

	return (hRes == 1);
}

void DX8Wrapper::Invalidate_Cached_Render_States(void)
{
	render_state_changed=0;

	int a;
	for (a=0;a<sizeof(RenderStates)/(sizeof(unsigned)*2);++a) {
		if (RenderStates[a] != 0x12345678)
		{
			RenderStates[VKRS_MAX + a] = RenderStates[a];
		}
		RenderStates[a]=0x12345678;
	}
	for (a=0;a<MAX_TEXTURE_STAGES;++a) 
	{
		for (int b=0; b< VKTSS_MAX;b++)
		{
			TextureStageStates[a][b]=0x12345678;
		}
		//Need to explicitly set texture to NULL, otherwise app will not be able to
		//set it to null because of redundant state checker. MW
#ifdef INFO_VULKAN
		if (_Get_D3D_Device8())
			_Get_D3D_Device8()->SetTexture(a,NULL);
		if (Textures[a] != NULL) {
			Textures[a]->Release();
		}
#endif
		Textures[a] = {};
	}

	ShaderClass::Invalidate();

	//Need to explicitly set render_state texture pointers to NULL. MW
	Release_Render_State();

	// (gth) clear the matrix shadows too
	for (int i=0; i<((int)VkTS::WORLD)+1; i++) {
		DX8Transforms[i][0].Set(0,0,0,0);
		DX8Transforms[i][1].Set(0,0,0,0);
		DX8Transforms[i][2].Set(0,0,0,0);
		DX8Transforms[i][3].Set(0,0,0,0);
	}

}

void DX8Wrapper::Do_Onetime_Device_Dependent_Shutdowns(void)
{
	/*
	** Shutdown ww3d systems
	*/
	int i;
	for (i=0;i<MAX_VERTEX_STREAMS;++i) {
		if (render_state.vertex_buffers[i]) render_state.vertex_buffers[i]->Release_Engine_Ref();
		REF_PTR_RELEASE(render_state.vertex_buffers[i]);
	}
	if (render_state.index_buffer) render_state.index_buffer->Release_Engine_Ref();
	REF_PTR_RELEASE(render_state.index_buffer);
	REF_PTR_RELEASE(render_state.material);
	for (i=0;i< MAX_TEXTURE_STAGES;++i) REF_PTR_RELEASE(render_state.Textures[i]);


	TextureLoader::Deinit();
	SortingRendererClass::Deinit();
	DynamicVBAccessClass::_Deinit();
	DynamicIBAccessClass::_Deinit();
	ShatterSystem::Shutdown();
	PointGroupClass::_Shutdown();
	VertexMaterialClass::Shutdown();
	BoxRenderObjClass::Shutdown();
	SHD_SHUTDOWN;
	TheDX8MeshRenderer.Shutdown();
	MissingTexture::_Deinit();

#ifdef INFO_VULKAN
	if (CurrentCaps) {
		delete CurrentCaps;
		CurrentCaps=NULL;
	}
#endif

}


bool DX8Wrapper::Create_Device(void)
{
#ifdef INFO_VULKAN
	WWASSERT(D3DDevice==NULL);	// for now, once you've created a device, you're stuck with it!

	D3DCAPS9 caps;
	if 
	(
		FAILED
		(
			D3DInterface->GetDeviceCaps
			(
				CurRenderDevice,
				WW3D_DEVTYPE,
				&caps
			)
		)
	)
	{
		return false;
	}

	::ZeroMemory(&CurrentAdapterIdentifier, sizeof(D3DADAPTER_IDENTIFIER9));
	
	if
	(
		FAILED
		( 
			D3DInterface->GetAdapterIdentifier
			(
				CurRenderDevice,
				D3DENUM_WHQL_LEVEL,
				&CurrentAdapterIdentifier
			)
			)	
	) 
	{
		return false;
	}

#ifndef _XBOX
	
	Vertex_Processing_Behavior=(caps.DevCaps&D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
		D3DCREATE_MIXED_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// enable this when all 'get' dx calls are removed KJM
	/*if (caps.DevCaps&D3DDEVCAPS_PUREDEVICE)
	{
		Vertex_Processing_Behavior|=D3DCREATE_PUREDEVICE;
	}*/

#else // XBOX
	Vertex_Processing_Behavior=D3DCREATE_PUREDEVICE;
#endif // XBOX

#ifdef CREATE_DX8_MULTI_THREADED
	Vertex_Processing_Behavior|=D3DCREATE_MULTITHREADED;
	_DX8SingleThreaded=false;
#else
	_DX8SingleThreaded=true;
#endif

	if (DX8Wrapper_PreserveFPU)
		Vertex_Processing_Behavior |= D3DCREATE_FPU_PRESERVE;

#ifdef CREATE_DX8_FPU_PRESERVE
	Vertex_Processing_Behavior|=D3DCREATE_FPU_PRESERVE;
#endif

	HRESULT hr=D3DInterface->CreateDevice
	(
		CurRenderDevice,
		WW3D_DEVTYPE,
		_Hwnd,
		Vertex_Processing_Behavior,
		&_PresentParameters,
		&D3DDevice 
	);

	if (FAILED(hr)) 
	{
		// The device selection may fail because the device lied that it supports 32 bit zbuffer with 16 bit
		// display. This happens at least on Voodoo2.

		if ((_PresentParameters.BackBufferFormat==D3DFMT_R5G6B5 ||
			_PresentParameters.BackBufferFormat==D3DFMT_X1R5G5B5 ||
			_PresentParameters.BackBufferFormat==D3DFMT_A1R5G5B5) &&
			(_PresentParameters.AutoDepthStencilFormat==D3DFMT_D32 ||
			_PresentParameters.AutoDepthStencilFormat==D3DFMT_D24S8 ||
			_PresentParameters.AutoDepthStencilFormat==D3DFMT_D24X8)) 
		{
			_PresentParameters.AutoDepthStencilFormat=D3DFMT_D16;
			hr = D3DInterface->CreateDevice
			(
				CurRenderDevice,
				WW3D_DEVTYPE,
				_Hwnd,
				Vertex_Processing_Behavior,
				&_PresentParameters,
				&D3DDevice 
			);

			if (FAILED(hr)) 
			{
				return false;
			}
        }
		else 
		{
				return false;
		}
	}

	/*
	** Initialize all subsystems
	*/
#endif
	Do_Onetime_Device_Dependent_Inits();
	return true;
}

bool DX8Wrapper::Reset_Device(bool reload_assets)
{
	WWDEBUG_SAY(("Resetting device.\n"));
	DX8_THREAD_ASSERT();
	if ((IsInitted) //&& (D3DDevice != NULL)
		) {
		// Release all non-MANAGED stuff
		WW3D::_Invalidate_Textures();

		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i) 
		{
			Set_Vertex_Buffer (NULL,i);
		}
		Set_Index_Buffer (NULL, 0);
		if (m_pCleanupHook) {
			m_pCleanupHook->ReleaseResources();
		}
		DynamicVBAccessClass::_Deinit();
		DynamicIBAccessClass::_Deinit();
		DX8TextureManagerClass::Release_Textures();
		SHD_SHUTDOWN_SHADERS;

		// Reset frame count to reflect the flipping chain being reset by Reset()
		FrameCount = 0;

		memset(Vertex_Shader_Constants,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
		memset(Pixel_Shader_Constants,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);

#ifdef INFO_VULKAN
		HRESULT hr=_Get_D3D_Device8()->TestCooperativeLevel();
		if (hr != D3DERR_DEVICELOST )
		{	DX8CALL_HRES(Reset(&_PresentParameters),hr)
			if (hr != D3D_OK)
				return false;	//reset failed.
		}
		else
			return false;	//device is lost and can't be reset.

#endif
		if (reload_assets)
		{
			DX8TextureManagerClass::Recreate_Textures();
			if (m_pCleanupHook) {
				m_pCleanupHook->ReAcquireResources();
			}
		}
		Invalidate_Cached_Render_States();
		Set_Default_Global_Render_States();
		SHD_INIT_SHADERS;
		WWDEBUG_SAY(("Device reset completed\n"));
		return true;
	}
	WWDEBUG_SAY(("Device reset failed\n"));
	return false;
}

void DX8Wrapper::Release_Device(void)
{
#ifdef INFO_VULKAN
	if (D3DDevice) {

		for (int a=0;a<MAX_TEXTURE_STAGES;++a)
		{	//release references to any textures that were used in last rendering call
			DX8CALL(SetTexture(a,NULL));
		}

		DX8CALL(SetStreamSource(0, NULL, 0, 0));	//release reference count on last rendered vertex buffer
		DX8CALL(SetIndices(NULL));	//release reference count on last rendered index buffer


		/*
		** Release the current vertex and index buffers
		*/
		for (unsigned i=0;i<MAX_VERTEX_STREAMS;++i) 
		{
			if (render_state.vertex_buffers[i]) render_state.vertex_buffers[i]->Release_Engine_Ref();
			REF_PTR_RELEASE(render_state.vertex_buffers[i]);
		}
		if (render_state.index_buffer) render_state.index_buffer->Release_Engine_Ref();
		REF_PTR_RELEASE(render_state.index_buffer);

		/*
		** Shutdown all subsystems
		*/
#endif
		Do_Onetime_Device_Dependent_Shutdowns();
#ifdef INFO_VULKAN

		/*
		** Release the device
		*/

		D3DDevice->Release();
		D3DDevice=NULL;
	}
#endif
}
#include <Winuser.h>
void DX8Wrapper::Enumerate_Devices()
{
	DX8_Assert();
	int w, h;
	DEVMODE devMode;
	devMode.dmSize = sizeof(devMode);

	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
	}
	w = devMode.dmPelsWidth;
	h = devMode.dmPelsHeight;

	_RenderDeviceNameTable.Add("Vulkan");
	_RenderDeviceShortNameTable.Add("Vulkan");	// for now, just add the same name to the "pretty name table"
	RenderDeviceDescClass desc;
	int lastRes[8] = { 1024, 576, 960, 540, 768, 576, 800, 600 };
	int incrRes[8] = { 16 * 16, 16 * 9, 10 * 16,10 * 9, 16 * 16, 16 * 12, 10 * 16, 10 * 9 };

	for (;;)
	{
		int bestIndex = 0;
		for (int i = 1; i < 4; ++i)
		{
			if (lastRes[bestIndex * 2 + 0] > lastRes[i * 2 + 0] ||
				(lastRes[bestIndex * 2 + 0] == lastRes[i * 2 + 0] && lastRes[bestIndex * 2 + 1] > lastRes[i * 2 + 1]))
				bestIndex = i;
		}
		
		if (lastRes[bestIndex * 2 + 0] > w)
			break;
		if (lastRes[bestIndex * 2 + 1] > h)
		{
			lastRes[bestIndex * 2 + 0] += incrRes[bestIndex * 2 + 0];
			lastRes[bestIndex * 2 + 1] += incrRes[bestIndex * 2 + 1];
			continue;
		}

		desc.add_resolution(lastRes[bestIndex * 2 + 0], lastRes[bestIndex * 2 + 1], 32);
		lastRes[bestIndex * 2 + 0] += incrRes[bestIndex * 2 + 0];
		lastRes[bestIndex * 2 + 1] += incrRes[bestIndex * 2 + 1];
	}
	/*
	** Add the render device to our table
	*/
	_RenderDeviceDescriptionTable.Add(desc);
#ifdef INFO_VULKAN
	int adapter_count = D3DInterface->GetAdapterCount();
	for (int adapter_index=0; adapter_index<adapter_count; adapter_index++) {

		D3DADAPTER_IDENTIFIER9 id;
		::ZeroMemory(&id, sizeof(D3DADAPTER_IDENTIFIER9));
		HRESULT res = D3DInterface->GetAdapterIdentifier(adapter_index,D3DENUM_WHQL_LEVEL,&id);

		if (res == D3D_OK) {

			/*
			** Set up the render device description
			** TODO: Fill in more fields of the render device description?  (need some lookup tables)
			*/
			RenderDeviceDescClass desc;
			desc.set_device_name(id.Description);
			desc.set_driver_name(id.Driver);

			char buf[64];
			sprintf(buf,"%d.%d.%d.%d", //"%04x.%04x.%04x.%04x",
				HIWORD(id.DriverVersion.HighPart),
				LOWORD(id.DriverVersion.HighPart),
				HIWORD(id.DriverVersion.LowPart),
				LOWORD(id.DriverVersion.LowPart));

			desc.set_driver_version(buf);

			D3DInterface->GetDeviceCaps(adapter_index,WW3D_DEVTYPE,&desc.Caps);
			D3DInterface->GetAdapterIdentifier(adapter_index,D3DENUM_WHQL_LEVEL,&desc.AdapterIdentifier);

			DX8Caps dx8caps(D3DInterface,desc.Caps,WW3D_FORMAT_UNKNOWN,desc.AdapterIdentifier);

			/*
			** Enumerate the resolutions
			*/
			desc.reset_resolution_list();
			int mode_count = D3DInterface->GetAdapterModeCount(adapter_index, D3DFMT_X8R8G8B8);
			for (int mode_index=0; mode_index<mode_count; mode_index++) {
				D3DDISPLAYMODE d3dmode;
				::ZeroMemory(&d3dmode, sizeof(D3DDISPLAYMODE));
				HRESULT res = D3DInterface->EnumAdapterModes(adapter_index, D3DFMT_X8R8G8B8,mode_index,&d3dmode);

				if (res == D3D_OK) {
					int bits = 0;
					switch (d3dmode.Format)
					{
						case D3DFMT_R8G8B8:
						case D3DFMT_A8R8G8B8:
						case D3DFMT_X8R8G8B8:		bits = 32; break;

						case D3DFMT_R5G6B5:
						case D3DFMT_X1R5G5B5:		bits = 16; break;
					}

					// Some cards fail in certain modes, DX8Caps keeps list of those.
					if (!dx8caps.Is_Valid_Display_Format(d3dmode.Width,d3dmode.Height,D3DFormat_To_WW3DFormat(d3dmode.Format))) {
						bits=0;
					}

					/*
					** If we recognize the format, add it to the list
					** TODO: should we handle more formats?  will any cards report more than 24 or 16 bit?
					*/
					if (bits != 0) {
						desc.add_resolution(d3dmode.Width,d3dmode.Height,bits);
					}
				}
			}

			// IML: If the device has one or more valid resolutions add it to the device list.
			// NOTE: Testing has shown that there are drivers with zero resolutions.
			if (desc.Enumerate_Resolutions().Count() > 0) {

				/*
				** Set up the device name
				*/
				StringClass device_name(id.Description,true);
				_RenderDeviceNameTable.Add(device_name);
				_RenderDeviceShortNameTable.Add(device_name);	// for now, just add the same name to the "pretty name table"

				/*
				** Add the render device to our table
				*/
				_RenderDeviceDescriptionTable.Add(desc);
			}
		}
	}
#endif
}

bool DX8Wrapper::Set_Any_Render_Device(void)
{
	// Then fullscreen
	int dev_number = 0;
	for (; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if (Set_Render_Device(dev_number,-1,-1,-1,0,false)) {
			return true;
		}
	}

	// Try windowed first
	for (dev_number = 0; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if (Set_Render_Device(dev_number,-1,-1,-1,1,false)) {
			return true;
		}
	}

	return false;
}

bool DX8Wrapper::Set_Render_Device
(
	const char * dev_name,
	int width,
	int height,
	int bits,
	int windowed,
	bool resize_window
)
{
	for ( int dev_number = 0; dev_number < _RenderDeviceNameTable.Count(); dev_number++) {
		if ( strcmp( dev_name, _RenderDeviceNameTable[dev_number]) == 0) {
			return Set_Render_Device( dev_number, width, height, bits, windowed, resize_window );
		}

		if ( strcmp( dev_name, _RenderDeviceShortNameTable[dev_number]) == 0) {
			return Set_Render_Device( dev_number, width, height, bits, windowed, resize_window );
		}
	}
	return false;
}

void DX8Wrapper::Get_Format_Name(unsigned int format, StringClass *tex_format)
{
		*tex_format="Unknown";
#ifdef INFO_VULKAN
		switch (format) {
		case D3DFMT_A8R8G8B8: *tex_format="D3DFMT_A8R8G8B8"; break;
		case D3DFMT_R8G8B8: *tex_format="D3DFMT_R8G8B8"; break;
		case D3DFMT_A4R4G4B4: *tex_format="D3DFMT_A4R4G4B4"; break;
		case D3DFMT_A1R5G5B5: *tex_format="D3DFMT_A1R5G5B5"; break;
		case D3DFMT_R5G6B5: *tex_format="D3DFMT_R5G6B5"; break;
		case D3DFMT_L8: *tex_format="D3DFMT_L8"; break;
		case D3DFMT_A8: *tex_format="D3DFMT_A8"; break;
		case D3DFMT_P8: *tex_format="D3DFMT_P8"; break;
		case D3DFMT_X8R8G8B8: *tex_format="D3DFMT_X8R8G8B8"; break;
		case D3DFMT_X1R5G5B5: *tex_format="D3DFMT_X1R5G5B5"; break;
		case D3DFMT_R3G3B2: *tex_format="D3DFMT_R3G3B2"; break;
		case D3DFMT_A8R3G3B2: *tex_format="D3DFMT_A8R3G3B2"; break;
		case D3DFMT_X4R4G4B4: *tex_format="D3DFMT_X4R4G4B4"; break;
		case D3DFMT_A8P8: *tex_format="D3DFMT_A8P8"; break;
		case D3DFMT_A8L8: *tex_format="D3DFMT_A8L8"; break;
		case D3DFMT_A4L4: *tex_format="D3DFMT_A4L4"; break;
		case D3DFMT_V8U8: *tex_format="D3DFMT_V8U8"; break;
		case D3DFMT_L6V5U5: *tex_format="D3DFMT_L6V5U5"; break;  
		case D3DFMT_X8L8V8U8: *tex_format="D3DFMT_X8L8V8U8"; break;
		case D3DFMT_Q8W8V8U8: *tex_format="D3DFMT_Q8W8V8U8"; break;
		case D3DFMT_V16U16: *tex_format="D3DFMT_V16U16"; break;
		//case D3DFMT_W11V11U10: *tex_format="D3DFMT_W11V11U10"; break;
		case D3DFMT_UYVY: *tex_format="D3DFMT_UYVY"; break;
		case D3DFMT_YUY2: *tex_format="D3DFMT_YUY2"; break;
		case D3DFMT_DXT1: *tex_format="D3DFMT_DXT1"; break;
		case D3DFMT_DXT2: *tex_format="D3DFMT_DXT2"; break;
		case D3DFMT_DXT3: *tex_format="D3DFMT_DXT3"; break;
		case D3DFMT_DXT4: *tex_format="D3DFMT_DXT4"; break;
		case D3DFMT_DXT5: *tex_format="D3DFMT_DXT5"; break;
		case D3DFMT_D16_LOCKABLE: *tex_format="D3DFMT_D16_LOCKABLE"; break;
		case D3DFMT_D32: *tex_format="D3DFMT_D32"; break;
		case D3DFMT_D15S1: *tex_format="D3DFMT_D15S1"; break;
		case D3DFMT_D24S8: *tex_format="D3DFMT_D24S8"; break;
		case D3DFMT_D16: *tex_format="D3DFMT_D16"; break;
		case D3DFMT_D24X8: *tex_format="D3DFMT_D24X8"; break;
		case D3DFMT_D24X4S4: *tex_format="D3DFMT_D24X4S4"; break;
		default:	break;
		}
#endif
}

bool DX8Wrapper::Set_Render_Device(int dev, int width, int height, int bits, int windowed,
								   bool resize_window,bool reset_device, bool restore_assets)
{
	WWASSERT(IsInitted);
	WWASSERT(dev >= -1);
	WWASSERT(dev < _RenderDeviceNameTable.Count());

	/*
	** If user has never selected a render device, start out with device 0
	*/
	if ((CurRenderDevice == -1) && (dev == -1)) {
		CurRenderDevice = 0;
	} else if (dev != -1) {
		CurRenderDevice = dev;
	}
	
	/*
	** If user doesn't want to change res, set the res variables to match the 
	** current resolution
	*/
	if (width != -1)		ResolutionWidth = width;
	if (height != -1)		ResolutionHeight = height;
	
	// Initialize Render2DClass Screen Resolution
	Render2DClass::Set_Screen_Resolution( RectClass( 0, 0, ResolutionWidth, ResolutionHeight ) );

	if (bits != -1)		BitDepth = bits;
	if (windowed != -1)	IsWindowed = (windowed != 0);
	DX8Wrapper_IsWindowed = IsWindowed;

#ifdef INFO_VULKAN
	WWDEBUG_SAY(("Attempting Set_Render_Device: name: %s (%s:%s), width: %d, height: %d, windowed: %d\n",
		_RenderDeviceNameTable[CurRenderDevice],_RenderDeviceDescriptionTable[CurRenderDevice].Get_Driver_Name(),
		_RenderDeviceDescriptionTable[CurRenderDevice].Get_Driver_Version(),ResolutionWidth,ResolutionHeight,(IsWindowed ? 1 : 0)));
#endif

#ifdef _WINDOWS
	// PWG 4/13/2000 - changed so that if you say to resize the window it resizes
	// regardless of whether its windowed or not as OpenGL resizes its self around
	// the caption and edges of the window type you provide, so its important to 
	// push the client area to be the size you really want.
	// if ( resize_window && windowed ) {
	if (resize_window) {

		// Get the current dimensions of the 'render area' of the window
		RECT rect = { 0 };
		::GetClientRect (_Hwnd, &rect);

		// Is the window the correct size for this resolution?
		if ((rect.right-rect.left) != ResolutionWidth ||
			 (rect.bottom-rect.top) != ResolutionHeight) {			
			
			// Calculate what the main window's bounding rectangle should be to
			// accomodate this resolution
			rect.left = 0;
			rect.top = 0;
			rect.right = ResolutionWidth;
			rect.bottom = ResolutionHeight;
			DWORD dwstyle = ::GetWindowLong (_Hwnd, GWL_STYLE);
			AdjustWindowRect (&rect, dwstyle, FALSE);

			// Resize the window to fit this resolution
			if (!windowed)
				::SetWindowPos(_Hwnd, HWND_TOPMOST, 0, 0, rect.right-rect.left, rect.bottom-rect.top,SWP_NOSIZE |SWP_NOMOVE);
			else
				::SetWindowPos (_Hwnd,
								 NULL,
								 0,
								 0,
								 rect.right-rect.left,
								 rect.bottom-rect.top,
								 SWP_NOZORDER | SWP_NOMOVE);
		}
	}
#endif
#ifdef TODO_VULKAN
	//must be either resetting existing device or creating a new one.
	WWASSERT(reset_device || D3DDevice == NULL);
	
	/*
	** Initialize values for D3DPRESENT_PARAMETERS members. 	
	*/
	::ZeroMemory(&_PresentParameters, sizeof(D3DPRESENT_PARAMETERS));

	_PresentParameters.BackBufferWidth = ResolutionWidth;
	_PresentParameters.BackBufferHeight = ResolutionHeight;
	_PresentParameters.BackBufferCount = IsWindowed ? 1 : 2;
	
	_PresentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	//I changed this to discard all the time (even when full-screen) since that the most efficient. 07-16-03 MW:
	_PresentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;//IsWindowed ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_FLIP;		// Shouldn't this be D3DSWAPEFFECT_FLIP?
	_PresentParameters.hDeviceWindow = _Hwnd;
	_PresentParameters.Windowed = IsWindowed;

	_PresentParameters.EnableAutoDepthStencil = TRUE;				// Driver will attempt to match Z-buffer depth
	_PresentParameters.Flags=0;											// We're not going to lock the backbuffer
	
	_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	_PresentParameters.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

	/*
	** Set up the buffer formats.  Several issues here:
	** - if in windowed mode, the backbuffer must use the current display format.
	** - the depth buffer must use 
	*/
	if (IsWindowed) {

		D3DDISPLAYMODE desktop_mode;
		::ZeroMemory(&desktop_mode, sizeof(D3DDISPLAYMODE));
		D3DInterface->GetAdapterDisplayMode( CurRenderDevice, &desktop_mode );

		DisplayFormat=_PresentParameters.BackBufferFormat = desktop_mode.Format;

		// In windowed mode, define the bitdepth from desktop mode (as it can't be changed)
		switch (_PresentParameters.BackBufferFormat) {
		case D3DFMT_X8R8G8B8:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_R8G8B8: BitDepth=32; break;
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_R5G6B5: BitDepth=16; break;
		case D3DFMT_L8:
		case D3DFMT_A8:
		case D3DFMT_P8: BitDepth=8; break;
		default:
			// Unknown backbuffer format probably means the device can't do windowed
			return false;
		}

		if (BitDepth==32 && D3DInterface->CheckDeviceType(0,D3DDEVTYPE_HAL,desktop_mode.Format,D3DFMT_A8R8G8B8, TRUE) == D3D_OK)
		{	//promote 32-bit modes to include destination alpha
			_PresentParameters.BackBufferFormat = D3DFMT_A8R8G8B8;
		}

		/*
		** Find a appropriate Z buffer
		*/
		if (!Find_Z_Mode(DisplayFormat,_PresentParameters.BackBufferFormat,&_PresentParameters.AutoDepthStencilFormat))
		{
			// If opening 32 bit mode failed, try 16 bit, even if the desktop happens to be 32 bit
			if (BitDepth==32) {
				BitDepth=16;
				_PresentParameters.BackBufferFormat=D3DFMT_R5G6B5;
				if (!Find_Z_Mode(_PresentParameters.BackBufferFormat,_PresentParameters.BackBufferFormat,&_PresentParameters.AutoDepthStencilFormat)) {
					_PresentParameters.AutoDepthStencilFormat=D3DFMT_UNKNOWN;
				}
			}
			else {
				_PresentParameters.AutoDepthStencilFormat=D3DFMT_UNKNOWN;
			}
		}

	} else {

		/*
		** Try to find a mode that matches the user's desired bit-depth.
		*/
		Find_Color_And_Z_Mode(ResolutionWidth,ResolutionHeight,BitDepth,&DisplayFormat,
			&_PresentParameters.BackBufferFormat,&_PresentParameters.AutoDepthStencilFormat);
	}

	/*
	** Time to actually create the device.
	*/
	if (_PresentParameters.AutoDepthStencilFormat==D3DFMT_UNKNOWN) {
		if (BitDepth==32) {
			_PresentParameters.AutoDepthStencilFormat=D3DFMT_D32;
		}
		else {
			_PresentParameters.AutoDepthStencilFormat=D3DFMT_D16;
		}
	}

	StringClass displayFormat;
	StringClass backbufferFormat;

	Get_Format_Name(DisplayFormat,&displayFormat);
	Get_Format_Name(_PresentParameters.BackBufferFormat,&backbufferFormat);

	WWDEBUG_SAY(("Using Display/BackBuffer Formats: %s/%s\n",displayFormat,backbufferFormat));
	
	bool ret;

	if (reset_device)
		ret = Reset_Device(restore_assets);	//reset device without restoring data - we're likely switching out of the app.
	else
		ret = Create_Device();

	WWDEBUG_SAY(("Reset/Create_Device done, reset_device=%d, restore_assets=%d\n", reset_device, restore_assets));

	return ret;
#else
	Create_Device();
	return true;
#endif
}

bool DX8Wrapper::Toggle_Windowed(void)
{
#ifdef WW3D_DX8
	// State OK?
	assert (IsInitted);
	if (IsInitted) {

		// Get information about the current render device's resolutions
		const RenderDeviceDescClass &render_device = Get_Render_Device_Desc ();
		const DynamicVectorClass<ResolutionDescClass> &resolutions = render_device.Enumerate_Resolutions ();

		// Loop through all the resolutions supported by the current device.
		// If we aren't currently running under one of these resolutions,
		// then we should probably		 to the closest resolution before
		// toggling the windowed state.
		int curr_res = -1;
		for (int res = 0;
		     (res < resolutions.Count ()) && (curr_res == -1);
			  res ++) {

			// Is this the resolution we are looking for?
			if ((resolutions[res].Width == ResolutionWidth) &&
				 (resolutions[res].Height == ResolutionHeight) &&
				 (resolutions[res].BitDepth == BitDepth)) {
				curr_res = res;
			}
		}

		if (curr_res == -1) {

			// We don't match any of the standard resolutions,
			// so set the first resolution and toggle the windowed state.
			return Set_Device_Resolution (resolutions[0].Width,
								 resolutions[0].Height,
								 resolutions[0].BitDepth,
								 !IsWindowed, true);
		} else {

			// Toggle the windowed state
			return Set_Device_Resolution (-1, -1, -1, !IsWindowed, true);
		}
	}
#endif //WW3D_DX8

	return false;
}

void DX8Wrapper::Set_Swap_Interval(int swap)
{
	//I'm just going to use a set number of swap chains
#if INFO_VULKAN
	switch (swap) {
		case 0: _PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; break;
		case 1: _PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE ; break;
		case 2: _PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_TWO; break;
		case 3: _PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_THREE; break;
		default: _PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE ; break;
	}

	Reset_Device();
#endif
}

int DX8Wrapper::Get_Swap_Interval(void)
{
	return 1;// _PresentParameters.PresentationInterval;
}

bool DX8Wrapper::Has_Stencil(void)
{
	bool has_stencil = true;//(_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24S8 ||
					//	_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24X4S4);
	return has_stencil;
}

int DX8Wrapper::Get_Render_Device_Count(void)
{
	return _RenderDeviceNameTable.Count();

}
int DX8Wrapper::Get_Render_Device(void)
{
	assert(IsInitted);
	return CurRenderDevice;
}

const RenderDeviceDescClass & DX8Wrapper::Get_Render_Device_Desc(int deviceidx)
{
	WWASSERT(IsInitted);

	if ((deviceidx == -1) && (CurRenderDevice == -1)) {
		CurRenderDevice = 0;
	}

	// if the device index is -1 then we want the current device
	if (deviceidx == -1) {
		WWASSERT(CurRenderDevice >= 0);
		WWASSERT(CurRenderDevice < _RenderDeviceNameTable.Count());
		return _RenderDeviceDescriptionTable[CurRenderDevice];
	}

	// We can only ask for multiple device information if the devices
	// have been detected.
	WWASSERT(deviceidx >= 0);
	WWASSERT(deviceidx < _RenderDeviceNameTable.Count());
	return _RenderDeviceDescriptionTable[deviceidx];
}

const char * DX8Wrapper::Get_Render_Device_Name(int device_index)
{
	device_index = device_index % _RenderDeviceShortNameTable.Count();
	return _RenderDeviceShortNameTable[device_index];
}

bool DX8Wrapper::Set_Device_Resolution(int width,int height,int bits,int windowed, bool resize_window)
{
#ifdef TODO_VULKAN
	if (D3DDevice != NULL) {

		if (width != -1) {
			_PresentParameters.BackBufferWidth = ResolutionWidth = width;
		}
		if (height != -1) {
			_PresentParameters.BackBufferHeight = ResolutionHeight = height;
		}
		if (resize_window)
		{

			// Get the current dimensions of the 'render area' of the window
			RECT rect = { 0 };
			::GetClientRect (_Hwnd, &rect);

			// Is the window the correct size for this resolution?
			if ((rect.right-rect.left) != ResolutionWidth ||
				 (rect.bottom-rect.top) != ResolutionHeight)
			{			
				
				// Calculate what the main window's bounding rectangle should be to
				// accomodate this resolution
				rect.left = 0;
				rect.top = 0;
				rect.right = ResolutionWidth;
				rect.bottom = ResolutionHeight;
				DWORD dwstyle = ::GetWindowLong (_Hwnd, GWL_STYLE);
				AdjustWindowRect (&rect, dwstyle, FALSE);

				// Resize the window to fit this resolution
				if (!windowed)
					::SetWindowPos(_Hwnd, HWND_TOPMOST, 0, 0, rect.right-rect.left, rect.bottom-rect.top,SWP_NOSIZE |SWP_NOMOVE);
				else
					::SetWindowPos (_Hwnd,
									 NULL,
									 0,
									 0,
									 rect.right-rect.left,
									 rect.bottom-rect.top,
									 SWP_NOZORDER | SWP_NOMOVE);
			}
		}
#pragma message("TODO: support changing windowed status and changing the bit depth")
		return Reset_Device();
	} else {
		return false;
	}
#else
	return false;
#endif
}

void DX8Wrapper::Get_Device_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed)
{
	WWASSERT(IsInitted);

	set_w = ResolutionWidth;
	set_h = ResolutionHeight;
	set_bits = BitDepth;
	set_windowed = IsWindowed;

	return ;
}

void DX8Wrapper::Get_Render_Target_Resolution(int & set_w,int & set_h,int & set_bits,bool & set_windowed)
{
	WWASSERT(IsInitted);
#ifdef TODO_VULKAN
	if (CurrentRenderTarget != NULL) {
		D3DSURFACE_DESC info;
		CurrentRenderTarget->GetDesc (&info);

		set_w				= info.Width;
		set_h				= info.Height;
		set_bits			= BitDepth;		// should we get the actual bit depth of the target?
		set_windowed	= IsWindowed;	// this doesn't really make sense for render targets (shouldn't matter)...

	} else {
		Get_Device_Resolution (set_w, set_h, set_bits, set_windowed);
	}
#else
	set_w = target.swapChainExtent.width;
	set_h = target.swapChainExtent.height;
	set_bits = BitDepth;
	set_windowed = IsWindowed;
#endif
	return ;
}


#ifdef INFO_VULKAN

bool DX8Wrapper::Find_Color_And_Z_Mode(int resx,int resy,int bitdepth,D3DFORMAT * set_colorbuffer,D3DFORMAT * set_backbuffer,D3DFORMAT * set_zmode)
{
	static D3DFORMAT _formats16[] =
	{
		D3DFMT_R5G6B5,
		D3DFMT_X1R5G5B5,
		D3DFMT_A1R5G5B5
	};

	static D3DFORMAT _formats32[] =
	{
		D3DFMT_A8R8G8B8,
		D3DFMT_X8R8G8B8,
		D3DFMT_R8G8B8,
	};

	/*
	** Select the table that we're going to use to search for a valid backbuffer format
	*/
	D3DFORMAT * format_table = NULL;
	int format_count = 0;

	if (BitDepth == 16) {
		format_table = _formats16;
		format_count = sizeof(_formats16) / sizeof(D3DFORMAT);
	} else {
		format_table = _formats32;
		format_count = sizeof(_formats32) / sizeof(D3DFORMAT);
	}

	/*
	** now search for a valid format
	*/
	bool found = false;
	unsigned int mode = 0;

	int format_index = 0;
	for (; format_index < format_count; format_index++) {
		found |= Find_Color_Mode(format_table[format_index],resx,resy,&mode);
		if (found) break;
	}

	if (!found) {
		return false;
	} else {
		*set_backbuffer=*set_colorbuffer = format_table[format_index];
	}

	if (bitdepth==32 && *set_colorbuffer == D3DFMT_X8R8G8B8 && D3DInterface->CheckDeviceType(0,D3DDEVTYPE_HAL,*set_colorbuffer,D3DFMT_A8R8G8B8, TRUE) == D3D_OK)
	{	//promote 32-bit modes to include destination alpha when supported
		*set_backbuffer = D3DFMT_A8R8G8B8;
	}

	/*
	** We found a backbuffer format, now find a zbuffer format
	*/
	return Find_Z_Mode(*set_colorbuffer,*set_backbuffer, set_zmode);
};


// find the resolution mode with at least resx,resy with the highest supported
// refresh rate
bool DX8Wrapper::Find_Color_Mode(D3DFORMAT colorbuffer, int resx, int resy, UINT *mode)
{
	UINT i,j,modemax;
	UINT rx,ry;
	D3DDISPLAYMODE dmode;
	::ZeroMemory(&dmode, sizeof(D3DDISPLAYMODE));

	rx=(unsigned int) resx;
	ry=(unsigned int) resy;

	bool found=false;

	modemax=D3DInterface->GetAdapterModeCount(D3DADAPTER_DEFAULT, colorbuffer);

	i=0;

	while (i<modemax && !found)
	{
		D3DInterface->EnumAdapterModes(D3DADAPTER_DEFAULT, colorbuffer, i, &dmode);
		if (dmode.Width==rx && dmode.Height==ry && dmode.Format==colorbuffer) {
			WWDEBUG_SAY(("Found valid color mode.  Width = %d Height = %d Format = %d\r\n",dmode.Width,dmode.Height,dmode.Format));
			found=true;
		}
		i++;
	}

	i--; // this is the first valid mode

	// no match
	if (!found) {
		WWDEBUG_SAY(("Failed to find a valid color mode\r\n"));
		return false;
	}

	// go to the highest refresh rate in this mode
	bool stillok=true;

	j=i;
	while (j<modemax && stillok)
	{
		D3DInterface->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_A8R8G8B8, j, &dmode);
		if (dmode.Width==rx && dmode.Height==ry && dmode.Format==colorbuffer)
			stillok=true; else stillok=false;
		j++;
	}

	if (stillok==false) *mode=j-2;
	else *mode=i;

	return true;
}

// Helper function to find a Z buffer mode for the colorbuffer
// Will look for greatest Z precision
bool DX8Wrapper::Find_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT *zmode)
{
	//MW: Swapped the next 2 tests so that Stencil modes get tested first.
	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D24S8))
	{
		*zmode=D3DFMT_D24S8;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D24S8\n"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D32))
	{
		*zmode=D3DFMT_D32;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D32\n"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D24X8))
	{
		*zmode=D3DFMT_D24X8;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D24X8\n"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D24X4S4))
	{
		*zmode=D3DFMT_D24X4S4;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D24X4S4\n"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D16))
	{
		*zmode=D3DFMT_D16;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D16\n"));
		return true;
	}

	if (Test_Z_Mode(colorbuffer,backbuffer,D3DFMT_D15S1))
	{
		*zmode=D3DFMT_D15S1;
		WWDEBUG_SAY(("Found zbuffer mode D3DFMT_D15S1\n"));
		return true;
	}

	// can't find a match
	WWDEBUG_SAY(("Failed to find a valid zbuffer mode\r\n"));
	return false;
}

bool DX8Wrapper::Test_Z_Mode(D3DFORMAT colorbuffer,D3DFORMAT backbuffer, D3DFORMAT zmode)
{
	// See if we have this mode first
	if (FAILED(D3DInterface->CheckDeviceFormat(D3DADAPTER_DEFAULT,WW3D_DEVTYPE,
		colorbuffer,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_SURFACE,zmode)))
	{
		WWDEBUG_SAY(("CheckDeviceFormat failed.  Colorbuffer format = %d  Zbufferformat = %d\n",colorbuffer,zmode));
		return false;
	}

	// Then see if it matches the color buffer
	if(FAILED(D3DInterface->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, WW3D_DEVTYPE,
		colorbuffer,backbuffer,zmode)))
	{
		WWDEBUG_SAY(("CheckDepthStencilMatch failed.  Colorbuffer format = %d  Backbuffer format = %d Zbufferformat = %d\n",colorbuffer,backbuffer,zmode));
		return false;
	}
	return true;
}
#endif


void DX8Wrapper::Reset_Statistics()
{
	matrix_changes	= 0;
	material_changes = 0;
	vertex_buffer_changes = 0;
	index_buffer_changes = 0;
	light_changes = 0;
	texture_changes = 0;
	render_state_changes =0;
	texture_stage_state_changes =0;
	draw_calls =0;

	number_of_DX8_calls = 0;
	last_frame_matrix_changes = 0;
	last_frame_material_changes = 0;
	last_frame_vertex_buffer_changes = 0;
	last_frame_index_buffer_changes = 0;
	last_frame_light_changes = 0;
	last_frame_texture_changes = 0;
	last_frame_render_state_changes = 0;
	last_frame_texture_stage_state_changes = 0;
	last_frame_number_of_DX8_calls = 0;
	last_frame_draw_calls =0;
}

void DX8Wrapper::Begin_Statistics()
{
	matrix_changes=0;
	material_changes=0;
	vertex_buffer_changes=0;
	index_buffer_changes=0;
	light_changes=0;
	texture_changes = 0;
	render_state_changes =0;
	texture_stage_state_changes =0;
	number_of_DX8_calls=0;
	draw_calls=0;
}

void DX8Wrapper::End_Statistics()
{
	last_frame_matrix_changes=matrix_changes;
	last_frame_material_changes=material_changes;
	last_frame_vertex_buffer_changes=vertex_buffer_changes;
	last_frame_index_buffer_changes=index_buffer_changes;
	last_frame_light_changes=light_changes;
	last_frame_texture_changes = texture_changes;
	last_frame_render_state_changes = render_state_changes;
	last_frame_texture_stage_state_changes = texture_stage_state_changes;
	last_frame_number_of_DX8_calls=number_of_DX8_calls;
	last_frame_draw_calls=draw_calls;
}

unsigned DX8Wrapper::Get_Last_Frame_Matrix_Changes()			{ return last_frame_matrix_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Material_Changes()		{ return last_frame_material_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Vertex_Buffer_Changes()	{ return last_frame_vertex_buffer_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Index_Buffer_Changes()	{ return last_frame_index_buffer_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Light_Changes()			{ return last_frame_light_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Texture_Changes()			{ return last_frame_texture_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Render_State_Changes()	{ return last_frame_render_state_changes; }
unsigned DX8Wrapper::Get_Last_Frame_Texture_Stage_State_Changes()	{ return last_frame_texture_stage_state_changes; }
unsigned DX8Wrapper::Get_Last_Frame_DX8_Calls()					{ return last_frame_number_of_DX8_calls; }
unsigned DX8Wrapper::Get_Last_Frame_Draw_Calls()				{ return last_frame_draw_calls; }
unsigned long DX8Wrapper::Get_FrameCount(void) {return FrameCount;}

void DX8_Assert()
{
#ifdef INFO_VULKAN
	WWASSERT(DX8Wrapper::_Get_D3D8());
#endif
	DX8_THREAD_ASSERT();
}

void DX8Wrapper::Begin_Scene(void)
{
	DX8_THREAD_ASSERT();

	{
		VkColorBlendEquationEXT colorBlendAttachment = {};
		colorBlendAttachment.srcColorBlendFactor = (VkBlendFactor)
			(RenderStates[VKRS_SRCBLEND] != 0x12345678 ? RenderStates[VKRS_SRCBLEND] : RenderStates[VKRS_MAX + VKRS_SRCBLEND]);
		colorBlendAttachment.dstColorBlendFactor = (VkBlendFactor)
			(RenderStates[VKRS_DESTBLEND] != 0x12345678 ? RenderStates[VKRS_DESTBLEND] : RenderStates[VKRS_MAX + VKRS_DESTBLEND]);
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		if (RenderStates[VKRS_ALPHABLENDENABLE])
		{
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		}
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		vkCmdSetColorBlendEquationEXT(WWVKRENDER.currentCmd, 0, 1, &colorBlendAttachment);
	}
	{
		vkCmdSetStencilOp(WWVKRENDER.currentCmd, VK_STENCIL_FRONT_AND_BACK,
			(VkStencilOp)(RenderStates[VKRS_STENCILFAIL] != 0x12345678 ? RenderStates[VKRS_STENCILFAIL] : RenderStates[VKRS_MAX + VKRS_STENCILFAIL]),
			(VkStencilOp)(RenderStates[VKRS_STENCILPASS] != 0x12345678 ? RenderStates[VKRS_STENCILPASS] : RenderStates[VKRS_MAX + VKRS_STENCILPASS]),
			(VkStencilOp)(RenderStates[VKRS_STENCILZFAIL] != 0x12345678 ? RenderStates[VKRS_STENCILZFAIL] : RenderStates[VKRS_MAX + VKRS_STENCILZFAIL]),
			(VkCompareOp)(RenderStates[VKRS_STENCILFUNC] != 0x12345678 ? RenderStates[VKRS_STENCILFUNC] : RenderStates[VKRS_MAX + VKRS_STENCILFUNC]));
	}
	{
		VkColorComponentFlags flags[4] = {};
		unsigned int flagCount = 0;
		if (0x1 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_R_BIT; }
		if (0x2 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_G_BIT; }
		if (0x4 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_B_BIT; }
		if (0x8 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_A_BIT; }
		flagCount = 1;
		if (RenderStates[VKRS_COLORWRITEENABLE] == 0x12345678)
		{
			flags[0] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			flagCount = 1;
		}
		vkCmdSetColorWriteMaskEXT(target.currentCmd, 0, flagCount, flags);
	}
	{
		vkCmdSetDepthBias(target.currentCmd, RenderStates[VKRS_DEPTHBIAS] * -0.000005f, 0, 0);
	}
	{
		vkCmdSetDepthWriteEnable(target.currentCmd, RenderStates[VKRS_ZWRITEENABLE] == 0x12345678 ? VK_TRUE : RenderStates[VKRS_ZWRITEENABLE]);
	}
	{
		vkCmdSetDepthCompareOp(target.currentCmd, RenderStates[VKRS_ZFUNC] == 0x12345678 ? 
			VK_COMPARE_OP_LESS_OR_EQUAL : (VkCompareOp)RenderStates[VKRS_ZFUNC]);
	}
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
	{
		vkCmdSetStencilCompareMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILMASK] == 0x12345678 ?
			0xffffffff : RenderStates[VKRS_STENCILMASK]);
	}
	{
		vkCmdSetStencilWriteMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILWRITEMASK] == 0x12345678 ? 
			0xffffffff : RenderStates[VKRS_STENCILWRITEMASK]);
	}
	{
		vkCmdSetStencilReference(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILREF]);
	}
	{
		vkCmdSetStencilTestEnable(WWVKRENDER.currentCmd, RenderStates[VKRS_STENCILENABLE] == 0x12345678 ? 
			VK_FALSE : RenderStates[VKRS_STENCILENABLE]);
	}
}

void DX8Wrapper::End_Scene(bool flip_frames)
{
	DX8_THREAD_ASSERT();
#ifdef INFO_VULKAN
	DX8CALL(EndScene());

	if (flip_frames) {
		DX8_Assert();
		HRESULT hr;
		{
			WWPROFILE("DX8Device::Present()");
			hr=_Get_D3D_Device8()->Present(NULL, NULL, NULL, NULL);
		}

		number_of_DX8_calls++;

		if (SUCCEEDED(hr)) {
#ifdef EXTENDED_STATS
			if (stats.m_sleepTime) {
				::Sleep(stats.m_sleepTime);
			}
#endif
			IsDeviceLost=false;
			FrameCount++;
		}
		else {
			IsDeviceLost=true;
		}

		// If the device was lost we need to check for cooperative level and possibly reset the device
		if (hr==D3DERR_DEVICELOST) {
			hr=_Get_D3D_Device8()->TestCooperativeLevel();
			if (hr==D3DERR_DEVICENOTRESET) {
				Reset_Device();
			}
			else {
				// Sleep it not active
				ThreadClass::Sleep_Ms(200);
			}
		}
		else {
			DX8_ErrorCode(hr);
		}
	}

	// Each frame, release all of the buffers and textures.
	Set_Vertex_Buffer(NULL);
	Set_Index_Buffer(NULL,0);
	for (int i=0;i<CurrentCaps->Get_Max_Textures_Per_Pass();++i) Set_Texture(i,NULL);
	Set_Material(NULL);
#endif
	target.EndRender();
}

//**********************************************************************************************
//! Clear current render device
/*! KM
/* 5/17/02 KM Fixed support for render to texture with depth/stencil buffers
*/
void DX8Wrapper::Clear(bool clear_color, bool clear_z_stencil, const Vector3 &color, float dest_alpha, float z, unsigned int stencil)
{
	DX8_THREAD_ASSERT();

	// If we try to clear a stencil buffer which is not there, the entire call will fail
	// KJM fixed this to get format from back buffer (incase render to texture is used)
	/*bool has_stencil = (	_PresentParameters.AutoDepthStencilFormat == D3DFMT_D15S1 ||
								_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24S8 ||
								_PresentParameters.AutoDepthStencilFormat == D3DFMT_D24X4S4);*/
	bool has_stencil=true;
#ifdef INFO_VULKAN
	IDirect3DSurface9* depthbuffer;

	_Get_D3D_Device8()->GetDepthStencilSurface(&depthbuffer);
	number_of_DX8_calls++;

	if (depthbuffer)
	{
		D3DSURFACE_DESC desc;
		depthbuffer->GetDesc(&desc);
		has_stencil=
		(
			desc.Format==D3DFMT_D15S1 ||
			desc.Format==D3DFMT_D24S8 ||
			desc.Format==D3DFMT_D24X4S4
		);

		// release ref
		depthbuffer->Release();
	}

	DWORD flags = 0;
	if (clear_color) flags |= D3DCLEAR_TARGET;
	if (clear_z_stencil) flags |= D3DCLEAR_ZBUFFER;
	if (clear_z_stencil && has_stencil) flags |= D3DCLEAR_STENCIL;
	if (flags)
	{
		DX8CALL(Clear(0, NULL, flags, Convert_Color(color,dest_alpha), z, stencil));
	}
#else
	VkClearRect rect = {
		0, 0,
		WWVKRENDER.swapChainExtent.width, WWVKRENDER.swapChainExtent.height,
		0, 1
	};
	VkClearAttachment attachments[2] = { };
	int ci = 0;
	if (clear_color)
	{
		memcpy(attachments[ci].clearValue.color.float32, &color.X, sizeof(float) * 3);
		attachments[ci].clearValue.color.float32[3] = dest_alpha;
		attachments[ci].colorAttachment = 0;
		attachments[ci].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ci++;
	}
	if (clear_z_stencil)
	{
		attachments[ci].clearValue.depthStencil.depth = z;
		attachments[ci].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (has_stencil)
		{
			attachments[ci].clearValue.depthStencil.stencil = stencil;
			attachments[ci].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		ci++;
	}
	vkCmdClearAttachments(
		WWVKRENDER.currentCmd,
		ci,
		attachments,
		1,
		&rect);
#endif
}

void DX8Wrapper::Set_Viewport(CONST VkViewport* pViewport)
{
	DX8_THREAD_ASSERT();
	vkCmdSetViewport(target.currentCmd, 0, 1, pViewport);
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer. A reference to previous vertex buffer is released and
// this one is assigned the current vertex buffer. The DX8 vertex buffer will
// actually be set in Apply() which is called by Draw_Indexed_Triangles().
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream)
{
	render_state.vba_offset=0;
	render_state.vba_count=0;
	if (render_state.vertex_buffers[stream]) {
		render_state.vertex_buffers[stream]->Release_Engine_Ref();
	}
	REF_PTR_SET(render_state.vertex_buffers[stream],const_cast<VertexBufferClass*>(vb));
	if (vb) {
		vb->Add_Engine_Ref();
		render_state.vertex_buffer_types[stream]=vb->Type();
	}
	else {
		render_state.vertex_buffer_types[stream]=BUFFER_TYPE_INVALID;
	}
	render_state_changed|=VERTEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
//
// Set index buffer. A reference to previous index buffer is released and
// this one is assigned the current index buffer. The DX8 index buffer will
// actually be set in Apply() which is called by Draw_Indexed_Triangles().
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Index_Buffer(const IndexBufferClass* ib,unsigned short index_base_offset)
{
	render_state.iba_offset=0;
	if (render_state.index_buffer) {
		render_state.index_buffer->Release_Engine_Ref();
	}
	REF_PTR_SET(render_state.index_buffer,const_cast<IndexBufferClass*>(ib));
	render_state.index_base_offset=index_base_offset;
	if (ib) {
		ib->Add_Engine_Ref();
		render_state.index_buffer_type=ib->Type();
	}
	else {
		render_state.index_buffer_type=BUFFER_TYPE_INVALID;
	}
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

// ----------------------------------------------------------------------------
//
// Set vertex buffer using dynamic access object.
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Vertex_Buffer(const DynamicVBAccessClass& vba_)
{
	// Release all streams (only one stream allowed in the legacy pipeline)
	for (int i=1;i<MAX_VERTEX_STREAMS;++i) {
		DX8Wrapper::Set_Vertex_Buffer(NULL, i);
	}

	if (render_state.vertex_buffers[0]) render_state.vertex_buffers[0]->Release_Engine_Ref();
	DynamicVBAccessClass& vba=const_cast<DynamicVBAccessClass&>(vba_);
	render_state.vertex_buffer_types[0]=vba.Get_Type();
	render_state.vba_offset=vba.VertexBufferOffset;
	render_state.vba_count=vba.Get_Vertex_Count();
	REF_PTR_SET(render_state.vertex_buffers[0],vba.VertexBuffer);
	render_state.vertex_buffers[0]->Add_Engine_Ref();
	render_state_changed|=VERTEX_BUFFER_CHANGED;
	render_state_changed|=INDEX_BUFFER_CHANGED;		// vba_offset changes so index buffer needs to be reset as well.
}

VertexBufferClass* DX8Wrapper::Get_Vertex_Buffer()
{
	return render_state.vertex_buffers[0];
}

// ----------------------------------------------------------------------------
//
// Set index buffer using dynamic access object.
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Set_Index_Buffer(const DynamicIBAccessClass& iba_,unsigned short index_base_offset)
{
	if (render_state.index_buffer) render_state.index_buffer->Release_Engine_Ref();

	DynamicIBAccessClass& iba=const_cast<DynamicIBAccessClass&>(iba_);
	render_state.index_base_offset=index_base_offset;
	render_state.index_buffer_type=iba.Get_Type();
	render_state.iba_offset=iba.IndexBufferOffset;
	REF_PTR_SET(render_state.index_buffer,iba.IndexBuffer);
	render_state.index_buffer->Add_Engine_Ref();
	render_state_changed|=INDEX_BUFFER_CHANGED;
}

IndexBufferClass* DX8Wrapper::Set_Index_Buffer()
{
	return render_state.index_buffer;
}

// ----------------------------------------------------------------------------
//
// Private function for the special case of rendering polygons from sorting
// index and vertex buffers.
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Sorting_IB_VB(
	unsigned primitive_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	WWASSERT(render_state.vertex_buffer_types[0]==BUFFER_TYPE_SORTING || render_state.vertex_buffer_types[0]==BUFFER_TYPE_DYNAMIC_SORTING);
	WWASSERT(render_state.index_buffer_type==BUFFER_TYPE_SORTING || render_state.index_buffer_type==BUFFER_TYPE_DYNAMIC_SORTING);

	// Fill dynamic vertex buffer with sorting vertex buffer vertices
	DynamicVBAccessClass dyn_vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,vertex_count);
	{
		DynamicVBAccessClass::WriteLockClass lock(&dyn_vb_access);
		VertexFormatXYZNDUV2* src = static_cast<SortingVertexBufferClass*>(render_state.vertex_buffers[0])->Vertices.data();
		VertexFormatXYZNDUV2* dest= lock.Get_Formatted_Vertex_Array();
		src += render_state.vba_offset + render_state.index_base_offset + min_vertex_index;
		unsigned  size = dyn_vb_access.FVF_Info().Get_FVF_Size()*vertex_count/sizeof(unsigned);
		unsigned *dest_u =(unsigned*) dest;
		unsigned *src_u = (unsigned*) src;

		for (unsigned i=0;i<size;++i) {
			*dest_u++=*src_u++;
		}
	}

	unsigned index_count = 0;
	switch (primitive_type) {
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: index_count = polygon_count * 3; break;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: index_count = polygon_count + 2; break;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: index_count = polygon_count + 2; break;
	default: WWASSERT(0); break; // Unsupported primitive type
	}
	// Fill dynamic index buffer with sorting index buffer vertices
	DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC_DX8, index_count);
	{
		DynamicIBAccessClass::WriteLockClass lock(&dyn_ib_access);
		unsigned short* dest = lock.Get_Index_Array();
		unsigned short* src = NULL;
		src = static_cast<SortingIndexBufferClass*>(render_state.index_buffer)->Get_Indices();
		src += render_state.iba_offset + start_index;

		try {
			for (unsigned short i = 0; i < index_count; ++i) {
				unsigned short index = *src++;
				index -= min_vertex_index;
				WWASSERT(index < vertex_count);
				*dest++ = index;
			}
			IndexBufferExceptionFunc();
		}
		catch (...) {
			IndexBufferExceptionFunc();
		}
	}

#ifdef INFO_VULKAN
	DX8CALL(SetStreamSource(
		0,
		static_cast<DX8VertexBufferClass*>(dyn_vb_access.VertexBuffer)->Get_DX8_Vertex_Buffer(),
		0,
		dyn_vb_access.FVF_Info().Get_FVF_Size()));
	// If using FVF format VB, set the FVF as vertex shader (may not be needed here KM)
	unsigned fvf=dyn_vb_access.FVF_Info().Get_FVF();
	if (fvf!=0) {
		DX8CALL(SetFVF(fvf));
	}
	DX8_RECORD_VERTEX_BUFFER_CHANGE();


	DX8CALL(SetIndices(
		static_cast<DX8IndexBufferClass*>(dyn_ib_access.IndexBuffer)->Get_DX8_Index_Buffer()));
	DX8_RECORD_INDEX_BUFFER_CHANGE();

	DX8_RECORD_DRAW_CALLS();
	DX8CALL(DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		dyn_vb_access.VertexBufferOffset,
		0,		// start vertex
		vertex_count,
		dyn_ib_access.IndexBufferOffset,
		polygon_count));

	DX8_RECORD_RENDER(polygon_count,vertex_count,render_state.shader);
#endif		
	DX8Wrapper::Apply_Render_State_Changes();
	auto pipelines = DX8Wrapper::FindClosestPipelines(dynamic_fvf_type);
	assert(pipelines != PIPELINE_WWVK_MAX);
	switch (pipelines) {
	case 0:
	default: assert(false);
	}
}
void DX8Wrapper::Convert_Sorting_IB_VB(
	SortingVertexBufferClass* sortingV,
	SortingIndexBufferClass* sortingI,
	VK::Buffer& vbo,
	VK::Buffer& ibo
	)
{
	// Fill dynamic vertex buffer with sorting vertex buffer vertices
	VkBufferTools::CreateVertexBuffer(&DX8Wrapper::_GetRenderTarget(),
		sortingV->FVF_Info().Get_FVF_Size() * sortingV->Get_Vertex_Count(), (void*)sortingV->Get_Vertices(), vbo);
	VkBufferTools::CreateIndexBuffer(&DX8Wrapper::_GetRenderTarget(), sortingI->Get_Index_Count() * sizeof(uint16_t),
		sortingI->Get_Indices(), ibo);
#ifdef INFO_VULKAN
	DX8CALL(SetStreamSource(
		0,
		static_cast<DX8VertexBufferClass*>(dyn_vb_access.VertexBuffer)->Get_DX8_Vertex_Buffer(),
		0,
		dyn_vb_access.FVF_Info().Get_FVF_Size()));
	// If using FVF format VB, set the FVF as vertex shader (may not be needed here KM)
	unsigned fvf = dyn_vb_access.FVF_Info().Get_FVF();
	if (fvf != 0) {
		DX8CALL(SetFVF(fvf));
	}
	DX8_RECORD_VERTEX_BUFFER_CHANGE();

	unsigned index_count = 0;
	switch (primitive_type) {
	case D3DPT_TRIANGLELIST: index_count = polygon_count * 3; break;
	case D3DPT_TRIANGLESTRIP: index_count = polygon_count + 2; break;
	case D3DPT_TRIANGLEFAN: index_count = polygon_count + 2; break;
	default: WWASSERT(0); break; // Unsupported primitive type
	}

	// Fill dynamic index buffer with sorting index buffer vertices
	DynamicIBAccessClass dyn_ib_access(BUFFER_TYPE_DYNAMIC_DX8, index_count);
	{
		DynamicIBAccessClass::WriteLockClass lock(&dyn_ib_access);
		unsigned short* dest = lock.Get_Index_Array();
		unsigned short* src = NULL;
		src = static_cast<SortingIndexBufferClass*>(render_state.index_buffer)->index_buffer;
		src += render_state.iba_offset + start_index;

		try {
			for (unsigned short i = 0; i < index_count; ++i) {
				unsigned short index = *src++;
				index -= min_vertex_index;
				WWASSERT(index < vertex_count);
				*dest++ = index;
			}
			IndexBufferExceptionFunc();
		}
		catch (...) {
			IndexBufferExceptionFunc();
		}
	}

	DX8CALL(SetIndices(
		static_cast<DX8IndexBufferClass*>(dyn_ib_access.IndexBuffer)->Get_DX8_Index_Buffer()));
	DX8_RECORD_INDEX_BUFFER_CHANGE();

	DX8_RECORD_DRAW_CALLS();
	DX8CALL(DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST,
		dyn_vb_access.VertexBufferOffset,
		0,		// start vertex
		vertex_count,
		dyn_ib_access.IndexBufferOffset,
		polygon_count));

	DX8_RECORD_RENDER(polygon_count, vertex_count, render_state.shader);
#endif
#if 0
	DX8Wrapper::Apply_Render_State_Changes();
	auto pipelines = DX8Wrapper::FindClosestPipelines(dynamic_fvf_type);
	assert(pipelines != PIPELINE_WWVK_MAX);
	switch (pipelines) {
	case 0:
	default: assert(false);
	}
#endif
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

#ifdef INFO_VULKAN
void DX8Wrapper::Draw_Triangles(
	unsigned buffer_type,
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	if (buffer_type==BUFFER_TYPE_SORTING || buffer_type==BUFFER_TYPE_DYNAMIC_SORTING) {
		SortingRendererClass::Insert_Triangles(start_index,polygon_count,min_vertex_index,vertex_count);
	}
	else {
		Draw(D3DPT_TRIANGLELIST,start_index,polygon_count,min_vertex_index,vertex_count);
	}
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Triangles(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw(D3DPT_TRIANGLELIST,start_index,polygon_count,min_vertex_index,vertex_count);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DX8Wrapper::Draw_Strip(
	unsigned short start_index,
	unsigned short polygon_count,
	unsigned short min_vertex_index,
	unsigned short vertex_count)
{
	Draw(D3DPT_TRIANGLESTRIP,start_index,polygon_count,min_vertex_index,vertex_count);
}
#endif

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------
void ApplyVkStates(unsigned int* RenderStates)
{

	{
		VkColorBlendEquationEXT colorBlendAttachment = {};
		colorBlendAttachment.srcColorBlendFactor = (VkBlendFactor)
			(RenderStates[VKRS_SRCBLEND] != 0x12345678 ? RenderStates[VKRS_SRCBLEND] : RenderStates[VKRS_MAX + VKRS_SRCBLEND]);
		colorBlendAttachment.dstColorBlendFactor = (VkBlendFactor)
			(RenderStates[VKRS_DESTBLEND] != 0x12345678 ? RenderStates[VKRS_DESTBLEND] : RenderStates[VKRS_MAX + VKRS_DESTBLEND]);
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		if ((RenderStates[VKRS_ALPHABLENDENABLE] == 0x12345678 ? RenderStates[VKRS_ALPHABLENDENABLE + VKRS_MAX] : RenderStates[VKRS_ALPHABLENDENABLE]))
		{
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		}
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		vkCmdSetColorBlendEquationEXT(WWVKRENDER.currentCmd, 0, 1, &colorBlendAttachment);
	}
	{
		vkCmdSetStencilOp(WWVKRENDER.currentCmd, VK_STENCIL_FRONT_AND_BACK,
			(VkStencilOp)(RenderStates[VKRS_STENCILFAIL] != 0x12345678 ? RenderStates[VKRS_STENCILFAIL] : RenderStates[VKRS_MAX + VKRS_STENCILFAIL]),
			(VkStencilOp)(RenderStates[VKRS_STENCILPASS] != 0x12345678 ? RenderStates[VKRS_STENCILPASS] : RenderStates[VKRS_MAX + VKRS_STENCILPASS]),
			(VkStencilOp)(RenderStates[VKRS_STENCILZFAIL] != 0x12345678 ? RenderStates[VKRS_STENCILZFAIL] : RenderStates[VKRS_MAX + VKRS_STENCILZFAIL]),
			(VkCompareOp)(RenderStates[VKRS_STENCILFUNC] != 0x12345678 ? RenderStates[VKRS_STENCILFUNC] : RenderStates[VKRS_MAX + VKRS_STENCILFUNC]));
	}
	{
		VkColorComponentFlags flags[4] = {};
		unsigned int flagCount = 0;
		unsigned int colorMask = (RenderStates[VKRS_COLORWRITEENABLE] == 0x12345678 ? RenderStates[VKRS_COLORWRITEENABLE + VKRS_MAX] : RenderStates[VKRS_COLORWRITEENABLE]);
		if (0x1 & colorMask) { flags[flagCount] |= VK_COLOR_COMPONENT_R_BIT; }
		if (0x2 & colorMask) { flags[flagCount] |= VK_COLOR_COMPONENT_G_BIT; }
		if (0x4 & colorMask) { flags[flagCount] |= VK_COLOR_COMPONENT_B_BIT; }
		if (0x8 & colorMask) { flags[flagCount] |= VK_COLOR_COMPONENT_A_BIT; }
		flagCount = 1;
		if (colorMask == 0x12345678)
		{
			flags[0] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			flagCount = 1;
		}
		vkCmdSetColorWriteMaskEXT(WWVKRENDER.currentCmd, 0, flagCount, flags);
	}
	{
		vkCmdSetDepthBias(WWVKRENDER.currentCmd,
			(RenderStates[VKRS_DEPTHBIAS] == 0x12345678 ? RenderStates[VKRS_DEPTHBIAS + VKRS_MAX] : RenderStates[VKRS_DEPTHBIAS]) * -0.000005f, 0, 0);
	}
	{
		vkCmdSetDepthWriteEnable(WWVKRENDER.currentCmd,
			(RenderStates[VKRS_ZWRITEENABLE] == 0x12345678 ? RenderStates[VKRS_ZWRITEENABLE + VKRS_MAX] : RenderStates[VKRS_ZWRITEENABLE]));
	}
	{
		vkCmdSetDepthCompareOp(WWVKRENDER.currentCmd,
			(VkCompareOp)(RenderStates[VKRS_ZFUNC] == 0x12345678 ? RenderStates[VKRS_ZFUNC + VKRS_MAX] : RenderStates[VKRS_ZFUNC]));
	}
	{
		VkCullModeFlags cull = {};
		switch ((RenderStates[VKRS_CULLMODE] == 0x12345678 ? RenderStates[VKRS_CULLMODE + VKRS_MAX] : RenderStates[VKRS_CULLMODE]))
		{
		case VK_FRONT_FACE_MAX_ENUM: cull = VK_CULL_MODE_NONE; break;
		default:
		case VK_FRONT_FACE_CLOCKWISE: cull = VK_CULL_MODE_BACK_BIT; break;
		case VK_FRONT_FACE_COUNTER_CLOCKWISE: cull = VK_CULL_MODE_FRONT_BIT; break;
		}
		vkCmdSetCullMode(WWVKRENDER.currentCmd, cull);
	}
	{
		vkCmdSetStencilCompareMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK,
			(RenderStates[VKRS_STENCILMASK] == 0x12345678 ? RenderStates[VKRS_STENCILMASK + VKRS_MAX] : RenderStates[VKRS_STENCILMASK]));
	}
	{
		vkCmdSetStencilWriteMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK,
			(RenderStates[VKRS_STENCILWRITEMASK] == 0x12345678 ? RenderStates[VKRS_STENCILWRITEMASK + VKRS_MAX] : RenderStates[VKRS_STENCILWRITEMASK]));
	}
	{
		vkCmdSetStencilReference(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK,
			(RenderStates[VKRS_STENCILREF] == 0x12345678 ? RenderStates[VKRS_STENCILREF + VKRS_MAX] : RenderStates[VKRS_STENCILREF]));
	}
	{
		vkCmdSetStencilTestEnable(WWVKRENDER.currentCmd, RenderStates[VKRS_STENCILENABLE] == 0x12345678 ?
			RenderStates[VKRS_STENCILENABLE + VKRS_MAX] : RenderStates[VKRS_STENCILENABLE]);
	}
}
void DX8Wrapper::Apply_Render_State_Changes()
{
	SNAPSHOT_SAY(("DX8Wrapper::Apply_Render_State_Changes()\n"));

	if (!render_state_changed) 
	{
		ApplyVkStates(RenderStates);
		return;
	}
	if (render_state_changed&SHADER_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply shader\n"));
		render_state.shader.Apply();
	}

	unsigned mask=TEXTURE0_CHANGED;
	int i = 0;
	for (;i< MAX_TEXTURE_STAGES;++i,mask<<=1)
	{
		if (render_state_changed&mask) 
		{
			SNAPSHOT_SAY(("DX8 - apply texture %d (%s)\n",i,render_state.Textures[i] ? render_state.Textures[i]->Get_Full_Path() : "NULL"));

			if (render_state.Textures[i]) 
			{
				render_state.Textures[i]->Apply(i);
			}
			else 
			{
				TextureBaseClass::Apply_Null(i);
			}
		}
	}

	if (render_state_changed&MATERIAL_CHANGED) 
	{
		SNAPSHOT_SAY(("DX8 - apply material\n"));
		VertexMaterialClass* material=const_cast<VertexMaterialClass*>(render_state.material);
		if (material) 
		{
			material->Apply();
		}
		else VertexMaterialClass::Apply_Null();
	}


	if (render_state_changed&LIGHTS_CHANGED)
	{
		unsigned mask=LIGHT0_CHANGED;
		unsigned lightMax = 0;
		for (unsigned index=0;index<4;++index,mask<<=1) {
			if (render_state_changed&mask) {
				SNAPSHOT_SAY(("DX8 - apply light %d\n",index));
				if (render_state.LightEnable[index]) {
#ifdef MESH_RENDER_SNAPSHOT_ENABLED		
					if ( WW3D::Is_Snapshot_Activated() ) {
						LightGeneric * light = &(render_state.Lights.lights[index]);
						static char * _light_types[] = { "Unknown", "Point","Spot", "Directional" };
						WWASSERT((light->Type[0] >= 0) && (light->Type[0] <= 3));

						SNAPSHOT_SAY((" type = %s amb = %4.2f,%4.2f,%4.2f  diff = %4.2f,%4.2f,%4.2f spec = %4.2f, %4.2f, %4.2f\n",
							_light_types[light->Type[0]],
							light->Ambient[0],light->Ambient[1],light->Ambient[2],
							light->Diffuse[0],light->Diffuse[1],light->Diffuse[2],
							light->Specular[0],light->Specular[1],light->Specular[2]));
						SNAPSHOT_SAY((" pos = %f, %f, %f  dir = %f, %f, %f\n",
							light->Position[0], light->Position[1], light->Position[2],
							light->Direction[0], light->Direction[1], light->Direction[2]));
					}
#endif
					if (index == 0)
					{
						LightGeneric* light = &(render_state.Lights.lights[index]);
						light->Ambient[0] = Ambient_Color.X;
						light->Ambient[1] = Ambient_Color.Y;
						light->Ambient[2] = Ambient_Color.Z;
					}
					Set_Light(index,&render_state.Lights.lights[index]);
				}
				else {
					Set_Light(index,NULL);
					SNAPSHOT_SAY((" clearing light to NULL\n"));
				}
			}
		}
		target.PushSingleFrameBuffer(LightUbo);
		render_state.Lights.lightCount[0] = lightMax + 1;
		VkBufferTools::CreateUniformBuffer(&target, sizeof(LightCollection), &render_state.Lights, LightUbo);
	}

	if (render_state_changed&WORLD_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply world matrix\n"));
		_Set_DX8_Transform(VkTS::WORLD,render_state.world);
	}
	if (render_state_changed&VIEW_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply view matrix\n"));
		_Set_DX8_Transform(VkTS::VIEW,render_state.view);
	}
#ifdef INFO_VULKAN
	if (render_state_changed&VERTEX_BUFFER_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply vb change\n"));
		for (i=0;i<MAX_VERTEX_STREAMS;++i) {
			if (render_state.vertex_buffers[i]) {
				switch (render_state.vertex_buffer_types[i]) {//->Type()) {
				case BUFFER_TYPE_DX8:
				case BUFFER_TYPE_DYNAMIC_DX8:
					DX8CALL(SetStreamSource(
						i,
						static_cast<DX8VertexBufferClass*>(render_state.vertex_buffers[i])->Get_DX8_Vertex_Buffer(),
						0,
						render_state.vertex_buffers[i]->FVF_Info().Get_FVF_Size()));
					DX8_RECORD_VERTEX_BUFFER_CHANGE();
					{
						// If the VB format is FVF, set the FVF as a vertex shader
						unsigned fvf=render_state.vertex_buffers[i]->FVF_Info().Get_FVF();
						if (fvf!=0) {
							Set_Vertex_Shader(fvf);
						}
					}
					break;
				case BUFFER_TYPE_SORTING:
				case BUFFER_TYPE_DYNAMIC_SORTING:
					break;
				default:
					WWASSERT(0);
				}
			} else {
				DX8CALL(SetStreamSource(i,NULL, 0, 0));
				DX8_RECORD_VERTEX_BUFFER_CHANGE();
			}
		}
	}
	if (render_state_changed&INDEX_BUFFER_CHANGED) {
		SNAPSHOT_SAY(("DX8 - apply ib change\n"));
		if (render_state.index_buffer) {
			switch (render_state.index_buffer_type) {//->Type()) {
			case BUFFER_TYPE_DX8:
			case BUFFER_TYPE_DYNAMIC_DX8:
				DX8CALL(SetIndices(
					static_cast<DX8IndexBufferClass*>(render_state.index_buffer)->Get_DX8_Index_Buffer()));
				DX8_RECORD_INDEX_BUFFER_CHANGE();
				break;
			case BUFFER_TYPE_SORTING:
			case BUFFER_TYPE_DYNAMIC_SORTING:
				break;
			default:
				WWASSERT(0);
			}
		}
		else {
			DX8CALL(SetIndices(
				NULL));
			DX8_RECORD_INDEX_BUFFER_CHANGE();
		}
	}
#endif

	render_state_changed&=((unsigned)WORLD_IDENTITY|(unsigned)VIEW_IDENTITY);

	ApplyVkStates(RenderStates);
	SNAPSHOT_SAY(("DX8Wrapper::Apply_Render_State_Changes() - finished\n"));
}

#ifdef INFO_VULKAN
VK::Texture DX8Wrapper::_Create_DX8_Texture
(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	VK::Texture texture = {};

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.

	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateTexture(
		DX8Wrapper::_Get_D3D_Device8(),
		width,
		height,
		mip_level_count,
		0,
		WW3DFormat_To_D3DFormat(format),
		pool,
		&texture);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY) {
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets...\n"));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateTexture(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			height,
			mip_level_count,
			0,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture);
		if (SUCCEEDED(ret)) {
			WWDEBUG_SAY(("...Texture creation succesful.\n"));
		}
		else {
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d\n",width,height,format_name,mip_level_count));
		}

	}
	DX8_ErrorCode(ret);
	return texture;
}
#endif

#ifdef INFO_VULKAN
IDirect3DTexture9 * DX8Wrapper::_Create_DX8_Texture
(
	const char *filename,
	MipCountType mip_level_count
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture9 *texture = NULL;

	// NOTE: If the original image format is not supported as a texture format, it will
	// automatically be converted to an appropriate format.
	// NOTE: It is possible to get the size and format of the original image file from this
	// function as well, so if we later want to second-guess D3DX's format conversion decisions
	// we can do so after this function is called..
	unsigned result = D3DXCreateTextureFromFileExA(
		_Get_D3D_Device8(),
		filename,
		D3DX_DEFAULT,
		D3DX_DEFAULT,
		mip_level_count,//create_mipmaps ? 0 : 1,
		0,
		D3DFMT_UNKNOWN,
		D3DPOOL_MANAGED,
		D3DX_FILTER_BOX,
		D3DX_FILTER_BOX,
		0,
		NULL,
		NULL,
		&texture);

	if (result != D3D_OK) {
		return MissingTexture::_Get_Missing_Texture();
	}

	// Make sure texture wasn't paletted!
	D3DSURFACE_DESC desc;
	texture->GetLevelDesc(0,&desc);
	if (desc.Format==D3DFMT_P8) {
		texture->Release();
		return MissingTexture::_Get_Missing_Texture();
	}
	return texture;
}
#endif

#ifdef INFO_VULKAN
IDirect3DTexture9 * DX8Wrapper::_Create_DX8_Texture
(
	IDirect3DSurface9 *surface,
	MipCountType mip_level_count
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture9 *texture = NULL;

	D3DSURFACE_DESC surface_desc;
	::ZeroMemory(&surface_desc, sizeof(D3DSURFACE_DESC));
	surface->GetDesc(&surface_desc);

	// This function will create a texture with a different (but similar) format if the surface is
	// not in a supported texture format.
	WW3DFormat format=D3DFormat_To_WW3DFormat(surface_desc.Format);
	texture = _Create_DX8_Texture(surface_desc.Width, surface_desc.Height, format, mip_level_count);

	// Copy the surface to the texture
	IDirect3DSurface9 *tex_surface = NULL;
	texture->GetSurfaceLevel(0, &tex_surface);
	DX8_ErrorCode(D3DXLoadSurfaceFromSurface(tex_surface, NULL, NULL, surface, NULL, NULL, D3DX_FILTER_BOX, 0));
	tex_surface->Release();

	// Create mipmaps if needed
	if (mip_level_count!=MIP_LEVELS_1) 
	{
		DX8_ErrorCode(D3DXFilterTexture(texture, NULL, 0, D3DX_FILTER_BOX));
	}

	return texture;
}
#endif

/*!
 * KJM create depth stencil texture
 */
#ifdef INFO_VULKAN
IDirect3DTexture9 * DX8Wrapper::_Create_DX8_ZTexture
(
	unsigned int width,
	unsigned int height,
	WW3DZFormat zformat,
	MipCountType mip_level_count
	, D3DPOOL pool
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DTexture9* texture = NULL;

	D3DFORMAT zfmt=WW3DZFormat_To_D3DFormat(zformat);

	unsigned ret=DX8Wrapper::_Get_D3D_Device8()->CreateTexture
	(
		width,
		height,
		mip_level_count,
		D3DUSAGE_DEPTHSTENCIL, 
		zfmt, 
		pool,
		&texture, nullptr
	);

	if (ret==D3DERR_NOTAVAILABLE) 
	{
		Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
		return NULL;
	}

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY) 
	{
		WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets...\n"));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=DX8Wrapper::_Get_D3D_Device8()->CreateTexture
		(
			width,
			height,
			mip_level_count,
			D3DUSAGE_DEPTHSTENCIL, 
			zfmt, 
			pool,
			&texture, nullptr
		);

		if (SUCCEEDED(ret)) 
		{
			WWDEBUG_SAY(("...Render target creation succesful.\n"));
		}
		else 
		{
			WWDEBUG_SAY(("...Render target creation failed.\n"));
		}
		if (ret==D3DERR_OUTOFVIDEOMEMORY) 
		{
			Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
			return NULL;
		}
	}

	DX8_ErrorCode(ret);

	texture->AddRef(); // don't release this texture

	// Just return the texture, no reduction
	// allowed for render targets.

	return texture;
}
#endif

#ifdef INFO_VULKAN
/*!
 * KJM create cube map texture
 */
IDirect3DCubeTexture9* DX8Wrapper::_Create_DX8_Cube_Texture
(
	unsigned int width,
	unsigned int height,
	WW3DFormat format,
	MipCountType mip_level_count,
	bool rendertarget
)
{
	WWASSERT(width==height);
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DCubeTexture9* texture=NULL;

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.

	// Render target may return NOTAVAILABLE, in
	// which case we return NULL.
	if (rendertarget) 
	{
		unsigned ret=D3DXCreateCubeTexture
		(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			mip_level_count,
			D3DUSAGE_RENDERTARGET,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture
		);

		if (ret==D3DERR_NOTAVAILABLE) 
		{
			Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
			return NULL;
		}

		// If ran out of texture ram, try invalidating some textures and mesh cache.
		if (ret==D3DERR_OUTOFVIDEOMEMORY) 
		{
			WWDEBUG_SAY(("Error: Out of memory while creating render target. Trying to release assets...\n"));
			// Free all textures that haven't been used in the last 5 seconds
			TextureClass::Invalidate_Old_Unused_Textures(5000);

			// Invalidate the mesh cache
			WW3D::_Invalidate_Mesh_Cache();

			ret=D3DXCreateCubeTexture
			(
				DX8Wrapper::_Get_D3D_Device8(),
				width,
				mip_level_count,
				D3DUSAGE_RENDERTARGET,
				WW3DFormat_To_D3DFormat(format),
				pool,
				&texture
			);

			if (SUCCEEDED(ret))
			{
				WWDEBUG_SAY(("...Render target creation succesful.\n"));
			}
			else 
			{
				WWDEBUG_SAY(("...Render target creation failed.\n"));
			}
			if (ret==D3DERR_OUTOFVIDEOMEMORY) 
			{
				Non_Fatal_Log_DX8_ErrorCode(ret,__FILE__,__LINE__);
				return NULL;
			}
		}

		DX8_ErrorCode(ret);
		// Just return the texture, no reduction
		// allowed for render targets.
		return texture;
	}

	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateCubeTexture
	(
		DX8Wrapper::_Get_D3D_Device8(),
		width,
		mip_level_count,
		0,
		WW3DFormat_To_D3DFormat(format),
		pool,
		&texture
	);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY) 
	{
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets...\n"));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateCubeTexture
		(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			mip_level_count,
			0,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture
		);
		if (SUCCEEDED(ret)) 
		{
			WWDEBUG_SAY(("...Texture creation succesful.\n"));
		}
		else 
		{
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d\n",width,height,format_name,mip_level_count));
		}

	}
	DX8_ErrorCode(ret);

	return texture;
}

/*!
 * KJM create volume texture
 */
IDirect3DVolumeTexture9* DX8Wrapper::_Create_DX8_Volume_Texture
(
	unsigned int width,
	unsigned int height,
	unsigned int depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	D3DPOOL pool
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	IDirect3DVolumeTexture9* texture=NULL;

	// Paletted textures not supported!
	WWASSERT(format!=D3DFMT_P8);

	// NOTE: If 'format' is not supported as a texture format, this function will find the closest
	// format that is supported and use that instead.


	// We should never run out of video memory when allocating a non-rendertarget texture.
	// However, it seems to happen sometimes when there are a lot of textures in memory and so
	// if it happens we'll release assets and try again (anything is better than crashing).
	unsigned ret=D3DXCreateVolumeTexture
	(
		DX8Wrapper::_Get_D3D_Device8(),
		width,
		height,
		depth,
		mip_level_count,
		0,
		WW3DFormat_To_D3DFormat(format),
		pool,
		&texture
	);

	// If ran out of texture ram, try invalidating some textures and mesh cache.
	if (ret==D3DERR_OUTOFVIDEOMEMORY) 
	{
		WWDEBUG_SAY(("Error: Out of memory while creating texture. Trying to release assets...\n"));
		// Free all textures that haven't been used in the last 5 seconds
		TextureClass::Invalidate_Old_Unused_Textures(5000);

		// Invalidate the mesh cache
		WW3D::_Invalidate_Mesh_Cache();

		ret=D3DXCreateVolumeTexture
		(
			DX8Wrapper::_Get_D3D_Device8(),
			width,
			height,
			depth,
			mip_level_count,
			0,
			WW3DFormat_To_D3DFormat(format),
			pool,
			&texture
		);
		if (SUCCEEDED(ret)) 
		{
			WWDEBUG_SAY(("...Texture creation succesful.\n"));
		}
		else 
		{
			StringClass format_name(0,true);
			Get_WW3D_Format_Name(format, format_name);
			WWDEBUG_SAY(("...Texture creation failed. (%d x %d, format: %s, mips: %d\n",width,height,format_name,mip_level_count));
		}

	}
	DX8_ErrorCode(ret);

	return texture;
}
#endif


VK::Surface DX8Wrapper::_Create_DX8_Surface(unsigned int width, unsigned int height, WW3DFormat format)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();

#ifdef INFO_VULKAN
	IDirect3DSurface9 *surface = NULL;

	// Paletted surfaces not supported!
	WWASSERT(format!=D3DFMT_P8);

	DX8CALL(CreateOffscreenPlainSurface(width, height, WW3DFormat_To_D3DFormat(format), D3DPOOL_SYSTEMMEM, &surface, nullptr));

	return surface;
#else
	VK::Surface ret = {};
	ret.width = width;
	ret.height = height;
	ret.format = WW3DFormat_To_D3DFormat(format);
	ret.buffer.resize(width * height * VK::SizeOfFormat(ret.format));
	return ret;
#endif
}

VK::Surface DX8Wrapper::_Create_DX8_Surface(const char *filename_)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();

	// Note: Since there is no "D3DXCreateSurfaceFromFile" and no "GetSurfaceInfoFromFile" (the
	// latter is supposed to be added to D3DX in a future version), we create a texture from the
	// file (w/o mipmaps), check that its surface is equal to the original file data (which it
	// will not be if the file is not in a texture-supported format or size). If so, copy its
	// surface (we might be able to just get its surface and add a ref to it but I'm not sure so
	// I'm not going to risk it) and release the texture. If not, create a surface according to
	// the file data and use D3DXLoadSurfaceFromFile. This is a horrible hack, but it saves us
	// having to write file loaders. Will fix this when D3DX provides us with the right functions.
	// Create a surface the size of the file image data
#ifdef INFO_VULKAN
	IDirect3DSurface9 *surface = NULL;
#endif

	{

		file_auto_ptr myfile(_TheFileFactory,filename_);
		// If file not found, create a surface with missing texture in it

		if (!myfile->Is_Available()) {
			// If file not found, try the dds format
			// else create a surface with missing texture in it
			char compressed_name[200];
			strncpy(compressed_name,filename_, 200);
			char *ext = strstr(compressed_name, ".");
			if ( (strlen(ext)==4) && 
				  ( (ext[1] == 't') || (ext[1] == 'T') ) && 
				  ( (ext[2] == 'g') || (ext[2] == 'G') ) && 
				  ( (ext[3] == 'a') || (ext[3] == 'A') ) ) {
				ext[1]='d';
				ext[2]='d';
				ext[3]='s';
			}
			file_auto_ptr myfile2(_TheFileFactory,compressed_name);
			if (!myfile2->Is_Available())
				return MissingTexture::_Create_Missing_Surface();
		}
	}

	StringClass filename_string(filename_,true);
	VK::Surface surface=TextureLoader::Load_Surface_Immediate(
		filename_string,
		WW3D_FORMAT_UNKNOWN,
		true);
	return surface;
}


/***********************************************************************************************
 * DX8Wrapper::_Update_Texture -- Copies a texture from system memory to video memory          *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   4/26/2001  hy : Created.                                                                  *
 *=============================================================================================*/
void DX8Wrapper::_Update_Texture(TextureClass *system, TextureClass *video)
{
	WWASSERT(system);
	WWASSERT(video);
#ifdef TODO_VULKAN
	DX8CALL(UpdateTexture(system->Peek_D3D_Base_Texture(),video->Peek_D3D_Base_Texture()));
#endif
}

void DX8Wrapper::Compute_Caps(WW3DFormat display_format)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
#ifdef INFO_VULKAN
	delete CurrentCaps;
	CurrentCaps=new DX8Caps(_Get_D3D8(),D3DDevice,display_format,Get_Current_Adapter_Identifier());
#endif
}


void DX8Wrapper::Set_Light(unsigned index, const LightGeneric* light)
{
	if (light) {
		render_state.Lights.lights[index]=*light;
		render_state.LightEnable[index]=true;
	}
	else {
		render_state.Lights.lights[index].Type[0] = 99;
		render_state.LightEnable[index]=false;
	}
	render_state_changed|=(LIGHT0_CHANGED<<index);
}

void DX8Wrapper::Set_Light(unsigned index,const LightClass &light)
{
	LightGeneric dlight = {};
	Vector3 temp;

	dlight.Type[0] = (uint32_t)light.Get_Type();

	light.Get_Diffuse(&temp);
	temp*=light.Get_Intensity();
	dlight.Diffuse[0] = temp.X;
	dlight.Diffuse[1] =temp.Y;
	dlight.Diffuse[2] =temp.Z;
	dlight.Diffuse[3] =1.0f;

	light.Get_Specular(&temp);
	temp*=light.Get_Intensity();
	dlight.Specular[0]=temp.X;
	dlight.Specular[1]=temp.Y;
	dlight.Specular[2]=temp.Z;
	dlight.Specular[3]=1.0f;

	light.Get_Ambient(&temp);
	temp*=light.Get_Intensity();
	dlight.Ambient[0]=temp.X;
	dlight.Ambient[1]=temp.Y;
	dlight.Ambient[2]=temp.Z;
	dlight.Ambient[3]=1.0f;

	temp=light.Get_Position();
	memcpy(dlight.Position, &temp, sizeof(float) * 3);

	light.Get_Spot_Direction(temp);
	memcpy(dlight.Direction, &temp, sizeof(float) * 3);

	dlight.Range[0] = light.Get_Attenuation_Range();
	dlight.Falloff[0] =light.Get_Spot_Exponent();
	dlight.Theta[0] =light.Get_Spot_Angle();
	dlight.Phi[0] =light.Get_Spot_Angle();

	// Inverse linear light 1/(1+D)
	double a,b;
	light.Get_Far_Attenuation_Range(a,b);
	dlight.Attenuation0[0] =1.0f;
	if (fabs(a-b)<1e-5)
		// if the attenuation range is too small assume uniform with cutoff
		dlight.Attenuation1[0] =0.0f;
	else
		// this will cause the light to drop to half intensity at the first far attenuation
		dlight.Attenuation1[0] =(float) 1.0/a;
	dlight.Attenuation2[0] =0.0f;

	Set_Light(index,&dlight);
}

VK::Buffer DX8Wrapper::UboLight()
{
	return LightUbo;
}

VK::Buffer DX8Wrapper::UboMaterial()
{
	return MaterialUbo;
}

//**********************************************************************************************
//! Set the light environment. This is a lighting model which used up to four
//! directional lights to produce the lighting.
/*! 5/27/02 KJM Added shader light environment support
*/
void DX8Wrapper::Set_Light_Environment(LightEnvironmentClass* light_env)
{
	// Shader light environment support															*
//	if (Light_Environment && light_env && (*Light_Environment)==(*light_env)) return;

	Light_Environment=light_env;

	if (light_env) 
	{
		int light_count = light_env->Get_Light_Count();
		unsigned int color=Convert_Color(light_env->Get_Equivalent_Ambient(),0.0f);
		if (RenderStates[VKRS_AMBIENT]!=color)
		{
			Set_DX8_Render_State(VKRS_AMBIENT,color);
//buggy Radeon 9700 driver doesn't apply new ambient unless the material also changes.
#if 1
			render_state_changed|=MATERIAL_CHANGED;
#endif
		}

		LightGeneric light;		
		int l = 0;
		for (;l<light_count;++l) {
			
			::ZeroMemory(&light, sizeof(LightGeneric));
			
			light.Type[0]  = (uint32_t)LightClass::DIRECTIONAL;
			(Vector3&)light.Diffuse=light_env->Get_Light_Diffuse(l);
			Vector3 dir=-light_env->Get_Light_Direction(l);
			memcpy(light.Direction, &dir, sizeof(Vector3));

			// (gth) TODO: put specular into LightEnvironment?  Much work to be done on lights :-)'
			if (l==0) {
				light.Specular[0] = light.Specular[1] = light.Specular[2] = 1.0f;
			}

			if (light_env->isPointLight(l)) {
				light.Type[0] = (uint32_t)LightClass::POINT;
				(Vector3&)light.Diffuse=light_env->getPointDiffuse(l);
				(Vector3&)light.Ambient=light_env->getPointAmbient(l);
				memcpy(light.Position, &light_env->getPointCenter(l), sizeof(Vector3));
				light.Range[0] = light_env->getPointOrad(l);
				
				// Inverse linear light 1/(1+D)
				double a,b;
				b = light_env->getPointOrad(l);
				a = light_env->getPointIrad(l);

//(gth) CNC3 Generals code for the attenuation factors is causing the lights to over-brighten
//I'm changing the Attenuation0 parameter to 1.0 to avoid this problem.				
#if 0
				light.Attenuation0=0.01f;
#else
				light.Attenuation0[0] = 1.0f;
#endif
				if (fabs(a-b)<1e-5)
					// if the attenuation range is too small assume uniform with cutoff
					light.Attenuation1[0] =0.0f;
				else
					// this will cause the light to drop to half intensity at the first far attenuation
					light.Attenuation1[0] =(float) 0.1/a;
	
				light.Attenuation2[0] =8.0f/(b*b);
			}

			Set_Light(l,&light);
		}

		for (;l<4;++l) {
			Set_Light(l,NULL);
		}
	}
/*	else {
		for (int l=0;l<4;++l) {
			Set_Light(l,NULL);
		}
	}
*/
}

#ifdef INFO_VULKAN
IDirect3DSurface9 * DX8Wrapper::_Get_DX8_Front_Buffer()
{
	DX8_THREAD_ASSERT();
	D3DDISPLAYMODE mode;

	DX8CALL(GetDisplayMode(0, &mode));

	IDirect3DSurface9 * fb=NULL;

	DX8CALL(CreateOffscreenPlainSurface(mode.Width, mode.Height, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &fb, nullptr));

	DX8CALL(GetFrontBufferData(0, fb));
	return fb;
}
#endif
#ifdef INFO_VULKAN
SurfaceClass * DX8Wrapper::_Get_DX8_Back_Buffer(unsigned int num)
{
	DX8_THREAD_ASSERT();

	SurfaceClass *surf=NULL;
	IDirect3DSurface9 * bb;
	DX8CALL(GetBackBuffer(0, num,D3DBACKBUFFER_TYPE_MONO,&bb));
	if (bb)
	{
		surf=NEW_REF(SurfaceClass,(bb));
		bb->Release();
	}
	return surf;
}
#endif


TextureClass *
DX8Wrapper::Create_Render_Target (int width, int height, WW3DFormat format)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	number_of_DX8_calls++;

#ifdef INFO_VULKAN
	// Use the current display format if format isn't specified
	if (format==WW3D_FORMAT_UNKNOWN) {
		D3DDISPLAYMODE mode;
		DX8CALL(GetDisplayMode(0,&mode));
		format=D3DFormat_To_WW3DFormat(mode.Format);
	}
#endif

	// If render target format isn't supported return NULL
#ifdef INFO_VULKAN
	if (!Get_Current_Caps()->Support_Render_To_Texture_Format(format)) 
#endif
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target format is not supported\r\n"));
		return NULL;
	}

	//
	//	Note: We're going to force the width and height to be powers of two and equal
	//
#ifdef TODO_VULKAN
	const D3DCAPS9& dx8caps=Get_Current_Caps()->Get_DX8_Caps();
	float poweroftwosize = width;
	if (height > 0 && height < width) {
		poweroftwosize = height;
	}
	poweroftwosize = ::Find_POT (poweroftwosize);

	if (poweroftwosize>dx8caps.MaxTextureWidth) {
		poweroftwosize=dx8caps.MaxTextureWidth;
	}
	if (poweroftwosize>dx8caps.MaxTextureHeight) {
		poweroftwosize=dx8caps.MaxTextureHeight;
	}

	width = height = poweroftwosize;

	//
	//	Attempt to create the render target
	//
#endif
	TextureClass * tex = NEW_REF(TextureClass,(width,height,format,MIP_LEVELS_1,true));

	// 3dfx drivers are lying in the CheckDeviceFormat call and claiming
	// that they support render targets!
	if (tex->Peek_D3D_Base_Texture().image == NULL) 
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target creation failed!\r\n"));
		REF_PTR_RELEASE(tex);
	}

	return tex;
}

//**********************************************************************************************
//! Create render target with associated depth stencil buffer
/*! KJM
*/
void DX8Wrapper::Create_Render_Target
(
	int width, 
	int height, 
	WW3DFormat format,
	WW3DZFormat zformat,
	TextureClass** target,
	ZTextureClass** depth_buffer
)
{
	DX8_THREAD_ASSERT();
	DX8_Assert();
	number_of_DX8_calls++;

	// Use the current display format if format isn't specified
	if (format==WW3D_FORMAT_UNKNOWN) 
	{
		*target=NULL;
		*depth_buffer=NULL;
		return;
/*		D3DDISPLAYMODE mode;
		DX8CALL(GetDisplayMode(&mode));
		format=D3DFormat_To_WW3DFormat(mode.Format);*/
	}

	// If render target format isn't supported return NULL
#ifdef INFO_VULKAN
	if (!Get_Current_Caps()->Support_Render_To_Texture_Format(format) ||
		 !Get_Current_Caps()->Support_Depth_Stencil_Format(zformat)) 
#endif
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target with depth format is not supported\r\n"));
		return;
	}

	//	Note: We're going to force the width and height to be powers of two and equal
#ifdef TODO_VULKAN
	const D3DCAPS9& dx8caps=Get_Current_Caps()->Get_DX8_Caps();
	float poweroftwosize = width;
	if (height > 0 && height < width) 
	{
		poweroftwosize = height;
	}
	poweroftwosize = ::Find_POT (poweroftwosize);

	if (poweroftwosize>dx8caps.MaxTextureWidth) 
	{
		poweroftwosize=dx8caps.MaxTextureWidth;
	}

	if (poweroftwosize>dx8caps.MaxTextureHeight) 
	{
		poweroftwosize=dx8caps.MaxTextureHeight;
	}

	width = height = poweroftwosize;

	//	Attempt to create the render target
	TextureClass* tex=NEW_REF(TextureClass,(width,height,format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT,true));

	// 3dfx drivers are lying in the CheckDeviceFormat call and claiming
	// that they support render targets!
	if (tex->Peek_D3D_Base_Texture() == NULL) 
	{
		WWDEBUG_SAY(("DX8Wrapper - Render target creation failed!\r\n"));
		REF_PTR_RELEASE(tex);
	}

	*target=tex;

	// attempt to create the depth stencil buffer
	*depth_buffer=NEW_REF
	(
		ZTextureClass,
		(
			width,
			height,
			zformat,
			MIP_LEVELS_1,
			TextureClass::POOL_DEFAULT
		)
	);
#endif
}

/*!
 * Set render target
 * KM Added optional custom z target
 */
void DX8Wrapper::Set_Render_Target_With_Z
(
	TextureClass* texture,
	ZTextureClass* ztexture
)
{
#ifdef TODO_VULKAN
	WWASSERT(texture!=NULL);
	IDirect3DSurface9 * d3d_surf = texture->Get_D3D_Surface_Level();
	WWASSERT(d3d_surf != NULL);

	IDirect3DSurface9* d3d_zbuf=NULL;
	if (ztexture!=NULL)
	{

		d3d_zbuf=ztexture->Get_D3D_Surface_Level();
		WWASSERT(d3d_zbuf!=NULL);
		Set_Render_Target(d3d_surf,d3d_zbuf);
		d3d_zbuf->Release();
	}
	else
	{
		Set_Render_Target(d3d_surf,true);
	}
	d3d_surf->Release();
#endif
	VkExtent2D extent = { (uint32_t)texture->Get_Width(), (uint32_t)texture->Get_Height() };
	target.BeginFramebuffer(extent, WW3DFormat_To_D3DFormat( texture->Get_Texture_Format()));
	auto& currFbo = target.singleFrame[target.currentFrame].fbos.back();
	texture->Set_D3D_Base_Texture(currFbo.image);
	currFbo.image = {};
	if (ztexture)
	{
		ztexture->Set_D3D_Base_Texture(currFbo.depth);
		currFbo.depth = {};
	}
	IsRenderToTexture = true;
}


void
DX8Wrapper::Set_Render_Target()
{
#ifdef TODO_VULKAN
//#ifndef _XBOX
	DX8_THREAD_ASSERT();
	DX8_Assert();

	//
	//	Should we restore the default render target set a new one?
	//
	if (render_target == NULL || render_target == DefaultRenderTarget) 
	{
		// If there is currently a custom render target, default must NOT be NULL.
		if (CurrentRenderTarget) 
		{
			WWASSERT(DefaultRenderTarget!=NULL);
		}

		//
		//	Restore the default render target
		//
		if (DefaultRenderTarget != NULL) 
		{
			DX8CALL(SetRenderTarget (1, DefaultRenderTarget));
			DX8CALL(SetDepthStencilSurface(DefaultDepthBuffer));
			DefaultRenderTarget->Release ();
			DefaultRenderTarget = NULL;
			if (DefaultDepthBuffer) 
			{
				DefaultDepthBuffer->Release ();
				DefaultDepthBuffer = NULL;
			}
		}

		//
		//	Release our hold on the "current" render target
		//
		if (CurrentRenderTarget != NULL) 
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = NULL;
		}

		if (CurrentDepthBuffer!=NULL)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=NULL;
		}

	} 
	else if (render_target != CurrentRenderTarget) 
	{
		WWASSERT(DefaultRenderTarget==NULL);

		//
		//	We'll need the depth buffer later...
		//
		if (DefaultDepthBuffer == NULL) 
		{
//		IDirect3DSurface9 *depth_buffer = NULL;
			DX8CALL(GetDepthStencilSurface (&DefaultDepthBuffer));
		}

		//
		//	Get a pointer to the default render target (if necessary)
		//
		if (DefaultRenderTarget == NULL) 
		{
			DX8CALL(GetRenderTarget (1,&DefaultRenderTarget));
		}

		//
		//	Release our hold on the old "current" render target
		//
		if (CurrentRenderTarget != NULL) 
		{
			CurrentRenderTarget->Release ();
			CurrentRenderTarget = NULL;
		}

		if (CurrentDepthBuffer!=NULL)
		{
			CurrentDepthBuffer->Release();
			CurrentDepthBuffer=NULL;
		}

		//
		//	Keep a copy of the current render target (for housekeeping)
		//
		CurrentRenderTarget = render_target;
		WWASSERT (CurrentRenderTarget != NULL);
		if (CurrentRenderTarget != NULL) 
		{
			CurrentRenderTarget->AddRef ();

			//
			//	Switch render targets
			//
			if (use_default_depth_buffer) 
			{
				DX8CALL(SetRenderTarget (1, CurrentRenderTarget));
				DX8CALL(SetDepthStencilSurface(DefaultDepthBuffer));
			}
			else 
			{
				DX8CALL(SetRenderTarget (1, CurrentRenderTarget));
			}
		}
	}

	//
	//	Free our hold on the depth buffer
	//
//	if (depth_buffer != NULL) {
//		depth_buffer->Release ();
//		depth_buffer = NULL;
//	}

#endif
	IsRenderToTexture = false;
	return ;
//#endif // XBOX
}


//**********************************************************************************************
//! Set render target with depth stencil buffer
/*! KJM
*/
#ifdef INFO_VULKAN
void DX8Wrapper::Flush_DX8_Resource_Manager(unsigned int bytes)
{
	DX8_Assert();
	DX8CALL(EvictManagedResources());
}
#endif

unsigned int DX8Wrapper::Get_Free_Texture_RAM()
{
	DX8_Assert();
	number_of_DX8_calls++;
#ifdef INFO_VULKAN
	return DX8Wrapper::_Get_D3D_Device8()->GetAvailableTextureMem();
#else
	return 0;
#endif
}

// Converts a linear gamma ramp to one that is controlled by:
// Gamma - controls the curvature of the middle of the curve
// Bright - controls the minimum value of the curve
// Contrast - controls the difference between the maximum and the minimum of the curve
void DX8Wrapper::Set_Gamma(float gamma,float bright,float contrast,bool calibrate,bool uselimit)
{
	gamma=Bound(gamma,0.6f,6.0f);
	bright=Bound(bright,-0.5f,0.5f);
	contrast=Bound(contrast,0.5f,2.0f);
	float oo_gamma=1.0f/gamma;

	DX8_Assert();
	number_of_DX8_calls++;

#ifdef INFO_VULKAN
	DWORD flag=(calibrate?D3DSGR_CALIBRATE:D3DSGR_NO_CALIBRATION);
#endif
	struct GammaRamp
	{
		uint16_t red[256], green[256], blue[256];
	};
	GammaRamp ramp;
	float			 limit;	

	// IML: I'm not really sure what the intent of the 'limit' variable is. It does not produce useful results for my purposes.
	if (uselimit) {
		limit=(contrast-1)/2*contrast;
	} else {
		limit = 0.0f;
	}

	// HY - arrived at this equation after much trial and error.
	for (int i=0; i<256; i++) {
		float in,out;
		in=i/256.0f;
		float x=in-limit;
		x=Bound(x,0.0f,1.0f);
		x=powf(x,oo_gamma);
		out=contrast*x+bright;
		out=Bound(out,0.0f,1.0f);
		ramp.red[i]=(WORD) (out*65535);
		ramp.green[i]=(WORD) (out*65535);
		ramp.blue[i]=(WORD) (out*65535);
	}

#ifdef INFO_VULKAN
	if (Get_Current_Caps()->Support_Gamma())	{
		DX8Wrapper::_Get_D3D_Device8()->SetGammaRamp(0,flag,&ramp);
	} else 
#endif
	{
		HWND hwnd = GetDesktopWindow();
		HDC hdc = GetDC(hwnd);
		if (hdc)
		{
			SetDeviceGammaRamp (hdc, &ramp);
			ReleaseDC (hwnd, hdc);
		}
	}
}

//**********************************************************************************************
//! Resets render device to default state
/*!
*/
void DX8Wrapper::Apply_Default_State()
{
	SNAPSHOT_SAY(("DX8Wrapper::Apply_Default_State()\n"));

	// only set states used in game
	Set_DX8_Render_State(VKRS_ZWRITEENABLE, TRUE);
	Set_DX8_Render_State(VKRS_ALPHATESTENABLE, FALSE);
	//Set_DX8_Render_State(VKRS_LASTPIXEL, FALSE);
	Set_DX8_Render_State(VKRS_SRCBLEND, VK_BLEND_FACTOR_ONE);
	Set_DX8_Render_State(VKRS_DESTBLEND, VK_BLEND_FACTOR_ZERO);
	Set_DX8_Render_State(VKRS_CULLMODE, VK_FRONT_FACE_CLOCKWISE);
	Set_DX8_Render_State(VKRS_ZFUNC, VK_COMPARE_OP_LESS_OR_EQUAL);
	Set_DX8_Render_State(VKRS_ALPHAREF, 0);
	Set_DX8_Render_State(VKRS_ALPHAFUNC, VK_COMPARE_OP_LESS_OR_EQUAL);
	Set_DX8_Render_State(VKRS_ALPHABLENDENABLE, FALSE);
	Set_DX8_Render_State(VKRS_FOGENABLE, FALSE);
	Set_DX8_Render_State(VKRS_SPECULARENABLE, FALSE);
	Set_DX8_Render_State(VKRS_DEPTHBIAS, 0);
//	Set_DX8_Render_State(VKRS_RANGEFOGENABLE, FALSE);
	Set_DX8_Render_State(VKRS_STENCILENABLE, FALSE);
	Set_DX8_Render_State(VKRS_STENCILFAIL, VK_STENCIL_OP_KEEP);
	Set_DX8_Render_State(VKRS_STENCILZFAIL, VK_STENCIL_OP_KEEP);
	Set_DX8_Render_State(VKRS_STENCILPASS, VK_STENCIL_OP_KEEP);
	Set_DX8_Render_State(VKRS_STENCILFUNC, VK_COMPARE_OP_ALWAYS);
	Set_DX8_Render_State(VKRS_STENCILREF, 0);
	Set_DX8_Render_State(VKRS_STENCILMASK, 0xffffffff);
	Set_DX8_Render_State(VKRS_STENCILWRITEMASK, 0xffffffff);
	Set_DX8_Render_State(VKRS_TEXTUREFACTOR, 0);
	Set_DX8_Render_State(VKRS_LIGHTING, FALSE);
	Set_DX8_Render_State(VKRS_COLORWRITEENABLE, 0x0000000f);
	//Set_DX8_Render_State(VKRS_TWEENFACTOR, 0);
	Set_DX8_Render_State(VKRS_BLENDOP, VK_BLEND_OP_ADD);
	//Set_DX8_Render_State(VKRS_POSITIONORDER, D3DORDER_CUBIC);
	//Set_DX8_Render_State(VKRS_NORMALORDER, D3DORDER_LINEAR);

	// disable TSS stages
	int i;
	for (i=0; i< MAX_TEXTURE_STAGES; i++)
	{
		Set_DX8_Texture_Stage_State(i, VKTSS_COLOROP, VKTOP_DISABLE);
		Set_DX8_Texture_Stage_State(i, VKTSS_COLORARG1, VKTA_TEXTURE);
		Set_DX8_Texture_Stage_State(i, VKTSS_COLORARG2, VKTA_DIFFUSE);

		Set_DX8_Texture_Stage_State(i, VKTSS_ALPHAOP, VKTOP_DISABLE);
		Set_DX8_Texture_Stage_State(i, VKTSS_ALPHAARG1, VKTA_TEXTURE);
		Set_DX8_Texture_Stage_State(i, VKTSS_ALPHAARG2, VKTA_DIFFUSE);
		Set_DX8_Texture_Stage_State(i, VKTSS_TEXCOORDINDEX, i);
		

		Set_DX8_Sampler_Stage_State(i, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		Set_DX8_Sampler_Stage_State(i, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		Set_DX8_Texture_Stage_State(i, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
		Set_Texture(i,NULL);
	}

//	DX8Wrapper::Set_Material(NULL);
	VertexMaterialClass::Apply_Null();

	for (unsigned index=0;index<4;++index) {
		SNAPSHOT_SAY(("Clearing light %d to NULL\n",index));
		Set_Light(index,NULL);
	}

	// set up simple default TSS 
	Vector4 vconst[MAX_VERTEX_SHADER_CONSTANTS];
	memset(vconst,0,sizeof(Vector4)*MAX_VERTEX_SHADER_CONSTANTS);
	Set_Vertex_Shader_Constant(0, vconst, MAX_VERTEX_SHADER_CONSTANTS);

	Vector4 pconst[MAX_PIXEL_SHADER_CONSTANTS];
	memset(pconst,0,sizeof(Vector4)*MAX_PIXEL_SHADER_CONSTANTS);
	Set_Pixel_Shader_Constant(0, pconst, MAX_PIXEL_SHADER_CONSTANTS);

#ifdef INFO_VULKAN
	Set_Vertex_Shader(DX8_FVF_XYZNDUV2);
	Set_Pixel_Shader(0);
#endif
	Set_Pipeline(PIPELINE_WWVK_MAX);
	ShaderClass::Invalidate();

	if (!WWVKRENDER.currentCmd)
		return;

	{
		VkColorBlendEquationEXT colorBlendAttachment = {};
		colorBlendAttachment.srcColorBlendFactor = (VkBlendFactor)
			(RenderStates[VKRS_SRCBLEND] != 0x12345678 ? RenderStates[VKRS_SRCBLEND] : RenderStates[VKRS_MAX + VKRS_SRCBLEND]);
		colorBlendAttachment.dstColorBlendFactor = (VkBlendFactor)
			(RenderStates[VKRS_DESTBLEND] != 0x12345678 ? RenderStates[VKRS_DESTBLEND] : RenderStates[VKRS_MAX + VKRS_DESTBLEND]);
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		if (RenderStates[VKRS_ALPHABLENDENABLE])
		{
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		}
		else
		{
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		}
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		vkCmdSetColorBlendEquationEXT(WWVKRENDER.currentCmd, 0, 1, &colorBlendAttachment);
	}
	{
		vkCmdSetStencilOp(WWVKRENDER.currentCmd, VK_STENCIL_FRONT_AND_BACK,
			(VkStencilOp)(RenderStates[VKRS_STENCILFAIL] != 0x12345678 ? RenderStates[VKRS_STENCILFAIL] : RenderStates[VKRS_MAX + VKRS_STENCILFAIL]),
			(VkStencilOp)(RenderStates[VKRS_STENCILPASS] != 0x12345678 ? RenderStates[VKRS_STENCILPASS] : RenderStates[VKRS_MAX + VKRS_STENCILPASS]),
			(VkStencilOp)(RenderStates[VKRS_STENCILZFAIL] != 0x12345678 ? RenderStates[VKRS_STENCILZFAIL] : RenderStates[VKRS_MAX + VKRS_STENCILZFAIL]),
			(VkCompareOp)(RenderStates[VKRS_STENCILFUNC] != 0x12345678 ? RenderStates[VKRS_STENCILFUNC] : RenderStates[VKRS_MAX + VKRS_STENCILFUNC]));
	}
	{
		VkColorComponentFlags flags[4] = {};
		unsigned int flagCount = 0;
		if (0x1 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_R_BIT; }
		if (0x2 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_G_BIT; }
		if (0x4 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_B_BIT; }
		if (0x8 & RenderStates[VKRS_COLORWRITEENABLE]) { flags[flagCount] |= VK_COLOR_COMPONENT_A_BIT; }
		flagCount = 1;
		if (RenderStates[VKRS_COLORWRITEENABLE] == 0x12345678)
		{
			flags[0] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			flagCount = 1;
		}
		vkCmdSetColorWriteMaskEXT(target.currentCmd, 0, flagCount, flags);
	}
	{
		vkCmdSetDepthBias(target.currentCmd, RenderStates[VKRS_DEPTHBIAS] * -0.000005f, -1, 0);
	}
	{
		vkCmdSetDepthWriteEnable(target.currentCmd, RenderStates[VKRS_ZWRITEENABLE]);
	}
	{
		vkCmdSetDepthCompareOp(target.currentCmd, (VkCompareOp)RenderStates[VKRS_ZFUNC]);
	}
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
	{
		vkCmdSetStencilCompareMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILMASK]);
	}
	{
		vkCmdSetStencilWriteMask(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILWRITEMASK]);
	}
	{
		vkCmdSetStencilReference(WWVKRENDER.currentCmd, VK_STENCIL_FACE_FRONT_AND_BACK, RenderStates[VKRS_STENCILREF]);
	}
	{
		vkCmdSetStencilTestEnable(WWVKRENDER.currentCmd, RenderStates[VKRS_STENCILENABLE]);
	}
}

#ifdef INFO_VULKAN
const char* DX8Wrapper::Get_DX8_Render_State_Name(D3DRENDERSTATETYPE state)
{
	switch (state) {
	case VKRS_ZENABLE                       : return "VKRS_ZENABLE";
	case VKRS_FILLMODE                      : return "VKRS_FILLMODE";
	case VKRS_SHADEMODE                     : return "VKRS_SHADEMODE";
	//case VKRS_LINEPATTERN                   : return "VKRS_LINEPATTERN";
	case VKRS_ZWRITEENABLE                  : return "VKRS_ZWRITEENABLE";
	case VKRS_ALPHATESTENABLE               : return "VKRS_ALPHATESTENABLE";
	case VKRS_LASTPIXEL                     : return "VKRS_LASTPIXEL";
	case VKRS_SRCBLEND                      : return "VKRS_SRCBLEND";
	case VKRS_DESTBLEND                     : return "VKRS_DESTBLEND";
	case VKRS_CULLMODE                      : return "VKRS_CULLMODE";
	case VKRS_ZFUNC                         : return "VKRS_ZFUNC";
	case VKRS_ALPHAREF                      : return "VKRS_ALPHAREF";
	case VKRS_ALPHAFUNC                     : return "VKRS_ALPHAFUNC";
	case VKRS_DITHERENABLE                  : return "VKRS_DITHERENABLE";
	case VKRS_ALPHABLENDENABLE              : return "VKRS_ALPHABLENDENABLE";
	case VKRS_FOGENABLE                     : return "VKRS_FOGENABLE";
	case VKRS_SPECULARENABLE                : return "VKRS_SPECULARENABLE";
	//case VKRS_ZVISIBLE                      : return "VKRS_ZVISIBLE";
	case VKRS_FOGCOLOR                      : return "VKRS_FOGCOLOR";
	case VKRS_FOGTABLEMODE                  : return "VKRS_FOGTABLEMODE";
	case VKRS_FOGSTART                      : return "VKRS_FOGSTART";
	case VKRS_FOGEND                        : return "VKRS_FOGEND";
	case VKRS_FOGDENSITY                    : return "VKRS_FOGDENSITY";
	//case VKRS_EDGEANTIALIAS                 : return "VKRS_EDGEANTIALIAS";
	case VKRS_DEPTHBIAS                         : return "VKRS_DEPTHBIAS";
	case VKRS_RANGEFOGENABLE                : return "VKRS_RANGEFOGENABLE";
	case VKRS_STENCILENABLE                 : return "VKRS_STENCILENABLE";
	case VKRS_STENCILFAIL                   : return "VKRS_STENCILFAIL";
	case VKRS_STENCILZFAIL                  : return "VKRS_STENCILZFAIL";
	case VKRS_STENCILPASS                   : return "VKRS_STENCILPASS";
	case VKRS_STENCILFUNC                   : return "VKRS_STENCILFUNC";
	case VKRS_STENCILREF                    : return "VKRS_STENCILREF";
	case VKRS_STENCILMASK                   : return "VKRS_STENCILMASK";
	case VKRS_STENCILWRITEMASK              : return "VKRS_STENCILWRITEMASK";
	case VKRS_TEXTUREFACTOR                 : return "VKRS_TEXTUREFACTOR";
	case VKRS_WRAP0                         : return "VKRS_WRAP0";
	case VKRS_WRAP1                         : return "VKRS_WRAP1";
	case VKRS_WRAP2                         : return "VKRS_WRAP2";
	case VKRS_WRAP3                         : return "VKRS_WRAP3";
	case VKRS_WRAP4                         : return "VKRS_WRAP4";
	case VKRS_WRAP5                         : return "VKRS_WRAP5";
	case VKRS_WRAP6                         : return "VKRS_WRAP6";
	case VKRS_WRAP7                         : return "VKRS_WRAP7";
	case VKRS_CLIPPING                      : return "VKRS_CLIPPING";
	case VKRS_LIGHTING                      : return "VKRS_LIGHTING";
	case VKRS_AMBIENT                       : return "VKRS_AMBIENT";
	case VKRS_FOGVERTEXMODE                 : return "VKRS_FOGVERTEXMODE";
	case VKRS_COLORVERTEX                   : return "VKRS_COLORVERTEX";
	case VKRS_LOCALVIEWER                   : return "VKRS_LOCALVIEWER";
	case VKRS_NORMALIZENORMALS              : return "VKRS_NORMALIZENORMALS";
	case VKRS_DIFFUSEMATERIALSOURCE         : return "VKRS_DIFFUSEMATERIALSOURCE";
	case VKRS_SPECULARMATERIALSOURCE        : return "VKRS_SPECULARMATERIALSOURCE";
	case VKRS_AMBIENTMATERIALSOURCE         : return "VKRS_AMBIENTMATERIALSOURCE";
	case VKRS_EMISSIVEMATERIALSOURCE        : return "VKRS_EMISSIVEMATERIALSOURCE";
	case VKRS_VERTEXBLEND                   : return "VKRS_VERTEXBLEND";
	case VKRS_CLIPPLANEENABLE               : return "VKRS_CLIPPLANEENABLE";
	//case VKRS_SOFTWAREVERTEXPROCESSING      : return "VKRS_SOFTWAREVERTEXPROCESSING";
	case VKRS_POINTSIZE                     : return "VKRS_POINTSIZE";
	case VKRS_POINTSIZE_MIN                 : return "VKRS_POINTSIZE_MIN";
	case VKRS_POINTSPRITEENABLE             : return "VKRS_POINTSPRITEENABLE";
	case VKRS_POINTSCALEENABLE              : return "VKRS_POINTSCALEENABLE";
	case VKRS_POINTSCALE_A                  : return "VKRS_POINTSCALE_A";
	case VKRS_POINTSCALE_B                  : return "VKRS_POINTSCALE_B";
	case VKRS_POINTSCALE_C                  : return "VKRS_POINTSCALE_C";
	case VKRS_MULTISAMPLEANTIALIAS          : return "VKRS_MULTISAMPLEANTIALIAS";
	case VKRS_MULTISAMPLEMASK               : return "VKRS_MULTISAMPLEMASK";
	case VKRS_PATCHEDGESTYLE                : return "VKRS_PATCHEDGESTYLE";
	//case VKRS_PATCHSEGMENTS                 : return "VKRS_PATCHSEGMENTS";
	case VKRS_DEBUGMONITORTOKEN             : return "VKRS_DEBUGMONITORTOKEN";
	case VKRS_POINTSIZE_MAX                 : return "VKRS_POINTSIZE_MAX";
	case VKRS_INDEXEDVERTEXBLENDENABLE      : return "VKRS_INDEXEDVERTEXBLENDENABLE";
	case VKRS_COLORWRITEENABLE              : return "VKRS_COLORWRITEENABLE";
	case VKRS_TWEENFACTOR                   : return "VKRS_TWEENFACTOR";
	case VKRS_BLENDOP                       : return "VKRS_BLENDOP";
//	case VKRS_POSITIONORDER                 : return "VKRS_POSITIONORDER";
//	case VKRS_NORMALORDER                   : return "VKRS_NORMALORDER";
	default											  : return "UNKNOWN";
	}
}
#endif

#ifdef INFO_VULKAN
const char* DX8Wrapper::Get_DX8_Texture_Stage_State_Name(D3DTEXTURESTAGESTATETYPE state)
{
	switch (state) {
	case VKTSS_COLOROP                   : return "VKTSS_COLOROP";
	case VKTSS_COLORARG1                 : return "VKTSS_COLORARG1";
	case VKTSS_COLORARG2                 : return "VKTSS_COLORARG2";
	case VKTSS_ALPHAOP                   : return "VKTSS_ALPHAOP";
	case VKTSS_ALPHAARG1                 : return "VKTSS_ALPHAARG1";
	case VKTSS_ALPHAARG2                 : return "VKTSS_ALPHAARG2";
	case VKTSS_BUMPENVMAT00              : return "VKTSS_BUMPENVMAT00";
	case VKTSS_BUMPENVMAT01              : return "VKTSS_BUMPENVMAT01";
	case VKTSS_BUMPENVMAT10              : return "VKTSS_BUMPENVMAT10";
	case VKTSS_BUMPENVMAT11              : return "VKTSS_BUMPENVMAT11";
	case VKTSS_TEXCOORDINDEX             : return "VKTSS_TEXCOORDINDEX";
	case VKTSS_BUMPENVLSCALE             : return "VKTSS_BUMPENVLSCALE";
	case VKTSS_BUMPENVLOFFSET            : return "VKTSS_BUMPENVLOFFSET";
	case VKTSS_TEXTURETRANSFORMFLAGS     : return "VKTSS_TEXTURETRANSFORMFLAGS";
	case VKTSS_COLORARG0                 : return "VKTSS_COLORARG0";
	case VKTSS_ALPHAARG0                 : return "VKTSS_ALPHAARG0";
	case VKTSS_RESULTARG                 : return "VKTSS_RESULTARG";
	default										  : return "UNKNOWN";
	}
}
const char* DX8Wrapper::Get_DX8_Sampler_Stage_State_Name(D3DSAMPLERSTATETYPE state)
{
	switch (state) {
	case VKSAMP_ADDRESSU: return "VKSAMP_ADDRESSU";
	case VKSAMP_ADDRESSV: return "VKSAMP_ADDRESSV";
	case VKSAMP_BORDERCOLOR: return "VKSAMP_BORDERCOLOR";
	case VKSAMP_MAGFILTER: return "VKSAMP_MAGFILTER";
	case VKSAMP_MINFILTER: return "VKSAMP_MINFILTER";
	case VKSAMP_MIPFILTER: return "VKSAMP_MIPFILTER";
	case VKSAMP_MIPMAPLODBIAS: return "VKSAMP_MIPMAPLODBIAS";
	case VKSAMP_MAXMIPLEVEL: return "VKSAMP_MAXMIPLEVEL";
	case VKSAMP_MAXANISOTROPY: return "VKSAMP_MAXANISOTROPY";
	case VKSAMP_ADDRESSW: return "VKSAMP_ADDRESSW";
	default: return "UNKNOWN";
	}
}
#endif
#ifdef INFO_VULKAN
void DX8Wrapper::Get_DX8_Render_State_Value_Name(StringClass& name, D3DRENDERSTATETYPE state, unsigned value)
{
	switch (state) {
	case VKRS_ZENABLE:
		name=Get_DX8_ZBuffer_Type_Name(value);
		break;

	case VKRS_FILLMODE:
		name=Get_DX8_Fill_Mode_Name(value);
		break;

	case VKRS_SHADEMODE:
		name=Get_DX8_Shade_Mode_Name(value);
		break;

	//case VKRS_LINEPATTERN:
	case VKRS_FOGCOLOR:
	case VKRS_ALPHAREF:
	case VKRS_STENCILMASK:
	case VKRS_STENCILWRITEMASK:
	case VKRS_TEXTUREFACTOR:
	case VKRS_AMBIENT:
	case VKRS_CLIPPLANEENABLE:
	case VKRS_MULTISAMPLEMASK:
		name.Format("0x%x",value);
		break;

	case VKRS_ZWRITEENABLE:
	case VKRS_ALPHATESTENABLE:
	case VKRS_LASTPIXEL:
	case VKRS_DITHERENABLE:
	case VKRS_ALPHABLENDENABLE:
	case VKRS_FOGENABLE:
	case VKRS_SPECULARENABLE:
	case VKRS_STENCILENABLE:
	case VKRS_RANGEFOGENABLE:
	//case VKRS_EDGEANTIALIAS:
	case VKRS_CLIPPING:
	case VKRS_LIGHTING:
	case VKRS_COLORVERTEX:
	case VKRS_LOCALVIEWER:
	case VKRS_NORMALIZENORMALS:
	//case VKRS_SOFTWAREVERTEXPROCESSING:
	case VKRS_POINTSPRITEENABLE:
	case VKRS_POINTSCALEENABLE:
	case VKRS_MULTISAMPLEANTIALIAS:
	case VKRS_INDEXEDVERTEXBLENDENABLE:
		name=value ? "TRUE" : "FALSE";
		break;

	case VKRS_SRCBLEND:
	case VKRS_DESTBLEND:
		name=Get_DX8_Blend_Name(value);
		break;

	case VKRS_CULLMODE:
		name=Get_DX8_Cull_Mode_Name(value);
		break;

	case VKRS_ZFUNC:
	case VKRS_ALPHAFUNC:
	case VKRS_STENCILFUNC:
		name=Get_DX8_Cmp_Func_Name(value);
		break;


	case VKRS_FOGTABLEMODE:
	case VKRS_FOGVERTEXMODE:
		name=Get_DX8_Fog_Mode_Name(value);
		break;

	case VKRS_FOGSTART:
	case VKRS_FOGEND:
	case VKRS_FOGDENSITY:
	case VKRS_POINTSIZE:
	case VKRS_POINTSIZE_MIN:
	case VKRS_POINTSCALE_A:
	case VKRS_POINTSCALE_B:
	case VKRS_POINTSCALE_C:
	case VKRS_POINTSIZE_MAX:
	case VKRS_TWEENFACTOR:
	case VKRS_DEPTHBIAS:
		name.Format("%f",*(float*)&value);
		break;

	case VKRS_STENCILREF:
		name.Format("%d",value);
		break;

	case VKRS_STENCILFAIL:
	case VKRS_STENCILZFAIL:
	case VKRS_STENCILPASS:
		name=Get_DX8_Stencil_Op_Name(value);
		break;

	case VKRS_WRAP0:
	case VKRS_WRAP1:
	case VKRS_WRAP2:
	case VKRS_WRAP3:
	case VKRS_WRAP4:
	case VKRS_WRAP5:
	case VKRS_WRAP6:
	case VKRS_WRAP7:
		name="0";
		if (value&D3DWRAP_U) name+="|D3DWRAP_U";
		if (value&D3DWRAP_V) name+="|D3DWRAP_V";
		if (value&D3DWRAP_W) name+="|D3DWRAP_W";
		break;

	case VKRS_DIFFUSEMATERIALSOURCE:
	case VKRS_SPECULARMATERIALSOURCE:
	case VKRS_AMBIENTMATERIALSOURCE:
	case VKRS_EMISSIVEMATERIALSOURCE:
		name=Get_DX8_Material_Source_Name(value);
		break;

	case VKRS_VERTEXBLEND:
		name=Get_DX8_Vertex_Blend_Flag_Name(value);
		break;

	case VKRS_PATCHEDGESTYLE:
		name=Get_DX8_Patch_Edge_Style_Name(value);
		break;

	case VKRS_DEBUGMONITORTOKEN:
		name=Get_DX8_Debug_Monitor_Token_Name(value);
		break;

	case VKRS_COLORWRITEENABLE:
		name="0";
		if (value&(1<<0)) name+="|(1<<0)";
		if (value&(1<<1)) name+="|(1<<1)";
		if (value&(1<<2)) name+="|(1<<2)";
		if (value&(1<<3)) name+="|(1<<3)";
		break;
	case VKRS_BLENDOP:
		name=Get_DX8_Blend_Op_Name(value);
		break;
	default:
		name.Format("UNKNOWN (%d)",value);
		break;
	}
}

void DX8Wrapper::Get_DX8_Texture_Stage_State_Value_Name(StringClass& name, D3DTEXTURESTAGESTATETYPE state, unsigned value)
{
	switch (state) {
	case VKTSS_COLOROP:
	case VKTSS_ALPHAOP:
		name=Get_DX8_Texture_Op_Name(value);
		break;

	case VKTSS_COLORARG0:
	case VKTSS_COLORARG1:
	case VKTSS_COLORARG2:
	case VKTSS_ALPHAARG0:
	case VKTSS_ALPHAARG1:
	case VKTSS_ALPHAARG2:
	case VKTSS_RESULTARG:
		name=Get_DX8_Texture_Arg_Name(value);
		break;

	case VKTSS_TEXTURETRANSFORMFLAGS:
		name=Get_DX8_Texture_Transform_Flag_Name(value);

	// Floating point values
	case VKTSS_BUMPENVMAT00:
	case VKTSS_BUMPENVMAT01:
	case VKTSS_BUMPENVMAT10:
	case VKTSS_BUMPENVMAT11:
	case VKTSS_BUMPENVLSCALE:
	case VKTSS_BUMPENVLOFFSET:
		name.Format("%f",*(float*)&value);
		break;

	case VKTSS_TEXCOORDINDEX:
		if ((value&0xffff0000)==VKTSS_TCI_CAMERASPACENORMAL) {
			name.Format("VKTSS_TCI_CAMERASPACENORMAL|%d",value&0xffff);
		}
		else if ((value&0xffff0000)==VKTSS_TCI_CAMERASPACEPOSITION) {
			name.Format("VKTSS_TCI_CAMERASPACEPOSITION|%d",value&0xffff);
		}
		else if ((value&0xffff0000)==VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR) {
			name.Format("VKTSS_TCI_CAMERASPACEREFLECTIONVECTOR|%d",value&0xffff);
		}
		else {
			name.Format("%d",value);
		}
		break;

	default:
		name.Format("UNKNOWN (%d)",value);
		break;
	}
}

void DX8Wrapper::Get_DX8_Sampler_Stage_State_Value_Name(StringClass& name, D3DSAMPLERSTATETYPE state, unsigned value)
{
	switch (state) {
		case VKSAMP_ADDRESSU:
		case VKSAMP_ADDRESSV:
		case VKSAMP_ADDRESSW:
		name = Get_DX8_Texture_Address_Name(value);
		break;

	case VKSAMP_MAGFILTER:
	case VKSAMP_MINFILTER:
	case VKSAMP_MIPFILTER:
		name = Get_DX8_Texture_Filter_Name(value);
		break;

		// Floating point values
	case VKSAMP_MIPMAPLODBIAS:
		name.Format("%f", *(float*)&value);
		break;

		// Integer value
	case VKSAMP_MAXMIPLEVEL:
	case VKSAMP_MAXANISOTROPY:
		name.Format("%d", value);
		break;
		// Hex values
	case VKSAMP_BORDERCOLOR:
		name.Format("0x%x", value);
		break;

	default:
		name.Format("UNKNOWN (%d)", value);
		break;
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Op_Name(unsigned value)
{
	switch (value) {
	case VKTOP_DISABLE                      : return "VKTOP_DISABLE";
	case VKTOP_SELECTARG1                   : return "VKTOP_SELECTARG1";
	case VKTOP_SELECTARG2                   : return "VKTOP_SELECTARG2";
	case VKTOP_MODULATE                     : return "VKTOP_MODULATE";
	case VKTOP_MODULATE2X                   : return "VKTOP_MODULATE2X";
	case VKTOP_MODULATE4X                   : return "VKTOP_MODULATE4X";
	case VKTOP_ADD                          : return "VKTOP_ADD";
	case VKTOP_ADDSIGNED                    : return "VKTOP_ADDSIGNED";
	case VKTOP_ADDSIGNED2X                  : return "VKTOP_ADDSIGNED2X";
	case VKTOP_SUBTRACT                     : return "VKTOP_SUBTRACT";
	case VKTOP_ADDSMOOTH                    : return "VKTOP_ADDSMOOTH";
	case VKTOP_BLENDDIFFUSEALPHA            : return "VKTOP_BLENDDIFFUSEALPHA";
	case VKTOP_BLENDTEXTUREALPHA            : return "VKTOP_BLENDTEXTUREALPHA";
	case VKTOP_BLENDFACTORALPHA             : return "VKTOP_BLENDFACTORALPHA";
	case VKTOP_BLENDTEXTUREALPHAPM          : return "VKTOP_BLENDTEXTUREALPHAPM";
	case VKTOP_BLENDCURRENTALPHA            : return "VKTOP_BLENDCURRENTALPHA";
	case VKTOP_PREMODULATE                  : return "VKTOP_PREMODULATE";
	case VKTOP_MODULATEALPHA_ADDCOLOR       : return "VKTOP_MODULATEALPHA_ADDCOLOR";
	case VKTOP_MODULATECOLOR_ADDALPHA       : return "VKTOP_MODULATECOLOR_ADDALPHA";
	case VKTOP_MODULATEINVALPHA_ADDCOLOR    : return "VKTOP_MODULATEINVALPHA_ADDCOLOR";
	case VKTOP_MODULATEINVCOLOR_ADDALPHA    : return "VKTOP_MODULATEINVCOLOR_ADDALPHA";
	case VKTOP_BUMPENVMAP                   : return "VKTOP_BUMPENVMAP";
	case VKTOP_BUMPENVMAPLUMINANCE          : return "VKTOP_BUMPENVMAPLUMINANCE";
	case VKTOP_DOTPRODUCT3                  : return "VKTOP_DOTPRODUCT3";
	case VKTOP_MULTIPLYADD                  : return "VKTOP_MULTIPLYADD";
	case VKTOP_LERP                         : return "VKTOP_LERP";
	default										     : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Arg_Name(unsigned value)
{
	switch (value) {
	case VKTA_CURRENT			: return "VKTA_CURRENT";
	case VKTA_DIFFUSE			: return "VKTA_DIFFUSE";
	case VKTA_SELECTMASK		: return "VKTA_SELECTMASK";
	case VKTA_SPECULAR			: return "VKTA_SPECULAR";
	case VKTA_TEMP				: return "VKTA_TEMP";
	case VKTA_TEXTURE			: return "VKTA_TEXTURE";
	case VKTA_TFACTOR			: return "VKTA_TFACTOR";
	case VKTA_ALPHAREPLICATE	: return "VKTA_ALPHAREPLICATE";
	case VKTA_COMPLEMENT		: return "VKTA_COMPLEMENT";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Filter_Name(unsigned value)
{
	switch (value) {
	case D3DTEXF_NONE				: return "D3DTEXF_NONE";
	case VK_FILTER_NEAREST			: return "VK_FILTER_NEAREST";
	case VK_FILTER_LINEAR			: return "VK_FILTER_LINEAR";
	case D3DTEXF_ANISOTROPIC	: return "D3DTEXF_ANISOTROPIC";
	//case D3DTEXF_FLATCUBIC		: return "D3DTEXF_FLATCUBIC";
	//case D3DTEXF_GAUSSIANCUBIC	: return "D3DTEXF_GAUSSIANCUBIC";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Address_Name(unsigned value)
{
	switch (value) {
	case VK_SAMPLER_ADDRESS_MODE_REPEAT		: return "VK_SAMPLER_ADDRESS_MODE_REPEAT";
	case D3DTADDRESS_MIRROR		: return "D3DTADDRESS_MIRROR";
	case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE		: return "VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE";
	case D3DTADDRESS_BORDER		: return "D3DTADDRESS_BORDER";
	case D3DTADDRESS_MIRRORONCE: return "D3DTADDRESS_MIRRORONCE";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Texture_Transform_Flag_Name(unsigned value)
{
	switch (value) {
	case VKTTFF_DISABLE			: return "VKTTFF_DISABLE";
	case VKTTFF_COUNT1			: return "VKTTFF_COUNT1";
	case VKTTFF_COUNT2			: return "VKTTFF_COUNT2";
	case VKTTFF_COUNT3			: return "VKTTFF_COUNT3";
	case VKTTFF_COUNT4			: return "VKTTFF_COUNT4";
	case VKTTFF_PROJECTED		: return "VKTTFF_PROJECTED";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_ZBuffer_Type_Name(unsigned value)
{
	switch (value) {
	case D3DZB_FALSE				: return "D3DZB_FALSE";
	case D3DZB_TRUE				: return "D3DZB_TRUE";
	case D3DZB_USEW				: return "D3DZB_USEW";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Fill_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DFILL_POINT			: return "D3DFILL_POINT";
	case VK_POLYGON_MODE_LINE		: return "VK_POLYGON_MODE_LINE";
	case VK_POLYGON_MODE_FILL			: return "VK_POLYGON_MODE_FILL";
	default					      : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Shade_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DSHADE_FLAT			: return "D3DSHADE_FLAT";
	case D3DSHADE_GOURAUD		: return "D3DSHADE_GOURAUD";
	case D3DSHADE_PHONG			: return "D3DSHADE_PHONG";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Blend_Name(unsigned value)
{
	switch (value) {
	case VK_BLEND_FACTOR_ZERO                : return "VK_BLEND_FACTOR_ZERO";
	case VK_BLEND_FACTOR_ONE                 : return "VK_BLEND_FACTOR_ONE";
	case VK_BLEND_FACTOR_SRC_COLOR            : return "VK_BLEND_FACTOR_SRC_COLOR";
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR         : return "VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR";
	case VK_BLEND_FACTOR_SRC_ALPHA            : return "VK_BLEND_FACTOR_SRC_ALPHA";
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA         : return "VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA";
	case VK_BLEND_FACTOR_DST_ALPHA           : return "VK_BLEND_FACTOR_DST_ALPHA";
	case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA        : return "VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA";
	case VK_BLEND_FACTOR_DST_COLOR           : return "VK_BLEND_FACTOR_DST_COLOR";
	case VK_BLEND_FACTOR_INVDESTCOLOR        : return "VK_BLEND_FACTOR_INVDESTCOLOR";
	case VK_BLEND_FACTOR_SRC_ALPHASAT         : return "VK_BLEND_FACTOR_SRC_ALPHASAT";
	case VK_BLEND_FACTOR_BOTHSRCALPHA        : return "VK_BLEND_FACTOR_BOTHSRCALPHA";
	case VK_BLEND_FACTOR_BOTHINVSRCALPHA     : return "VK_BLEND_FACTOR_BOTHINVSRCALPHA";
	default									 : return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Cull_Mode_Name(unsigned value)
{
	switch (value) {
	case VK_FRONT_FACE_MAX_ENUM				: return "VK_FRONT_FACE_MAX_ENUM";
	case VK_FRONT_FACE_CLOCKWISE				: return "VK_FRONT_FACE_CLOCKWISE";
	case VK_FRONT_FACE_COUNTER_CLOCKWISE				: return "VK_FRONT_FACE_COUNTER_CLOCKWISE";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Cmp_Func_Name(unsigned value)
{
	switch (value) {
	case VK_COMPARE_OP_NEVER          : return "VK_COMPARE_OP_NEVER";
	case VK_COMPARE_OP_LESS           : return "VK_COMPARE_OP_LESS";
	case VK_COMPARE_OP_EQUAL          : return "VK_COMPARE_OP_EQUAL";
	case VK_COMPARE_OP_LESS_OR_EQUAL      : return "VK_COMPARE_OP_LESS_OR_EQUAL";
	case VK_COMPARE_OP_GREATER        : return "VK_COMPARE_OP_GREATER";
	case VK_COMPARE_OP_NOT_EQUAL       : return "VK_COMPARE_OP_NOT_EQUAL";
	case VK_COMPARE_OP_GREATER_OR_EQUAL   : return "VK_COMPARE_OP_GREATER_OR_EQUAL";
	case VK_COMPARE_OP_ALWAYS         : return "VK_COMPARE_OP_ALWAYS";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Fog_Mode_Name(unsigned value)
{
	switch (value) {
	case D3DFOG_NONE				: return "D3DFOG_NONE";
	case D3DFOG_EXP				: return "D3DFOG_EXP";
	case D3DFOG_EXP2				: return "D3DFOG_EXP2";
	case D3DFOG_LINEAR			: return "D3DFOG_LINEAR";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Stencil_Op_Name(unsigned value)
{
	switch (value) {
	case VK_STENCIL_OP_KEEP		: return "VK_STENCIL_OP_KEEP";
	case VK_STENCIL_OP_ZERO		: return "VK_STENCIL_OP_ZERO";
	case VK_STENCIL_OP_REPLACE	: return "VK_STENCIL_OP_REPLACE";
	case VK_STENCIL_OP_INCRSAT	: return "VK_STENCIL_OP_INCRSAT";
	case VK_STENCIL_OP_DECRSAT	: return "VK_STENCIL_OP_DECRSAT";
	case VK_STENCIL_OP_INVERT	: return "VK_STENCIL_OP_INVERT";
	case VK_STENCIL_OP_INCR		: return "VK_STENCIL_OP_INCR";
	case VK_STENCIL_OP_DECR		: return "VK_STENCIL_OP_DECR";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Material_Source_Name(unsigned value)
{
	switch (value) {
	case D3DMCS_MATERIAL			: return "D3DMCS_MATERIAL";
	case D3DMCS_COLOR1			: return "D3DMCS_COLOR1";
	case D3DMCS_COLOR2			: return "D3DMCS_COLOR2";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Vertex_Blend_Flag_Name(unsigned value)
{
	switch (value) {
	case D3DVBF_DISABLE			: return "D3DVBF_DISABLE";
	case D3DVBF_1WEIGHTS			: return "D3DVBF_1WEIGHTS";
	case D3DVBF_2WEIGHTS			: return "D3DVBF_2WEIGHTS";
	case D3DVBF_3WEIGHTS			: return "D3DVBF_3WEIGHTS";
	case D3DVBF_TWEENING			: return "D3DVBF_TWEENING";
	case D3DVBF_0WEIGHTS			: return "D3DVBF_0WEIGHTS";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Patch_Edge_Style_Name(unsigned value)
{
	switch (value) {
	case D3DPATCHEDGE_DISCRETE	: return "D3DPATCHEDGE_DISCRETE";
   case D3DPATCHEDGE_CONTINUOUS:return "D3DPATCHEDGE_CONTINUOUS";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Debug_Monitor_Token_Name(unsigned value)
{
	switch (value) {
	case D3DDMT_ENABLE			: return "D3DDMT_ENABLE";
	case D3DDMT_DISABLE			: return "D3DDMT_DISABLE";
	default							: return "UNKNOWN";
	}
}

const char* DX8Wrapper::Get_DX8_Blend_Op_Name(unsigned value)
{
	switch (value) {
	case VK_BLEND_OP_ADD			: return "VK_BLEND_OP_ADD";
	case VK_BLEND_OP_SUBTRACT	: return "VK_BLEND_OP_SUBTRACT";
	case VK_BLEND_OP_REVERSE_SUBTRACT: return "VK_BLEND_OP_REVERSE_SUBTRACT";
	case VK_BLEND_OP_MIN			: return "VK_BLEND_OP_MIN";
	case VK_BLEND_OP_MAX			: return "VK_BLEND_OP_MAX";
	default							: return "UNKNOWN";
	}
}
#endif


//============================================================================
// DX8Wrapper::getBackBufferFormat
//============================================================================

WW3DFormat	DX8Wrapper::getBackBufferFormat( void )
{
#ifdef TODO_VULKAN
	return D3DFormat_To_WW3DFormat( _PresentParameters.BackBufferFormat );
#else
	return (WW3DFormat)0;
#endif
}
