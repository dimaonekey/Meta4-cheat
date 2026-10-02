#pragma once

#include "imgui.h"
#include "bar.hpp"
#include "theme/theme.hpp"
#include "Android_draw/draw.h"
#include <cstdio>
#include <cstring>

namespace watermark {

    inline void render() {
        ImGuiIO& io = ImGui::GetIO();
        float sw = io.DisplaySize.x;

        float sc = sw / 1920.f;

        float pad_x = 16.f * sc;
        float pad_y = 10.f * sc;
        float text_sz = 22.f * sc;
        float rounding = 22.f * sc;
        float sep_w = 1.5f * sc;
        float sep_h = 14.f * sc;
        float sep_gap = 10.f * sc;

        char fps_buf[16];
        snprintf(fps_buf, sizeof(fps_buf), "%.0f", io.Framerate);

        ImVec2 ts1 = espFont ? espFont->CalcTextSizeA(text_sz, FLT_MAX, 0.f, "euphoria") : ImVec2(0, text_sz);
        ImVec2 ts2 = espFont ? espFont->CalcTextSizeA(text_sz, FLT_MAX, 0.f, "external") : ImVec2(0, text_sz);
        ImVec2 ts3 = espFont ? espFont->CalcTextSizeA(text_sz, FLT_MAX, 0.f, fps_buf) : ImVec2(0, text_sz);
        ImVec2 ts4 = espFont ? espFont->CalcTextSizeA(text_sz, FLT_MAX, 0.f, " fps") : ImVec2(0, text_sz);

        float content_w = pad_x + ts1.x + sep_gap + sep_w + sep_gap + ts2.x + sep_gap + sep_w + sep_gap + ts3.x + ts4.x + pad_x;
        float content_h = pad_y + ts1.y + pad_y;

        float cx = sw * 0.5f;
        float cy = 100.f * sc;

        float rx = cx - content_w * 0.5f;
        float ry = cy - content_h * 0.5f;

        ImDrawList* dl = ImGui::GetForegroundDrawList();

        dl->AddRectFilled(
            ImVec2(rx, ry),
            ImVec2(rx + content_w, ry + content_h),
            ui::theme::wm_bg(),
            rounding
        );

        float cur_x = rx + pad_x;
        float text_y = ry + (content_h - ts1.y) * 0.5f;

        if (espFont) {
            ImU32 txt = ui::theme::wm_text();

            dl->AddText(espFont, text_sz, ImVec2(cur_x, text_y), txt, "Acheron");
            cur_x += ts1.x;

            float sep_cy = ry + content_h * 0.5f;
            cur_x += sep_gap;
            dl->AddLine(ImVec2(cur_x, sep_cy - sep_h * 0.5f), ImVec2(cur_x, sep_cy + sep_h * 0.5f), ui::theme::sep(), sep_w);
            cur_x += sep_w + sep_gap;

            dl->AddText(espFont, text_sz, ImVec2(cur_x, text_y), txt, "external");
            cur_x += ts2.x;

            cur_x += sep_gap;
            dl->AddLine(ImVec2(cur_x, sep_cy - sep_h * 0.5f), ImVec2(cur_x, sep_cy + sep_h * 0.5f), ui::theme::sep(), sep_w);
            cur_x += sep_w + sep_gap;

            dl->AddText(espFont, text_sz, ImVec2(cur_x, text_y), txt, fps_buf);
            cur_x += ts3.x;
            dl->AddText(espFont, text_sz, ImVec2(cur_x, text_y), txt, " fps");
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowPos(ImVec2(rx, ry));
        ImGui::SetNextWindowSize(ImVec2(content_w, content_h));
        ImGui::Begin("##wm", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        if (ImGui::InvisibleButton("##wm_btn", ImVec2(content_w, content_h))) {
            ui::bar::g_open = !ui::bar::g_open;
        }
        ImGui::End();
        ImGui::PopStyleVar(1);
    }

}
