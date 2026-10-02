// src/globals.cpp
// Определяем только glow_tex — iconTex, g_sw, g_sh уже определены
// в include-дереве visuals.cpp (через visuals.hpp / chams2.hpp или draw.o).

#include "Android_draw/draw.h"

namespace chams {
ImTextureID glow_tex = nullptr;
}
ImTextureID iconTex  = nullptr;

float g_sw = 0.f;
float g_sh = 0.f;
