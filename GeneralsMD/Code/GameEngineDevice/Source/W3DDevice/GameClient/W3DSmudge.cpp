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

// W3DSmudge.cpp ////////////////////////////////////////////////////////////////////////////////
// Smudge System implementation
// Author: Mark Wilczynski, June 2003
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "Lib/Basetype.h"
#include "always.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GameMemory.h"
#include "GameClient/view.h"
#include "GameClient/display.h"
#include "WW3D2/texture.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/sortingrenderer.h"

#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

SmudgeManager *TheSmudgeManager=NULL;

W3DSmudgeManager::W3DSmudgeManager(void)
{
}

W3DSmudgeManager::~W3DSmudgeManager()
{
	ReleaseResources();
}

void W3DSmudgeManager::init(void)
{
	SmudgeManager::init();
	ReAcquireResources();
}

void W3DSmudgeManager::reset (void)
{
	SmudgeManager::reset();	//base
}

void W3DSmudgeManager::ReleaseResources(void)
{
#ifdef USE_COPY_RECTS
	REF_PTR_RELEASE(m_backgroundTexture);
#endif
	REF_PTR_RELEASE(m_indexBuffer);
}

//Make sure (SMUDGE_DRAW_SIZE * 12) < 65535 because that's the max index buffer size.
#define SMUDGE_DRAW_SIZE	500	//draw at most 50 smudges per call. Tweak value to improve CPU/GPU parallelism.

void W3DSmudgeManager::ReAcquireResources(void)
{
	ReleaseResources();

#ifdef INFO_VULKAN
	SurfaceClass *surface=DX8Wrapper::_Get_DX8_Back_Buffer();
	SurfaceClass::SurfaceDescription surface_desc;

	surface->Get_Description(surface_desc);
	REF_PTR_RELEASE(surface);

	#ifdef USE_COPY_RECTS
	m_backgroundTexture = MSGNEW("TextureClass") TextureClass(TheTacticalView->getWidth(),TheTacticalView->getHeight(),surface_desc.Format,MIP_LEVELS_1,TextureClass::POOL_DEFAULT, true);
	#endif

	m_backBufferWidth = surface_desc.Width;
	m_backBufferHeight = surface_desc.Height;
#else
	m_backBufferWidth = WWVKRENDER.swapChainExtent.width;
	m_backBufferHeight = WWVKRENDER.swapChainExtent.height;
#endif

	m_indexBuffer=NEW_REF(DX8IndexBufferClass,(SMUDGE_DRAW_SIZE*4*3));	//allocate 4 triangles per smudge, each with 3 indices.

	// Fill up the IB with static vertex indices that will be used for all smudges.
	{
		DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
		UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
		//quad of 4 triangles:
		//	0-----3
		//  |\   /|
		//  |  4  |
		//	|/   \|
		//  1-----2
		Int vbCount=0;
		for (Int i=0; i<SMUDGE_DRAW_SIZE; i++)
		{
			//Top
			ib[0]=vbCount;
			ib[1]=vbCount+4;
			ib[2]=vbCount+3;
			//Right
			ib[3]=vbCount+3;
			ib[4]=vbCount+4;
			ib[5]=vbCount+2;
			//Bottom
			ib[6]=vbCount+2;
			ib[7]=vbCount+4;
			ib[8]=vbCount+1;
			//Left
			ib[9]=vbCount+1;
			ib[10]=vbCount+4;
			ib[11]=vbCount+0;

			vbCount += 5;
			ib+=12;
		}
	}
}

