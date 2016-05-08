#version 430
#extension GL_ARB_bindless_texture : require

out vec4 fragColor;

in VS_OUT
{
  flat uint drawID;
} fs_in;

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

layout(shared) buffer MaterialIDBlock
{
  uint materialID[];
};

in vec2 v_tex;

void main()
{
  const uint materialIDVal = materialID[model[fs_in.drawID].modelId];
  vec4 tex = texture2D(material[materialIDVal].baseTex, v_tex);

  #ifdef REQUIRE_ALPHA_GEQUAL
    if (tex.a < REQUIRE_ALPHA_GEQUAL)
      discard;
  #endif

  fragColor = tex;
}
