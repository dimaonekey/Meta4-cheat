#pragma once

// ── Порт из meowrele: jni/func/mapcollider.hpp ────────────────────────────
//  Перенесён в neverlose10. Сохранён namespace collider_esp (draw/load_map),
//  конфиг живет под cfg::world::collider_*  (см. src/ui/cfg.hpp "namespace world").
//  Include-пути переписаны под дерево neverlose10:
//    src/ui/theme/theme.hpp  ── здесь лежит g_sw / g_sh
//    src/other/vector3.h     ── Vector3
//    src/game/game.hpp       ── struct matrix

#include "imgui.h"
#include "../game/game.hpp"
#include "../other/vector3.h"
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"
#include "map.h"

namespace collider_esp {

inline void draw(ImDrawList* /*dl*/, const matrix& vm, const Vector3& cam_pos)
{
    if (!cfg::world::collider_enabled) return;
    if (!map_service::g_loaded) return;

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    if (!bg) return;
    map_service::Update();

    const ImVec4& c     = cfg::world::collider_color;
    const float   alpha = cfg::world::collider_alpha;

    map_service::g_color = IM_COL32(
        (int)(c.x * 255.f),
        (int)(c.y * 255.f),
        (int)(c.z * 255.f),
        (int)(alpha * 255.f));

    map_service::g_cam_pos.x = cam_pos.x;
    map_service::g_cam_pos.y = cam_pos.y;
    map_service::g_cam_pos.z = cam_pos.z;
    map_service::g_max_dist  = cfg::world::collider_max_dist;

    map_service::g_enabled = true;
    map_service::Render(bg, vm, g_sw, g_sh);
    map_service::g_enabled = false;
}

inline void load_map(int index)
{
    cfg::world::collider_map_id       = index;
    map_service::g_collider_map_index = index;
    map_service::LoadColliderMapByIndex(index);
}

} // namespace collider_esp