/*Copies a portion of the current render target into a specified buffer*/
#ifdef INFO_VULKAN
Int copyRect(unsigned char *buf, Int bufSize, int oX, int oY, int width, int height)
{
	Int result = 0;
 	IDirect3DSurface9 *surface=NULL;	///<previous render target
 	IDirect3DSurface9 *tempSurface=NULL;
	HRESULT hr = S_OK;

 	LPDIRECT3DDEVICE9 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	if (!m_pDev)
		goto error;

 	m_pDev->GetRenderTarget(1, &surface);

	if (!surface)
		goto error;

 	D3DSURFACE_DESC desc;
 
 	surface->GetDesc(&desc);
 
	RECT srcRect;
	srcRect.left=oX;
	srcRect.top=oY;
	srcRect.right=oX+width;
	srcRect.bottom=oY+height;

	POINT dstPoint;
	dstPoint.x=0;
	dstPoint.y=0;

 	hr=m_pDev->CreateOffscreenPlainSurface(  width, height, desc.Format, D3DPOOL_SYSTEMMEM, &tempSurface, nullptr);

	if (hr != S_OK)
		goto error;
 
 	hr=m_pDev->UpdateSurface(surface,&srcRect,tempSurface,&dstPoint);

	if (hr != S_OK)
		goto error;

 	D3DLOCKED_RECT lrect;
 
 	hr=tempSurface->LockRect(&lrect,NULL,D3DLOCK_READONLY);

	if (hr != S_OK)
		goto error;

 	tempSurface->GetDesc(&desc);

	if (desc.Width*desc.Height*4 < (uint32_t)bufSize)
		bufSize = (int)(desc.Width * desc.Height * 4);
		
	memcpy(buf,lrect.pBits,bufSize);
	result = bufSize;

error:
	if (surface)
		surface->Release();
	if (tempSurface)
		tempSurface->Release();
	return result;
}
#endif

#define UNIQUE_COLOR	(0x12345678)
#define BLOCK_SIZE	(8)

void W3DSmudgeManager::render(RenderInfoClass &rinfo)
{
	//Verify that the card supports the effect.
	CameraClass &camera=rinfo.Camera;
	Vector3 vsVert;
	Vector4 ssVert;
	Real uvSpanX,uvSpanY;
	Vector3 vertex_offsets[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

#define THE_COLOR (0x00ffeedd)

	UnsignedInt vertexDiffuse[5]={THE_COLOR,THE_COLOR,THE_COLOR,THE_COLOR,THE_COLOR};

	Matrix4x4 proj;
	Matrix3D view;

	camera.Get_View_Matrix(&view);
	camera.Get_Projection_Matrix(&proj);

	SurfaceClass::SurfaceDescription surface_desc;
#ifdef USE_COPY_RECTS
	SurfaceClass *background=m_backgroundTexture->Get_Surface_Level();
	background->Get_Description(surface_desc);
#else
	//D3DSURFACE_DESC D3DDesc;

	VK::Texture backTexture=W3DShaderManager::getRenderTexture();
	if (!backTexture.image || !W3DShaderManager::isRenderingToTexture())
		return;	//this card doesn't support render targets.

	//backTexture->GetLevelDesc(0,&D3DDesc);

	surface_desc.Width = backTexture.width;
	surface_desc.Height = backTexture.height;
#endif

	Real texClampX = (Real)TheTacticalView->getWidth()/(Real)surface_desc.Width;
	Real texClampY = (Real)TheTacticalView->getHeight()/(Real)surface_desc.Height;

	Real texScaleX = texClampX*0.5f;
	Real texScaleY = texClampY*0.5f;

	//Do a first pass over the smudges to determine how many are visible
	//and to fill in their world-space positions and screen uv coordinates.
	//TODO: Optimize out this extra pass!
	//TODO: Find size of screen rectangle that actually needs copying.

	SmudgeSet *set=m_usedSmudgeSetList.Head();	//first set that didn't fit into render batch.
	Int count = 0;

	if (set)
	{	//there are possibly some smudges to render, so make sure background particles have finished drawing.
		SortingRendererClass::Flush();	//draw sorted translucent polys like particles.
	}

	while (set)
	{
		Smudge *smudge=set->getUsedSmudgeList().Head();

		while (smudge)
		{
			//Get view-space center
			Matrix3D::Transform_Vector(view,smudge->m_pos,&vsVert);

			//Get 5 view-space vertices
			Smudge::smudgeVertex *verts=smudge->m_verts;

			//Do center vertex outside 'for' loop since it's different.
			verts[4].pos = vsVert;

			for (Int i=0; i<4; i++)
			{
				verts[i].pos = vsVert + vertex_offsets[i] * smudge->m_size;
				//Ge uv coordinates for each vertex
				ssVert = proj * verts[i].pos;
				Real oow = 1.0f/ssVert.W;
				ssVert *= oow;	//returned in camera space which is -1,-1 (bottom-left) to 1,1 (top-right)
				//convert camera space to uv space: 0,0 (top-left), 1,1 (bottom-right)
				verts[i].uv.Set((ssVert.X+1.0f)*texScaleX,(1.0f-ssVert.Y)*texScaleY);

				Vector2 &thisUV=verts[i].uv;

				//Clamp coordinates so we're not referencing texels outside the view.
				if (thisUV.X > texClampX)
					smudge->m_offset.X = 0;
				else
				if (thisUV.X < 0)
					smudge->m_offset.X = 0;

				if (thisUV.Y > texClampY)
					smudge->m_offset.Y = 0;
				else
				if (thisUV.Y < 0)
					smudge->m_offset.Y = 0;

			}

			//Finish center vertex
			//Ge uv coordinates by interpolating corner uv coordinates and applying desired offset.
			uvSpanX=verts[3].uv.X - verts[0].uv.X;
			uvSpanY=verts[1].uv.Y - verts[0].uv.Y;
			verts[4].uv.X=verts[0].uv.X+uvSpanX*(0.5f+smudge->m_offset.X);
			verts[4].uv.Y=verts[0].uv.Y+uvSpanY*(0.5f+smudge->m_offset.X);

			count++;	//increment visible smudge count.
			smudge=smudge->Succ();
		}

		set=set->Succ();	//advance to next node.
	}

	if (!count)
	{
#ifdef USE_COPY_RECTS
		REF_PTR_RELEASE(background);
#endif
		return;	//nothing to render.
	}

#ifdef USE_COPY_RECTS
	SurfaceClass *backBuffer=DX8Wrapper::_Get_DX8_Back_Buffer();

	backBuffer->Get_Description(surface_desc);

	//Copy the area of backbuffer occupied by smudges into an alternate buffer.
	background->Copy(0,0,0,0,surface_desc.Width,surface_desc.Height,backBuffer);

	REF_PTR_RELEASE(background);
	REF_PTR_RELEASE(backBuffer);
#endif

	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(VkTS::WORLD,identity);	
	DX8Wrapper::Set_Transform(VkTS::VIEW,identity);	

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);
	//DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueSpriteShader);

	DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);
