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

uniform sampler2D waterTex;

float waterDepth = 4.0;		
float fullDepth = 5.0;		// Depth at which to use full murkiness (shallower water will be clearer)


in vec4 worldPos;
in vec4 v_tex;
in vec4 v_shadow;
in vec2 v_los;


float get_shadow(vec4 coords)
{
  #if USE_SHADOW && !DISABLE_RECEIVE_SHADOWS
    #if USE_SHADOW_SAMPLER
      #if USE_SHADOW_PCF
        vec2 offset = fract(coords.xy - 0.5);
        vec4 size = vec4(offset + 1.0, 2.0 - offset);
		vec4 weight = (vec4(1.0, 1.0, -0.5, -0.5) + (coords.xy - 0.5*offset).xyxy) * shadowScale.zwzw;
        return (1.0/9.0)*dot(size.zxzx*size.wwyy,
          vec4(texture(shadowTex, vec3(weight.zw, coords.z)).r,
               texture(shadowTex, vec3(weight.xw, coords.z)).r,
               texture(shadowTex, vec3(weight.zy, coords.z)).r,
               texture(shadowTex, vec3(weight.xy, coords.z)).r));
      #else
        return texture(shadowTex, coords.xyz).r;
      #endif
    #else
      if (coords.z >= 1.0)
        return 1.0;
      return (coords.z <= texture2D(shadowTex, coords.xy).x ? 1.0 : 0.0);
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

	vec3 n, l, h, v;		// Normal, light vector, half-vector and view vector (vector to eye)
	float ndotl, ndoth, ndotv;
	float fresnel;
	float t;				// Temporary variable
	vec2 reflCoords, refrCoords;
	vec3 reflColor, refrColor, specular;
	float losMod;

	//vec4 wtex = textureGrad(waterTex, vec3(fract(v_tex.xy), v_tex.z), dFdx(v_tex.xy), dFdy(v_tex.xy));
	vec4 wtex = texture2D(waterTex, fract(v_tex.xy));

	n = normalize(wtex.xzy - vec3(0.5, 0.5, 0.5));
	l = -sunDir;
	v = normalize(cameraPos - worldPos.xyz);
	h = normalize(l + v);
	
	ndotl = dot(n, l);
	ndoth = dot(n, h);
	ndotv = dot(n, v);
	
	fresnel = pow(1.0 - ndotv, 0.8);	// A rather random Fresnel approximation
	
	//refrCoords = (0.5*gl_TexCoord[2].xy - 0.8*matTemplWater[matTemplIdVal].waviness*n.xz) / gl_TexCoord[2].w + 0.5;	// Unbias texture coords
	//reflCoords = (0.5*gl_TexCoord[1].xy + matTemplWater[matTemplIdVal].waviness*n.xz) / gl_TexCoord[1].w + 0.5;	// Unbias texture coords
	
	//vec3 dir = normalize(v + vec3(matTemplWater[matTemplIdVal].waviness*n.x, 0.0, matTemplWater[matTemplIdVal].waviness*n.z));

	vec3 eye = reflect(v, n);
	
	vec3 tex = texture(skyCube, eye).rgb;

	reflColor = mix(tex, sunColor * matTemplWater[matTemplIdVal].reflectionTint,
					matTemplWater[matTemplIdVal].reflectionTintStrength);

	//waterDepth = 4.0 + 2.0 * dot(abs(v_tex.zw - 0.5), vec2(0.5));
	waterDepth = 4.0;
	
	//refrColor = (0.5 + 0.5*ndotl) * mix(texture2D(refractionMap, refrCoords).rgb, sunColor * tint,
	refrColor = (0.5 + 0.5*ndotl) * mix(vec3(0.3), sunColor * matTemplWater[matTemplIdVal].waterTint,
					matTemplWater[matTemplIdVal].murkiness * clamp(waterDepth / fullDepth, 0.0, 1.0)); // Murkiness and tint at this pixel (tweaked based on lighting and depth)
	
	specular = pow(max(0.0, ndoth), 150.0f) * sunColor * matTemplWater[matTemplIdVal].specularStrength;

	losMod = texture2D(losTex, v_los).a;

	//losMod = texture2D(losMap, gl_TexCoord[3].st).a;

#if USE_SHADOW
	float shadow = get_shadow(vec4(v_shadow.xy - 8*matTemplWater[matTemplIdVal].waviness*n.xz, v_shadow.zw));
	float fresShadow = mix(fresnel, fresnel*shadow, dot(sunColor, vec3(0.16666)));
#else
	float fresShadow = fresnel;
#endif
	
	vec3 color = mix(refrColor + 0.3*specular, reflColor + specular, fresShadow);

	fragColor.rgb = color * losMod;


	//fragColor.rgb = mix(refrColor + 0.3*specular, reflColor + specular, fresnel) * losMod;
	
	// Make alpha vary based on both depth (so it blends with the shore) and view angle (make it
	// become opaque faster at lower view angles so we can't look "underneath" the water plane)
	t = 18.0 * max(0.0, 0.7 - v.y);
	fragColor.a = 0.15 * waterDepth * (1.2 + t + fresnel);
}

