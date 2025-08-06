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
	if (strip) {
		SNAPSHOT_SAY(("Draw_Strip(%d,%d,%d,%d)\n",index_offset,index_count-2,min_vertex_index,vertex_index_range));
		auto pipelines = DX8Wrapper::FindClosestPipelines(DX8Wrapper::Get_Vertex_Buffer()->FVF_Info().FVF, 
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		assert(pipelines.size() == 1);
		switch (pipelines[0]) {
		case PIPELINE_WWVK_FVF_DUV2_DropUV_Strip:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_DUV2_DropUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_DUV2_DropUV_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_ARef_Strip:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_NUV_ARef_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_ARef_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_ARefNoCull_Strip:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_NUV_ARefNoCull_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_ARefNoCull_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_Strip:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_N_Strip:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_N_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_N_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZN), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_AREF_Strip:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_NDUV_AREF_StripDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_AREF_Strip(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNDUV1), (WorldMatrix_AlphaRef*)&push);
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
		case PIPELINE_WWVK_FVF_NDUV2_DropUV_NoAlpha:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDUV2_DropUV_NoAlphaDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(), 
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV2_DropUV_NoAlpha(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_DUV2_DropUV:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_DUV2_DropUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_DUV2_DropUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZDUV2), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_DUV2_DropUV_ARef:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_DUV2_DropUV_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView());
			WWVK_DrawFVF_DUV2_DropUV_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZDUV2), &push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_ARef:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_NUV_ARefDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_ARef(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_ARefNoCull:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_NUV_ARefNoCullDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_ARefNoCull(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix_AlphaRef*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUVDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_NoDepth:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_NoDepthDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_NoDepth(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_NoAlphaBlend:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NUV_NoAlphaBlendDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_NoAlphaBlend(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_DROPUV_REFLUVT:
		{
			WorldMatrixUVT push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::Get_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_DROPUV_REFLUVTDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_DROPUV_REFLUVT(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NUV_DROPUV_REFLUVT_NoAlphaBlend:
		{
			WorldMatrixUVT push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			DX8Wrapper::Get_Transform(VkTS::TEXTURE0, *(Matrix4x4*)&push.uvt);
			WWVK_UpdateFVF_NUV_DROPUV_REFLUVT_NoAlphaBlendDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NUV_DROPUV_REFLUVT_NoAlphaBlend(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNUV1), (WorldMatrixUVT*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_N:
		{
			WorldMatrix push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.world);
			WWVK_UpdateFVF_NDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_N(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZN), (WorldMatrix*)&push);
			break;
		}
		case PIPELINE_WWVK_FVF_NDUV_AREF:
		{
			WorldMatrix_AlphaRef push;
			DX8Wrapper::Get_Transform(VkTS::WORLD, *(Matrix4x4*)&push.vert.world);
			push.frag.ref[0] = DX8Wrapper::Get_DX8_Render_State(VKRS_ALPHAREF);
			WWVK_UpdateFVF_NDUV_AREFDescriptorSets(&WWVKRENDER, WWVKPIPES, sets,
				&DX8Wrapper::Get_DX8_Texture(0), DX8Wrapper::UboProj(), DX8Wrapper::UboView(),
				DX8Wrapper::UboLight(), DX8Wrapper::UboMaterial());
			WWVK_DrawFVF_NDUV_AREF(WWVKPIPES, WWVKRENDER.currentCmd, sets,
				((DX8IndexBufferClass*)DX8Wrapper::Set_Index_Buffer())->Get_DX8_Index_Buffer().buffer, index_count, index_offset,
				VK_INDEX_TYPE_UINT16, ((DX8VertexBufferClass*)DX8Wrapper::Get_Vertex_Buffer())->Get_DX8_Vertex_Buffer().buffer,
				min_vertex_index * sizeof(VertexFormatXYZNDUV1), (WorldMatrix_AlphaRef*)&push);
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