#ifdef USE_COPY_RECTS
	DX8Wrapper::Set_Texture(0,m_backgroundTexture);
#else
	DX8Wrapper::Set_DX8_Texture(0,backTexture);
	//Need these states in case texture is non-power-of-2
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_ADDRESSV, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MAGFILTER, VK_FILTER_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_Stage_State( 0, VKSAMP_MINFILTER, VK_FILTER_LINEAR);
	DX8Wrapper::Set_DX8_Sampler_Stage_State(0, VKSAMP_MIPFILTER, VK_FILTER_NEAREST);
#endif
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	DX8Wrapper::Apply_Render_State_Changes();

	//Disable reading texture alpha since it's undefined.
	//DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKTSS_COLOROP,VKTOP_SELECTARG1);		
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKTSS_ALPHAOP,VKTOP_SELECTARG2);

	Int smudgesRemaining=count;
	set=m_usedSmudgeSetList.Head();	//first smudge set that needs rendering.
	Smudge	*remainingSmudgeStart=set->getUsedSmudgeList().Head();	//first smudge that needs rendering.

	while (smudgesRemaining)	//keep drawing smudges until we run out.
	{
		//Now that we know how many smudges need rendering, allocate vertex buffer space and copy verts.
		count=smudgesRemaining;

		if (count > SMUDGE_DRAW_SIZE)
			count = SMUDGE_DRAW_SIZE;

		Int smudgesInRenderBatch=0;

		DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,count*5);	//allocate 5 verts per smudge.
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb_access);
			VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();

			while (set)
			{
				Smudge *smudge=remainingSmudgeStart;

				while (smudge)
				{
					Smudge::smudgeVertex *smVerts = smudge->m_verts;

					//Check if we exceeded maximum number of smudges allowed per draw call.
					if (smudgesInRenderBatch >= count)
					{	remainingSmudgeStart = smudge;
						goto flushSmudges;
					}

					//Set center vertex opacity.
					vertexDiffuse[4] = ((Int)(smudge->m_opacity * 255.0f) << 24) | THE_COLOR;

					for (Int i=0; i<5; i++)
					{
						verts->x=smVerts->pos.X;
						verts->y=smVerts->pos.Y;
						verts->z=smVerts->pos.Z;
						verts->nx=0;	//keep AGP write-combining active
						verts->ny=0;
						verts->nz=0;
						verts->diffuse=vertexDiffuse[i];	//set to transparent
						verts->u1=smVerts->uv.X;
						verts->v1=smVerts->uv.Y;
						verts->u2=0;	//keep AGP write-combining active
						verts->v2=0;
						verts++;
						smVerts++;
					}

					smudgesInRenderBatch++;
					smudge=smudge->Succ();
				}

				set=set->Succ();	//advance to next node.

				if (set)	//start next batch at beginning of set.
					remainingSmudgeStart = set->getUsedSmudgeList().Head();
			}
