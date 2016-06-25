#version 430
#extension GL_ARB_bindless_texture : require

// TODO: hardcoded maximum buffer sizes are bad
const int MAX_INSTANCES = 2000;
const int MAX_MATERIAL_TEMPLATES = 2000;
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

struct MatTemplStruct
{
//#if USE_SPECULAR
  float specularPower;
  vec3 specularColor;
//#endif

//#if USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX || USE_AO
  vec4 effectSettings;
//#endif

  vec4 windData;
};

layout(shared) buffer MatTemplBlock
{
  MatTemplStruct matTempl[];
};


out vec4 v_lighting;
out vec2 v_tex;
out vec2 v_los;

#if USE_SHADOW
  out vec4 v_shadow;
#endif

#if (USE_INSTANCING || USE_GPU_SKINNING) && USE_AO
  out vec2 v_tex2;
#endif

#if USE_SPECULAR || USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX
  out vec4 v_normal;
  #if (USE_INSTANCING || USE_GPU_SKINNING) && (USE_NORMAL_MAP || USE_PARALLAX)
    out vec4 v_tangent;
    //varying vec3 v_bitangent;
  #endif
  #if USE_SPECULAR || USE_SPECULAR_MAP
    out vec3 v_half;
  #endif
  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_PARALLAX
    out vec3 v_eyeVec;
  #endif
#endif

in vec3 a_vertex;
in vec3 a_normal;
#if (USE_INSTANCING || USE_GPU_SKINNING)
  in vec4 a_tangent;
#endif
in vec2 a_uv0;
in vec2 a_uv1;

#if USE_GPU_SKINNING
  const int MAX_INFLUENCES = 4;
  const int MAX_BONES = 64;

  // TODO: skindBlendMatrices could be set to a fixed size (MAX_INSTANCES * MAX_BONES), but the Nvidia drivers have a bug
  // that causes terribly long (tens of minutes) linking times when large fixed size SSBOs are used.
  buffer GPUSkinningBlock 
  {
    mat4 skinBlendMatrices[];
  } gpuSkinning;
  in vec4 a_skinJoints;
  in vec4 a_skinWeights;
#endif


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

//mat4 draws[a_drawId].instancingTransform = mat4(1.0, 0,   0,   0,
//                        0  , 1.0, 0,   0,
//                        0  , 0,   1.0, 0,
//                        200, 200, 300, 1.0);

  vs_out.drawID = a_drawId;

#if 0
  if (vec2(1.0, 1.0) != matTempl[matTemplIdVal].windData.xy)
  {
	const vec4 vertices[] = vec4[](vec4( 0.25+materialIDVal*0.05, -0.25, 0.5, 1.0),
                                       vec4( 0.25+materialIDVal*0.05,  0.25, 0.5, 1.0),
                                       vec4(-0.25+materialIDVal*0.05, -0.25, 0.5, 1.0));
	//const vec4 vertices[] = vec4[](vec4( 0.25, -0.25, 0.1, 1.0),
        //                               vec4( 0.25,  0.25, 0.1, 1.0),
        //                               vec4(-0.25, -0.25, 0.1, 1.0));
	if (gl_VertexID < 3)
	{
        	gl_Position = vertices[gl_VertexID] * mat4(1.0); //draws[a_drawId].instancingTransform;
	}
	return;
  }
