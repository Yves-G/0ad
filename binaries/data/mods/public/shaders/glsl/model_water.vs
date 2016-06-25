#version 430
#extension GL_ARB_bindless_texture : require

in uint a_drawId;

out VS_OUT
{
    flat uint drawID;
} vs_out;

layout(shared) buffer FrameUBO
{
	vec4 sim_time;

	mat4 transform;
	vec3 cameraPos;

	mat4 shadowTransform;
	vec4 shadowScale;

	vec3 ambient;	// only used in fragment shader
	vec3 sunColor;
	vec3 sunDir;

	vec3 fogColor;	// only used in fragment shader
	vec2 fogParams;	// only used in fragment shader

	vec2 losTransform;
	layout (bindless_sampler) sampler2D losTex;

// TODO: It has to be ensured that all blocks in all shaders are the same
// (they must not have different defines that cause a difference)

  #if USE_SHADOW_SAMPLER
    layout (bindless_sampler) sampler2DShadow shadowTex;
  #else
    layout (bindless_sampler) sampler2D shadowTex;
  #endif

};

// TODO: make block members conditional again
struct DrawStruct
{
  uint modelId;
  mat4 instancingTransform;
};

layout(shared) buffer DrawBlock
{
  DrawStruct draws[];
};

struct ModelStruct
{
  uint matId;
  vec4 playerColor;
  vec3 shadingColor;
};

layout(shared) buffer ModelBlock
{
  ModelStruct models[];
};

struct MaterialStruct
{
  uint templateMatId;
  vec3 objectColor;
  layout (bindless_sampler) sampler2D baseTex;
  layout (bindless_sampler) sampler2D aoTex;
  layout (bindless_sampler) sampler2D normTex;
  layout (bindless_sampler) sampler2D specTex;
};

layout(shared) buffer MaterialUBO
{
  MaterialStruct material[];
};

struct MatTemplWaterStruct
{
  vec2 translation;
// TODO: shininess is in the material files but it doesn't seem to be used anywhere
// shininess
  float specularStrength;
  float waviness;
  vec3 waterTint;
  float murkiness;
  vec3 reflectionTint;
  float reflectionTintStrength;
};

layout(shared) buffer MatTemplWaterBlock
{
  MatTemplWaterStruct matTemplWater[];
};

in vec3 a_vertex;
in vec3 a_normal;
#if USE_INSTANCING
  in vec4 a_tangent;
#endif
in vec2 a_uv0;
in vec2 a_uv1;

out vec4 worldPos;
out vec4 v_tex;
out vec4 v_shadow;
out vec2 v_los;


vec4 fakeCos(vec4 x)
{
	vec4 tri = abs(fract(x + 0.5) * 2.0 - 1.0);
	return tri * tri *(3.0 - 2.0 * tri);  
}



void main()
{
	const uint modelIdVal = draws[a_drawId].modelId;
	const uint materialIDVal = models[modelIdVal].matId;
	const uint matTemplIdVal = material[materialIDVal].templateMatId;

	vs_out.drawID = a_drawId;

	const vec4 vertices[] = vec4[](vec4( 0.25, -0.25, 0.5, 1.0),
                                       vec4( 0.25,  0.25, 0.5, 1.0),
                                       vec4(-0.25, -0.25, 0.5, 1.0));

	worldPos = draws[a_drawId].instancingTransform * vec4(a_vertex, 1.0);
	
	v_tex.xy = (a_uv0 + worldPos.xz) / 5.0 + sim_time.x * matTemplWater[matTemplIdVal].translation;

	v_tex.zw = a_uv0;

	#if USE_SHADOW
		v_shadow = shadowTransform * worldPos;
		#if USE_SHADOW_SAMPLER && USE_SHADOW_PCF
			v_shadow.xy *= shadowScale.xy;
		#endif  
	#endif

	v_los = worldPos.xz * losTransform.x + losTransform.y;

	gl_Position = transform * worldPos;
}

