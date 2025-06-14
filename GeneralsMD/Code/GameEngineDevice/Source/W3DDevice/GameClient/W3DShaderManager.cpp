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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: W3DShaderManager.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//                                                                          
//                       Westwood Studios Pacific.                          
//                                                                          
//                       Confidential Information                           
//                Copyright (C) 2001 - All Rights Reserved                  
//                                                                          
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: W3DShaderManager.cpp
//
// Created:   Mark Wilczynski, August 2001
//
// Desc:      Perform tests on currently selected WW3D/D3D device to determine
//			  which of our rendering features are supported.  The system allows
//			  setting up a few custom shaders that are selected based on video
//			  card features.
//
//			  To add a new shader to the system:
//			  0) Add your shader to the ShaderTypes enum
//			  1) Create shader using W3DShaderInterface
//			  2) Repeat step 1 for any alternate shaders
//			  3) Create list of alternate shaders sorted by order of preference.
//				 The first shader which passes hardware validation will be selected.
//			  4) Add list from step 3) to MasterShaderList[].
//
//-----------------------------------------------------------------------------

#include "dx8wrapper.h"
#include "assetmgr.h"
#include "Lib/BaseType.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "GameClient/view.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/display.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "common/GlobalData.h"
#include "common/GameLOD.h"
#include "dx8caps.h"
#include "common/gamelod.h"
#include "VkBufferTools.h"
//#include "Benchmark.h"

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

// Turn this on to turn off pixel shaders. jba[4/3/2003]
#define do_not_DISABLE_PIXEL_SHADERS 1

/** Interface definition for custom shaders we define in our app.  These shaders can perform more complex
	operations than those allowed in the WW3D2 shader system.
*/
class W3DShaderInterface
{
public:
	Int getNumPasses(void) {return m_numPasses;};	///<return number of passes needed for this shader
	virtual Int set(Int pass) {return TRUE;};		///<setup shader for the specified rendering pass.
	 ///do any custom resetting necessary to bring W3D in sync.
	virtual void reset(void) {
		ShaderClass::Invalidate();
		DX8Wrapper::Set_Texture(0, NULL);
		DX8Wrapper::Set_Texture(1, NULL);
	};
	virtual Int init(void) = 0;			///<perform any one time initialization and validation
	virtual Int shutdown(void) { return TRUE;};			///<release resources used by shader
protected:
	Int m_numPasses;						///<number of passes to complete shader
};

//this table will contain custom versions of each shader tuned for specific video card and user options.
static W3DFilterInterface *W3DFilters[FT_MAX];
static W3DShaderInterface *W3DShaders[W3DShaderManager::ST_MAX];
static Int W3DShadersPassCount[W3DShaderManager::ST_MAX];	//number of passes for each of the above shaders
TextureClass *W3DShaderManager::m_Textures[8];
W3DShaderManager::ShaderTypes W3DShaderManager::m_currentShader;
FilterTypes W3DShaderManager::m_currentFilter=FT_NULL_FILTER; ///< Last filter that was set.
Int W3DShaderManager::m_currentShaderPass;
ChipsetType W3DShaderManager::m_currentChipset;
GraphicsVenderID W3DShaderManager::m_currentVendor;
__int64 W3DShaderManager::m_driverVersion;

Bool W3DShaderManager::m_renderingToTexture = false;
//VK::Texture W3DShaderManager::m_oldRenderSurface=NULL;	///<previous render target
VK::Texture W3DShaderManager::m_renderTexture = {};		///<texture into which rendering will be redirected.
//VK::Texture W3DShaderManager::m_newRenderSurfacew=NULL;	///<new render target inside m_renderTexture
//VK::Texture W3DShaderManager::m_oldDepthSurface=NULL;	///<previous depth buffer surface
/*===========================================================================================*/
/*=========      Screen Shaders	=============================================================*/
/*===========================================================================================*/

class ScreenDefaultFilter : public W3DFilterInterface
{
public:
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode); ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(enum FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender); ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(enum FilterModes mode){return true;} ///< Called when the filter is started, one time before the first prerender.
protected:
	virtual Int set(enum FilterModes mode);		///<setup shader for the specified rendering pass.
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
};

ScreenDefaultFilter screenDefaultFilter;

///Default filter that just renders screen to off-screen texture and then copies it the the screen.
///Useful because we added some full-time unit effects (microwave tank smudge) to Generals MD that need access
///to the background as a texture.  This filter makes that texture always available for these effects.
W3DFilterInterface *ScreenDefaultFilterList[]=
{
	&screenDefaultFilter,
	NULL
};

Int ScreenDefaultFilter::init(void)
{
	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return FALSE;
	}

	//Can render to texture, but we don't know if it can read and write to the same texture.
	//Since there is no D3D caps bit to tell you this, we will just hard-code some specific
	//cards that we know should work.

	Int res;

	if ((res=W3DShaderManager::getChipset()) != DC_UNKNOWN)
	{
		if ( res >=	DC_GEFORCE2)
		{	
			//Check if their driver is newer than what we tested for this vendor
/*			if (TheGameLODManager)
			{
				if (TheGameLODManager->getTestedDriverVersion(W3DShaderManager::getCurrentVendor()) < W3DShaderManager::getCurrentDriverVersion())
					return FALSE;
			}*/
		}
	}

	W3DFilters[FT_VIEW_DEFAULT]=&screenDefaultFilter;

	return TRUE;
}

Bool ScreenDefaultFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	//Right now this filter is only used for smudges, so don't bother if none are present.
	if (TheSmudgeManager)
	{	if (((W3DSmudgeManager *)TheSmudgeManager)->getSmudgeCountLastFrame() == 0)
			return FALSE;
	}
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenDefaultFilter::postRender(enum FilterModes mode, Coord2D& scrollDelta, Bool& doExtraRender)
{
	VK::Texture tex = W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex.image, ("Require rendered texture."));
	if (!tex.image) return false;
	if (!set(mode)) return false;

#ifdef INFO_VULKAN
	LPDIRECT3DDEVICE9 pDev = DX8Wrapper::_Get_D3D_Device8();
#endif
	struct _TRANS_LIT_TEX_VERTEX {
		Vector3 p;
		DWORD color;   // diffuse color    
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

#ifdef INFO_VULKAN
	DX8Wrapper::Set_Texture(0, tex);	//previously rendered frame inside this texture
#endif
	TheTacticalView->getOrigin(&xpos, &ypos);
	width = TheTacticalView->getWidth();
	height = TheTacticalView->getHeight();

	float scaledW = (Real)(xpos + width) / (Real)TheDisplay->getWidth();
	float scaledH = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//bottom right
	v[0].p = { scaledW * 2 - 1.f, scaledH * 2 - 1.f, 0.0f };
	v[0].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top right
	v[1].p = { scaledW * 2 - 1.f, -1.f, 0.0f };
	v[1].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = { (float)xpos - 1.f, scaledH * 2 - 1.f, 0.0f };
	v[2].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top left
	v[3].p = { (float)xpos - 1.f,  -1, 0.0f };
	v[3].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos) / (Real)TheDisplay->getHeight();

	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
#ifdef INFO_VULKAN
	pDev->SetFVF(VKFVF_XYZRHW | VKFVF_DIFFUSE | VKFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
#else
	Matrix4x4 ident(true);

	VK::Buffer vbo;
	VkBufferTools::CreateVertexBuffer(&WWVKRENDER, sizeof(_TRANS_LIT_TEX_VERTEX) * 4, v, vbo);

	std::vector<VkDescriptorSet> sets;
	DX8Wrapper::Apply_Render_State_Changes();
	WWVK_UpdateFVF_DUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &tex, DX8Wrapper::UboIdent(), DX8Wrapper::UboIdent());
	WWVK_DrawFVF_DUV_Strip_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, 4, vbo.buffer, 0, (WorldMatrix*)&ident);
	WWVKRENDER.PushSingleFrameBuffer(vbo);
#endif

	reset();
	return true;
}

Int ScreenDefaultFilter::set(enum FilterModes mode)
{
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
	DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
	DX8Wrapper::Set_Texture(0,NULL);
	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

	DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_ALWAYS);
	DX8Wrapper::Set_DX8_Render_State(VKRS_ZWRITEENABLE,FALSE);
	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

	return true;
}

void ScreenDefaultFilter::reset(void)
{
	DX8Wrapper::Set_Texture(0,NULL);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

/*=========  ScreenBWFilter	=============================================================*/
///converts viewport to black & white.

Int ScreenBWFilter::m_fadeFrames;
Int ScreenBWFilter::m_curFadeFrame;
Real ScreenBWFilter::m_curFadeValue;
Int ScreenBWFilter::m_fadeDirection;

ScreenBWFilter screenBWFilter;

///List of different BW shader implementations in order of preference
W3DFilterInterface *ScreenBWFilterList[]=
{
	&screenBWFilter,
	NULL
};

Int ScreenBWFilter::init(void)
{
	Int res;
#ifdef INFO_VULKAN
	HRESULT hr;

	m_dwBWPixelShader = NULL;
#endif
	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}

	if ((res=W3DShaderManager::getChipset()) != 0)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader decleration
#ifdef INFO_VULKAN
			DWORD Declaration[] = { 0 };
			
			//Monochrome pixel shader.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\monochrome.pso", Declaration, 0, false, (void**) & m_dwBWPixelShader);
			if (FAILED(hr))
				return FALSE;
#endif

			W3DFilters[FT_VIEW_BW_FILTER]=&screenBWFilter;

			return TRUE;
		}
	}
	return FALSE;
}

Bool ScreenBWFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenBWFilter::postRender(enum FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	VK::Texture tex = W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex.image, ("Require rendered texture."));
	if (!tex.image) return false;
	if (!set(mode)) return false;

#ifdef INFO_VULKAN
	LPDIRECT3DDEVICE9 pDev=DX8Wrapper::_Get_D3D_Device8();
#endif

	struct _TRANS_LIT_TEX_VERTEX {
		Vector3 p;
		DWORD color;   // diffuse color    
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	//DX8Wrapper::Set_Texture(0,tex);	//previously rendered frame inside this texture
	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = { (float)xpos + 1.f, (float)ypos + 1.f, 0.0f };
	v[0].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top right
	v[1].p = { (float)xpos + 1.f, -1.f, 0.0f };
	v[1].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = { (float)xpos - 1.f, (float)ypos + 1.f, 0.0f };
	v[2].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top left
	v[3].p = { (float)xpos - 1.f,  -1, 0.0f };
	v[3].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
#ifdef INFO_VULKAN
	pDev->SetFVF(VKFVF_XYZRHW | VKFVF_DIFFUSE | VKFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
#else
	Matrix4x4 ident(true);

	VK::Buffer vbo;
	VkBufferTools::CreateVertexBuffer(&WWVKRENDER, sizeof(_TRANS_LIT_TEX_VERTEX) * 4, v, vbo);

	std::vector<VkDescriptorSet> sets;
	WWVK_UpdateFVF_DUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &tex, DX8Wrapper::UboIdent(), DX8Wrapper::UboIdent());
	WWVK_DrawFVF_DUV_Strip_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, 4, vbo.buffer, 0, (WorldMatrix*)&ident);
	WWVKRENDER.PushSingleFrameBuffer(vbo);
#endif

	reset();
	return true;
}

