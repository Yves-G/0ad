#version 430

#extension GL_ARB_bindless_texture : require

// TODO: hardcoded maximum buffer sizes are bad
const int MAX_MATERIAL_TEMPLATES = 2000;

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

#if USE_SHADOW
  in vec4 v_shadow;
#endif

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

in vec4 v_lighting;
in vec2 v_tex;
in vec2 v_los;

#if (USE_INSTANCING || USE_GPU_SKINNING) && USE_AO
  in vec2 v_tex2;
#endif

#if USE_SPECULAR || USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX
  in vec4 v_normal;
  #if (USE_INSTANCING || USE_GPU_SKINNING) && (USE_NORMAL_MAP || USE_PARALLAX)
    in vec4 v_tangent;
    //in vec3 v_bitangent;
  #endif
  #if USE_SPECULAR || USE_SPECULAR_MAP
    in vec3 v_half;
  #endif
  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_PARALLAX
    in vec3 v_eyeVec;
  #endif
#endif

float get_shadow()
{
  float shadowBias = 0.003;
  #if USE_SHADOW && !DISABLE_RECEIVE_SHADOWS
    float biasedShdwZ = v_shadow.z - shadowBias;
    //#if USE_SHADOW_SAMPLER
      #if USE_SHADOW_PCF
        vec2 offset = fract(v_shadow.xy - 0.5);
        vec4 size = vec4(offset + 1.0, 2.0 - offset);
        vec4 weight = (vec4(1.0, 1.0, -0.5, -0.5) + (v_shadow.xy - 0.5*offset).xyxy) * shadowScale.zwzw;
        return (1.0/9.0)*dot(size.zxzx*size.wwyy,
          vec4(texture(shadowTex, vec3(weight.zw, biasedShdwZ)).r,
               texture(shadowTex, vec3(weight.xw, biasedShdwZ)).r,
               texture(shadowTex, vec3(weight.zy, biasedShdwZ)).r,
               texture(shadowTex, vec3(weight.xy, biasedShdwZ)).r));
      #else
        return texture(shadowTex, vec3(v_shadow.xy, biasedShdwZ)).r;
      #endif
    //#else
    //  if (biasedShdwZ >= 1.0)
    //    return 1.0;
    //  return (biasedShdwZ < texture(shadowTex, v_shadow.xy).x ? 1.0 : 0.0);
    //#endif
  #else
    return 1.0;
  #endif
}

vec3 get_fog(vec3 color)
{
  float density = fogParams.x;
  float maxFog = fogParams.y;

  const float LOG2 = 1.442695;
  float z = gl_FragCoord.z / gl_FragCoord.w;
  float fogFactor = exp2(-density * density * z * z * LOG2);

  fogFactor = fogFactor * (1.0 - maxFog) + maxFog;

  fogFactor = clamp(fogFactor, 0.0, 1.0);

  return mix(fogColor, color, fogFactor);
}

