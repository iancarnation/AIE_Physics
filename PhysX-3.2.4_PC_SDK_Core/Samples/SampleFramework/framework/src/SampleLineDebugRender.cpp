/*
 * Copyright 2008-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.  Users and possessors of this source code
 * are hereby granted a nonexclusive, royalty-free license to use this code
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.   This source code is a "commercial item" as
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer  software"  and "commercial computer software
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 *
 * Any use of this source code in individual and commercial software must
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */
#include <SampleLineDebugRender.h>
#include <Renderer.h>
#include <RendererVertexBuffer.h>
#include <RendererVertexBufferDesc.h>
#include <RendererMesh.h>
#include <RendererMeshDesc.h>
#include <SampleAssetManager.h>
#include <SampleMaterialAsset.h>
#include <RendererMemoryMacros.h>

using namespace SampleFramework;

SampleLineDebugRender::SampleLineDebugRender(SampleRenderer::Renderer &renderer, SampleAssetManager &assetmanager) :
	m_renderer(renderer),
	m_assetmanager(assetmanager)
{
	m_material = static_cast<SampleMaterialAsset*>(m_assetmanager.getAsset("materials/simple_unlit.xml", SampleAsset::ASSET_MATERIAL));
	PX_ASSERT(m_material);
	PX_ASSERT(m_material->getNumVertexShaders() == 1);
	m_meshContext.material = m_material ? m_material->getMaterial(0) : 0;

	m_maxVerts        = 0;
	m_numVerts        = 0;
	m_vertexbuffer    = 0;
	m_mesh            = 0;
	m_lockedPositions = 0;
	m_positionStride  = 0;
	m_lockedColors    = 0;
	m_colorStride     = 0;

	checkResizeLine(2048);
}

SampleLineDebugRender::~SampleLineDebugRender(void)
{
	checkUnlock();
	SAFE_RELEASE(m_vertexbuffer);
	SAFE_RELEASE(m_mesh);
	if(m_material)
	{
		m_assetmanager.returnAsset(*m_material);
	}
}

void SampleLineDebugRender::addLine(const PxVec3 &p0, const PxVec3 &p1, const SampleRenderer::RendererColor &color)
{
	checkResizeLine(m_numVerts+2);
	addVert(p0, color);
	addVert(p1, color);
}

void SampleLineDebugRender::queueForRenderLine(void)
{
	if(m_meshContext.mesh)
	{
		checkUnlock();
		m_mesh->setVertexBufferRange(0, m_numVerts);
		m_renderer.queueMeshForRender(m_meshContext);
	}
}

void SampleLineDebugRender::clearLine(void)
{
	m_numVerts = 0;
}