Int ScreenBWFilter::set(enum FilterModes mode)
{
#ifdef INFO_VULKAN
	HRESULT hr;
#endif

	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface tinted by pixel shader

		if (m_fadeDirection > 0)
		{	//turning effect on
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;

			if (fade<m_fadeFrames)
			{
				m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
			}
			else
			{
				m_curFadeFrame = 0;
				m_curFadeValue = 1.0f;
				m_fadeDirection = 0;
			}
		}
		else
		if (m_fadeDirection < 0)
		{	//turning effect off
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;
			if (fade<m_fadeFrames)
			{
				m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			}
			else
			{	m_curFadeValue = 0.0f;
				TheTacticalView->setViewFilterMode(FM_NULL_MODE);
				TheTacticalView->setViewFilter(FT_NULL_FILTER);
				m_curFadeFrame = 0;
				m_fadeDirection = 0;
			}
		}

		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,NULL);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(VKRS_ZWRITEENABLE,FALSE);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

#ifdef INFO_VULKAN
		hr=DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBWPixelShader);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstantF(0,   DirectX::XMVECTOR(0.3f, 0.59f, 0.11f, 1.0f), 1);
#endif
		DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_Monochrome);

		DirectX::XMVECTOR	color = { 1.0f,1.0f,1.0f,1.0f };	//multiply color

		if (mode == FM_VIEW_BW_BLACK_AND_WHITE)
		{	//back & white mode
			color.m128_f32[0] = 1.0f;
			color.m128_f32[1] =1.0f;
			color.m128_f32[2] =1.0f;
		}
		if (mode == FM_VIEW_BW_RED_AND_WHITE)
		{	//red is on
			color.m128_f32[0] = 1.0f;
			color.m128_f32[1] = 0.0f;
			color.m128_f32[2] = 0.0f;
			//inverse red is on
			//red is on
//			color.x = 0.0f;
//			color.y = 1.0f;
//			color.z = 1.0f;
		}
		if (mode == FM_VIEW_BW_GREEN_AND_WHITE)
		{
			color.m128_f32[0] = 0.0f;
			color.m128_f32[1] = 1.0f;
			color.m128_f32[2] = 0.0f;
		}

#ifdef INFO_VULKAN
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstantF(1,   color, 1);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstantF(2,	DirectX::XMVECTOR(m_curFadeValue, m_curFadeValue, m_curFadeValue, 1.0f), 1);
#endif
/*		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(2,   DirectX::XMVECTOR(150.0f/255.0f, 150.0f/255.0f, 150.0f/255.0f, 0.0f), 1);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(3,   DirectX::XMVECTOR((765.0f/450.0f)/3, (765.0f/450.0f)/3, (765.0f/450.0f)/3, 1.0f), 1);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(4,   DirectX::XMVECTOR(0.5f, 0.5f, 0.5f, 0), 1);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(5,   DirectX::XMVECTOR((60.0f)/255.0f, (60.0f)/255.0f, (60.0f)/255.0f, 0), 1);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(6,   DirectX::XMVECTOR((157.0f)/255.0f, (157.0f)/255.0f, (157.0f)/255.0f, 0), 1);
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(7,   DirectX::XMVECTOR((30.0f)/255.0f, (30.0f)/255.0f, (30.0f)/255.0f, 0), 1);
		*/
		return true;
	}
	return false;
}

void ScreenBWFilter::reset(void)
{
	DX8Wrapper::Set_Texture(0,NULL);	//previously rendered frame inside this texture
#ifdef INFO_VULKAN
	DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(0);	//turn off pixel shader
#endif
	DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_MAX);
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenBWFilter::shutdown(void)
{
#ifdef INFO_VULKAN
	if (m_dwBWPixelShader)
		m_dwBWPixelShader->Release();

	m_dwBWPixelShader=NULL;
#endif

	return TRUE;
}

/*=========  ScreenCrossFadeFilter	=============================================================*/
///Fades screen between 2 different views of the scene with both being visible at once.

Int ScreenCrossFadeFilter::m_fadeFrames;
Int ScreenCrossFadeFilter::m_curFadeFrame;
Real ScreenCrossFadeFilter::m_curFadeValue;
Int ScreenCrossFadeFilter::m_fadeDirection;
TextureClass *ScreenCrossFadeFilter::m_fadePatternTexture=NULL;
Bool ScreenCrossFadeFilter::m_skipRender = FALSE;

ScreenCrossFadeFilter screenCrossFadeFilter;

///List of different BW shader implementations in order of preference
///@todo: Add a version that doesn't require pixel shader
W3DFilterInterface *ScreenCrossFadeFilterList[]=
{
	&screenCrossFadeFilter,
	NULL
};

Int ScreenCrossFadeFilter::init(void)
{
	if (!TheDisplay)
		return FALSE;	//effect is useless without a view so no point initializing for the WB, etc.

	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture())
		// Have to be able to render to texture.
		return FALSE;

	//Load an alpha mask texture that will mix foreground/background views.
	m_fadePatternTexture=WW3DAssetManager::Get_Instance()->Get_Texture("exmask_g.tga");
	if (!m_fadePatternTexture)
		return FALSE;
	m_fadePatternTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_fadePatternTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_fadePatternTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

	W3DFilters[FT_VIEW_CROSSFADE]=&screenCrossFadeFilter;

	return TRUE;
}

Bool ScreenCrossFadeFilter::updateFadeLevel(void)
{
	if (m_fadeDirection > 0)
	{	//turning effect on
		m_curFadeFrame++;
		Int fade = m_curFadeFrame;

		if (fade<m_fadeFrames)
		{
			m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
		}
		else
		{
			m_curFadeFrame = 0;
			m_curFadeValue = 1.0f;
			m_fadeDirection = 0;
			return false;
		}
	}
	else
	if (m_fadeDirection < 0)
	{	//turning effect off
		Int fade = m_curFadeFrame;
		if (fade<m_fadeFrames)
		{
			m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			m_curFadeFrame++;
		}
		else
		{	m_curFadeValue = 0.0f;
			TheTacticalView->setViewFilterMode(FM_NULL_MODE);
			TheTacticalView->setViewFilter(FT_NULL_FILTER);
			m_curFadeFrame = 0;
			m_fadeDirection = 0;
			return false;
		}
	}
	return true;
}

Bool ScreenCrossFadeFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	if (updateFadeLevel())
	{	//if fade has not completed
		W3DShaderManager::startRenderToTexture();
		scenePassMode=SCENE_PASS_ALPHA_MASK;
		skipRender = false;
		m_skipRender=true;	//tell the postRender function not to draw into framebuffer yet.
		return true;
	}
	//fade must have completed
	return true;
}

Bool ScreenCrossFadeFilter::postRender(enum FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	VK::Texture tex = {};

	if (m_skipRender)
	{
		//don't render anything to frame buffer because we still need to draw the new scene
		//that we're fading into.  Okay to render on the next call.
		m_skipRender = false;
		doExtraRender = TRUE;
		tex =	W3DShaderManager::endRenderToTexture();
		return true;	
	}

	tex=W3DShaderManager::getRenderTexture();

	DEBUG_ASSERTCRASH(tex.image, ("Require last rendered texture."));
	if (!tex.image) return false;
	if (!set(mode)) return false;

#ifdef INFO_VULKAN
	LPDIRECT3DDEVICE9 pDev=DX8Wrapper::_Get_D3D_Device8();
#endif 
	struct _TRANS_LIT_TEX_VERTEX {
		Vector3 p;
		DWORD color;   // diffuse color    
		float	u;
		float	v;
		float	u1;
		float	v1;
	} v[4];

	Int xpos, ypos, width, height;
	Real radius = 0.0f;
#ifdef INFO_VULKAN
	DX8Wrapper::Set_Texture(0,tex);	//previously rendered frame inside this texture
#endif
	if (mode == FM_VIEW_CROSSFADE_CIRCLE)
	{
		DX8Wrapper::Set_Texture(1,m_fadePatternTexture);
		//Use the current fade level to scale the mask texture, for other modes the texture
		//comes pre-scaled so doesn't require uv scaling.
		radius = (1.0f-m_curFadeValue)*2.0f;
		if (radius <= 0)
			radius = 0.01f;
		radius = 0.5f/radius;
	}

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

/*	Real radius = (1.0f-m_curFadeValue);
	if (radius <= 0)
		radius = 0.01f;
	radius = 25.0f-radius*24.75f;
*/
	//bottom right
	v[0].p = { (float)xpos + 1.f, (float)ypos + 1.f, 0.0f };
	v[0].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	v[0].u1 = 0.5f + radius;	v[0].v1 = 0.5f + radius;
	//top right
	v[1].p = { (float)xpos + 1.f, -1.f, 0.0f };
	v[1].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	v[1].u1 = 0.5f + radius;	v[1].v1 = 0.5f - radius;
	//bottom left
	v[2].p = { (float)xpos - 1.f, (float)ypos + 1.f, 0.0f };
	v[2].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	v[2].u1 = 0.5f - radius;	v[2].v1 = 0.5f + radius;
	//top left
	v[3].p = { (float)xpos - 1.f,  -1, 0.0f };
	v[3].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	v[3].u1 = 0.5f - radius;	v[3].v1 = 0.5f - radius;

	DWORD diffuse = 0xffffffff;//((Int)((m_curFadeValue) * 255.0f) << 24) | 0x00ffffff;	//store alpha value in vertex diffuse

	v[0].color = diffuse;
	v[1].color = diffuse;
	v[2].color = diffuse;
	v[3].color = diffuse;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
#ifdef INFO_VULKAN
	pDev->SetFVF(VKFVF_XYZRHW | VKFVF_DIFFUSE | VKFVF_TEX2);

	//		m_DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKSAMP_MAGFILTER,VK_FILTER_NEAREST); 
	//		m_pDev->SetSamplerState( 0, VKSAMP_MINFILTER,VK_FILTER_NEAREST); 

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
#else
	Matrix4x4 ident(true);

	VK::Buffer vbo;
	VkBufferTools::CreateVertexBuffer(&WWVKRENDER, sizeof(_TRANS_LIT_TEX_VERTEX) * 4, v, vbo);

	DX8Wrapper::Apply_Render_State_Changes();
	std::vector<VkDescriptorSet> sets;
	WWVK_UpdateFVF_DUV2_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &tex, &DX8Wrapper::Get_DX8_Texture(1), 
		DX8Wrapper::UboIdent(), DX8Wrapper::UboIdent());
	WWVK_DrawFVF_DUV2_Strip_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, 4, vbo.buffer, 0, (WorldMatrix*)&ident);
	WWVKRENDER.PushSingleFrameBuffer(vbo);
#endif

	reset();
	return true;
}
Int ScreenCrossFadeFilter::set(enum FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface
		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);
		DX8Wrapper::Set_Texture(0,NULL);
		DX8Wrapper::Set_Texture(1,NULL);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

		if (mode == FM_VIEW_CROSSFADE_CIRCLE)
		{	//cross-fading using circle mask stored in stage 1
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_COLORARG1, VKTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_COLORARG2, VKTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_COLOROP,   VKTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_ALPHAARG1, VKTA_TEXTURE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_ALPHAARG2, VKTA_CURRENT );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_ALPHAOP,   VKTOP_MODULATE );
			DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXCOORDINDEX, 1 );
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MIPFILTER, VK_FILTER_MAX_ENUM);
		}

		DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(VKRS_ZWRITEENABLE,FALSE);
		return true;
	}
	return false;
}

void ScreenCrossFadeFilter::reset(void)
{
#ifdef INFO_VULKAN
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_COLOROP,   VKTOP_DISABLE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_ALPHAOP,   VKTOP_DISABLE );
#endif
	DX8Wrapper::Set_Texture(0,NULL);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenCrossFadeFilter::shutdown(void)
{
	REF_PTR_RELEASE(m_fadePatternTexture);

	return TRUE;
}

/*=========  ScreenMotionBlurFilter	=============================================================*/
///applies motion blur to viewport.

ScreenMotionBlurFilter screenMotionBlurFilter;

