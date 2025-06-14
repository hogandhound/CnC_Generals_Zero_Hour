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
 *                     $Archive:: /Commando/Code/ww3d2/dx8polygonrenderer.h                   $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                      $Author:: Jani_p                                                      $*
 *                                                                                             *
 *                     $Modtime:: 7/12/01 6:38p                                               $*
 *                                                                                             *
 *                    $Revision:: 22                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#if defined(_MSC_VER)
#pragma once
#endif

#ifndef DX8_POLYGON_RENDERER_H
#define DX8_POLYGON_RENDERER_H


#include "always.h"
#include "meshmdl.h"
#include "dx8list.h"
#include "sortingrenderer.h"
#include "mesh.h"
#include "dx8wrapper.h"

class DX8PolygonRendererClass;
class DX8TextureCategoryClass;


/**
** DX8PolygonRendererClass
** This is a record of a batch/range of polygons to be rendered.  These hang off of the DX8TextureCategoryClass's
** and are rendered after the system installs a vertex buffer and textures in the DX8 wrapper.
*/
class DX8PolygonRendererClass : public MultiListObjectClass 
{
	MeshModelClass *				mmc;
	DX8TextureCategoryClass *	texture_category;			
	unsigned							index_offset;				// absolute index of index 0 for our parent mesh
	unsigned							vertex_offset;				// absolute index of vertex 0 for our parent mesh
	unsigned							index_count;				// number of indices
	unsigned							min_vertex_index;			// relative index of the first vertex our polys reference
	unsigned							vertex_index_range;		// range to the last vertex our polys reference
	bool								strip;						// is this a strip?
	unsigned							pass;					// rendering pass

public:
	DX8PolygonRendererClass(
		unsigned index_count,
		MeshModelClass* mmc_,
		DX8TextureCategoryClass* tex_cat,
		unsigned vertex_offset,
		unsigned index_offset,
		bool strip,
		unsigned pass);
	DX8PolygonRendererClass(const DX8PolygonRendererClass& src,MeshModelClass* mmc_);
	~DX8PolygonRendererClass();

	void								Render(/*const Matrix3D & tm,*/int base_vertex_offset);
	void								Render_Sorted(/*const Matrix3D & tm,*/int base_vertex_offset,const SphereClass & bounding_sphere);
	void								Set_Vertex_Index_Range(unsigned min_vertex_index_,unsigned vertex_index_range_);
	
	unsigned							Get_Vertex_Offset(void)	{ return vertex_offset; }
	unsigned							Get_Index_Offset(void)	{ return index_offset; }
	inline unsigned						Get_Pass(void)	{ return pass; }

	MeshModelClass*				Get_Mesh_Model_Class() { return mmc; }
	DX8TextureCategoryClass*	Get_Texture_Category() { return texture_category; }
	void								Set_Texture_Category(DX8TextureCategoryClass* tc) { texture_category=tc; }

	void Log();
};

// ----------------------------------------------------------------------------

inline void DX8PolygonRendererClass::Set_Vertex_Index_Range(unsigned min_vertex_index_,unsigned vertex_index_range_)
{
//	WWDEBUG_SAY(("Set_Vertex_Index_Range - min: %d, range: %d\n",min_vertex_index_,vertex_index_range_));
//	if (vertex_index_range_>30000) {
//		int a=0;
//		a++;
//	}
	min_vertex_index=min_vertex_index_;
	vertex_index_range=vertex_index_range_;
}

// ----------------------------------------------------------------------------

