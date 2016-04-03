/* Copyright (C) 2016 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "lib/allocators/allocator_adapters.h"
#include "lib/allocators/arena.h"
#include "lib/ogl.h"
#include "maths/Vector3D.h"
#include "maths/Vector4D.h"

#include "ps/CLogger.h"
#include "ps/Profile.h"

#include "graphics/Color.h"
#include "graphics/LightEnv.h"
#include "graphics/Material.h"
#include "graphics/Model.h"
#include "graphics/ModelDef.h"
#include "graphics/MultiDrawIndirectCommands.h"
#include "graphics/ShaderManager.h"
#include "graphics/TextureManager.h"
#include "graphics/UniformBlockManager.h"

#include "renderer/MikktspaceWrap.h"
#include "renderer/GL4ModelRenderer.h"
#include "renderer/ModelRendererSharedImpl.h"
#include "renderer/ModelVertexRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/RenderModifiers.h"
#include "renderer/SkyManager.h"
#include "renderer/TimeManager.h"
#include "renderer/WaterManager.h"

#if ARCH_X86_X64
# include "lib/sysdep/arch/x86_x64/x86_x64.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////
// ModelRenderer implementation

#if ARCH_X86_X64
static bool g_EnableSSE = false;
#endif

/*
void ModelRenderer::Init()
{
#if ARCH_X86_X64
	if (x86_x64::Cap(x86_x64::CAP_SSE))
		g_EnableSSE = true;
#endif
}*/

// Submit one model.
// TODO: Currenty we have some duplicated code here because it's the same function as in ShaderModelRenderer.
// It can't be implemented in the base class because m_VertexRenderer isn't available there.
// Maybe this function will have to be different than the one in ShaderModelRenderer in the end anyway, otherwise it might
// make sense to add an additional base class for this kind of functions between ModelRenderer and these two class templates.
template <bool TGpuSkinning, typename RenderModifierT>
void GL4ModelRenderer<TGpuSkinning, RenderModifierT>::Submit(int cullGroup, CModel* model)
{
	CModelDefPtr mdef = model->GetModelDef();
	CModelRData* rdata = (CModelRData*)model->GetRenderData();

	// Ensure model data is valid
	const void* key = m_VertexRenderer.get();
	if (!rdata || rdata->GetKey() != key)
	{
		rdata = m_VertexRenderer->CreateModelData(key, model);
		model->SetRenderData(rdata);
		model->SetDirty(~0u);
	}

	m->submissions[cullGroup].push_back(model);
}

// Call update for all submitted models and enter the rendering phase
template <bool TGpuSkinning, typename RenderModifierT>
void GL4ModelRenderer<TGpuSkinning, RenderModifierT>::PrepareModels()
{
	for (int cullGroup = 0; cullGroup < CRenderer::CULL_MAX; ++cullGroup)
	{
		for (size_t i = 0; i < m->submissions[cullGroup].size(); ++i)
		{
			CModel* model = m->submissions[cullGroup][i];

			model->ValidatePosition();

			CModelRData* rdata = static_cast<CModelRData*>(model->GetRenderData());
			ENSURE(rdata->GetKey() == m_VertexRenderer.get()); // TODO: Careful! Check if this is valid!

			m_VertexRenderer->UpdateModelData(model, rdata, rdata->m_UpdateFlags);
			rdata->m_UpdateFlags = 0;
		}
	}
}

// Clear the submissions list
template <bool TGpuSkinning, typename RenderModifierT>
void GL4ModelRenderer<TGpuSkinning, RenderModifierT>::EndFrame()
{
	for (int cullGroup = 0; cullGroup < CRenderer::CULL_MAX; ++cullGroup)
		m->submissions[cullGroup].clear();
}