Coord3D ScreenMotionBlurFilter::m_zoomToPos;
Bool ScreenMotionBlurFilter::m_zoomToValid = false;

ScreenMotionBlurFilter::ScreenMotionBlurFilter():
m_decrement(false),
m_maxCount(0),
m_lastFrame(0), 
m_skipRender(false)
{
}
///List of different motion blur implementations in order of preference
W3DFilterInterface *ScreenMotionBlurFilterList[]=
{
	&screenMotionBlurFilter,
	NULL
};

Int ScreenMotionBlurFilter::init(void)
{
	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}
	W3DFilters[FT_VIEW_MOTION_BLUR_FILTER]=this;
	return true;
}

Bool ScreenMotionBlurFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = m_skipRender;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenMotionBlurFilter::postRender(enum FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	VK::Texture tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex.image, ("Require rendered texture."));
	if (!tex.image) return false;
	if (!set(mode)) return false;
#ifdef INFO_VULKAN

	LPDIRECT3DDEVICE9 pDev=DX8Wrapper::_Get_D3D_Device8();
#endif

	Bool continueEffect = true;
	struct _TRANS_LIT_TEX_VERTEX {
		Vector3 p;
		DWORD color;   // diffuse color    
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;
#ifdef INFO_VULKAN
	DX8Wrapper::_Get_D3D_Device8()->SetTexture(0,tex);	//previously rendered frame inside this texture
#endif
	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = { (float)xpos + 1.f, (float)ypos + 1.f, 0.0f };
	v[0].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top right
	v[1].p = { (float)xpos + 1.f, -1.f, 0.0f };
	v[1].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = { (float)xpos - 1.f, (float)ypos + 1.f, 0.0f };
	v[2].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top left
	v[3].p = { (float)xpos - 1.f,  -1, 0.0f };
	v[3].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	if (m_additive) {
		DX8Wrapper::Set_DX8_Render_State(VKRS_SRCBLEND,VK_BLEND_FACTOR_SRC_ALPHA);
		DX8Wrapper::Set_DX8_Render_State(VKRS_DESTBLEND,VK_BLEND_FACTOR_ONE);
	} else { 
		DX8Wrapper::Set_DX8_Render_State(VKRS_SRCBLEND,VK_BLEND_FACTOR_SRC_ALPHA);
		DX8Wrapper::Set_DX8_Render_State(VKRS_DESTBLEND,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
	}
	DX8Wrapper::Set_DX8_Render_State(VKRS_ALPHABLENDENABLE,false);
	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	DX8Wrapper::Apply_Render_State_Changes();
#ifdef INFO_VULKAN
	pDev->SetFVF(VKFVF_XYZRHW | VKFVF_DIFFUSE | VKFVF_TEX1);
#endif

	Coord2D center;
	center.x = 0.5f;
	center.y = 0.5f;
	Bool pan = false;
	if (mode>=FM_VIEW_MB_PAN_ALPHA) {
		Real len = sqrt(scrollDelta.x*scrollDelta.x + scrollDelta.y*scrollDelta.y);
		//center.x += 0.5f * (scrollDelta.x/len);
		center.y -= 0.5f; // * (scrollDelta.y/len);
		m_decrement = false;
		m_maxCount = (len*200*m_panFactor/(Real)DEFAULT_PAN_FACTOR);
		if (m_maxCount<m_panFactor/2) 
			m_maxCount = m_panFactor/2;
		if (m_maxCount>m_panFactor) 
			m_maxCount=m_panFactor;
		pan = true;
		m_priorDelta = scrollDelta;
	} else if (mode == FM_VIEW_MB_END_PAN_ALPHA) {
		Real len = sqrt(m_priorDelta.x*m_priorDelta.x + m_priorDelta.y*m_priorDelta.y);
		center.x += 0.5f * (m_priorDelta.x/len);
		center.y -= 0.5f * (m_priorDelta.y/len);
		m_decrement = false;
		m_maxCount--;
		if (m_maxCount<2) {
			continueEffect = false;
		}
		pan = true;
	}


	m_skipRender = false;
	if (!pan && m_lastFrame != TheGameLogic->getFrame()) {
		if (m_decrement) {
			m_maxCount-=COUNT_STEP;
			if (m_maxCount<1) {
				m_decrement = false;
				continueEffect = false;
			}	else {
				m_skipRender = true;
			}
		} else {
			m_maxCount+=COUNT_STEP;
			if (m_maxCount>=MAX_COUNT) {
				m_decrement = true;
				if (m_doZoomTo && m_zoomToValid) {
					TheTacticalView->lookAt(&m_zoomToPos);
				} else {
					continueEffect = false;
				}
			}	else {
				m_skipRender = true;
			}
		}
	}
	Int	 i, j;
	if (!pan) {
		for (i=0; i<4; i++) {
			Real factor = 1.0f - (m_maxCount/(Real)MAX_COUNT)*0.90f;
			factor = sqrt(factor);
			v[i].u = ((v[i].u-center.x)*factor) + center.x;
			v[i].v = ((v[i].v-center.y)*factor) + center.y;
		}
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_ALPHAARG1, VKTA_CURRENT);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_ALPHAARG2, VKTA_TEXTURE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_ALPHAOP, VKTOP_SELECTARG1);
#ifdef INFO_VULKAN
	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
#else
	DX8Wrapper::Set_DX8_Render_State(VKRS_ALPHABLENDENABLE,true);
	Matrix4x4 ident(true);

	VK::Buffer vbo;
	VkBufferTools::CreateVertexBuffer(&WWVKRENDER, sizeof(_TRANS_LIT_TEX_VERTEX) * 4, v, vbo);

	DX8Wrapper::Apply_Render_State_Changes();
	std::vector<VkDescriptorSet> sets;
	WWVK_UpdateFVF_DUV2_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &tex, &DX8Wrapper::Get_DX8_Texture(1),
		DX8Wrapper::UboIdent(), DX8Wrapper::UboIdent());
	WWVK_DrawFVF_DUV2_Strip_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, 4, vbo.buffer, 0, (WorldMatrix*)&ident);
	WWVKRENDER.PushSingleFrameBuffer(vbo);
#endif

	DX8Wrapper::Apply_Render_State_Changes();
	{
		Int limit = m_maxCount;
		if (m_maxCount>30) limit = 30;
		for (j=0; j<limit; j++) {
			for (i=0; i<4; i++) {
				Real factor = 0.99f;
				if (m_additive) factor = 0.98f;
				Int alpha = 0x15;
				if (m_additive) {
					alpha = 0x09;
					if (m_maxCount>limit) {
						alpha += (m_maxCount-limit)/5;
					}
					if (m_maxCount==MAX_COUNT) alpha += 60;
				}
				v[i].color = (alpha<<24)|0x00ffffff; // 
				if (pan) {
					v[i].u = ((v[i].u-center.x)*(factor+.006)) + center.x;
					v[i].v = ((v[i].v-center.y)*factor) + center.y;
				} else {
					v[i].u = ((v[i].u-center.x)*factor) + center.x;
					v[i].v = ((v[i].v-center.y)*factor) + center.y;
				}
			}
#ifdef INFO_VULKAN
			pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
#else
			Matrix4x4 ident(true);

			VK::Buffer vbo;
			VkBufferTools::CreateVertexBuffer(&WWVKRENDER, sizeof(_TRANS_LIT_TEX_VERTEX) * 4, v, vbo);

			DX8Wrapper::Apply_Render_State_Changes();
			std::vector<VkDescriptorSet> sets;
			WWVK_UpdateFVF_DUV2_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &tex, &DX8Wrapper::Get_DX8_Texture(1),
				DX8Wrapper::UboIdent(), DX8Wrapper::UboIdent());
			WWVK_DrawFVF_DUV2_Strip_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, 4, vbo.buffer, 0, (WorldMatrix*)&ident);
			WWVKRENDER.PushSingleFrameBuffer(vbo);
#endif
		}
	}
	m_lastFrame = TheGameLogic->getFrame();
	if (pan){
		m_skipRender = false;
	}
	reset();
	if (!continueEffect) {
		m_zoomToValid = false;
	}
	return continueEffect;
}

Bool ScreenMotionBlurFilter::setup(enum FilterModes mode)
{

	m_additive = false;

	if (mode == FM_VIEW_MB_IN_AND_OUT_SATURATE ||
			mode == FM_VIEW_MB_IN_SATURATE ||
			mode == FM_VIEW_MB_OUT_SATURATE) {
		m_additive = true;
	}

	m_doZoomTo = false;
	if (mode == FM_VIEW_MB_IN_AND_OUT_SATURATE ||
			mode == FM_VIEW_MB_IN_AND_OUT_ALPHA ) {
		m_doZoomTo = true;
	}
	if (mode >= FM_VIEW_MB_PAN_ALPHA)	{
		m_panFactor = (int)mode - FM_VIEW_MB_PAN_ALPHA;
		if (m_panFactor<1) m_panFactor = DEFAULT_PAN_FACTOR;
	}
	m_skipRender = false;
	if (mode != FM_VIEW_MB_END_PAN_ALPHA) 
		m_maxCount = 0;
	m_decrement = false;
	m_skipRender = false;
	switch (mode) {
		case FM_VIEW_MB_OUT_SATURATE:
		case FM_VIEW_MB_OUT_ALPHA:
			m_maxCount = MAX_COUNT;
			m_decrement = TRUE;
			break;
	}
	return true;
}

Int ScreenMotionBlurFilter::set(enum FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface motion blurred

		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,NULL);
		DX8Wrapper::Set_Texture(1,NULL);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_ALWAYS);
		DX8Wrapper::Set_DX8_Render_State(VKRS_ZWRITEENABLE,FALSE);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices
	}
	return TRUE;
}

void ScreenMotionBlurFilter::reset(void)
{
	DX8Wrapper::Set_Texture(0,NULL);	//previously rendered frame inside this texture
	DX8Wrapper::Invalidate_Cached_Render_States();
}

Int ScreenMotionBlurFilter::shutdown(void)
{
	return TRUE;
}

/*===========================================================================================*/
/*=========      Shroud Shaders	=============================================================*/
/*===========================================================================================*/

///Shroud layer rendering shader
class ShroudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} shroudTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *ShroudShaderList[]=
{
	&shroudTextureShader,
	NULL
};

//#define SHROUD_STRETCH_FACTOR	(1.0f/MAP_XY_FACTOR)	//1 texel per heightmap cell width

Int ShroudTextureShader::init(void)
{
	W3DShaders[W3DShaderManager::ST_SHROUD_TEXTURE]=&shroudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_SHROUD_TEXTURE]=1;

	return TRUE;
}

//Setup a texture projection in the given stage that applies our shroud.
Int ShroudTextureShader::set(Int stage)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
	DX8Wrapper::Set_Texture(stage, W3DShaderManager::getShaderTexture(0));	//shroud always stored in texture 0

	if (stage == 0)
	{
#if defined(_DEBUG) || defined(_INTERNAL)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaSpriteShader);
	else
		DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
#else
	DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
#endif
	}
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	
	DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_EQUAL);

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != 0)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		DirectX::XMMATRIX inv;
		DirectX::XMVECTOR det;

		Matrix4x4 curView;
		DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

		inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

		DirectX::XMMATRIX scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		offset = DirectX::XMMatrixTranslation(xoffset, yoffset, 0);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		scale = DirectX::XMMatrixScaling(width, height, 1.f);
		*((DirectX::XMMATRIX *)&curView) = (inv * offset) * scale;
		DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+stage), *((Matrix4x4*)&curView));
	}
	m_stageOfSet=stage;
	return TRUE;
}

void ShroudTextureShader::reset(void)
{
	DX8Wrapper::Set_Texture(m_stageOfSet,NULL);
	DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_LESS_OR_EQUAL);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  VKTSS_TEXCOORDINDEX, m_stageOfSet);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
}

