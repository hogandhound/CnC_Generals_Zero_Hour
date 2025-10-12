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

// FILE: W3DSnow.h /////////////////////////////////////////////////////////

#include "W3DDevice/GameClient/W3DSnow.h"
#include "W3DDevice/GameClient/heightmap.h"
#include "GameClient/View.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/assetmgr.h"


#ifdef _INTERNAL
// for occasional debugging...
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

#define VKFVF_POINTVERTEX (VKFVF_XYZ)
#define SNOW_BUFFER_SIZE 4096	//size of vertex buffer holding particles.
#define SNOW_BATCH_SIZE	2048	//we render at most this many particles per drawprimitive call.  This number * 6 must be less than 65536 to fit into index buffer.

struct POINTVERTEX
{
    Vector3 v;	//center of particle.
};

W3DSnowManager::W3DSnowManager(void)
{
	m_indexBuffer=NULL;
	m_snowTexture=NULL;
	m_VertexBufferD3D = {};
}

W3DSnowManager::~W3DSnowManager()
{
	ReleaseResources();
}

void W3DSnowManager::init( void )
{
	SnowManager::init();
	ReAcquireResources();
}

/** Releases all W3D/D3D assets before a reset.. */
void W3DSnowManager::ReleaseResources(void)
{
	REF_PTR_RELEASE(m_snowTexture);

	DX8Wrapper::_GetRenderTarget().PushSingleFrameBuffer(m_VertexBufferD3D);
	m_VertexBufferD3D = {};

	REF_PTR_RELEASE(m_indexBuffer);
}

/** (Re)allocates all W3D/D3D assets after a reset.. */
Bool W3DSnowManager::ReAcquireResources(void)
{
	ReleaseResources();

	if (!TheWeatherSetting->m_snowEnabled)
		return TRUE;	//no need for resources if snow is disabled.

	if (TheWeatherSetting->m_usePointSprites
#ifdef INFO_VULKAN
		&& DX8Wrapper::Get_Current_Caps()->Support_PointSprites()
#endif
		)
	{
#ifdef INFO_VULKAN
		LPDIRECT3DDEVICE9 m_pDev=DX8Wrapper::_Get_D3D_Device8();

		DEBUG_ASSERTCRASH(m_pDev, ("Trying to ReAquireResources on W3DSnowManager without device"));

		if (m_VertexBufferD3D == NULL)
		{	// Create vertex buffer

			if (FAILED(m_pDev->CreateVertexBuffer
			(
				SNOW_BUFFER_SIZE*sizeof(POINTVERTEX),
				D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC|D3DUSAGE_POINTS, 
				VKFVF_POINTVERTEX,
				D3DPOOL_DEFAULT, 
				&m_VertexBufferD3D, nullptr
			)))
				return FALSE;
		}
#endif
	}
	else
	{
		m_indexBuffer=NEW_REF(DX8IndexBufferClass,(SNOW_BATCH_SIZE *6));	//allocate 2 triangles per flake, each with 3 indices.

		// Fill up the IB with static vertex indices that will be used for all smudges.
		{
			DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
			UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
			//quad of 4 triangles:
			//	0-----3
			//  |\   /|
			//  |  X  |
			//	|/   \|
			//  1-----2
			Int vbCount=0;
			for (Int i=0; i<SNOW_BATCH_SIZE; i++)
			{
				//Top
				ib[0]=vbCount+3;
				ib[1]=vbCount;
				ib[2]=vbCount+2;
				//Bottom
				ib[3]=vbCount+2;
				ib[4]=vbCount;
				ib[5]=vbCount+1;
		
				vbCount += 4;
				ib+=6;
			}
		}
	}

	m_snowTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWeatherSetting->m_snowTexture.str());

	m_dwBase = SNOW_BUFFER_SIZE;
	m_dwDiscard = SNOW_BUFFER_SIZE;
	m_dwFlush = SNOW_BATCH_SIZE;

	return TRUE;
}

void W3DSnowManager::updateIniSettings(void)
{
	//Call base class
	SnowManager::updateIniSettings();

	if (m_snowTexture && _stricmp(m_snowTexture->Get_Texture_Name(),TheWeatherSetting->m_snowTexture.str()) != 0)
	{	
		REF_PTR_RELEASE(m_snowTexture);
		m_snowTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWeatherSetting->m_snowTexture.str());
	}
}

