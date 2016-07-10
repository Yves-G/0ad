#version 430
#extension GL_ARB_bindless_texture : require

in VS_OUT
{
  flat uint drawID;
} fs_in;

out vec4 fragColor;

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
	layout (bindless_sampler) samplerCube skyCube;
	layout (bindless_sampler) sampler2D waterTex;

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

in vec4 v_tex;
in vec4 v_shadow;
in vec2 v_los;
in vec3 v_half;
in vec3 v_normal;
in float v_transp;
in vec3 v_lighting;

float get_shadow()
{
  #if USE_SHADOW && !DISABLE_RECEIVE_SHADOWS
    #if USE_SHADOW_SAMPLER
      #if USE_SHADOW_PCF
        vec2 offset = fract(v_shadow.xy - 0.5);
        vec4 size = vec4(offset + 1.0, 2.0 - offset);
        vec4 weight = (vec4(1.0, 1.0, -0.5, -0.5) + (v_shadow.xy - 0.5*offset).xyxy) * shadowScale.zwzw;
        return (1.0/9.0)*dot(size.zxzx*size.wwyy,
          vec4(texture(shadowTex, vec3(weight.zw, v_shadow.z)).r,
               texture(shadowTex, vec3(weight.xw, v_shadow.z)).r,
               texture(shadowTex, vec3(weight.zy, v_shadow.z)).r,
               texture(shadowTex, vec3(weight.xy, v_shadow.z)).r));
      #else
        return texture(shadowTex, v_shadow.xyz).r;
      #endif
    #else
      if (v_shadow.z >= 1.0)
        return 1.0;
      return (v_shadow.z <= texture(shadowTex, v_shadow.xy).x ? 1.0 : 0.0);
    #endif
  #else
    return 1.0;
  #endif
}


void main()
{
	const uint modelIdVal = draws[fs_in.drawID].modelId;
	const uint materialIDVal = models[modelIdVal].matId;
	const uint matTemplIdVal = material[materialIDVal].templateMatId;

	//vec4 texdiffuse = textureGrad(material[materialIDVal].baseTex, vec3(fract(v_tex.xy), v_tex.z), dFdx(v_tex.xy), dFdy(v_tex.xy));
	vec4 texdiffuse = texture(material[materialIDVal].baseTex, fract(v_tex.xy));

	if (texdiffuse.a < 0.25)
		discard;

	texdiffuse.a *= v_transp;

	vec3 specular = sunColor * matTempl[matTemplIdVal].specularColor * pow(max(0.0, dot(normalize(v_normal), v_half)), matTempl[matTemplIdVal].specularPower);

	vec3 color = (texdiffuse.rgb * v_lighting + specular) * get_shadow();
        color += texdiffuse.rgb * ambient;

	#if !IGNORE_LOS
		float los = texture(losTex, v_los).a;
		los = los < 0.03 ? 0.0 : los;
		color *= los;
	#endif

	fragColor.rgb = color;
	fragColor.a = texdiffuse.a;
}

