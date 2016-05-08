/* Copyright (C) 2012 Wildfire Games.
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

/*
 * Implementation of common RenderModifiers
 */

#include "precompiled.h"

#include "lib/ogl.h"
#include "maths/Vector2D.h"
#include "maths/Vector3D.h"
#include "maths/Vector4D.h"
#include "maths/Matrix3D.h"

#include "ps/Game.h"

#include "graphics/GameView.h"
#include "graphics/LightEnv.h"
#include "graphics/LOSTexture.h"
#include "graphics/Model.h"
#include "graphics/TextureManager.h"
#include "graphics/UniformBlockManager.h"

#include "renderer/RenderModifiers.h"
#include "renderer/Renderer.h"
#include "renderer/ShadowMap.h"

#include <boost/algorithm/string.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////
// LitRenderModifier implementation

LitRenderModifier::LitRenderModifier()
	: m_Shadow(0), m_LightEnv(0)
{
}

LitRenderModifier::~LitRenderModifier()
{
}

// Set the shadow map for subsequent rendering
void LitRenderModifier::SetShadowMap(const ShadowMap* shadow)
{
	m_Shadow = shadow;
}

// Set the light environment for subsequent rendering
void LitRenderModifier::SetLightEnv(const CLightEnv* lightenv)
{
	m_LightEnv = lightenv;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// ShaderRenderModifier implementation

BaseShaderRenderModifier::BaseShaderRenderModifier()
{
}


void GL4ShaderRenderModifier::SetFrameUniforms()
{
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	CStrIntern frameUBO("FrameUBO");
	CStrIntern modelBlock("ModelBlock");
	UniformBinding transformBinding = uniformBlockManager.GetBinding(frameUBO, str_transform, false);
	UniformBinding cameraPosBinding = uniformBlockManager.GetBinding(frameUBO, str_cameraPos, false);
	
	uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(transformBinding, g_Renderer.GetViewCamera().GetViewProjection());
	uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(cameraPosBinding, g_Renderer.GetViewCamera().GetOrientation().GetTranslation());
	
	if (GetShadowMap())
	{
		UniformBinding shadowTransformBinding = uniformBlockManager.GetBinding(frameUBO, str_shadowTransform, false);
		UniformBinding shadowScaleBinding = uniformBlockManager.GetBinding(frameUBO, str_shadowScale, false);

		int width = GetShadowMap()->GetWidth();
		int height = GetShadowMap()->GetHeight();
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(shadowScaleBinding, CVector4D(width, height, 1.0f / width, 1.0f / height));
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(shadowTransformBinding, GetShadowMap()->GetTextureMatrix());
	}
	
	if (GetLightEnv())
	{
		UniformBinding ambientBinding = uniformBlockManager.GetBinding(frameUBO, str_ambient, false);
		UniformBinding sunDirBinding = uniformBlockManager.GetBinding(frameUBO, str_sunDir, false);
		UniformBinding sunColorBinding = uniformBlockManager.GetBinding(frameUBO, str_sunColor, false);
		
		UniformBinding fogColorBinding = uniformBlockManager.GetBinding(frameUBO, str_fogColor, false);
		UniformBinding fogParamsBinding = uniformBlockManager.GetBinding(frameUBO, str_fogParams, false);
		
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(ambientBinding, GetLightEnv()->m_UnitsAmbientColor);
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(sunDirBinding, GetLightEnv()->GetSunDir());
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(sunColorBinding, GetLightEnv()->m_SunColor);
		
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(fogColorBinding, GetLightEnv()->m_FogColor);
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(fogParamsBinding, CVector2D(GetLightEnv()->m_FogFactor, GetLightEnv()->m_FogMax));
	}
	
	//if (shader->GetTextureBinding(str_losTex).Active())
	//{
	UniformBinding losTransformBinding = uniformBlockManager.GetBinding(frameUBO, str_losTransform, false);
	CLOSTexture& los = g_Renderer.GetScene().GetLOSTexture();
	// Don't bother sending the whole matrix, we just need two floats (scale and translation)
	uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(losTransformBinding, CVector2D(los.GetTextureMatrix()[0], los.GetTextureMatrix()[12]));

	//}

	// TODO: Bindings should probably be used differently now (with uniform blocks)
	m_BindingInstancingTransform = uniformBlockManager.GetBinding(modelBlock, str_instancingTransform, true);
	m_BindingModelID = uniformBlockManager.GetBinding(modelBlock, str_modelId, true);
}

void GL4ShaderRenderModifier::BeginPass(const CShaderProgramPtr& shader)
{
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	CStrIntern frameUBO("FrameUBO");
	
	UniformBinding shadowTexBinding = uniformBlockManager.GetBinding(frameUBO, str_shadowTex, false);
	if (GetShadowMap() && shadowTexBinding.Active())
	{
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(shadowTexBinding, GetShadowMap()->GetBindlessTexture());
	}	

	UniformBinding losTexBinding = uniformBlockManager.GetBinding(frameUBO, str_losTex, false);
	if (losTexBinding.Active())
	{
		CLOSTexture& los = g_Renderer.GetScene().GetLOSTexture();
		uniformBlockManager.SetUniform<UniformBlockManager::NOT_INSTANCED>(losTexBinding, los.GetTextureSmoothBindlessHandle());
	}
}

// TODO: this is not used by for the GL4ModelRenderer (and not required).
// There should be a cleaner separation for this kind of differences.
void ShaderRenderModifier::PrepareModel(const CShaderProgramPtr& shader, CModel* model)
{
	if (m_BindingInstancingTransform.Active())
		shader->Uniform(m_BindingInstancingTransform, model->GetTransform());

	if (m_BindingShadingColor.Active())
		shader->Uniform(m_BindingShadingColor, model->GetShadingColor());

	if (m_BindingPlayerColor.Active())
		shader->Uniform(m_BindingPlayerColor, g_Game->GetPlayerColor(model->GetPlayerID()));
}

void ShaderRenderModifier::BeginPass(const CShaderProgramPtr& shader)
{
	shader->Uniform(str_transform, g_Renderer.GetViewCamera().GetViewProjection());
	shader->Uniform(str_cameraPos, g_Renderer.GetViewCamera().GetOrientation().GetTranslation());

	if (GetShadowMap() && shader->GetTextureBinding(str_shadowTex).Active())
	{
		shader->BindTexture(str_shadowTex, GetShadowMap()->GetTexture());
		shader->Uniform(str_shadowTransform, GetShadowMap()->GetTextureMatrix());
		int width = GetShadowMap()->GetWidth();
		int height = GetShadowMap()->GetHeight();
		shader->Uniform(str_shadowScale, width, height, 1.0f / width, 1.0f / height); 
	}

	if (GetLightEnv())
	{
		shader->Uniform(str_ambient, GetLightEnv()->m_UnitsAmbientColor);
		shader->Uniform(str_sunDir, GetLightEnv()->GetSunDir());
		shader->Uniform(str_sunColor, GetLightEnv()->m_SunColor);
		
		shader->Uniform(str_fogColor, GetLightEnv()->m_FogColor);
		shader->Uniform(str_fogParams, GetLightEnv()->m_FogFactor, GetLightEnv()->m_FogMax, 0.f, 0.f);
	}

	if (shader->GetTextureBinding(str_losTex).Active())
	{
		CLOSTexture& los = g_Renderer.GetScene().GetLOSTexture();
		shader->BindTexture(str_losTex, los.GetTextureSmooth());
		// Don't bother sending the whole matrix, we just need two floats (scale and translation)
		shader->Uniform(str_losTransform, los.GetTextureMatrix()[0], los.GetTextureMatrix()[12], 0.f, 0.f);
	}

	m_BindingInstancingTransform = shader->GetUniformBinding(str_instancingTransform);
	m_BindingShadingColor = shader->GetUniformBinding(str_shadingColor);
	m_BindingPlayerColor = shader->GetUniformBinding(str_playerColor);
}