///Shroud layer rendering shader
class FlatShroudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} flatShroudTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *FlatShroudShaderList[]=
{
	&flatShroudTextureShader,
	NULL
};

//#define SHROUD_STRETCH_FACTOR	(1.0f/MAP_XY_FACTOR)	//1 texel per heightmap cell width

Int FlatShroudTextureShader::init(void)
{
	W3DShaders[W3DShaderManager::ST_FLAT_SHROUD_TEXTURE]=&flatShroudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_SHROUD_TEXTURE]=1;

	return TRUE;
}

//Setup a texture projection in the given stage that applies our shroud.
Int FlatShroudTextureShader::set(Int stage)
{
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	if (stage < 2)
		DX8Wrapper::Set_Texture(stage, W3DShaderManager::getShaderTexture(stage));
	else	//stages larger than 1 are not supported by W3D so set them directly
		DX8Wrapper::Set_DX8_Texture(stage, W3DShaderManager::getShaderTexture(stage)->Peek_D3D_Texture());

	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLORARG1, VKTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLORARG2, VKTA_CURRENT );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLOROP,   VKTOP_MODULATE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAOP,   VKTOP_DISABLE );
	//DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != 0)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		DirectX::XMMATRIX inv;
		DirectX::XMVECTOR det;

		Matrix4x4 curView;
		DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

		inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

		DirectX::XMMATRIX scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		offset = DirectX::XMMatrixTranslation(xoffset, yoffset, 0.f);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		scale = DirectX::XMMatrixScaling(width, height, 1.f);
		*((DirectX::XMMATRIX *)&curView) = (inv * offset) * scale;
		DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+stage), *((Matrix4x4*)&curView));
	}
	m_stageOfSet=stage;
	return TRUE;
}

void FlatShroudTextureShader::reset(void)
{
	if (m_stageOfSet < MAX_TEXTURE_STAGES)
		DX8Wrapper::Set_Texture(m_stageOfSet,NULL);
	DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_LESS_OR_EQUAL);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  VKTSS_TEXCOORDINDEX, m_stageOfSet);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
}

///Mask layer rendering shader
class MaskTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
} maskTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *MaskShaderList[]=
{
	&maskTextureShader,
	NULL
};

Int MaskTextureShader::init(void)
{
	W3DShaders[W3DShaderManager::ST_MASK_TEXTURE]=&maskTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_MASK_TEXTURE]=1;

	return TRUE;
}

Int MaskTextureShader::set(Int pass)
{
	Real fadeLevel=ScreenCrossFadeFilter::getCurrentFadeValue();

	//Use the current fade level to scale the mask texture
	Real radius = (1.0f-fadeLevel)*2.0f;
	if (radius <= 0)
		radius = 0.01f;
	radius = 0.5f/radius;

	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.

	//For now we're always going to project the texture coming from the crossfade effect
	DX8Wrapper::Set_Texture(0, ScreenCrossFadeFilter::getCurrentMaskTexture());
	ShaderClass shader=ShaderClass::_PresetOpaqueShader;
	shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
	DX8Wrapper::Set_Shader(shader);
	DX8Wrapper::Apply_Render_State_Changes();
	Matrix4x4 curView;
	DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);

	DirectX::XMMATRIX inv;
	DirectX::XMVECTOR det;

	//Get inverse view matrix so we can transform camera space points back to world space
	inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

	DirectX::XMMATRIX scale,offset,offsetTextureCenter;
	Coord3D centerPos;

	//Find center of projection (this should be returned from some other filter, etc. but
	//for now assume terrain location at center of screen.
	if (TheTacticalView)
	{	Int xpos,ypos;

		TheTacticalView->getOrigin(&xpos,&ypos);

		ICoord2D screenPos;
		screenPos.x=(Real)TheTacticalView->getWidth()*0.5f;
		screenPos.y=(Real)TheTacticalView->getHeight()*0.5f;
		TheTacticalView->screenToTerrain(&screenPos,&centerPos);
	}

	offset = DirectX::XMMatrixTranslation(-centerPos.x, -centerPos.y,0);

	offsetTextureCenter = DirectX::XMMatrixTranslation(0.5f, 0.5f, 0);	//shift coordinates so center of projection falls at uv 0.5,0.5

	Real worldTexelWidth=(1.0f-fadeLevel)*25.0f;	//9 worked well for circle but weird shape requires more stretch to cover.
	Real worldTexelHeight=(1.0f-fadeLevel)*25.0f;

	///@todo: Fix this to work with non 128x128 textures.
	if (worldTexelWidth != 0 && worldTexelHeight != 0)
	{	Real widthScale = 1.0f/(worldTexelWidth*128.0f);
		Real heightScale = 1.0f/(worldTexelHeight*128.0f);
		scale = DirectX::XMMatrixScaling(widthScale, heightScale, 1);
		*((DirectX::XMMATRIX *)&curView) = ((inv * offset) * scale)*offsetTextureCenter;
	}
	else
	{	scale = DirectX::XMMatrixScaling(0, 0, 1);	//scaling by 0 will set uv coordinates to 0,0
		*((DirectX::XMMATRIX *)&curView) = ((inv * offset) * scale);
	}

	DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE0, *((Matrix4x4*)&curView));
	return TRUE;
}

void MaskTextureShader::reset(void)
{
	DX8Wrapper::Set_Texture(0,NULL);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  VKTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);	
}

/*===========================================================================================*/
/*=========      Terrain Shaders	=========================================================*/
/*===========================================================================================*/


///regular terrain shader that should work on all multi-texture video cards (slowest version)
class FlatTerrainShaderPixelShader : public W3DShaderInterface
{
public:
#ifdef INFO_VULKAN
	IDirect3DPixelShader9* m_dwBasePixelShader;	///<handle to terrain D3D pixel shader
	IDirect3DPixelShader9* m_dwBaseNoise1PixelShader;	///<handle to terrain/single noise D3D pixel shader
	IDirect3DPixelShader9* m_dwBaseNoise2PixelShader;	///<handle to terrain/double noise D3D pixel shader
	IDirect3DPixelShader9* m_dwBase0PixelShader;	///<handle to terrain only pixel shader
#endif
	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int shutdown(void);			///<release resources used by shader
} flatTerrainShaderPixelShader;

//Offsets into constant register pool used by vertex shader
#define CV_WORLDVIEWPROJ_0	0	//4 vectors for transform of world->clip space.

///Pixel shader based terrain shader - fastest method for the newest cards.
class TerrainShaderPixelShader : public W3DShaderInterface
{
#ifdef INFO_VULKAN
	IDirect3DPixelShader9* m_dwBasePixelShader;	///<handle to terrain D3D pixel shader
	IDirect3DPixelShader9* m_dwBaseNoise1PixelShader;	///<handle to terrain/single noise D3D pixel shader
	IDirect3DPixelShader9* m_dwBaseNoise2PixelShader;	///<handle to terrain/double noise D3D pixel shader
#endif
public:
	float m_xSlidePerSecond;	 ///< How far the clouds move per second.
	float m_ySlidePerSecond;	 ///< How far the clouds move per second.
	int	  m_curTick;
	float m_xOffset;
	float m_yOffset;

	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init(void);
	void updateNoise1(DirectX::XMMATRIX* destMatrix, DirectX::XMMATRIX* curViewInverse, Bool doUpdate = true);
	void updateNoise2(DirectX::XMMATRIX* destMatrix, DirectX::XMMATRIX* curViewInverse, Bool doUpdate = true);
	///<perform any one time initialization and validation
	virtual Int shutdown(void);			///<release resources used by shader
} terrainShaderPixelShader;

///List of different terrain shader implementations in order of preference
W3DShaderInterface *TerrainShaderList[]=
{
	&terrainShaderPixelShader,
	NULL
};

///List of different terrain shader implementations in order of preference
W3DShaderInterface *FlatTerrainShaderList[]=
{
	&flatTerrainShaderPixelShader,
	NULL
};

void TerrainShaderPixelShader::updateNoise1(DirectX::XMMATRIX *destMatrix,DirectX::XMMATRIX *curViewInverse, Bool doUpdate)
{
	#define STRETCH_FACTOR ((float)(1/(63.0*MAP_XY_FACTOR/2))) /* covers 63/2 tiles */

	DirectX::XMMATRIX scale;

	scale = DirectX::XMMatrixScaling(STRETCH_FACTOR, STRETCH_FACTOR,1);
	*destMatrix = *curViewInverse * scale;

	DirectX::XMMATRIX offset;

	Int delta = m_curTick;
	m_curTick = WW3D::Get_Sync_Time();//::GetTickCount();
	delta = m_curTick-delta;
	m_xOffset += m_xSlidePerSecond*delta/1000;
	m_yOffset += m_ySlidePerSecond*delta/1000;


	//m_xOffset += m_xSlidePerSecond*delta/500;
	//m_yOffset += m_ySlidePerSecond*delta/500;


	//m_yOffset = sinf( (float)m_curTick * 0.0001f );
	//m_xOffset = cosf( (float)m_curTick * 0.0001f );

	while (m_xOffset > 1) m_xOffset -= 1;
	while (m_yOffset > 1) m_yOffset -= 1;
	while (m_xOffset < -1) m_xOffset += 1;
	while (m_yOffset < -1) m_yOffset += 1;

	offset = DirectX::XMMatrixTranslation(m_xOffset, m_yOffset,0);
	*destMatrix *= offset;
}

void TerrainShaderPixelShader::updateNoise2(DirectX::XMMATRIX *destMatrix,DirectX::XMMATRIX *curViewInverse, Bool doUpdate)
{
			
	DirectX::XMMATRIX scale;

	scale = DirectX::XMMatrixScaling(STRETCH_FACTOR, STRETCH_FACTOR,1);
	*destMatrix = *curViewInverse * scale;
}

Int TerrainShaderPixelShader::shutdown(void)
{
#ifdef INFO_VULKAN
	if (m_dwBasePixelShader)
		m_dwBasePixelShader->Release();

	if (m_dwBaseNoise1PixelShader)
		m_dwBaseNoise1PixelShader->Release();

	if (m_dwBaseNoise2PixelShader)
		m_dwBaseNoise2PixelShader->Release();

	m_dwBasePixelShader=NULL;
	m_dwBaseNoise1PixelShader=NULL;
	m_dwBaseNoise2PixelShader=NULL;
#endif

	return TRUE;
}

