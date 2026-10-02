// src/splash.hpp
#pragma once

#include "imgui.h"
#include "Android_draw/draw.h"  // espFont, iconTex

namespace {

// dl      — ForegroundDrawList
// sw, sh  — размер экрана
// sc      — scale (sw/1920)
// prog    — прогресс 0..1 (анимация бара)
// alpha   — прозрачность всего (1.f пока идёт, можно фейдить на выходе)
inline void splash(ImDrawList* dl, float sw, float sh,
                   float sc, float prog, float alpha) {

    // Затемнение фона
    ImU32 bg = IM_COL32(0, 0, 0, (int)(230 * alpha));
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(sw, sh), bg);

    float cx = sw * 0.5f;
    float cy = sh * 0.5f;

    // Иконка (если загружена)
    if (iconTex) {
        float isz = 80.f * sc;
        dl->AddImage(
            iconTex,
            ImVec2(cx - isz * 0.5f, cy - isz * 0.9f),
            ImVec2(cx + isz * 0.5f, cy + isz * 0.1f),
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, (int)(255 * alpha))
        );
    }

    // Текст под иконкой
    if (espFont) {
        float tsz  = 28.f * sc;
        const char* label = "Acheron";
        ImVec2 ts = espFont->CalcTextSizeA(tsz, FLT_MAX, 0.f, label);
        dl->AddText(espFont, tsz,
            ImVec2(cx - ts.x * 0.5f, cy + 18.f * sc),
            IM_COL32(255, 255, 255, (int)(200 * alpha)),
            label);

        float tsz2 = 18.f * sc;
        const char* sub = "external 0.39.2";
        ImVec2 ts2 = espFont->CalcTextSizeA(tsz2, FLT_MAX, 0.f, sub);
        dl->AddText(espFont, tsz2,
            ImVec2(cx - ts2.x * 0.5f, cy + 50.f * sc),
            IM_COL32(180, 180, 180, (int)(180 * alpha)),
            sub);
    }

    // Прогресс-бар
    float bw   = 200.f * sc;
    float bh   = 3.f   * sc;
    float bx   = cx - bw * 0.5f;
    float by   = cy + 80.f * sc;
    float brad = bh * 0.5f;

    // Фон бара
    dl->AddRectFilled(
        ImVec2(bx, by), ImVec2(bx + bw, by + bh),
        IM_COL32(60, 60, 60, (int)(200 * alpha)), brad);

    // Заполненная часть
    if (prog > 0.001f) {
        dl->AddRectFilled(
            ImVec2(bx, by), ImVec2(bx + bw * prog, by + bh),
            IM_COL32(255, 255, 255, (int)(240 * alpha)), brad);
    }
}

} // anonymous namespace