void W3DSnowManager::reset( void )
{
	SnowManager::reset();
}

void W3DSnowManager::update(void)
{

	m_time += WW3D::Get_Frame_Time() / 1000.0f;

	//find current time offset, adjusting for overflow
	m_time=fmod(m_time,m_fullTimePeriod);
}

#define MAXIMUM_CAMERA_DISTANCE 100000	//maximum distance of camera position from world origin.
#define ISPOW2(x)  (x && (x & (x-1)) == 0)	//is a number a power of 2?
#define MODPOW2(x,y) ((x) & (y-1))		//mod '%' operator for powers of 2.

// Helper function to stuff a FLOAT into a DWORD argument
inline DWORD FtoDW( FLOAT f ) { return *((DWORD*)&f); }

/*Recursively subdivide the large snow box enclosing the camera until we reach some predefined leaf size.  This
method is used so that very few off-screen particles end up getting rendered.  Culling them individually would
be too expensive since we're dealing with 1000's for this effect.*/
void W3DSnowManager::renderSubBox(RenderInfoClass &rinfo, Int originX, Int originY, Int cubeDimX, Int cubeDimY )
{
	//check if this box is too large and needs subdivision
	Int boxDimX=cubeDimX - originX;
	Int boxDimY=cubeDimY - originY;
	Int halfX=REAL_TO_INT_CEIL(boxDimX*0.5f);
	Int halfY=REAL_TO_INT_CEIL(boxDimY*0.5f);

	CameraClass &camera=rinfo.Camera;
	MinMaxAABoxClass mmbox;

	if (boxDimX > m_leafDim)
	{	//subdivide the box
		if (boxDimY > m_leafDim)
		{	//subdivide in both directions
			//Upper left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, (originY + halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX, originY + halfY, originX + halfX, cubeDimY);
			//Upper right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, (originY + halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY + halfY,cubeDimX, cubeDimY);
			//Lower left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX,originY,originX + halfX, originY + halfY);
			//Lower right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY, cubeDimX, originY + halfY);
			return;
		}
		else
		{	//only subdivide in x direction.
			//Left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX, originY, originX + halfX, cubeDimY);
			//Right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY, cubeDimX, cubeDimY);
			return;
		}
	}
	else
	if (boxDimY > m_leafDim)
	{	//only subdivide in y direction
		//Top
		mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, (originY+halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
		mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
		if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
			renderSubBox(rinfo, originX, originY+halfY,cubeDimX, cubeDimY);
		//Bottom
		mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
		mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
		if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
			renderSubBox(rinfo, originX, originY, cubeDimX, originY + halfY);
		return;
	}

	//Box too small to subdivide so render it.

	//Find total number of particles that need rendering.
	Int totalPart=(cubeDimY-originY)*(cubeDimX-originX);

	if (!totalPart)
		return;	//nothing to render.

	Int y=originY;	//loop counter.
	Int cubeOriginXRemainder = originX;	//loop counter - adjusted when not all particles fit into render buffer.
	Vector3 snowCenter;

	m_totalRendered += totalPart;

	while (totalPart)
	{
		Int batchSize=totalPart;

		if (batchSize > m_dwFlush)
			batchSize = m_dwFlush;

		if((m_dwBase + batchSize) > m_dwDiscard)
			m_dwBase = 0;

		std::vector<POINTVERTEX> verts;

#ifdef INFO_VULKAN
		if(m_VertexBufferD3D->Lock(m_dwBase * sizeof(POINTVERTEX), batchSize * sizeof(POINTVERTEX),
			(void **) &verts, m_dwBase ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD) != D3D_OK )
			return;	//couldn't lock buffer.
#else
		verts.resize(batchSize);
#endif

		Int numberInBatch=0;

		for (;y<cubeDimY; y++)
		{
			for (Int x=cubeOriginXRemainder; x<cubeDimX; x++)
			{
				if (numberInBatch >= batchSize)
				{	cubeOriginXRemainder = x;
					goto flush_particles;
				}

				//Get initial height from noise table.  We add a large value to make sure it's positive.  Then
				//modulate by table dimensions to find a value.
				Int noiseOffset=MODPOW2(x+MAXIMUM_CAMERA_DISTANCE,SNOW_NOISE_X)+MODPOW2(y+MAXIMUM_CAMERA_DISTANCE,SNOW_NOISE_Y)*SNOW_NOISE_X;
				if (noiseOffset > (SNOW_NOISE_X * SNOW_NOISE_Y))
					noiseOffset = 0;	//this should never happen but check to prevent buffer over/under flow.

				//find current height
				Real h0=m_snowCeiling-fmod(m_heightTraveled+m_startingHeights[noiseOffset],m_boxDimensions);

				//find world-space position of snow flake
				snowCenter.Set(x*m_emitterSpacing,y*m_emitterSpacing,h0);

				//Adjust position so snow flakes don't fall straight down.
				snowCenter.X += m_amplitude * WWMath::Fast_Sin( h0 * m_frequencyScaleX + (Real)x); 
				snowCenter.Y += m_amplitude * WWMath::Fast_Sin( h0 * m_frequencyScaleY + (Real)y); 

				*(Vector3 *)&verts[numberInBatch] = snowCenter;

				numberInBatch++;
			}
			//getting here means we did not overflow the render buffer, so reset x origin to normal.
			cubeOriginXRemainder = originX;	//reset to normal amount
		}

	flush_particles:
#ifdef INFO_VULKAN
		m_VertexBufferD3D->Unlock();
#endif

		DX8Wrapper::_GetRenderTarget().PushSingleFrameBuffer(m_VertexBufferD3D);
		VkBufferTools::CreateVertexBuffer(&DX8Wrapper::_GetRenderTarget(), numberInBatch * sizeof(POINTVERTEX),
			verts.data(), m_VertexBufferD3D);
		//Render any particles that may be queued up.
		if (numberInBatch)
		{
			Debug_Statistics::Record_DX8_Polys_And_Vertices(numberInBatch*2,numberInBatch*4,ShaderClass::_PresetOpaqueShader);
#ifdef INFO_VULKAN
			DX8Wrapper::_Get_D3D_Device8()->DrawPrimitive( D3DPT_POINTLIST, m_dwBase, numberInBatch);
#endif
			std::vector<VkDescriptorSet> sets;
			Matrix4x4 identity(true);
			WWVK_UpdateSnowDescriptorSets(&WWVKRENDER, WWVKPIPES, sets, (VK::Texture*) & m_snowTexture->Peek_D3D_Base_Texture(), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawSnow_NI(WWVKPIPES, WWVKRENDER.currentCmd, sets, numberInBatch, m_VertexBufferD3D.buffer, 0, (WorldMatrix*)&identity);
			totalPart -= numberInBatch;
			m_dwBase += numberInBatch;
		}

	}
}

void W3DSnowManager::render(RenderInfoClass &rinfo)
{
	if (!TheWeatherSetting->m_snowEnabled || !m_isVisible)
		return;

	//make sure the noise table is powers of 2 in dimensions.
	WWASSERT(ISPOW2(SNOW_NOISE_X) && ISPOW2(SNOW_NOISE_Y));

	//CameraClass &camera=rinfo.Camera;

	const Coord3D &cPos=TheTacticalView->get3DCameraPosition();
	Vector3 camPos(cPos.x,cPos.y,cPos.z);

	//Number of emitters from cube center to edge of visible extent.
	Int mumEmittersInHalf=(Int)floor(m_boxDimensions / m_emitterSpacing * 0.5f);

	//Find origin of visible cube surrounding camera.
	Int cubeCenterX=(Int)floor(camPos.X/m_emitterSpacing);
	Int cubeCenterY=(Int)floor(camPos.Y/m_emitterSpacing);

	//Find extents of visible cube surrounding camera.
	Int cubeOriginX=cubeCenterX - mumEmittersInHalf;	//top/left extents.
	Int cubeOriginY=cubeCenterY - mumEmittersInHalf;
	Int cubeDimX=cubeCenterX + mumEmittersInHalf;		//bottom/right extents.
	Int cubeDimY=cubeCenterY + mumEmittersInHalf;

 	const FrustumClass & frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);

	//Particles move outside the visible box as a result of local sine movement
	//so adjust bounding box to include them.
	bbox.Extent.X += m_amplitude+m_quadSize;
	bbox.Extent.Y += m_amplitude+m_quadSize;

	//Clip our visible snow rendering box
	if ((cubeOriginX * m_emitterSpacing ) < (bbox.Center.X - bbox.Extent.X))
		cubeOriginX = (Int)floor ((bbox.Center.X - bbox.Extent.X)/m_emitterSpacing);

	if ((cubeOriginY * m_emitterSpacing ) < (bbox.Center.Y - bbox.Extent.Y))
		cubeOriginY = (Int)floor ((bbox.Center.Y - bbox.Extent.Y)/m_emitterSpacing);

	if ((cubeDimX * m_emitterSpacing ) > (bbox.Center.X + bbox.Extent.X))
		cubeDimX = (Int)floor ((bbox.Center.X + bbox.Extent.X)/m_emitterSpacing);

	if ((cubeDimY * m_emitterSpacing ) > (bbox.Center.Y + bbox.Extent.Y))
		cubeDimY = (Int)floor ((bbox.Center.Y + bbox.Extent.Y)/m_emitterSpacing);

	if ((cubeDimY - cubeOriginY) < 0 || (cubeDimX-cubeOriginX) < 0)
		return;	//entire snow box is culled by either x or y screen boundary.

	//Find total number of particles that need rendering.
	Int totalPart=(cubeDimY-cubeOriginY)*(cubeDimX-cubeOriginX);

	if (totalPart <= 0)
		return;	//nothing to render.

	//Height at the top of the cube with camera at center.
	m_snowCeiling = camPos.Z + m_boxDimensions/2.0f;

	//Offset to allow cube extents to move with camera.
	Real cameraOffset = fmod (camPos.Z,m_boxDimensions);
	m_heightTraveled=m_time*m_velocity+cameraOffset;	//height that snow flake traveled this frame.

	Matrix4x4 identity(true);
	DX8Wrapper::Set_Transform(VkTS::WORLD,identity);	

	DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);

	//make sure we have all the resources we need
	if (!m_VertexBufferD3D.buffer)
		ReAcquireResources();

	if (!m_indexBuffer)
		ReAcquireResources();

	DX8Wrapper::Set_Texture(0,m_snowTexture);

	Vector3 snowCenter;

	DX8Wrapper::Apply_Render_State_Changes();
	
    // Set the render states for using point sprites
	DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSPRITEENABLE, TRUE );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSCALEENABLE,  TRUE );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSIZE,     FtoDW(m_pointSize) );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSIZE_MIN, FtoDW(m_minPointSize) );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSIZE_MAX, FtoDW(m_maxPointSize) );