template <bool TGpuSkinning, typename RenderModifierT>
void GL4ModelRenderer<TGpuSkinning, RenderModifierT>::PrepareUniformBuffers(size_t maxInstancesPerDraw, int flags, 
												const std::vector<SMRTechBucket, TechBucketsAllocator>& techBuckets)
{
	PROFILE3("PrepareUniformBuffers");
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	ENSURE(maxInstancesPerDraw != 0);
	
	size_t instanceId = 0;
	u64 materialUniformsSet = 0;	

	while (m->heapCounters.idxTechStart < techBuckets.size())
	{
		CShaderTechniquePtr currentTech = techBuckets[m->heapCounters.idxTechStart].tech;

		// Find runs [idxTechStart, idxTechEnd) in techBuckets of the same technique
		size_t idxTechEnd;
		for (idxTechEnd = m->heapCounters.idxTechStart + 1; idxTechEnd < techBuckets.size(); ++idxTechEnd)
		{
			if (techBuckets[idxTechEnd].tech != currentTech)
				break;
		}

		// For each of the technique's passes, render all the models in this run
		while (true)
		{
			if (m->heapCounters.pass == currentTech->GetNumPasses())
			{
				m->heapCounters.pass = 0;
				break;
			}

			const CShaderProgramPtr& shader = currentTech->GetShader(m->heapCounters.pass);
			int streamflags = shader->GetStreamFlags();
			
			//for (size_t idx = idxTechStart; idx < idxTechEnd; ++idx)
			
			while (true)	
			{
				if (m->heapCounters.idx == idxTechEnd)
				{
					m->heapCounters.idx = m->heapCounters.idxTechStart;
					break;
				}
				
				CModel** models = techBuckets[m->heapCounters.idx].models;
				size_t numModels = techBuckets[m->heapCounters.idx].numModels;
				//for (size_t modelIx = 0; modelIx < numModels; ++modelIx)
				
				while (true)
				{
					if (m->heapCounters.modelIx == numModels)
					{
						m->heapCounters.modelIx = 0;
						break;
					}
					
					CModel* model = models[m->heapCounters.modelIx];

					if (flags && !(model->GetFlags() & flags))
					{
						m->heapCounters.modelIx++;
						continue;
					}
				
					uniformBlockManager.SetCurrentInstance<UniformBlockManager::MODEL_INSTANCED>(instanceId);
					
					m_RenderModifier->SetModelUniforms(model);
					m_VertexRenderer->PrepareModel(shader, model);
					
					m_VertexRenderer->SetRenderModelInstanced(model);
					
					instanceId++;
					
					if (instanceId == maxInstancesPerDraw)
					{
						PROFILE3("upload uniforms (1)");
						uniformBlockManager.Upload();
						m_VertexRenderer->BindAndUpload();
						
						// Rare case. If the final batch matches the remaining number of draws to prepare exactly
						// (we would have returned anyway). In this case we must make sure to reset the counters on the heap.
						if (m->heapCounters.modelIx == numModels && m->heapCounters.pass == currentTech->GetNumPasses() && m->heapCounters.idx + 1 == techBuckets.size())
						{
							m->heapCounters.modelIx = 0;
							m->heapCounters.idx = 0;
							m->heapCounters.pass = 0;
							m->heapCounters.idxTechStart = 0;
						}
						else
						{
							m->heapCounters.modelIx++;
						}
						
						return;
					}
					
					m->heapCounters.modelIx++;
						
				}
				m->heapCounters.idx++;
			}
			m->heapCounters.pass++;
		}
		m->heapCounters.idx = m->heapCounters.idxTechStart = idxTechEnd;
	}
	
	{
		PROFILE3("upload uniforms (2)");
		uniformBlockManager.Upload();
		m_VertexRenderer->BindAndUpload();
		
		m->heapCounters.modelIx = 0;
		m->heapCounters.idx = 0;
		m->heapCounters.pass = 0;
		m->heapCounters.idxTechStart = 0;
	}
}

