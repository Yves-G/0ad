#version 430

const int MAX_INSTANCES = 2000;
const int MAX_MATERIALS = 64;

in VS_OUT
{
  uint drawID;
} fs_in;

uniform FrameUBO
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

// TODO: make these conditional again (in some way...)
uniform ModelUBO
{
  uint materialID[MAX_INSTANCES];
  mat4 instancingTransform[MAX_INSTANCES];
  //#if USE_OBJECTCOLOR
  //  vec3 objectColor[MAX_INSTANCES];
  //#else
  //#if USE_PLAYERCOLOR
    vec4 playerColor[MAX_INSTANCES];
  //#endif
  //#endif
  vec3 shadingColor[MAX_INSTANCES];
} model;


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
	gl_FragColor = vec4(get_fog(model.playerColor[fs_in.drawID].rgb), model.playerColor[fs_in.drawID].a);
}