Int TerrainShaderPixelShader::init( void )
{	
	Int res;
	m_xSlidePerSecond = -0.02f;
	m_ySlidePerSecond = 1.50f * m_xSlidePerSecond;
	m_curTick = 0;
	m_curTick = WW3D::Get_Sync_Time();//::GetTickCount();
	m_xOffset = 0;
	m_yOffset = 0;
#ifdef DISABLE_PIXEL_SHADERS
	return false;
#endif
	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if ((res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader decleration
#ifdef INFO_VULKAN
			DWORD Declaration[] = { 0 };

			//base version which doesn't apply any noise textures.
			HRESULT hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\terrain.pso", Declaration, 0, false, (void**)&m_dwBasePixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 1 noise texture.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\terrainnoise.pso", Declaration, 0, false, (void**)&m_dwBaseNoise1PixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 2 noise textures.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\terrainnoise2.pso", Declaration, 0, false, (void**) & m_dwBaseNoise2PixelShader);
			if (FAILED(hr))
				return FALSE;
#endif

			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int TerrainShaderPixelShader::set(Int pass)
{	
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	//setup base pass
	DX8Wrapper::Set_Texture(0, W3DShaderManager::getShaderTexture(0));
	DX8Wrapper::Set_Texture(1, W3DShaderManager::getShaderTexture(1));

	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	//tell pixel shader which UV set to use for each stage
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXCOORDINDEX, 0 );
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXCOORDINDEX, 1 );

	if (TheGlobalData && TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MAGFILTER, VK_FILTER_NEAREST);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MAGFILTER, VK_FILTER_NEAREST);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MIPFILTER, VK_FILTER_NEAREST);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
	}

	if (W3DShaderManager::getCurrentShader() >= W3DShaderManager::ST_TERRAIN_BASE_NOISE1)
	{	
		Matrix4x4 curView;
		DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

		DirectX::XMMATRIX inv;
		DirectX::XMVECTOR det;
		inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

		DX8Wrapper::Set_DX8_Texture_Stage_State(2,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(2,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	

		DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		
		if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE12)
		{	//full shader
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 3, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 3, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);
			DX8Wrapper::Set_Texture(2, W3DShaderManager::getShaderTexture(2));
			DX8Wrapper::Set_Texture(3, W3DShaderManager::getShaderTexture(3));
#ifdef INFO_VULKAN
			DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBaseNoise2PixelShader);
#endif
			DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_TerrainNoise2);

			DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);

			DX8Wrapper::Set_DX8_Sampler_Stage_State( 3, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
			DX8Wrapper::Set_DX8_Sampler_Stage_State( 3, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);

			updateNoise1(((DirectX::XMMATRIX*)&curView),&inv);	//update curView with texture matrix
			DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE2, curView);

			updateNoise2(((DirectX::XMMATRIX*)&curView),&inv);	//update curView with texture matrix
			DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE3, curView);

			DX8Wrapper::Set_DX8_Texture_Stage_State(3,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
			// Two output coordinates are used.
			DX8Wrapper::Set_DX8_Texture_Stage_State(3,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	
		}
		else
		{	//single noise texture shader
#ifdef INFO_VULKAN
			DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBaseNoise1PixelShader);
#endif
			DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_TerrainNoise);

			if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_TERRAIN_BASE_NOISE1)
			{	//cloud map
				DX8Wrapper::Set_Texture(2, W3DShaderManager::getShaderTexture(2));
				updateNoise1(((DirectX::XMMATRIX*)&curView),&inv);	//update curView with texture matrix
				DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
				DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
			}
			else
			{	//light map
				DX8Wrapper::Set_Texture(2, W3DShaderManager::getShaderTexture(3));
				updateNoise2(((DirectX::XMMATRIX*)&curView),&inv);	//update curView with texture matrix
				DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
				DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
			}
			DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE2, curView);
		}
	}
	else
	{	//just base texturing
		DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_Terrain);
#ifdef INFO_VULKAN
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBasePixelShader);
#endif
	}

	return TRUE;
}

void TerrainShaderPixelShader::reset(void)
{
	DX8Wrapper::Set_Texture(2,NULL);	//release reference to any texture
	DX8Wrapper::Set_Texture(3,NULL);	//release reference to any texture

#ifdef INFO_VULKAN
	DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(0);	//turn off pixel shader
#endif
	DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_MAX);

	DX8Wrapper::Set_Texture(0, NULL);
	DX8Wrapper::Set_Texture(1, NULL);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|1);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|2);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|3);

	DX8Wrapper::Invalidate_Cached_Render_States();
}

///Cloud layer rendering shader - used for objects similar to terrain which only need the cloud layer.
class CloudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int stage);		///<setup shader for the specified rendering pass.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} cloudTextureShader;

///List of different cloud shader implementations in order of preference
W3DShaderInterface *CloudShaderList[]=
{
	&cloudTextureShader,
	NULL
};

Int CloudTextureShader::init(void)
{
	W3DShaders[W3DShaderManager::ST_CLOUD_TEXTURE]=&cloudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_CLOUD_TEXTURE]=1;

	return TRUE;
}

/**Setup a certain texture stage to project our cloud texture*/
Int CloudTextureShader::set(Int stage)
{
	Matrix4x4 curView;
	DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

	DirectX::XMMATRIX inv;
	DirectX::XMVECTOR det;

	inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

	//Get a texture matrix that applies the current cloud position
	terrainShaderPixelShader.updateNoise1(((DirectX::XMMATRIX*)&curView),&inv,false);	//update curView with texture matrix

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	

	DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+stage), curView);

	DX8Wrapper::Set_DX8_Sampler_Stage_State( stage, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( stage, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( stage, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( stage, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);

	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLORARG1, VKTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLORARG2, VKTA_CURRENT );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLOROP,   VKTOP_MODULATE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAARG1, VKTA_TEXTURE );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAARG2, VKTA_CURRENT );
	DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAOP,   VKTOP_MODULATE );

	DX8Wrapper::Set_Texture(stage, W3DShaderManager::getShaderTexture(stage));

	m_stageOfSet=stage;
	return TRUE;
}

void CloudTextureShader::reset(void)
{
	//Free reference to texture
	DX8Wrapper::Set_Texture(m_stageOfSet, NULL);
	//Turn off texture projection
	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|m_stageOfSet);

	DX8Wrapper::Set_DX8_Texture_Stage_State( m_stageOfSet, VKTSS_COLOROP,   VKTOP_DISABLE );
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet, VKTSS_ALPHAOP, VKTOP_DISABLE);
}

/*===========================================================================================*/
/*=========      Road Shaders	=========================================================*/
/*===========================================================================================*/
class RoadShaderPixelShader : public W3DShaderInterface
{
#ifdef INFO_VULKAN
	IDirect3DPixelShader9*					m_dwBaseNoise2PixelShader;	///<handle to road/double noise D3D pixel shader
#endif

	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual void reset(void);		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual Int shutdown(void);			///<release resources used by shader
} roadShaderPixelShader;
class RoadShader2Stage : public W3DShaderInterface
{
	friend class RoadShaderPixelShader;	//pixel shader version uses some of the same features.

	virtual Int set(Int pass);		///<setup shader for the specified rendering pass.
	virtual Int init(void);			///<perform any one time initialization and validation
	virtual void reset(void);
} roadShader2Stage;
///List of different terrain shader implementations in order of preference
W3DShaderInterface *RoadShaderList[]=
{
	&roadShaderPixelShader,
	NULL
};

Int RoadShader2Stage::init(void)
{
	//no special device validation needed - anything in our min spec should handle this.
	W3DShaders[W3DShaderManager::ST_ROAD_BASE] = &roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE] = 1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE1] = &roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE1] = 1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE2] = &roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE2] = 1;

	return TRUE;
}

Int RoadShader2Stage::set(Int pass)
{
	//First stage always contains base texture.
	DX8Wrapper::Set_Texture(0, W3DShaderManager::getShaderTexture(0));
	//Force system to apply world/view transforms.
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC, VK_COMPARE_OP_LESS_OR_EQUAL);
	DX8Wrapper::Set_DX8_Render_State(VKRS_ZWRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(VKRS_LIGHTING, FALSE);

	// Modulate the diffuse color with the texture as lighting comes from diffuse.
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_COLORARG1, VKTA_TEXTURE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_COLORARG2, VKTA_DIFFUSE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_COLOROP, VKTOP_MODULATE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_ALPHAARG1, VKTA_TEXTURE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_ALPHAARG2, VKTA_DIFFUSE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_ALPHAOP, VKTOP_MODULATE);

	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_TEXCOORDINDEX, 0);
	DX8Wrapper::Set_DX8_Render_State(VKRS_ALPHABLENDENABLE, true);	//blend roads into terrain
	if (pass == 0)
	{
		DX8Wrapper::Set_DX8_Render_State(VKRS_SRCBLEND, VK_BLEND_FACTOR_SRC_ALPHA);
		DX8Wrapper::Set_DX8_Render_State(VKRS_DESTBLEND, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);

		if (W3DShaderManager::getCurrentShader() >= W3DShaderManager::ST_ROAD_BASE_NOISE1)
		{	//second texture unit will contain a noise pass
			Matrix4x4 curView;
			DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

			DirectX::XMMATRIX inv;
			DirectX::XMVECTOR det;
			inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);


			if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex)
				DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
			else
				DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_MIPFILTER, VK_FILTER_NEAREST);

			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
			// Two output coordinates are used.
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);

			DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
			DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);

			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_COLORARG1, VKTA_TEXTURE);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_COLORARG2, VKTA_CURRENT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_COLOROP, VKTOP_MODULATE);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_ALPHAARG1, VKTA_TEXTURE);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_ALPHAARG2, VKTA_CURRENT);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_ALPHAOP, VKTOP_MODULATE);

			{	//single noise texture shader
				if (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_ROAD_BASE_NOISE1)
				{	//cloud map
					DX8Wrapper::Set_Texture(1, W3DShaderManager::getShaderTexture(1));
					terrainShaderPixelShader.updateNoise1(((DirectX::XMMATRIX*)&curView), &inv, false);	//update curView with texture matrix
					DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
					DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
				}
				else
				{	//light map
					DX8Wrapper::Set_Texture(1, W3DShaderManager::getShaderTexture(2));
					terrainShaderPixelShader.updateNoise2(((DirectX::XMMATRIX*)&curView), &inv, false);	//update curView with texture matrix

					DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
					DX8Wrapper::Set_DX8_Sampler_Stage_State(1, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);

				}
				DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE1, curView);
			}
		}
		else
		{	//just base texturing

			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_COLOROP, VKTOP_DISABLE);
			DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_ALPHAOP, VKTOP_DISABLE);

		}
	}	//pass 0

	return TRUE;
}

void RoadShader2Stage::reset(void)
{
	ShaderClass::Invalidate();

	//Free references to textures
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(0, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU | 0);

	DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State(1, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU | 1);
}

Int RoadShaderPixelShader::shutdown(void)
{
#ifdef INFO_VULKAN
	if (m_dwBaseNoise2PixelShader)
		m_dwBaseNoise2PixelShader->Release();

	m_dwBaseNoise2PixelShader=NULL;
#endif

	return TRUE;
}

Int RoadShaderPixelShader::init( void )
{	
	Int res;

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (roadShader2Stage.init() && (res = W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
#ifdef INFO_VULKAN
			DWORD Declaration[] = { 0 };
			//version which blends 2 noise textures.
			HRESULT hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\roadnoise2.pso", &Declaration[0], 0, false, (void**) & m_dwBaseNoise2PixelShader);
			if (FAILED(hr))
				return FALSE;
#endif

			//Only set this shader for use in dual noise mode.  The 2Stage shader will take care of
			//all the other modes.
			W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE12]=&roadShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int RoadShaderPixelShader::set(Int pass)
{
	DX8Wrapper::Set_Texture(0,W3DShaderManager::getShaderTexture(0));
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	//tell pixel shader which UV set to use for each stage
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXCOORDINDEX, 0 );

	DX8Wrapper::Set_DX8_Render_State(VKRS_ZFUNC,VK_COMPARE_OP_LESS_OR_EQUAL);
	DX8Wrapper::Set_DX8_Render_State(VKRS_ZWRITEENABLE,FALSE);
	DX8Wrapper::Set_DX8_Render_State(VKRS_LIGHTING, FALSE);

	DX8Wrapper::Set_DX8_Render_State(VKRS_ALPHABLENDENABLE,true);	//blend roads into terrain
	DX8Wrapper::Set_DX8_Render_State(VKRS_SRCBLEND,VK_BLEND_FACTOR_SRC_ALPHA);
	DX8Wrapper::Set_DX8_Render_State(VKRS_DESTBLEND,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
	
	Matrix4x4 curView;
	DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

	DirectX::XMMATRIX inv;
	DirectX::XMVECTOR det;
	inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex)
	{	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
	}
	else
	{	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MIPFILTER, VK_FILTER_NEAREST);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MIPFILTER, VK_FILTER_NEAREST);
	}

	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
	// Two output coordinates are used.
	DX8Wrapper::Set_DX8_Texture_Stage_State(1,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	

	DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);

	DX8Wrapper::Set_Texture(1,W3DShaderManager::getShaderTexture(1));
	DX8Wrapper::Set_Texture(2,W3DShaderManager::getShaderTexture(2));