template <bool TGpuSkinning, typename RenderModifierT>
void GL4ModelRenderer<TGpuSkinning, RenderModifierT>::Render(const CShaderDefines& context, int cullGroup, int flags)
{
	if (m->submissions[cullGroup].empty())
		return;

	CMatrix3D worldToCam;
	g_Renderer.GetViewCamera().m_Orientation.GetInverse(worldToCam);

	/*
	 * Rendering approach:
	 * 
	 * m->submissions contains the list of CModels to render.
	 * 
	 * The data we need to render a model is:
	 *  - CShaderTechnique
	 *  - CTexture
	 *  - CShaderUniforms
	 *  - CModelDef (mesh data)
	 *  - CModel (model instance data)
	 * 
	 * For efficient rendering, we need to batch the draw calls to minimise state changes.
	 * (Uniform and texture changes are assumed to be cheaper than binding new mesh data,
	 * and shader changes are assumed to be most expensive.)
	 * First, group all models that share a technique to render them together.
	 * Within those groups, sub-group by CModelDef.
	 * Within those sub-groups, sub-sub-group by CTexture.
	 * Within those sub-sub-groups, sub-sub-sub-group by CShaderUniforms.
	 * 
	 * Alpha-blended models have to be sorted by distance from camera,
	 * then we can batch as long as the order is preserved.
	 * Non-alpha-blended models can be arbitrarily reordered to maximise batching.
	 * 
	 * For each model, the CShaderTechnique is derived from:
	 *  - The current global 'context' defines
	 *  - The CModel's material's defines
	 *  - The CModel's material's shader effect name
	 * 
	 * There are a smallish number of materials, and a smaller number of techniques.
	 * 
	 * To minimise technique lookups, we first group models by material,
	 * in 'materialBuckets' (a hash table).
	 * 
	 * For each material bucket we then look up the appropriate shader technique.
	 * If the technique requires sort-by-distance, the model is added to the
	 * 'sortByDistItems' list with its computed distance.
	 * Otherwise, the bucket's list of models is sorted by modeldef+texture+uniforms,
	 * then the technique and model list is added to 'techBuckets'.
	 * 
	 * 'techBuckets' is then sorted by technique, to improve batching when multiple
	 * materials map onto the same technique.
	 * 
	 * (Note that this isn't perfect batching: we don't sort across models in
	 * multiple buckets that share a technique. In practice that shouldn't reduce
	 * batching much (we rarely have one mesh used with multiple materials),
	 * and it saves on copying and lets us sort smaller lists.)
	 * 
	 * Extra tech buckets are added for the sorted-by-distance models without reordering.
	 * Finally we render by looping over each tech bucket, then looping over the model
	 * list in each, rebinding the GL state whenever it changes.
	 */

	Allocators::DynamicArena arena(256 * KiB);
	typedef ProxyAllocator<CModel*, Allocators::DynamicArena> ModelListAllocator;
	typedef std::vector<CModel*, ModelListAllocator> ModelList_t;
	typedef boost::unordered_map<SMRMaterialBucketKey, ModelList_t,
		SMRMaterialBucketKeyHash, std::equal_to<SMRMaterialBucketKey>,
		ProxyAllocator<std::pair<const SMRMaterialBucketKey, ModelList_t>, Allocators::DynamicArena>
	> MaterialBuckets_t;
	MaterialBuckets_t materialBuckets((MaterialBuckets_t::allocator_type(arena)));

	{
		PROFILE3("bucketing by material");

		for (size_t i = 0; i < m->submissions[cullGroup].size(); ++i)
		{
			CModel* model = m->submissions[cullGroup][i];

			uint32_t condFlags = 0;

			const CShaderConditionalDefines& condefs = model->GetMaterial().GetConditionalDefines();
			for (size_t j = 0; j < condefs.GetSize(); ++j)
			{
				const CShaderConditionalDefines::CondDefine& item = condefs.GetItem(j);
				int type = item.m_CondType;
				switch (type)
				{
					case DCOND_DISTANCE:
					{
						CVector3D modelpos = model->GetTransform().GetTranslation();
						float dist = worldToCam.Transform(modelpos).Z;
						
						float dmin = item.m_CondArgs[0];
						float dmax = item.m_CondArgs[1];
						
						if ((dmin < 0 || dist >= dmin) && (dmax < 0 || dist < dmax))
							condFlags |= (1 << j);
						
						break;
					}
				}
			}

			CShaderDefines defs = model->GetMaterial().GetShaderDefines(condFlags);
			SMRMaterialBucketKey key(model->GetMaterial().GetShaderEffect(), defs);

			MaterialBuckets_t::iterator it = materialBuckets.find(key);
			if (it == materialBuckets.end())
			{
				std::pair<MaterialBuckets_t::iterator, bool> inserted = materialBuckets.insert(
					std::make_pair(key, ModelList_t(ModelList_t::allocator_type(arena))));
				inserted.first->second.reserve(32);
				inserted.first->second.push_back(model);
			}
			else
			{
				it->second.push_back(model);
			}
		}
	}

	typedef ProxyAllocator<SMRSortByDistItem, Allocators::DynamicArena> SortByDistItemsAllocator;
	std::vector<SMRSortByDistItem, SortByDistItemsAllocator> sortByDistItems((SortByDistItemsAllocator(arena)));

	typedef ProxyAllocator<CShaderTechniquePtr, Allocators::DynamicArena> SortByTechItemsAllocator;
	std::vector<CShaderTechniquePtr, SortByTechItemsAllocator> sortByDistTechs((SortByTechItemsAllocator(arena)));
		// indexed by sortByDistItems[i].techIdx
		// (which stores indexes instead of CShaderTechniquePtr directly
		// to avoid the shared_ptr copy cost when sorting; maybe it'd be better
		// if we just stored raw CShaderTechnique* and assumed the shader manager
		// will keep it alive long enough)

	std::vector<SMRTechBucket, TechBucketsAllocator> techBuckets((TechBucketsAllocator(arena)));

	{
		PROFILE3("processing material buckets");
		for (MaterialBuckets_t::iterator it = materialBuckets.begin(); it != materialBuckets.end(); ++it)
		{
			CShaderTechniquePtr tech = g_Renderer.GetShaderManager().LoadEffect(it->first.effect, context, it->first.defines);

			// Skip invalid techniques (e.g. from data file errors)
			if (!tech)
				continue;

			if (tech->GetSortByDistance())
			{
				// Add the tech into a vector so we can index it
				// (There might be duplicates in this list, but that doesn't really matter)
				if (sortByDistTechs.empty() || sortByDistTechs.back() != tech)
					sortByDistTechs.push_back(tech);
				size_t techIdx = sortByDistTechs.size()-1;

				// Add each model into sortByDistItems
				for (size_t i = 0; i < it->second.size(); ++i)
				{
					SMRSortByDistItem itemWithDist;
					itemWithDist.techIdx = techIdx;

					CModel* model = it->second[i];
					itemWithDist.model = model;

					CVector3D modelpos = model->GetTransform().GetTranslation();
					itemWithDist.dist = worldToCam.Transform(modelpos).Z;

					sortByDistItems.push_back(itemWithDist);
				}
			}
			else
			{
				// Sort model list by modeldef+texture, for batching
				// TODO: This only sorts by base texture. While this is an OK approximation
				// for most cases (as related samplers are usually used together), it would be better
				// to take all the samplers into account when sorting here.
				std::sort(it->second.begin(), it->second.end(), SMRBatchModel());

				// Add a tech bucket pointing at this model list
				SMRTechBucket techBucket = { tech, &it->second[0], it->second.size() };
				techBuckets.push_back(techBucket);
			}
		}
	}

	{
		PROFILE3("sorting tech buckets");
		// Sort by technique, for better batching
		std::sort(techBuckets.begin(), techBuckets.end(), SMRCompareTechBucket());
	}

	// List of models corresponding to sortByDistItems[i].model
	// (This exists primarily because techBuckets wants a CModel**;
	// we could avoid the cost of copying into this list by adding
	// a stride length into techBuckets and not requiring contiguous CModel*s)
	std::vector<CModel*, ModelListAllocator> sortByDistModels((ModelListAllocator(arena)));
	if (!sortByDistItems.empty())
	{
		{
			PROFILE3("sorting items by dist");
			std::sort(sortByDistItems.begin(), sortByDistItems.end(), SMRCompareSortByDistItem());
		}

		{
			PROFILE3("batching dist-sorted items");

			sortByDistModels.reserve(sortByDistItems.size());

			// Find runs of distance-sorted models that share a technique,
			// and create a new tech bucket for each run

			size_t start = 0; // start of current run
			size_t currentTechIdx = sortByDistItems[start].techIdx;

			for (size_t end = 0; end < sortByDistItems.size(); ++end)
			{
				sortByDistModels.push_back(sortByDistItems[end].model);

				size_t techIdx = sortByDistItems[end].techIdx;
				if (techIdx != currentTechIdx)
				{
					// Start of a new run - push the old run into a new tech bucket
					SMRTechBucket techBucket = { sortByDistTechs[currentTechIdx], &sortByDistModels[start], end-start };
					techBuckets.push_back(techBucket);
					start = end;
					currentTechIdx = techIdx;
				}
			}

			// Add the tech bucket for the final run
			SMRTechBucket techBucket = { sortByDistTechs[currentTechIdx], &sortByDistModels[start], sortByDistItems.size()-start };
			techBuckets.push_back(techBucket);
		}
	}

	{
		PROFILE3("rendering bucketed submissions");

		size_t idxTechStart = 0;
		
		size_t preparedModelUniformsLeft = 2000;
		const size_t maxInstancesPerDraw = 2000;
		
		UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
		
		// Set all per-frame uniforms
		UniformBinding binding = uniformBlockManager.GetBinding(CStrIntern("FrameUBO"), CStrIntern("sim_time"), false);
		if (binding.Active())
		{
			double time = g_Renderer.GetTimeManager().GetGlobalTime();
			// TODO: Why don't we just use a single float instead of vec4?
			uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(binding, CVector4D(time, 0, 0, 0));
		}
		
		m_RenderModifier->SetFrameUniforms();
		
		// prepare the first batch of uniforms.
		// This causes an upload of all modified uniform buffers, and will also take care of uploading
		// the per-frame uniforms
		PrepareUniformBuffers(maxInstancesPerDraw, flags, techBuckets);
		
		// This vector keeps track of texture changes during rendering. It is kept outside the
		// loops to avoid excessive reallocations. The token allocation of 64 elements 
		// should be plenty, though it is reallocated below (at a cost) if necessary.
		typedef ProxyAllocator<CTexture*, Allocators::DynamicArena> TextureListAllocator;
		std::vector<CTexture*, TextureListAllocator> currentTexs((TextureListAllocator(arena)));
		currentTexs.reserve(64);
		
		// texBindings holds the identifier bindings in the shader, which can no longer be defined 
		// statically in the ShaderRenderModifier class. texBindingNames uses interned strings to
		// keep track of when bindings need to be reevaluated.
		typedef ProxyAllocator<CShaderProgram::Binding, Allocators::DynamicArena> BindingListAllocator;
		std::vector<CShaderProgram::Binding, BindingListAllocator> texBindings((BindingListAllocator(arena)));
		texBindings.reserve(64);

		typedef ProxyAllocator<CStrIntern, Allocators::DynamicArena> BindingNamesListAllocator;
		std::vector<CStrIntern, BindingNamesListAllocator> texBindingNames((BindingNamesListAllocator(arena)));
		texBindingNames.reserve(64);

		while (idxTechStart < techBuckets.size())
		{
			CShaderTechniquePtr currentTech = techBuckets[idxTechStart].tech;
			
			// Set to true when a new instanced draw command gets added.
			// Everytime we do a state-change that requires adding a new DrawElementsIndirectCommand,
			// we have to set this to false
			// TODO: Completely useless currently because we only ever draw one instance
			bool sameInstance = false;

			// Find runs [idxTechStart, idxTechEnd) in techBuckets of the same technique
			size_t idxTechEnd;
			for (idxTechEnd = idxTechStart + 1; idxTechEnd < techBuckets.size(); ++idxTechEnd)
			{
				if (techBuckets[idxTechEnd].tech != currentTech)
					break;
			}

			// For each of the technique's passes, render all the models in this run
			for (int pass = 0; pass < currentTech->GetNumPasses(); ++pass)
			{
				currentTech->BeginPass(pass);

				const CShaderProgramPtr& shader = currentTech->GetShader(pass);
				int streamflags = shader->GetStreamFlags();
				
				// TODO: Check the return value and force drawing if it's false
				// TODO: There should be a smarter way to figure out if bindings are already set up correctly.
				uniformBlockManager.EnsureBlockBinding(shader);

				m_RenderModifier->BeginPass(shader);

				m_VertexRenderer->BeginPass(streamflags);
				
				// When the shader technique changes, textures need to be
				// rebound, so ensure there are no remnants from the last pass.
				// (the vector size is set to 0, but memory is not freed)
				currentTexs.clear();
				texBindings.clear();
				texBindingNames.clear();
				
				CModelDef* currentModeldef = NULL;
				CShaderUniforms currentStaticUniforms;

				for (size_t idx = idxTechStart; idx < idxTechEnd; ++idx)
				{
					CModel** models = techBuckets[idx].models;
					size_t numModels = techBuckets[idx].numModels;
					for (size_t i = 0; i < numModels; ++i)
					{

						CModel* model = models[i];
						
						// TODO: Instancing is completely useless currently because we only ever draw one instance
						sameInstance = false;

						if (flags && !(model->GetFlags() & flags))
							continue;

						const CMaterial::SamplersVector& samplers = model->GetMaterial().GetSamplers();
						size_t samplersNum = samplers.size();
						ogl_WarnIfError();
						// make sure the vectors are the right virtual sizes, and also
						// reallocate if there are more samplers than expected.
						if (currentTexs.size() != samplersNum)
						{
							currentTexs.resize(samplersNum, NULL);
							texBindings.resize(samplersNum, CShaderProgram::Binding());
							texBindingNames.resize(samplersNum, CStrIntern());
							
							// ensure they are definitely empty
							std::fill(texBindings.begin(), texBindings.end(), CShaderProgram::Binding());
							std::fill(currentTexs.begin(), currentTexs.end(), (CTexture*)NULL);
							std::fill(texBindingNames.begin(), texBindingNames.end(), CStrIntern());
						}
						
						ogl_WarnIfError();
						// bind the samplers to the shader
						for (size_t s = 0; s < samplersNum; ++s)
						{
							const CMaterial::TextureSampler& samp = samplers[s];
							
							CShaderProgram::Binding bind = texBindings[s];
							// check that the handles are current
							// and reevaluate them if necessary
							if (texBindingNames[s] == samp.Name && bind.Active())
							{
								bind = texBindings[s];
							}
							else
							{
								bind = shader->GetTextureBinding(samp.Name);
								texBindings[s] = bind;
								texBindingNames[s] = samp.Name;
							}

							// same with the actual sampler bindings
							CTexture* newTex = samp.Sampler.get();
							if (bind.Active() && newTex != currentTexs[s])
							{
								shader->BindTexture(bind, samp.Sampler->GetHandle());
								currentTexs[s] = newTex;
							}
						}
						ogl_WarnIfError();
						
						// Bind modeldef when it changes
						CModelDef* newModeldef = model->GetModelDef().get();
						if (newModeldef != currentModeldef)
						{
							currentModeldef = newModeldef;
							m_VertexRenderer->PrepareModelDef(shader, streamflags, *currentModeldef);
						}
						ogl_WarnIfError();

						//staticUniforms.SetUniforms(uniformBlockManager);
						/*
						// Bind all uniforms when any change
						CShaderUniforms newStaticUniforms = model->GetMaterial().GetStaticUniforms();
						if (newStaticUniforms != currentStaticUniforms)
						{
							currentStaticUniforms = newStaticUniforms;
							currentStaticUniforms.BindUniforms(shader);
						}*/
						
						const CShaderRenderQueries& renderQueries = model->GetMaterial().GetRenderQueries();
						
						for (size_t q = 0; q < renderQueries.GetSize(); q++)
						{
							CShaderRenderQueries::RenderQuery rq = renderQueries.GetItem(q);
							if (rq.first == RQUERY_WATER_TEX)
							{
								WaterManager* WaterMgr = g_Renderer.GetWaterManager();
								double time = WaterMgr->m_WaterTexTimer;
								double period = 1.6;
								int curTex = (int)(time*60/period) % 60;
								
								if (WaterMgr->m_RenderWater && WaterMgr->WillRenderFancyWater())
									shader->BindTexture(str_waterTex, WaterMgr->m_NormalMap[curTex]);
								else
									shader->BindTexture(str_waterTex, g_Renderer.GetTextureManager().GetErrorTexture());
							}
							else if (rq.first == RQUERY_SKY_CUBE)
							{
								shader->BindTexture(str_skyCube, g_Renderer.GetSkyManager()->GetSkyCube());
							}
						}

						//modifier->PrepareModel(shader, model);
			
						// TODO: will not need to be done for each models in the future		
						m_VertexRenderer->RenderModelsInstanced(1);
						//m_VertexRenderer->RenderModel(shader, streamflags, model, rdata);
				
						preparedModelUniformsLeft--;
						if (preparedModelUniformsLeft == 0)
						{
							m_VertexRenderer->ResetDrawID();
							m_VertexRenderer->ResetCommands();
							PrepareUniformBuffers(maxInstancesPerDraw, flags, techBuckets);
							
							// technically not true when less models than maxInstancesPerDraw models are left, 
							// but preparedModelUniformsLeft is only used to detect when PrepareUniformBuffers
							// needs to be called, so it does not matter.
							preparedModelUniformsLeft = maxInstancesPerDraw;
							
							// TODO: Force drawing (currently we draw always anyway, so it is already "forced",
							// but this will change when instancing works properly)
						}
					}
				}

				m_VertexRenderer->EndPass(streamflags);

				currentTech->EndPass(pass);
			}

			idxTechStart = idxTechEnd;
		}
	}
	
	m_VertexRenderer->ResetDrawID();
	m_VertexRenderer->ResetCommands();
}

template class GL4ModelRenderer<true, GL4ShaderRenderModifierPtr>;
template class GL4ModelRenderer<false, GL4ShaderRenderModifierPtr>;