void SampleLineDebugRender::checkResizeLine(PxU32 maxVerts)
{
	if(maxVerts > m_maxVerts)
	{
		m_maxVerts = maxVerts + (PxU32)(maxVerts*0.2f);

		SampleRenderer::RendererVertexBufferDesc vbdesc;
		vbdesc.hint = SampleRenderer::RendererVertexBuffer::HINT_DYNAMIC;
		vbdesc.semanticFormats[SampleRenderer::RendererVertexBuffer::SEMANTIC_POSITION] = SampleRenderer::RendererVertexBuffer::FORMAT_FLOAT3;		
		vbdesc.semanticFormats[SampleRenderer::RendererVertexBuffer::SEMANTIC_COLOR]    = SampleRenderer::RendererVertexBuffer::FORMAT_COLOR_NATIVE; // same as RendererColor
		vbdesc.maxVertices = m_maxVerts;
		SampleRenderer::RendererVertexBuffer *vertexbuffer = m_renderer.createVertexBuffer(vbdesc);
		PX_ASSERT(vertexbuffer);

		if(vertexbuffer)
		{
			PxU32 positionStride = 0;
			void *positions = vertexbuffer->lockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_POSITION, positionStride);

			PxU32 colorStride = 0;
			void *colors = vertexbuffer->lockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_COLOR, colorStride);

			PX_ASSERT(positions && colors);
			if(positions && colors)
			{
				if(m_numVerts > 0)
				{
					// if we have existing verts, copy them to the new VB...
					PX_ASSERT(m_lockedPositions);
					PX_ASSERT(m_lockedColors);
					if(m_lockedPositions && m_lockedColors)
					{
						for(PxU32 i=0; i<m_numVerts; i++)
						{
							memcpy(((PxU8*)positions) + (positionStride*i), ((PxU8*)m_lockedPositions) + (m_positionStride*i), sizeof(PxVec3));
							memcpy(((PxU8*)colors)    + (colorStride*i),    ((PxU8*)m_lockedColors)    + (m_colorStride*i),    sizeof(SampleRenderer::RendererColor));
						}
					}
					m_vertexbuffer->unlockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_COLOR);
					m_vertexbuffer->unlockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_POSITION);
				}
			}
			vertexbuffer->unlockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_COLOR);
			vertexbuffer->unlockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_POSITION);
		}
		if(m_vertexbuffer)
		{
			m_vertexbuffer->release();
			m_vertexbuffer    = 0;
			m_lockedPositions = 0;
			m_positionStride  = 0;
			m_lockedColors    = 0;
			m_colorStride     = 0;
		}
		SAFE_RELEASE(m_mesh);
		if(vertexbuffer)
		{
			m_vertexbuffer = vertexbuffer;
			SampleRenderer::RendererMeshDesc meshdesc;
			meshdesc.primitives       = SampleRenderer::RendererMesh::PRIMITIVE_LINES;
			meshdesc.vertexBuffers    = &m_vertexbuffer;
			meshdesc.numVertexBuffers = 1;
			meshdesc.firstVertex      = 0;
			meshdesc.numVertices      = m_numVerts;
			m_mesh = m_renderer.createMesh(meshdesc);
			PX_ASSERT(m_mesh);
		}
		m_meshContext.mesh = m_mesh;
	}
}

void SampleLineDebugRender::checkLock(void)
{
	if(m_vertexbuffer)
	{
		if(!m_lockedPositions)
		{
			m_lockedPositions = m_vertexbuffer->lockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_POSITION, m_positionStride);
			PX_ASSERT(m_lockedPositions);
		}
		if(!m_lockedColors)
		{
			m_lockedColors = m_vertexbuffer->lockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_COLOR, m_colorStride);
			PX_ASSERT(m_lockedColors);
		}
	}
}

void SampleLineDebugRender::checkUnlock(void)
{
	if(m_vertexbuffer)
	{
		if(m_lockedPositions)
		{
			m_vertexbuffer->unlockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_POSITION);
			m_lockedPositions = 0;
		}
		if(m_lockedColors)
		{
			m_vertexbuffer->unlockSemantic(SampleRenderer::RendererVertexBuffer::SEMANTIC_COLOR);
			m_lockedColors = 0;
		}
	}
}

void SampleLineDebugRender::addVert(const PxVec3 &p, const SampleRenderer::RendererColor &color)
{
	PX_ASSERT(m_maxVerts > m_numVerts);
	{
		checkLock();
		if(m_lockedPositions && m_lockedColors)
		{
			memcpy(((PxU8*)m_lockedPositions) + (m_positionStride*m_numVerts), &p,     sizeof(PxVec3));
#if defined(RENDERER_XBOX360)
			PxU8 colors[4] = {color.a, color.r, color.g, color.b};
			memcpy(((PxU8*)m_lockedColors)    + (m_colorStride*m_numVerts), colors, sizeof(PxU8) * 4);
#else
			memcpy(((PxU8*)m_lockedColors)    + (m_colorStride*m_numVerts), &color, sizeof(SampleRenderer::RendererColor));
#endif
			m_numVerts++;
		}
	}
}