#ifdef VERIFY_VULKAN
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSCALE_A,  FtoDW(0.00f) );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSCALE_B,  FtoDW(0.00f) );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSCALE_C,  FtoDW(1.00f) );

	DX8Wrapper::_Get_D3D_Device8()->SetStreamSource( 0, m_VertexBufferD3D, 0, sizeof(POINTVERTEX) );
    DX8Wrapper::_Get_D3D_Device8()->SetFVF( VKFVF_POINTVERTEX );
	m_dwBase = SNOW_BUFFER_SIZE;	//start with a new vertex buffer each frame.

	m_leafDim = 45;	//cull boxes that are 20x20 emitters in size. Making them much smaller will result in too many draw calls.
	m_totalRendered = 0;	//keep track of how many particles were rendered.

	//Particle centers can deviate from center by by amplitude of sine offset.  They also have radius m_quadSize.
	//Enlarge culling bounds to compensate.
	m_cullOverscan = m_amplitude+m_quadSize;
	renderSubBox(rinfo,cubeOriginX,cubeOriginY,cubeDimX,cubeDimY);

	// Reset render states
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSPRITEENABLE, FALSE );
    DX8Wrapper::Set_DX8_Render_State( VKRS_POINTSCALEENABLE,  FALSE );
#else
	m_dwBase = SNOW_BUFFER_SIZE;	//start with a new vertex buffer each frame.

	m_leafDim = 45;	//cull boxes that are 20x20 emitters in size. Making them much smaller will result in too many draw calls.
	m_totalRendered = 0;	//keep track of how many particles were rendered.

	//Particle centers can deviate from center by by amplitude of sine offset.  They also have radius m_quadSize.
	//Enlarge culling bounds to compensate.
	m_cullOverscan = m_amplitude + m_quadSize;
	renderSubBox(rinfo, cubeOriginX, cubeOriginY, cubeDimX, cubeDimY);
	DX8Wrapper::Set_DX8_Render_State(VKRS_POINTSPRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(VKRS_POINTSCALEENABLE, FALSE);
#endif

}