#endif

  #if USE_GPU_SKINNING
    vec3 p = vec3(0.0);
    vec3 n = vec3(0.0);
    for (int i = 0; i < MAX_INFLUENCES; ++i) {
      int joint = int(a_skinJoints[i]);
      if (joint != 0xff) {
        mat4 m = gpuSkinning.skinBlendMatrices[a_drawId * MAX_BONES + joint];
        p += vec3(m * vec4(a_vertex, 1.0)) * a_skinWeights[i];
        n += vec3(m * vec4(a_normal, 0.0)) * a_skinWeights[i];
      }
    }
    vec4 position = draws[a_drawId].instancingTransform * vec4(p, 1.0);
    mat3 normalMatrix = mat3(draws[a_drawId].instancingTransform[0].xyz, draws[a_drawId].instancingTransform[1].xyz, draws[a_drawId].instancingTransform[2].xyz);
    vec3 normal = normalMatrix * normalize(n);
    #if (USE_NORMAL_MAP || USE_PARALLAX)
      vec3 tangent = normalMatrix * a_tangent.xyz;
    #endif
  #else
  #if (USE_INSTANCING)
    vec4 position = draws[a_drawId].instancingTransform * vec4(a_vertex, 1.0);
    mat3 normalMatrix = mat3(draws[a_drawId].instancingTransform[0].xyz, draws[a_drawId].instancingTransform[1].xyz, draws[a_drawId].instancingTransform[2].xyz);
    vec3 normal = normalMatrix * a_normal;
    #if (USE_NORMAL_MAP || USE_PARALLAX)
      vec3 tangent = normalMatrix * a_tangent.xyz;
    #endif
  #else
    vec4 position = vec4(a_vertex, 1.0);
    vec3 normal = a_normal;
  #endif
  #endif


  #if USE_WIND
    vec2 wind = matTempl[matTemplIdVal].windData.xy;

    // fractional part of model position, clamped to >.4
    vec4 modelPos = draws[a_drawId].instancingTransform[3];
    modelPos = fract(modelPos);
    modelPos = clamp(modelPos, 0.4, 1.0);

    // crude measure of wind intensity
    float abswind = abs(wind.x) + abs(wind.y);

    vec4 cosVec;
    // these determine the speed of the wind's "cosine" waves.
    cosVec.w = 0.0;
    cosVec.x = sim_time.x * modelPos[0] + position.x;
    cosVec.y = sim_time.x * modelPos[2] / 3.0 + draws[a_drawId].instancingTransform[3][0];
    cosVec.z = sim_time.x * abswind / 4.0 + position.z;

    // calculate "cosines" in parallel, using a smoothed triangle wave
    cosVec = fakeCos(cosVec);

    float limit = clamp((a_vertex.x * a_vertex.z * a_vertex.y) / 3000.0, 0.0, 0.2);

    float diff = cosVec.x * limit; 
    float diff2 = cosVec.y * clamp(a_vertex.y / 60.0, 0.0, 0.25);

    // fluttering of model parts based on distance from model center (ie longer branches)
    position.xyz += cosVec.z * limit * clamp(abswind, 1.2, 1.7);

    // swaying of trunk based on distance from ground (higher parts sway more)
    position.xz += diff + diff2 * wind;
  #endif


  gl_Position = transform * position;

  #if USE_SPECULAR || USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX
    v_normal.xyz = normal;

    #if (USE_INSTANCING || USE_GPU_SKINNING) && (USE_NORMAL_MAP || USE_PARALLAX)
      v_tangent.xyz = tangent;
      vec3 bitangent = cross(v_normal.xyz, v_tangent.xyz) * a_tangent.w;
      v_normal.w = bitangent.x;
      v_tangent.w = bitangent.y;
      v_lighting.w = bitangent.z;
    #endif

    #if USE_SPECULAR || USE_SPECULAR_MAP || USE_PARALLAX
      vec3 eyeVec = cameraPos.xyz - position.xyz;
      #if USE_SPECULAR || USE_SPECULAR_MAP     
        vec3 sunVec = -sunDir;
        v_half = normalize(sunVec + normalize(eyeVec));
      #endif
      #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_PARALLAX
        v_eyeVec = eyeVec;
      #endif
    #endif
  #endif
  
  v_lighting.xyz = max(0.0, dot(normal, -sunDir)) * sunColor;

  v_tex = a_uv0;

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_AO
    v_tex2 = a_uv1;
  #endif

  #if USE_SHADOW
    v_shadow = shadowTransform * position;
    #if USE_SHADOW_SAMPLER && USE_SHADOW_PCF
      v_shadow.xy *= shadowScale.xy;
    #endif  
  #endif

  v_los = position.xz * losTransform.x + losTransform.y;

}