#ifdef INFO_VULKAN
	DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBaseNoise2PixelShader);
#endif
	DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_Road);

	DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 1, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);

	DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 2, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);

	terrainShaderPixelShader.updateNoise1(((DirectX::XMMATRIX*)&curView),&inv, false);	//get texture projection matrix
	DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE1, curView);

	terrainShaderPixelShader.updateNoise2(((DirectX::XMMATRIX*)&curView),&inv, false);	//get texture projection matrix
	DX8Wrapper::_Set_DX8_Transform(VkTS::TEXTURE2, curView);

	DX8Wrapper::Set_DX8_Texture_Stage_State(2,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
	// Two output coordinates are used.
	DX8Wrapper::Set_DX8_Texture_Stage_State(2,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);

	return TRUE;
}

void RoadShaderPixelShader::reset(void)
{
#ifdef INFO_VULKAN

	DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(0);	//turn off pixel shader
#endif
	DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_MAX);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|1);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|2);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|3);

	DX8Wrapper::Invalidate_Cached_Render_States();
}

/** List of all custom shader lists - each list in this list contains variations of the same
	shader to allow it to work on different hardware configurations.
*/
W3DShaderInterface **MasterShaderList[]=
{
	TerrainShaderList,
	ShroudShaderList,
	FlatShroudShaderList,
	RoadShaderList,
	MaskShaderList,
	CloudShaderList,
	FlatTerrainShaderList,
	NULL
};

/** List of all custom filter lists - each list in this list contains variations of the same
	filter to allow it to work on different hardware configurations.
*/
W3DFilterInterface **MasterFilterList[]=
{
	ScreenDefaultFilterList,
	ScreenBWFilterList,
	ScreenMotionBlurFilterList,
	ScreenCrossFadeFilterList,
	NULL
};

// W3DShaderManager::W3DShaderManager =========================================
/** Constructor - just clears some variables */
//=============================================================================
W3DShaderManager::W3DShaderManager(void)
{
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	//m_oldRenderSurface = NULL;
	m_renderTexture = {};
	//m_newRenderSurface = NULL;
	//m_oldDepthSurface = NULL;
	m_renderingToTexture = false;
	Int i;
	for (i=0; i<W3DShaderManager::ST_MAX; i++)
	{	W3DShaders[i]=NULL;
		W3DShadersPassCount[i]=0;
	}
	for (i=0; i<FT_MAX; i++)
	{	W3DFilters[i]=NULL;
	}
	for (i=0; i<8; i++)
	{
		m_Textures[i]=NULL;
	}
	m_currentShader=(W3DShaderManager::ShaderTypes)-1;
}

// W3DShaderManager::init =======================================================
/** Walk through all shaders and find versions suitable for current hardware */
//=============================================================================
void W3DShaderManager::init(void)
{
	int i,j;

	//D3DSURFACE_DESC desc;
	// For now, check & see if we are gf3 or higher on the food chain.

	ChipsetType res=DC_UNKNOWN;
	if ((res=W3DShaderManager::getChipset()) != 0)
	{
#ifdef INFO_VULKAN
		m_currentChipset = res;	//cache the current chipset.

		//Some of our effects require an offscreen render target, so try creating it here.
		HRESULT hr=DX8Wrapper::_Get_D3D_Device8()->GetRenderTarget(1, &m_oldRenderSurface);

		if (m_oldRenderSurface)
		{
			m_oldRenderSurface->GetDesc(&desc);

			hr = DX8Wrapper::_Get_D3D_Device8()->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &m_renderTexture, nullptr);
		}
		else
		{
			hr = S_FALSE;
		}

		if (hr != S_OK)
		{
			if (m_oldRenderSurface) m_oldRenderSurface->Release();
			m_oldRenderSurface = NULL;
			m_renderTexture = NULL;
		} else {
			hr = m_renderTexture->GetSurfaceLevel(0, &m_newRenderSurface);
			if (hr != S_OK)
			{
				if (m_renderTexture) m_renderTexture->Release();
				m_renderTexture = NULL;
				m_newRenderSurface = NULL;
			}	else {
				hr = DX8Wrapper::_Get_D3D_Device8()->GetDepthStencilSurface(&m_oldDepthSurface);
				if (hr != S_OK)
				{
					if (m_newRenderSurface) m_newRenderSurface->Release();
					if (m_renderTexture) m_renderTexture->Release();
					m_renderTexture = NULL;
					m_newRenderSurface = NULL;
					m_oldDepthSurface = NULL;
				}
			}
		}
#endif
	}

	W3DShaderInterface **shaders;

	for (i=0; MasterShaderList[i] != NULL; i++)
	{	
		shaders=MasterShaderList[i];
		for (j=0; shaders[j] != NULL; j++)
		{
			if (shaders[j]->init())
				break;	//found a working shader
		}
	}
	W3DFilterInterface **filters;

	for (i=0; MasterFilterList[i] != NULL; i++)
	{	
		filters=MasterFilterList[i];
		for (j=0; filters[j] != NULL; j++)
		{
			if (filters[j]->init())
				break;	//found a working shader
		}
	}

	DEBUG_LOG(("ShaderManager ChipsetID %d\n", res));
}

// W3DShaderManager::shutdown =======================================================
/** Any shaders which allocate resources will be allowed to free them */
//=============================================================================
void W3DShaderManager::shutdown(void)
{
#ifdef INFO_VULKAN
	if (m_newRenderSurface) m_newRenderSurface->Release();
	if (m_renderTexture) m_renderTexture->Release();
	if (m_oldRenderSurface) m_oldRenderSurface->Release();
	if (m_oldDepthSurface) m_oldDepthSurface->Release();
#endif
	DX8Wrapper::_GetRenderTarget().PushSingleTexture(m_renderTexture);
	m_renderTexture = {};
	//m_newRenderSurface = NULL;
	//m_oldDepthSurface = NULL;
	//m_oldRenderSurface = NULL;
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	//release any assets associated with a shader (vertex/pixel shaders, textures, etc.)
	Int i = 0;
	for (; i<W3DShaderManager::ST_MAX; i++) {
		if (W3DShaders[i]) {
			W3DShaders[i]->shutdown();
		}
	}

	for (i=0; i < FT_MAX; i++)
	{	
		if (W3DFilters[i])
		{
			W3DFilters[i]->shutdown();
		}
	}

}

// W3DShaderManager::getShaderPasses =======================================================
/** Return number of renderig passes required in perform the desired shader on current
	hardware.  App will need to re-render the polygons this many times to complete the
	effect.
 */
//=============================================================================
Int W3DShaderManager::getShaderPasses(ShaderTypes shader)
{
	return W3DShadersPassCount[shader];
}

// W3DShaderManager::setShader =======================================================
/** Must call this method before each rendering pass in order to perform proper D3D
	setup for each shader.
 */
//=============================================================================
Int W3DShaderManager::setShader(ShaderTypes shader, Int pass)
{
	if (shader == m_currentShader && pass == m_currentShaderPass)
		return TRUE;	//shader is already set
	m_currentShader=shader;
	m_currentShaderPass = pass;
	if (W3DShaders[shader])
		return W3DShaders[shader]->set(pass);
	return FALSE;
}

// W3DShaderManager::resetShader =======================================================
/** Must call this method after all polygons and rendering passes have been submitted.
	This method allows D3D to reset itself to a default state that doesn't conflict
	with the WW3D2 Shader system.
 */
//=============================================================================
void W3DShaderManager::resetShader(ShaderTypes shader)
{	
	if (m_currentShader == ST_INVALID)
		return;	//last shader is already reset.
	if (W3DShaders[shader])
		W3DShaders[shader]->reset();
	m_currentShader = ST_INVALID;
}
// W3DShaderManager::filterPreRender =======================================================
/** Call to view filter shaders before rendering starts.
 */
//=============================================================================
Bool W3DShaderManager::filterPreRender(FilterTypes filter, Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	if (W3DFilters[filter])
	{	Bool result=W3DFilters[filter]->preRender(skipRender,scenePassMode);
		if (result)
			m_currentFilter = filter;
		return result;
	}
	return FALSE;
}

// W3DShaderManager::filterPostRender =======================================================
/** Call to view filter shaders after rendering is complete.
 */
//=============================================================================
Bool W3DShaderManager::filterPostRender(FilterTypes filter, enum FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender)
{
	if (W3DFilters[filter])
		return W3DFilters[filter]->postRender(mode, scrollDelta,doExtraRender);

	m_currentFilter = FT_NULL_FILTER;
	return FALSE;
}

// W3DShaderManager::filterPostRender =======================================================
/** Call to view filter shaders after rendering is complete.
 */
//=============================================================================
	static Bool filterSetup(FilterTypes filter, enum FilterModes mode);
Bool W3DShaderManager::filterSetup(FilterTypes filter, enum FilterModes mode)
{
	if (W3DFilters[filter])
		return W3DFilters[filter]->setup(mode);
	return FALSE;
}

