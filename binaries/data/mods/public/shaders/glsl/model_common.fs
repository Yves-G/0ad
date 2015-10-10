#version 430

const int MAX_INSTANCES = 2000;
const int MAX_MATERIALS = 64;

in VS_OUT
{
  flat uint drawID;
} fs_in;

out vec4 fragColor;

layout(shared) uniform FrameUBO
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

} frame;

uniform sampler2D baseTex;
uniform sampler2D losTex;
uniform sampler2D aoTex;
uniform sampler2D normTex;
uniform sampler2D specTex;

#if USE_SHADOW
  in vec4 v_shadow;
  #if USE_SHADOW_SAMPLER
    uniform sampler2DShadow shadowTex;
//    #if USE_SHADOW_PCF
//      uniform vec4 shadowScale;
//    #endif
  #else
    uniform sampler2D shadowTex;
  #endif
#endif

// TODO: make these conditional again (in some way...)
layout(shared) buffer ModelUBO
{
  uint modelId[MAX_INSTANCES];
  //uint materialID[MAX_INSTANCES];
  mat4 instancingTransform[MAX_INSTANCES];
  //#if USE_OBJECTCOLOR
  //  vec3 objectColor[MAX_INSTANCES];
  //#else
  //#if USE_PLAYERCOLOR
  //  vec4 playerColor[MAX_INSTANCES];
  //#endif
  //#endif
  //vec3 shadingColor[MAX_INSTANCES];
} model;

layout(shared) buffer MaterialIDBlock
{
  uint materialID[];
};

layout(shared) buffer PlayerColorBlock
{
  vec4 playerColor[];
};

layout(shared) buffer ShadingColorBlock
{
  vec3 shadingColor[];
};

layout(shared) buffer MaterialUBO
{

//#if USE_SPECULAR
  float specularPower[MAX_MATERIALS];
  vec3 specularColor[MAX_MATERIALS];
//#endif

//#if USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX || USE_AO
  vec4 effectSettings[MAX_MATERIALS];
//#endif

  vec3 objectColor[MAX_MATERIALS];

  vec4 windData[MAX_MATERIALS];

} material;

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
        vec4 weight = (vec4(1.0, 1.0, -0.5, -0.5) + (v_shadow.xy - 0.5*offset).xyxy) * frame.shadowScale.zwzw;
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
    //  return (biasedShdwZ < texture2D(shadowTex, v_shadow.xy).x ? 1.0 : 0.0);
    //#endif
  #else
    return 1.0;
  #endif
}

vec3 get_fog(vec3 color)
{
  float density = frame.fogParams.x;
  float maxFog = frame.fogParams.y;

  const float LOG2 = 1.442695;
  float z = gl_FragCoord.z / gl_FragCoord.w;
  float fogFactor = exp2(-density * density * z * z * LOG2);

  fogFactor = fogFactor * (1.0 - maxFog) + maxFog;

  fogFactor = clamp(fogFactor, 0.0, 1.0);

  return mix(frame.fogColor, color, fogFactor);
}

void main()
{
  const uint materialIDVal = materialID[model.modelId[fs_in.drawID]];

  vec2 coord = v_tex;

  #if (USE_INSTANCING || USE_GPU_SKINNING) && (USE_PARALLAX || USE_NORMAL_MAP)
    vec3 bitangent = vec3(v_normal.w, v_tangent.w, v_lighting.w);
    mat3 tbn = mat3(v_tangent.xyz, bitangent, v_normal.xyz);
  #endif

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_PARALLAX
  {
    float h = texture2D(normTex, coord).a;

    vec3 eyeDir = normalize(v_eyeVec * tbn);
    float dist = length(v_eyeVec);

    vec2 move;
    float height = 1.0;
    float scale = material.effectSettings[materialIDVal].z;
	  
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
		  h = texture2D(normTex, coord).a;
		}
		  
		// Move back to where we collided with the surface.  
		// This assumes the surface is linear between the sample point before we 
		// intersect the surface and after we intersect the surface
		float hp = texture2D(normTex, coord - move).a;
		coord -= move * ((h - height) / (s + h - hp));
	}
  }
  #endif

  vec4 tex = texture2D(baseTex, coord);

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
    texdiffuse *= mix(material.objectColor[materialIDVal], vec3(1.0, 1.0, 1.0), tex.a);
  #else
  #if USE_PLAYERCOLOR
    texdiffuse *= mix(playerColor[model.modelId[fs_in.drawID]].rgb, vec3(1.0, 1.0, 1.0), tex.a);
  #endif
  #endif

  #if USE_SPECULAR || USE_SPECULAR_MAP || USE_NORMAL_MAP
    vec3 normal = v_normal.xyz;
  #endif

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_NORMAL_MAP
    vec3 ntex = texture2D(normTex, coord).rgb * 2.0 - 1.0;
    ntex.y = -ntex.y;
    normal = normalize(tbn * ntex);
    vec3 bumplight = max(dot(-frame.sunDir, normal), 0.0) * frame.sunColor;
    vec3 sundiffuse = (bumplight - v_lighting.rgb) * material.effectSettings[materialIDVal].x + v_lighting.rgb;
  #else
    vec3 sundiffuse = v_lighting.rgb;
  #endif

  vec4 specular = vec4(0.0);
  #if USE_SPECULAR || USE_SPECULAR_MAP
    vec3 specCol;
    float specPow;
    #if USE_SPECULAR_MAP
      vec4 s = texture2D(specTex, coord);
      specCol = s.rgb;
      specular.a = s.a;
      specPow = material.effectSettings[materialIDVal].y;
    #else
      specCol = material.specularColor[materialIDVal];
      specPow = material.specularPower[materialIDVal];
    #endif
    specular.rgb = frame.sunColor * specCol * pow(max(0.0, dot(normalize(normal), v_half)), specPow);
  #endif

  vec3 color = (texdiffuse * sundiffuse + specular.rgb) * get_shadow();
  vec3 ambColor = texdiffuse * frame.ambient;

  #if (USE_INSTANCING || USE_GPU_SKINNING) && USE_AO
    vec3 ao = texture2D(aoTex, v_tex2).rrr;
    ao = mix(vec3(1.0), ao * 2.0, material.effectSettings[materialIDVal].w);
    ambColor *= ao;
  #endif

  color += ambColor;

  #if USE_SPECULAR_MAP && USE_SELF_LIGHT
    color = mix(texdiffuse, color, specular.a);
  #endif

  color = get_fog(color);

  #if !IGNORE_LOS
    float los = texture2D(losTex, v_los).a;
    los = los < 0.03 ? 0.0 : los;
    color *= los;
  #endif

  color *= shadingColor[model.modelId[fs_in.drawID]];

  fragColor.rgb = color;
}