flushSmudges:
			DX8Wrapper::Set_Vertex_Buffer(vb_access);
		}

		//DX8Wrapper::Draw_Triangles(	0,smudgesInRenderBatch*4, 0, smudgesInRenderBatch*5);
		DX8Wrapper::Apply_Render_State_Changes();
		WWVKDSV;
		auto pipelines = DX8Wrapper::FindClosestPipelines(DX8Wrapper::Get_Vertex_Buffer()->FVF_Info().FVF);
		assert(pipelines.size() == 1);
		switch (pipelines[0]) {
		case PIPELINE_WWVK_FVF_NDUV2_NOL:
			WWVK_UpdateFVF_NDUV2_NOLDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &backTexture, &backTexture, DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_NDUV2_NOL(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				m_indexBuffer->Get_DX8_Index_Buffer().buffer, smudgesInRenderBatch * 4 * 3, 0, VK_INDEX_TYPE_UINT16,
				((DX8VertexBufferClass*)vb_access.Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer, 0, (WorldMatrix*)&identity);
			break;
		case PIPELINE_WWVK_FVF_NDUV2_NOL_DROPTEX_NoTexAlpha:
			WWVK_UpdateFVF_NDUV2_NOL_DROPTEX_NoTexAlphaDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, &backTexture, 
				DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_NDUV2_NOL_DROPTEX_NoTexAlpha(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				m_indexBuffer->Get_DX8_Index_Buffer().buffer, smudgesInRenderBatch*4*3,0,VK_INDEX_TYPE_UINT16,
				((DX8VertexBufferClass*)vb_access.Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer, 0, (WorldMatrix*)&identity);
			break;
		default:assert(false);
		}

//Debug Code which draws outline around smudge
/*		DX8Wrapper::_Get_D3D_Device8()->SetRenderState(VKRS_FILLMODE,VK_POLYGON_MODE_LINE);
		DX8Wrapper::_Get_D3D_Device8()->SetRenderState(VKRS_ALPHABLENDENABLE,FALSE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKTSS_COLOROP,VKTOP_SELECTARG2);			
		DX8Wrapper::Draw_Triangles(	0,smudgesInRenderBatch*4, 0, smudgesInRenderBatch*5);	
		DX8Wrapper::_Get_D3D_Device8()->SetRenderState(VKRS_FILLMODE,VK_POLYGON_MODE_FILL);
		DX8Wrapper::_Get_D3D_Device8()->SetRenderState(VKRS_ALPHABLENDENABLE,TRUE);
		DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKTSS_COLOROP,VKTOP_SELECTARG1);			
*/
		smudgesRemaining -= smudgesInRenderBatch;
	}
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKTSS_COLOROP,VKTOP_MODULATE);			
	DX8Wrapper::Set_DX8_Texture_Stage_State(0,VKTSS_ALPHAOP,VKTOP_MODULATE);
}