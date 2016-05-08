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

// TODO: It has to be ensured that all blocks in all shaders are the same
// (they must not have different defines that cause a difference)

  #if USE_SHADOW_SAMPLER
    layout (bindless_sampler) sampler2DShadow shadowTex;
  #else
    layout (bindless_sampler) sampler2D shadowTex;
  #endif

} frame;


// TODO: make block members conditional again
struct ModelStruct
{
  uint modelId;
  mat4 instancingTransform;
};

layout(shared) buffer ModelBlock
{
  ModelStruct model[];
};

layout(shared) buffer PlayerColorBlock
{
  vec4 playerColor[];
};

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
	const uint modelId = model[fs_in.drawID].modelId;
	fragColor = vec4(get_fog(playerColor[modelId].rgb), playerColor[modelId].a);
}