/*Draws 2 triangles covering the viewport given the current render states*/
void W3DShaderManager::drawViewport(Int color)
{
#ifdef INFO_VULKAN
	LPDIRECT3DDEVICE9 pDev=DX8Wrapper::_Get_D3D_Device8();
#endif

	struct _TRANS_LIT_TEX_VERTEX {
		Vector3 p;
		DWORD color;   // diffuse color    
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = { (float)xpos + 1.f, (float)ypos + 1.f, 0.0f };
	v[0].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top right
	v[1].p = { (float)xpos + 1.f, -1.f, 0.0f };
	v[1].u = (Real)(xpos + width) / (Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = { (float)xpos - 1.f, (float)ypos + 1.f, 0.0f };
	v[2].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos + height) / (Real)TheDisplay->getHeight();
	//top left
	v[3].p = { (float)xpos - 1.f,  -1, 0.0f };
	v[3].u = (Real)(xpos) / (Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos) / (Real)TheDisplay->getHeight();
	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
#ifdef INFO_VULKAN
	pDev->SetFVF(VKFVF_XYZRHW | VKFVF_DIFFUSE | VKFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
#else
	Matrix4x4 ident(true);

	VK::Buffer vbo;
	VkBufferTools::CreateVertexBuffer(&WWVKRENDER, sizeof(_TRANS_LIT_TEX_VERTEX) * 4, v, vbo);

	DX8Wrapper::Apply_Render_State_Changes();
	std::vector<VkDescriptorSet> sets;
	WWVK_UpdateFVF_DUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &DX8Wrapper::Get_DX8_Texture(0),
		DX8Wrapper::UboIdent(), DX8Wrapper::UboIdent());
	WWVK_DrawFVF_DUV_Strip_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, 4, vbo.buffer, 0, (WorldMatrix*)&ident);
	WWVKRENDER.PushSingleFrameBuffer(vbo);
#endif
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Starts rendering to a texture.
 */
//=============================================================================
void W3DShaderManager::startRenderToTexture(void)
{	
	DEBUG_ASSERTCRASH(!m_renderingToTexture, ("Already rendering to texture - cannot nest calls."));

	if (m_renderingToTexture// || m_newRenderSurface==NULL || m_oldDepthSurface==NULL
		) return;
	WWVKRENDER.BeginFramebuffer(WWVKRENDER.swapChainExtent, VK_FORMAT_R8G8B8A8_UNORM);
	m_renderTexture = WWVKRENDER.currentImage;
#ifdef INFO_VULKAN
	HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->SetRenderTarget(1, m_newRenderSurface);
	DX8Wrapper::_Get_D3D_Device8()->SetDepthStencilSurface(m_oldDepthSurface);
	DEBUG_ASSERTCRASH(hr==S_OK, ("Set target failed unexpectedly."));
	if (hr != S_OK)
		return;
#endif
	m_renderingToTexture = true;
	if (TheGlobalData->m_showSoftWaterEdge)
	{	//Soft water edges use frame buffer destination alpha so we must clear it to a known value.
		if (m_currentFilter == FT_VIEW_MOTION_BLUR_FILTER || m_currentFilter == FT_VIEW_CROSSFADE)
		{	//these filters rely on the previous frame being visible so we must be careful about clearing
			//frame buffer.  Only clear the alpha channel
			DX8Wrapper::Set_DX8_Render_State(VKRS_COLORWRITEENABLE,(1<<3));	//only clear alpha
			ShaderClass shader=ShaderClass::_PresetOpaqueSolidShader;
			shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
			shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
			DX8Wrapper::Set_Shader(shader);

			VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
			DX8Wrapper::Set_Material(vmat);
			REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
		
			drawViewport(0x00ffffff | (((Int)(TheWaterTransparency->m_minWaterOpacity*255.0f)) <<24));
			DX8Wrapper::Set_DX8_Render_State(VKRS_COLORWRITEENABLE,(1<<0)|(1<<1)|(1<<2));	//disable writes to alpha
		}
		else	//normal clear that overwrites everything.
			DX8Wrapper::Clear(true, false, Vector3( 0.0f, 0.0f, 0.0f ), TheWaterTransparency->m_minWaterOpacity);
	}
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Ends rendering to a texture.
 */
//=============================================================================
VK::Texture W3DShaderManager::endRenderToTexture(void)
{	
	DEBUG_ASSERTCRASH(m_renderingToTexture, ("Not rendering to texture."));
	if (!m_renderingToTexture) return {};
#ifdef INFO_VULKAN
	DX8Wrapper::_Get_D3D_Device8()->SetDepthStencilSurface(m_oldDepthSurface);
	if (hr != S_OK)
	{
		Log_DX8_ErrorCode(hr);
		DEBUG_WARNING(("Set target failed unexpectedly."));
	}
	if (hr == S_OK)
#endif
	{
		//assume render target texure will be in stage 0.  Most hardware has "conditional" support for
		//non-power-of-2 textures so we must force some required states:
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		//DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSW, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MIPFILTER, VK_FILTER_MAX_ENUM);

		m_renderingToTexture = false;
	}
	WWVKRENDER.EndFramebuffer();
	return m_renderTexture;
}

/**Returns texture containing the image that was last rendered using any of the effects requiring render target
textures.  Used mostly for cross-fading effects that need an unmodified version of the view before the effect
was applied.  NOTE: This texture does not survive device reset.. so quit effect on reset!*/
VK::Texture W3DShaderManager::getRenderTexture(void)
{
	return m_renderTexture;
}

enum GraphicsVenderID
{
	DC_NVIDIA_VENDOR_ID	= 0x10DE,
	DC_3DFX_VENDOR_ID	= 0x121A,
	DC_ATI_VENDOR_ID	= 0x1002
};

// W3DShaderManager::ChipsetType =======================================================
/** Returns the chipset used by the currently active rendering device.  Can be useful
	for coding around specific driver bugs.
 */
//=============================================================================
ChipsetType W3DShaderManager::getChipset( void )
{
	//check if globaldata has an override for current chipset
	if (TheGlobalData && TheGlobalData->m_chipSetType != DC_UNKNOWN)
		return (ChipsetType)TheGlobalData->m_chipSetType;

	ChipsetType chip= DC_GENERIC_PIXEL_SHADER_2_0;
#ifdef INFO_VULKAN //Don't bother
	IDirect3D9* d3d8Interface=DX8Wrapper::_Get_D3D8();

	if (d3d8Interface && DX8Wrapper::_Get_D3D_Device8())
	{

		D3DADAPTER_IDENTIFIER9 did;
		::ZeroMemory(&did, sizeof(D3DADAPTER_IDENTIFIER9));
	/*	HRESULT res = */ d3d8Interface->GetAdapterIdentifier(0, D3DENUM_WHQL_LEVEL,&did);
		*((LARGE_INTEGER*)&m_driverVersion) = did.DriverVersion;

		if(did.VendorId == DC_NVIDIA_VENDOR_ID)
		{
			m_currentVendor = DC_NVIDIA_VENDOR_ID;

			if (did.DeviceId == 0x20)
				return DC_TNT;
   
			if (did.DeviceId >= 0x28 && did.DeviceId < 0x100)
				return DC_TNT2;

			if ( (did.DeviceId >= 0x100 && did.DeviceId <= 0x103) ||	//GeForce
				 (did.DeviceId >= 0x110 && did.DeviceId <= 0x113) ||	//GeForce2 MX
						 (did.DeviceId >= 0x150 && did.DeviceId <= 0x153) )	//GeForce2
           		return DC_GEFORCE2;

			if (did.DeviceId >= 0x200 && did.DeviceId < 0x250)
				return DC_GEFORCE3;

			if (did.DeviceId >= 0x250)
				return DC_GEFORCE4;
		}
		else
		if(did.VendorId == DC_3DFX_VENDOR_ID)
		{
			m_currentVendor = DC_3DFX_VENDOR_ID;

			if (did.DeviceId == 0x0002)
				return DC_VOODOO2;
			if (did.DeviceId == 0x0005)
				return DC_VOODOO3;
			if (did.DeviceId == 0x0008)	///@todo: Just guessing on this one - find actual Voodoo4 deviceID.
				return DC_VOODOO4;
			if (did.DeviceId == 0x0009)
				return DC_VOODOO5;
		}
		else
		if(did.VendorId == DC_ATI_VENDOR_ID)
		{
			m_currentVendor = DC_ATI_VENDOR_ID;

			if (did.DeviceId == 0x5144)
				return DC_RADEON;
			if (did.DeviceId == 0x514C)
				return DC_RADEON_8500;
			if (did.DeviceId == 0x4e44)
				return DC_RADEON_9700;
		}

		//None of the vendor specific ID's matched so use generic means to classify the card
		Int maxTextures=DX8Wrapper::Get_Current_Caps()->Get_Max_Simultaneous_Textures();
		Real pixelShaderVersion;

		char buf[256];

		//Convert version to Real
		sprintf(buf,"%d.%d",DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version(),DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Minor_Version());
		sscanf(buf,"%f",&pixelShaderVersion);

		if (maxTextures >= 4)
		{	if (pixelShaderVersion >= 1.1f)
				chip=DC_GENERIC_PIXEL_SHADER_1_1;
			if (pixelShaderVersion >= 1.4f)
				chip=DC_GENERIC_PIXEL_SHADER_1_4;
			if (maxTextures >= 8 && pixelShaderVersion >= 2.0f)
				chip=DC_GENERIC_PIXEL_SHADER_2_0;
		}
	}	//D3D8 interface and device exist. 
#endif
	
	return chip;
}

//=============================================================================
// WaterRenderObjClass::LoadAndCreateShader
//=============================================================================
/** Loads and creates a D3D pixel or vertex shader.*/
//=============================================================================
#ifdef INFO_VULKAN
HRESULT W3DShaderManager::LoadAndCreateD3DShader(char* strFilePath, const DWORD* pDeclaration, DWORD Usage, Bool ShaderType, void** pHandle)
{
	if (getChipset() < DC_GENERIC_PIXEL_SHADER_1_1)
		return E_FAIL;	//don't allow loading any shaders if hardware can't handle it.
	
	try
	{
		File *file = NULL;
#ifdef INFO_VULKAN
		HRESULT hr;
#endif

		file = TheFileSystem->openFile(strFilePath, File::READ | File::BINARY);
		if (file == NULL)
		{
			OutputDebugString("Could not find file \n" );
			return E_FAIL;
		}

		FileInfo fileInfo;
		TheFileSystem->getFileInfo(AsciiString(strFilePath), &fileInfo);
		DWORD dwFileSize = fileInfo.sizeLow;

		const DWORD* pShader = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize);
		if (!pShader)
		{
			OutputDebugString( "Failed to allocate memory to load shader\n " );
			return E_FAIL;
		}

		file->read((void *)pShader, dwFileSize);

		file->close();
		file = NULL;

#ifdef INFO_VULKAN
		if (ShaderType == TRUE)//SHADERTYPE_VERTEX)
		{
			hr = DX8Wrapper::_Get_D3D_Device8()->CreateVertexShader(pShader, (IDirect3DVertexShader9**)pHandle);
		}
		else if (ShaderType == FALSE)//SHADERTYPE_PIXEL)
		{
			hr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(pShader, (IDirect3DPixelShader9**)pHandle);
		}

		HeapFree(GetProcessHeap(), 0, (void*)pShader);

		if (FAILED(hr))
		{
			OutputDebugString( "Failed to create shader\n "); 
			return E_FAIL;
		}
#endif
	}
	catch(...)
	{
		OutputDebugString( "Error opening file \n" );
		return E_FAIL;
	}

	return S_OK;
}
#endif

//For the MP test, we're enforcing high min-spec requirements that need to be verified.
#define MIN_INTEL_CPU_FREQ	1300
#define MIN_AMD_CPU_FREQ	1100
#define MIN_ACCEPTED_FREQUENCY	1300
#define MIN_ACCEPTED_MEMORY	(1024*1024*256)	//256 MB
#define MIN_ACCEPTED_TEXTURE_MEMORY	(1024*1024*30)	//30 MB

/**Hack to give gameengine access to this function*/
Bool testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, int64_t *cpuFreq, int64_t*numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{
	return W3DShaderManager::testMinimumRequirements(videoChipType,cpuType,cpuFreq,numRAM,intBenchIndex,floatBenchIndex,memBenchIndex);
}

Bool W3DShaderManager::testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, int64_t*cpuFreq, int64_t*numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{
	if (videoChipType)
		*videoChipType = getChipset();

	if (cpuType)
	{
		*cpuType = XX;	//unknown

		//Check if it's an Athlon
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_AMD &&
				CPUDetectClass::Get_AMD_Processor() >= CPUDetectClass::AMD_PROCESSOR_ATHLON_025)
				*cpuType = K7;

		//Check if it's a P3
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_INTEL &&
				CPUDetectClass::Get_Intel_Processor() >= CPUDetectClass::INTEL_PROCESSOR_PENTIUM_III_MODEL_7)
				*cpuType = P3;
		//Check if it's a P4
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_INTEL &&
				CPUDetectClass::Get_Intel_Processor() >= CPUDetectClass::INTEL_PROCESSOR_PENTIUM4)
				*cpuType = P4;
	}

	if (cpuFreq)
		*cpuFreq=CPUDetectClass::Get_Processor_Speed();

	if (numRAM)
		*numRAM=CPUDetectClass::Get_Total_Physical_Memory();

	if (intBenchIndex && floatBenchIndex && memBenchIndex)
	{
		//RunBenchmark(0, NULL, floatBenchIndex, intBenchIndex, memBenchIndex);
	}

	return TRUE;
}

