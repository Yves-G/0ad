#version 430

out vec4 fragColor;

uniform sampler2D baseTex;

in vec2 v_tex;

void main()
{
  vec4 tex = texture2D(baseTex, v_tex);

  #ifdef REQUIRE_ALPHA_GEQUAL
    if (tex.a < REQUIRE_ALPHA_GEQUAL)
      discard;
  #endif

  fragColor = tex;
}