inline void DX8PolygonRendererClass::Render(/*const Matrix3D & tm,*/int base_vertex_offset)
{
//	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);
//	SNAPSHOT_SAY(("Set_Transform\n"));
	SNAPSHOT_SAY(("Set_Index_Buffer_Index_Offset(%d)\n",base_vertex_offset));

	DX8Wrapper::Set_Index_Buffer_Index_Offset(base_vertex_offset);
	DX8Wrapper::Apply_Render_State_Changes();
	WWVKDSV;
	VK::Buffer ibo, vbo;
	bool freeBuffers = false;
	auto indexBuffer = DX8Wrapper::Set_Index_Buffer();
	auto vertexBuffer = DX8Wrapper::Get_Vertex_Buffer();
	if (indexBuffer->Type() == BUFFER_TYPE_SORTING && vertexBuffer->Type() == BUFFER_TYPE_SORTING)
	{
		DX8Wrapper::Convert_Sorting_IB_VB((SortingVertexBufferClass*)vertexBuffer, (SortingIndexBufferClass*)indexBuffer, vbo, ibo);
		freeBuffers = true;
	}
	else if (indexBuffer->Type() == BUFFER_TYPE_DX8 && vertexBuffer->Type() == BUFFER_TYPE_DX8)
	{
		ibo.buffer = ((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer;
		vbo.buffer = ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer;
	}
	else
	{
		assert(false);
	}
	// = ibo.buffer, index_count, index_offset,
	//	VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer;

	if (strip) {
		SNAPSHOT_SAY(("Draw_Strip(%d,%d,%d,%d)\n",index_offset,index_count-2,min_vertex_index,vertex_index_range));
		auto pipelines = DX8Wrapper::FindClosestPipelines(DX8Wrapper::Get_Vertex_Buffer()->FVF_Info().FVF, 
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		assert(pipelines.size() == 1);
		switch (pipelines[0]) {
		case PIPELINE_WWVK_FVF_DUV2_DropUV_Strip:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_DUV2_DropUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_DUV2_DropUV_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_ARef_Strip:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NUV_ARef_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_ARef_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_Strip:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_N_Strip:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_N_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_N_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZN), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_AREF_Strip:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NDUV_AREF_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_AREF_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		default: assert(false);
		}
#ifdef INFO_VULKAN
		DX8Wrapper::Draw_Strip(
			index_offset,
			index_count-2,
			min_vertex_index,
			vertex_index_range);
#endif
	}
	else {
		SNAPSHOT_SAY(("Draw_Triangles(%d,%d,%d,%d)\n",index_offset,index_count-2,min_vertex_index,vertex_index_range));
		auto pipelines = DX8Wrapper::FindClosestPipelines(DX8Wrapper::Get_Vertex_Buffer()->FVF_Info().FVF);
		assert(pipelines.size() == 1);
		switch (pipelines[0]) {
		case PIPELINE_WWVK_FVF_NUV_ARef:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NUV_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_2Tex_ARef:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NUV_2Tex_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_2Tex_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_UVT:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_UVTDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_UVT(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), &push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_UVT_UV:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_UVT_UVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_UVT_UV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), &push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_REFLUVT_UV:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_REFLUVT_UVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_REFLUVT_UV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), &push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_UVT12:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			UVT2 uvt2;
			VK::Buffer temp;
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&uvt2.m1);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE1, *(Matrix4x4*)&uvt2.m2);
			VkBufferTools::CreateUniformBuffer(&WWVKRENDER, sizeof(UVT2), &uvt2, temp);

			WWVK_UpdateFVF_NUV_UVT12DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				temp, DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_UVT12(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), &push);
			WWVKRENDER.PushSingleFrameBuffer(temp);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_DROPUV_REFLUVT_ARef:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			float aref = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			VK::Buffer temp;
			VkBufferTools::CreateUniformBuffer(&WWVKRENDER, sizeof(AlphaRef), &aref, temp);
			WWVK_UpdateFVF_NUV_DROPUV_REFLUVT_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial(), temp);
			WWVK_DrawFVF_NUV_DROPUV_REFLUVT_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNUV1), (WorldMatrixUVT*)&push);
			WWVKRENDER.PushSingleFrameBuffer(temp);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_NoTex:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_NoTexDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_NoTex(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_2Tex:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_2TexDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_2Tex(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_2Tex_NoDiffuse:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_2Tex_NoDiffuseDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_2Tex_NoDiffuse(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_NoDiffuse:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_NoDiffuseDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_NoDiffuse(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_CAMUVT_DROPUV_NOL_NoDiffuse:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_CAMUVT_DROPUV_NOL_NoDiffuseDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_NUV_CAMUVT_DROPUV_NOL_NoDiffuse(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_DROPUV_REFLUVT:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_DROPUV_REFLUVTDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_DROPUV_REFLUVT(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DropUV_ARef:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NDUV2_DropUV_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DropUV_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DropTex_ARef:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NDUV2_DropTex_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DropTex_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DropTex:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_DropTexDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DropTex(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DropTex_UV2:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_DropTex_UV2DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DropTex_UV2(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_CAMUVT_NOL_OnlyTex1:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			UVT2 temp;
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&temp.m1);
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			VK::Buffer uvt;
			VkBufferTools::CreateUniformBuffer(&WWVKRENDER, sizeof(UVT2), &temp, uvt);
			WWVK_UpdateFVF_NDUV2_CAMUVT_NOL_OnlyTex1DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(), uvt);
			WWVK_DrawFVF_NDUV2_CAMUVT_NOL_OnlyTex1(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			WWVKRENDER.PushSingleFrameBuffer(uvt);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_NOL_DROPTEX2:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_NOL_DROPTEX2DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_NDUV2_NOL_DROPTEX2(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_UVT1_UV1:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NDUV2_UVT1_UV1DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(), DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_UVT1_UV1(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DROPTEX2:
		{
			auto& render_state = DX8Wrapper::Get_Render_State();
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_DROPTEX2DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView(), DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DROPTEX2(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				(render_state.index_base_offset) * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DropUV:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_DropUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial() );
			WWVK_DrawFVF_NDUV2_DropUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_OnlyTex2:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_OnlyTex2DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_OnlyTex2(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_NoAlpha2:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_NoAlpha2DescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), &DX8Wrapper::Get_DX8_Texture(1), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_NoAlpha2(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV2_DROPUV_UVT2_UVT_PLUS_UVTRGB:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			UVT2 temp;
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&temp.m1);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE1, *(Matrix4x4*)&temp.m2);
			VK::Buffer uvt;
			VkBufferTools::CreateUniformBuffer(&WWVKRENDER, sizeof(UVT2), &temp, uvt);
			WWVK_UpdateFVF_NDUV2_DROPUV_UVT2_UVT_PLUS_UVTRGBDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				uvt, DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DROPUV_UVT2_UVT_PLUS_UVTRGB(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrix*)&push);
			WWVKRENDER.PushSingleFrameBuffer(uvt);
			break;
		}
		case PIPELINE_WWVK_FVF_DUV2_DropUV:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_DUV2_DropUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_DUV2_DropUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_DUV2_DropUV_ARef:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_DUV2_DropUV_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_DUV2_DropUV_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZDUV2), &push);
			break;
		}
		case PIPELINE_WWVK_FVF_N:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_N(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZN), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_N_NOL:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_N_NOLDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_N_NOL(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZN), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_N_NOL_CAMUVT:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_N_NOL_CAMUVTDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_N_NOL_CAMUVT(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZN), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_AREF:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF) / 255.f;
			WWVK_UpdateFVF_NDUV_AREFDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_AREF(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_DropUV_CAMUVT_NoDiffuse:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NDUV_DropUV_CAMUVT_NoDiffuseDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_DropUV_CAMUVT_NoDiffuse(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_REFLUVT_DROPUV:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NDUV_REFLUVT_DROPUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_REFLUVT_DROPUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_NoDiffuse:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NDUV_NoDiffuseDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_NoDiffuse(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_DropUV_CAMUVT_NoDiffuse_NOL:
		{
			WorldMatrixUVT push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NDUV_DropUV_CAMUVT_NoDiffuse_NOLDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_NDUV_DropUV_CAMUVT_NoDiffuse_NOL(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_UVT2_UVT_PLUS_UVTRGB:
		{
			WorldMatrix push;
			DX8Wrapper::_Get_DX8_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			UVT2 temp;
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&temp.m1);
			DX8Wrapper::_Get_DX8_Transform(VkTS::TEXTURE1, *(Matrix4x4*)&temp.m2);
			VK::Buffer uvt;
			VkBufferTools::CreateUniformBuffer(&WWVKRENDER, sizeof(UVT2), &temp, uvt);
			WWVK_UpdateFVF_NDUV_UVT2_UVT_PLUS_UVTRGBDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				uvt, DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_UVT2_UVT_PLUS_UVTRGB(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				ibo.buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, vbo.buffer,
				base_vertex_offset * sizeof(VertexFormatXYZNDUV1), (WorldMatrix*)&push);
			WWVKRENDER.PushSingleFrameBuffer(uvt);
			break;
		}
		default: assert(false);
		}
#ifdef INFO_VULKAN
		DX8Wrapper::Draw_Triangles(
			index_offset,
			index_count/3,
			min_vertex_index,
			vertex_index_range);
#endif
	}

	if (freeBuffers)
	{
		WWVKRENDER.PushSingleFrameBuffer(ibo);
		WWVKRENDER.PushSingleFrameBuffer(vbo);
	}
}

inline void DX8PolygonRendererClass::Render_Sorted(/*const Matrix3D & tm,*/int base_vertex_offset,const SphereClass & bounding_sphere)
{
	WWASSERT(!strip);	// Strips can't be sorted for now
//	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);
//	SNAPSHOT_SAY(("Set_Transform\n"));
	SNAPSHOT_SAY(("Set_Index_Buffer_Index_Offset(%d)\n",base_vertex_offset));
	SNAPSHOT_SAY(("Insert_Sorting_Triangles(%d,%d,%d,%d)\n",index_offset,index_count-2,min_vertex_index,vertex_index_range));

	DX8Wrapper::Set_Index_Buffer_Index_Offset(base_vertex_offset);
	SortingRendererClass::Insert_Triangles(
		bounding_sphere,
		index_offset,
		index_count/3,
		min_vertex_index,
		vertex_index_range);

}

#endif