/**Try to guess how well the video card will handle the game assuming very fast CPU*/
StaticGameLODLevel W3DShaderManager::getGPUPerformanceIndex(void)
{
	ChipsetType	chipType;
	StaticGameLODLevel detailSetting=STATIC_GAME_LOD_LOW;	//assume lowest settings for now.

	if ((chipType=getChipset()) != DC_UNKNOWN)
	{	//a known video card so we can make some assumptions
		if (chipType >=	DC_GEFORCE2)
			detailSetting=STATIC_GAME_LOD_LOW;	//these cards need multiple terrain passes.
		if (chipType >= DC_GENERIC_PIXEL_SHADER_1_1)	//these cards can do terrain in single pass.
			detailSetting=STATIC_GAME_LOD_HIGH;
	}

	return detailSetting;
}

/**We need a hardware independent method to compare different CPU's.  For lack of anything better, we'll
use time to calculate PIE using a slow random number algorithm.*/

/**Used to test function call overhead*/
void add(float *sum,float *addend)
{
	*sum = *sum + *addend;
}

/**Returns seconds needed to run the test*/
Real W3DShaderManager::GetCPUBenchTime(void)
{
	float ztot, yran, ymult, ymod, x, y, z, pi, prod;
    long int low, ixran, itot, j, iprod;

  	__int64 endTime64,freq64,startTime64;
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq64);
	QueryPerformanceCounter((LARGE_INTEGER *)&startTime64);

    ztot = 0.0;
    low = 1;
    ixran = 1907;
    yran = 5813.0;
    ymult = 1307.0;
    ymod = 5471.0;
    itot = 560000;	//total iterations. This value ends up running at ~30 fps on our P4-2.2Ghz.

    for(j=1; j<=itot; j++)
    {
		iprod = 27611 * ixran;
		ixran = iprod - 74383*(long int)(iprod/74383);
		x = (float)ixran / 74383.0;
		prod = ymult * yran;
		yran = (prod - ymod*(long int)(prod/ymod));
		y = yran / ymod;
		z = x*x + y*y;
		add(&ztot,&z);
		if ( z <= 1.0 )
		{
		  low = low + 1;
		}
	}
	pi = 4.0 * (float)low/(float)itot;

	QueryPerformanceCounter((LARGE_INTEGER *)&endTime64);
	return ((double)(endTime64-startTime64)/(double)(freq64));
}


// W3DShaderManager::setShroudTex =======================================================
/** Puts the shroud texture into a texture stage.
 */
//=============================================================================
Int W3DShaderManager::setShroudTex(Int stage)
{
	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != 0)
	{	 
		DX8Wrapper::Set_Texture(stage, shroud->getShroudTexture());

		DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLORARG1, VKTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLORARG2, VKTA_CURRENT );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAARG1, VKTA_TEXTURE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAARG2, VKTA_CURRENT );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_COLOROP,   VKTOP_MODULATE );
		DX8Wrapper::Set_DX8_Texture_Stage_State( stage, VKTSS_ALPHAOP,   VKTOP_SELECTARG2 );

		DirectX::XMMATRIX inv;
		DirectX::XMVECTOR det;

		Matrix4x4 curView;
		DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

		inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

		DirectX::XMMATRIX scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		offset = DirectX::XMMatrixTranslation(xoffset, yoffset, 0.f);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		scale = DirectX::XMMatrixScaling(width, height, 1.f);
		*((DirectX::XMMATRIX *)&curView) = (inv * offset) * scale;
		DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+stage), *((Matrix4x4*)&curView));
		return TRUE;
	}
	return FALSE;
}

Int FlatTerrainShaderPixelShader::shutdown(void)
{
#ifdef INFO_VULKAN
	if (m_dwBasePixelShader)
		m_dwBasePixelShader->Release();

	if (m_dwBase0PixelShader)
		m_dwBase0PixelShader->Release();

	if (m_dwBaseNoise1PixelShader)
		m_dwBaseNoise1PixelShader->Release();

	if (m_dwBaseNoise2PixelShader)
		m_dwBaseNoise2PixelShader->Release();

	m_dwBasePixelShader=NULL;
	m_dwBase0PixelShader=NULL;
	m_dwBaseNoise1PixelShader=NULL;
	m_dwBaseNoise2PixelShader=NULL;
#endif

	return TRUE;
}

Int FlatTerrainShaderPixelShader::init( void )
{	
	Int res;

#ifdef DISABLE_PIXEL_SHADERS
	return false;
#endif

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if ((res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			//this shader needs some assets that need to be loaded
			//shader decleration
#if 0
			DWORD Declaration[]=
			{
				(D3DVSD_STREAM(0)),
				(D3DVSD_REG(0, D3DVSDT_FLOAT3)), // Position
				(D3DVSD_REG(1, D3DVSDT_D3DCOLOR)), // Diffuse
				(D3DVSD_REG(2, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_REG(3, D3DVSDT_FLOAT2)), //  Texture Coordinates
				(D3DVSD_END())
			};
#endif

#ifdef INFO_VULKAN
			//base version which doesn't apply any noise textures.
			HRESULT hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrain.pso", &Declaration[0], 0, false, (void**) & m_dwBasePixelShader);
			if (FAILED(hr))
				return FALSE;

			//base version which doesn't apply any shroud textures.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrain0.pso", &Declaration[0], 0, false, (void**)&m_dwBase0PixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 1 noise texture.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrainnoise.pso", &Declaration[0], 0, false, (void**)&m_dwBaseNoise1PixelShader);
			if (FAILED(hr))
				return FALSE;

			//version which blends 2 noise textures.
			hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\fterrainnoise2.pso", &Declaration[0], 0, false, (void**)&m_dwBaseNoise2PixelShader);
			if (FAILED(hr))
				return FALSE;

#endif
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=&flatTerrainShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int FlatTerrainShaderPixelShader::set(Int pass)
{
	//setup base pass
	Int curStage = 1;
	// setup terrain [3/31/2003]


	DX8Wrapper::Set_DX8_Sampler_Stage_State(0,  VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State(0,  VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	DX8Wrapper::Set_Texture(0, W3DShaderManager::getShaderTexture(2));
	DX8Wrapper::Set_Texture(1, W3DShaderManager::getShaderTexture(2));
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Sampler_Stage_State( curStage, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( curStage, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	//tell pixel shader which UV set to use for each stage
	DX8Wrapper::Set_DX8_Texture_Stage_State( curStage, VKTSS_TEXCOORDINDEX, 0 );
	DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);	

	if (TheGlobalData && TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MINFILTER, VK_FILTER_NEAREST);
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MAGFILTER, VK_FILTER_NEAREST);
	}
	if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MIPFILTER, VK_FILTER_LINEAR);
	} else {
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MIPFILTER, VK_FILTER_NEAREST);
	}

	curStage = 0;

	W3DShroud *shroud = TheTerrainRenderObject->getShroud();
	if (shroud) {

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);

		//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
		//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
		{	
			DirectX::XMMATRIX inv;
			DirectX::XMVECTOR det;

			Matrix4x4 curView;
			DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

			inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

			DirectX::XMMATRIX scale,offset;

			//We need to make all world coordinates be relative to the heightmap data origin since that
			//is where the shroud begins.

			float xoffset = 0;
			float yoffset = 0;
			Real width=shroud->getCellWidth();
			Real height=shroud->getCellHeight();

			if (TheTerrainRenderObject->getMap())
			{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
				xoffset = -(float)shroud->getDrawOriginX() + width;
				yoffset = -(float)shroud->getDrawOriginY() + height;
			}

			offset = DirectX::XMMatrixTranslation(xoffset, yoffset, 0.f);

			width = 1.0f/(width*shroud->getTextureWidth());
			height = 1.0f/(height*shroud->getTextureHeight());
			scale = DirectX::XMMatrixScaling(width, height, 1.f);
			*((DirectX::XMMATRIX *)&curView) = (inv * offset) * scale;
			DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+curStage), *((Matrix4x4*)&curView));
		}
		DX8Wrapper::Set_DX8_Sampler_Stage_State( curStage, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( curStage, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( curStage, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State( curStage, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_Texture(curStage, shroud->getShroudTexture());
		curStage++;
		if (curStage==1) curStage++;
	}

	Bool doNoise1 = (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1 ||
						W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12);
	if (doNoise1) {	 // Cloud pass.
		Matrix4x4 curView;
		DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

		DirectX::XMMATRIX inv;
		DirectX::XMVECTOR det;
		inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	

		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage,  VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage,  VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);

		DX8Wrapper::Set_Texture(curStage, W3DShaderManager::getShaderTexture(2));
		terrainShaderPixelShader.updateNoise1(((DirectX::XMMATRIX*)&curView),&inv);	//update curView with texture matrix
		DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+curStage), *((Matrix4x4*)&curView));

		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
		
		curStage++;
		if (curStage==1) curStage++;
	}

	Bool doNoise2 = (W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2 ||
						W3DShaderManager::getCurrentShader() == W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12);
	if (doNoise2)
	{	
		Matrix4x4 curView;
		DX8Wrapper::_Get_DX8_Transform(VkTS::VIEW, curView);

		DirectX::XMMATRIX inv;
		DirectX::XMVECTOR det;
		inv = DirectX::XMMatrixInverse(&det, *(DirectX::XMMATRIX*)&curView);

		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  VKTSS_TEXCOORDINDEX, VKTSS_TCI_CAMERASPACEPOSITION);
		// Two output coordinates are used.
		DX8Wrapper::Set_DX8_Texture_Stage_State(curStage,  VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_COUNT2);	

		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage,  VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage,  VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_REPEAT);

		DX8Wrapper::Set_Texture(curStage, W3DShaderManager::getShaderTexture(3));
		terrainShaderPixelShader.updateNoise2(((DirectX::XMMATRIX*)&curView),&inv);	//update curView with texture matrix
		DX8Wrapper::_Set_DX8_Transform((VkTransformState )((int)VkTS::TEXTURE0+curStage), *((Matrix4x4*)&curView));
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
		DX8Wrapper::Set_DX8_Sampler_Stage_State(curStage, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
		
		curStage++;
		if (curStage==1) curStage++;
	}
#ifdef INFO_VULKAN
	if (curStage<2) {
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBase0PixelShader);
	}	else if (curStage==2) {
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBasePixelShader);
	}	else if (curStage==3) {
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBaseNoise1PixelShader);
	}else if (curStage==4) {
		DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(m_dwBaseNoise2PixelShader);
	}
#else
	if (curStage < 2) {
		DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_FVF_DUV);
	}
	else if (curStage == 2) {
		DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_FTerrain);
	}
	else if (curStage == 3) {
		DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_FTerrainNoise);
	}
	else if (curStage == 4) {
		DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_FTerrainNoise2);
	}
#endif
	DX8Wrapper::Set_DX8_Render_State(VKRS_ALPHABLENDENABLE, false);
	DX8Wrapper::Apply_Render_State_Changes();
	DX8Wrapper::Set_Texture(curStage, W3DShaderManager::getShaderTexture(3));
	return TRUE;
}

void FlatTerrainShaderPixelShader::reset(void)
{
	DX8Wrapper::Set_Texture(2,NULL);	//release reference to any texture
	DX8Wrapper::Set_Texture(3,NULL);	//release reference to any texture


	DX8Wrapper::Set_Texture(0, NULL);
	DX8Wrapper::Set_Texture(1, NULL);

#ifdef INFO_VULKAN
	DX8Wrapper::_Get_D3D_Device8()->SetPixelShader(0);	//turn off pixel shader
#endif
	DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_MAX);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 0, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|0);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 1, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|1);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 2, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|2);

	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, VKTSS_TEXTURETRANSFORMFLAGS, VKTTFF_DISABLE);
	DX8Wrapper::Set_DX8_Texture_Stage_State( 3, VKTSS_TEXCOORDINDEX, VKTSS_TCI_PASSTHRU|3);

	DX8Wrapper::Set_Pipeline(PIPELINE_WWVK_MAX);

	DX8Wrapper::Invalidate_Cached_Render_States();
}