void main()
{
  const uint modelIdVal = draws[fs_in.drawID].modelId;
  const uint materialIDVal = models[modelIdVal].matId;
  const uint matTemplIdVal = material[materialIDVal].templateMatId;

  vec2 coord = v_tex;

  #if (USE_INSTANCING || USE_GPU_SKINNING) && (USE_PARALLAX || USE_NORMAL_MAP)
    vec3 bitangent = vec3(v_normal.w, v_tangent.w, v_lighting.w);
    mat3 tbn = mat3(v_tangent.xyz, bitangent, v_normal.xyz);
  #endif

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_PARALLAX
  {
    float h = texture(material[materialIDVal].normTex, coord).a;

    vec3 eyeDir = normalize(v_eyeVec * tbn);
    float dist = length(v_eyeVec);

    vec2 move;
    float height = 1.0;
    float scale = matTempl[matTemplIdVal].effectSettings.z;
	  
    int iter = int(min(20, 25.0 - dist/10.0));
	
	if (iter > 0.01)
	{
		float s = 1.0/iter;
		float t = s;
		move = vec2(-eyeDir.x, eyeDir.y) * scale / (eyeDir.z * iter);
		vec2 nil = vec2(0.0);

		for (int i = 0; i < iter; ++i) {
		  height -= t;
		  t = (h < height) ? s : 0.0;
		  vec2 temp = (h < height) ? move : nil;
		  coord += temp;
		  h = texture(material[materialIDVal].normTex, coord).a;
		}
		  
		// Move back to where we collided with the surface.  
		// This assumes the surface is linear between the sample point before we 
		// intersect the surface and after we intersect the surface
		float hp = texture(material[materialIDVal].normTex, coord - move).a;
		coord -= move * ((h - height) / (s + h - hp));
	}
  }
  #endif

  vec4 tex = texture(material[materialIDVal].baseTex, coord);

  // Alpha-test as early as possible
  #ifdef REQUIRE_ALPHA_GEQUAL
    if (tex.a < REQUIRE_ALPHA_GEQUAL)
      discard;
  #endif

  #if USE_TRANSPARENT
    fragColor.a = tex.a;
  #else
    fragColor.a = 1.0;
  #endif
  
  vec3 texdiffuse = tex.rgb;

  // Apply-coloring based on texture alpha
  #if USE_OBJECTCOLOR
    texdiffuse *= mix(material[materialIDVal].objectColor, vec3(1.0, 1.0, 1.0), tex.a);
  #else
  #if USE_PLAYERCOLOR
    texdiffuse *= mix(models[modelIdVal].playerColor.rgb, vec3(1.0, 1.0, 1.0), tex.a);
  #endif
  #endif


  #if USE_SPECULAR || USE_SPECULAR_MAP || USE_NORMAL_MAP
    vec3 normal = v_normal.xyz;
  #endif

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_NORMAL_MAP
    vec3 ntex = texture(material[materialIDVal].normTex, coord).rgb * 2.0 - 1.0;
    ntex.y = -ntex.y;
    normal = normalize(tbn * ntex);
    vec3 bumplight = max(dot(-sunDir, normal), 0.0) * sunColor;
    vec3 sundiffuse = (bumplight - v_lighting.rgb) * matTempl[matTemplIdVal].effectSettings.x + v_lighting.rgb;
  #else
    vec3 sundiffuse = v_lighting.rgb;
  #endif

  vec4 specular = vec4(0.0);
  #if USE_SPECULAR || USE_SPECULAR_MAP
    vec3 specCol;
    float specPow;
    #if USE_SPECULAR_MAP
      vec4 s = texture(material[materialIDVal].specTex, coord);
      specCol = s.rgb;
      specular.a = s.a;
      specPow = matTempl[matTemplIdVal].effectSettings.y;
    #else
      specCol = matTempl[matTemplIdVal].specularColor;
      specPow = matTempl[matTemplIdVal].specularPower;
    #endif
    specular.rgb = sunColor * specCol * pow(max(0.0, dot(normalize(normal), v_half)), specPow);
  #endif

  vec3 color = (texdiffuse * sundiffuse + specular.rgb) * get_shadow();
  vec3 ambColor = texdiffuse * ambient;

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_AO
    vec3 ao = texture(material[materialIDVal].aoTex, v_tex2).rrr;
    ao = mix(vec3(1.0), ao * 2.0, matTempl[matTemplIdVal].effectSettings.w);
    ambColor *= ao;
  #endif

  color += ambColor;

  #if USE_SPECULAR_MAP && USE_SELF_LIGHT
    color = mix(texdiffuse, color, specular.a);
  #endif

  color = get_fog(color);

  #if !IGNORE_LOS
    float los = texture(losTex, v_los).a;
    los = los < 0.03 ? 0.0 : los;
    color *= los;
  #endif

  color *= models[modelIdVal].shadingColor;

  fragColor.rgb = color;
}
