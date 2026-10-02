#pragma once

#include <cstdint>
#include <imgui.h>
#include "../game/math.hpp"

namespace visuals {

void draw_trail(ImDrawList* dl, uint64_t player);
void update_trail(uint64_t player, const ImVec2& pos);
void dbox(const ImVec2&, const ImVec2&, float);
void dbox_corner(const ImVec2&, const ImVec2&, float, float);
void dhp(int, const ImVec2&, const ImVec2&, float, float);
void darrow(float target_scrx, float target_scry, float alpha, float distance);

void dnick(const char*, const ImVec2&, const ImVec2&, float, float, float);
void ddist(float dist, const ImVec2& pos, float sz, float a);

void darmor_bottom(int, const ImVec2&, const ImVec2&, float);

void draw_text_outlined(ImDrawList*, ImFont*, float, const ImVec2&, ImU32, const char*);

void draw_box3d(const Vector3&, const matrix&, const ImVec4&, float);
void draw_box3d_fill(const Vector3&, const matrix&, const ImVec4&, float);
void draw_box3d_fill_base(const Vector3& pos, const matrix& view, const ImVec4& col, float a);
void dskeleton         (ImDrawList* dl, const ImRect& r, uint64_t player, const matrix& vm);
void dping(int ping, const ImVec2& pos, float sz, float a);
void ddevice(uint64_t player, const ImVec2& box_min, const ImVec2& box_max, float a);
void draw();

// ← ДОБАВИТЬ ЭТИ ДВЕ СТРОКИ:
void update_bullet_tracers(uint64_t LocalPlayer);
void draw_bullet_tracers(const matrix& view);
void add_hit_marker(const Vector3& hit_pos);
void draw_hit_markers(const matrix& view);;
void draw_radar(float alpha);
    void draw_enemy_info_panel(float alpha);
    void draw_active_features(float alpha);
    struct EnemyEntry {
        float   world_x, world_y, world_z;  // позиция в мире
        float   local_x, local_y, local_z;  // позиция локала (для радара)
        int     health;
        float   distance;
        char    name[32];
        bool    valid;
    };

    static const int MAX_ENEMIES = 64;
    extern EnemyEntry g_enemies[MAX_ENEMIES];
    extern int        g_enemy_count;
    // Угол поворота локала по Y (yaw) для радара
    extern float      g_local_yaw;

}
