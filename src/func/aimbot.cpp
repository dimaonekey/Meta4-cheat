#include "aimbot.hpp"
#include "../ui/cfg.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"
#include <cmath>
#include <algorithm>
#include "imgui.h"

// ─────────────────────────────────────────────────────────────────────────────
//  WEAPON OFFSETS — исправлено по external (Expodium_external/offsets.hpp)
//  OFF_WRC_ACTIVE_WEAPON было 0xA8 → правильное 0xA0
// ─────────────────────────────────────────────────────────────────────────────
#define W_OFF_WEAPON_ROOT   oxorany(0x88)   // Player → WeaponRootController*
#define W_OFF_ACTIVE_WC     oxorany(0xA0)   // WeaponRootController → WeaponController* (FIX: was 0xA8)
#define W_OFF_WC_PROPS      oxorany(0xA8)   // WeaponController → WeaponProperties*
#define W_OFF_WP_ID         oxorany(0x18)   // WeaponProperties → weapon_id (int)
#define W_OFF_FIRE_BTN      oxorany(0x148)  // WeaponController → fireButton (uint8_t)

static inline bool valid_addr(uint64_t a) {
    return (a >= 0x10000ULL && a <= 0x7FFFFFFFFFFFULL);
}

// ─────────────────────────────────────────────────────────────────────────────
//  STATE
// ─────────────────────────────────────────────────────────────────────────────
namespace aimbot {
    static int   last_weapon    = 0;
    static float last_pitch     = 0.0f;
    static float last_yaw       = 0.0f;

    // triggerbot
    static float trigger_timer  = 0.0f;
    static float shot_duration  = 0.0f;
    static bool  pulse_state    = false;

    // back camera
    // Сохранённые углы ДО того как аим начал вести цель.
    // Восстанавливаются когда цель пропадает и back_camera включена.
    static float bc_saved_pitch  = 0.0f;
    static float bc_saved_yaw    = 0.0f;
    static bool  bc_has_saved    = false;   // есть ли сохранённая позиция
    static bool  bc_returning    = false;   // идёт ли возврат прямо сейчас
    static bool  bc_had_target   = false;   // была ли цель в прошлом тике

    // ─────────────────────────────────────────────────────────────────────────
    //  DRAW FOV
    //
    //  Рисует круг FOV поверх ImGui overlay.
    //  cfg::aim::fov_draw      — bool, включить отрисовку
    //  cfg::aim::fov           — float [0, 500], радиус в пикселях
    //  cfg::aim::fov_check     — bool, включить угловой фильтр цели
    //  cfg::aim::fov_color     — ImVec4, цвет круга (RGBA, из cfg/UI)
    //  cfg::aim::fov_segments  — int, кол-во сегментов (рекомендуется 64)
    //
    //  Круг рисуется в абсолютных координатах overlay-окна.
    //  При fov > половина экрана — круг выходит за край, это ок по ТЗ.
    // ─────────────────────────────────────────────────────────────────────────
    void draw_fov() {
        if (!cfg::aim::fov_draw) return;
        if (!cfg::aim::fov_check) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        const float cx     = g_sw * 0.5f;
        const float cy     = g_sh * 0.5f;
        const float radius = cfg::aim::fov; // [0, 500]

        // Конвертируем ImVec4 (0..1 RGBA) в packed 32-bit color
        ImU32 col = ImGui::ColorConvertFloat4ToU32(cfg::aim::fov_color);

        const int segs = (cfg::aim::fov_segments > 0) ? cfg::aim::fov_segments : 64;
        dl->AddCircle(ImVec2(cx, cy), radius, col, segs, 1.5f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  BACK CAMERA
    //
    //  Вызывается ПОСЛЕ выбора цели (best_target уже известен).
    //  has_target=true  → аим ведёт цель: сохраняем текущие углы (позиция
    //                     до аима), возврат сбрасывается.
    //  has_target=false → цель пропала: если back_camera включена и есть
    //                     сохранённая позиция — плавно возвращаем туда.
    //
    //  back_camera_speed [0.0, 10.0]:
    //    0.0 → мгновенный снэп (factor=1.0)
    //    10.0 → очень медленный возврат (factor≈0.024/тик)
    // ─────────────────────────────────────────────────────────────────────────
    void back_camera_tick(uint64_t AimingData, float current_pitch, float current_yaw, bool has_target) {
        if (!valid_addr(AimingData)) return;

        if (has_target) {
            // Цель есть — сохраняем углы ДО того как аим их изменит.
            // Сохраняем только в первый тик появления цели, чтобы не
            // перезаписать позицию уже после того как аим сдвинул прицел.
            if (!bc_had_target) {
                bc_saved_pitch = current_pitch;
                bc_saved_yaw   = current_yaw;
                bc_has_saved   = true;
            }
            bc_returning  = false;
            bc_had_target = true;
            return;
        }

        // Цель пропала
        bc_had_target = false;

        if (!cfg::aim::back_camera) {
            // Фича выключена — сбрасываем состояние
            bc_returning = false;
            return;
        }

        if (!bc_has_saved) return; // ещё не было ни одной цели — не знаем куда возвращать

        bc_returning = true;

        // ── Плавный возврат к сохранённым углам ──────────────────────────────
        const float spd = cfg::aim::back_camera_speed; // [0.0, 10.0]
        float factor;
        if (spd <= 0.0f) {
            factor = 1.0f;
        } else {
            factor = 1.0f / (1.0f + spd * 4.0f);
            // spd=1  → ~0.20, spd=5  → ~0.048, spd=10 → ~0.024
        }

        float cur_p = rpm<float>(AimingData + 0x18);
        float cur_y = rpm<float>(AimingData + 0x1C);

        float dp = bc_saved_pitch - cur_p;
        float dy = bc_saved_yaw   - cur_y;
        while (dy >  180.0f) dy -= 360.0f;
        while (dy < -180.0f) dy += 360.0f;

        float new_p = cur_p + dp * factor;
        float new_y = cur_y + dy * factor;
        while (new_y >  180.0f) new_y -= 360.0f;
        while (new_y < -180.0f) new_y += 360.0f;
        new_p = std::clamp(new_p, -89.0f, 89.0f);

        if (std::abs(dp) < 0.05f && std::abs(dy) < 0.05f) {
            new_p        = bc_saved_pitch;
            new_y        = bc_saved_yaw;
            bc_returning = false;
            bc_has_saved = false; // позиция достигнута, сбрасываем
        }

        wpm<float>(AimingData + 0x18, new_p);
        wpm<float>(AimingData + 0x1C, new_y);
        wpm<float>(AimingData + 0x24, new_p);
        wpm<float>(AimingData + 0x28, new_y);
    }

    static inline uint64_t get_weapon_ctrl(uint64_t lp) {
        if (!valid_addr(lp)) return 0;
        uint64_t wrc = rpm<uint64_t>(lp + W_OFF_WEAPON_ROOT);
        if (!valid_addr(wrc)) return 0;
        uint64_t wc  = rpm<uint64_t>(wrc + W_OFF_ACTIVE_WC);
        if (!valid_addr(wc))  return 0;
        return wc;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  TRIGGERBOT TICK
    //
    //  Логика сравнена с Expodium_external/combat.cpp:
    //  1. Weapon chain: WRC + 0xA0 (не 0xA8)
    //  2. Адаптивный пиксельный радиус: cfg::aim::trigger_range * (100/dist_m),
    //     зажато в [12, 80]px. trigger_range теперь = "базовый px-радиус @ 100m".
    //  3. 6 костей: head/neck/spine2/spine1/spine/pelvis
    //  4. shot_duration 0.08f (external) вместо 0.1f
    // ─────────────────────────────────────────────────────────────────────────
    static void triggerbot_tick(
        uint64_t lp,
        uint64_t PlayerManager,
        const matrix& ViewMatrix,
        const Vector3& CameraPos,
        int LocalTeam)
    {
        if (!cfg::aim::triggerbot) {
            if (pulse_state || trigger_timer > 0.f) {
                uint64_t wc = get_weapon_ctrl(lp);
                if (valid_addr(wc)) wpm<uint8_t>(wc + W_OFF_FIRE_BTN, 2);
                pulse_state   = false;
                trigger_timer = 0.f;
            }
            return;
        }

        uint64_t wc = get_weapon_ctrl(lp);
        if (!valid_addr(wc)) {
            if (pulse_state || trigger_timer > 0.f) {
                pulse_state   = false;
                trigger_timer = 0.f;
            }
            return;
        }

        uint64_t PlayerList = rpm<uint64_t>(PlayerManager + oxorany(0x28));
        if (!PlayerList) return;
        int PlayerCount = rpm<int>(PlayerList + oxorany(0x20));
        if (PlayerCount <= 0 || PlayerCount > 128) return;
        uint64_t ListBuffer = rpm<uint64_t>(PlayerList + oxorany(0x18));
        if (!ListBuffer) return;

        const float cx = g_sw * 0.5f;
        const float cy = g_sh * 0.5f;

        bool can_trigger = false;

        for (int i = 0; i < PlayerCount && !can_trigger; i++) {
            uint64_t Player = rpm<uint64_t>(ListBuffer + oxorany(0x30) + oxorany(0x18) * i);
            if (!Player || Player == lp) continue;

            uint8_t PlayerTeam = rpm<uint8_t>(Player + offsets::player::team);
            if (PlayerTeam == static_cast<uint8_t>(LocalTeam)) continue;

            if (player::health(Player) <= 0) continue;

            if (cfg::aim::trigger_visible_only && !player::is_visible(Player)) continue;

            player::bones_t bones;
            if (!player::get_bones(Player, bones)) continue;

            // Дистанция до игрока для адаптивного радиуса
            Vector3 proot = player::position(Player);
            float pdx  = proot.x - CameraPos.x;
            float pdy  = proot.y - CameraPos.y;
            float pdz  = proot.z - CameraPos.z;
            float dist_m = sqrtf(pdx*pdx + pdy*pdy + pdz*pdz);

            // Адаптивный радиус: cfg::aim::trigger_range * (100/dist_m)
            // trigger_range трактуется как базовый px-радиус @ 100м
            // зажато в [12, 80]px
            float ref_dist  = dist_m < 1.f ? 1.f : dist_m;
            float radius_px = cfg::aim::trigger_range * (100.f / ref_dist);
            if (radius_px < 12.f) radius_px = 12.f;
            if (radius_px > 80.f) radius_px = 80.f;

            // 6 костей (полное тело — надёжно в любой позе)
            Vector3 check_bones[6];
            check_bones[0] = bones.head;
            check_bones[1] = bones.neck;
            check_bones[2] = bones.spine2;
            check_bones[3] = bones.spine1;
            check_bones[4] = bones.spine;
            check_bones[5] = bones.pelvis;

            for (int j = 0; j < 6 && !can_trigger; j++) {
                Vector3& bpos = check_bones[j];
                if (bpos.x == 0.f && bpos.y == 0.f && bpos.z == 0.f) continue;

                ImVec2 screen;
                if (!world_to_screen(bpos, ViewMatrix, screen)) continue;

                float sdx = screen.x - cx;
                float sdy = screen.y - cy;
                float screen_dist = sqrtf(sdx*sdx + sdy*sdy);

                if (screen_dist < radius_px) {
                    can_trigger = true;
                }
            }
        }

        // Пульсирующий выстрел
        float dt = ImGui::GetIO().DeltaTime;

        if (can_trigger) {
            if (!pulse_state) {
                trigger_timer += dt;
                if (trigger_timer >= cfg::aim::trigger_delay) {
                    wpm<uint8_t>(wc + W_OFF_FIRE_BTN, 3); // press
                    pulse_state   = true;
                    shot_duration = 0.f;
                }
            } else {
                shot_duration += dt;
                if (shot_duration >= 0.08f) {              // FIX: was 0.1f
                    wpm<uint8_t>(wc + W_OFF_FIRE_BTN, 2); // release
                    pulse_state   = false;
                    trigger_timer = 0.f;
                }
            }
        } else {
            if (pulse_state || trigger_timer > 0.f) {
                wpm<uint8_t>(wc + W_OFF_FIRE_BTN, 2);
                pulse_state   = false;
                trigger_timer = 0.f;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  MAIN HANDLE
    // ─────────────────────────────────────────────────────────────────────────
    void handle() {
        if (!cfg::aim::enabled) return;

        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager) return;

        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));
        if (!LocalPlayer) return;

        uint64_t AimController = rpm<uint64_t>(LocalPlayer + offsets::player::aim_controller);
        if (!AimController) return;

        uint64_t AimingData = rpm<uint64_t>(AimController + offsets::aim::aiming_data);
        if (!AimingData) return;

        int current_weapon_id = 0;
        {
            uint64_t wrc = rpm<uint64_t>(LocalPlayer + W_OFF_WEAPON_ROOT);
            uint64_t wc  = wrc ? rpm<uint64_t>(wrc + W_OFF_ACTIVE_WC) : 0;
            if (wc && valid_addr(wc)) {
                uint64_t props = rpm<uint64_t>(wc + W_OFF_WC_PROPS);
                if (props && valid_addr(props))
                    current_weapon_id = rpm<int>(props + W_OFF_WP_ID);
            }
        }

        float current_pitch = rpm<float>(AimingData + 0x18);
        float current_yaw   = rpm<float>(AimingData + 0x1C);
        if (std::isnan(current_pitch) || std::isnan(current_yaw)) return;

        while (current_yaw >  180.0f) current_yaw -= 360.0f;
        while (current_yaw < -180.0f) current_yaw += 360.0f;

        bool  weapon_changed = (current_weapon_id != last_weapon);
        float p_delta_last   = std::abs(current_pitch - last_pitch);
        float y_delta_last   = std::abs(current_yaw   - last_yaw);
        if (y_delta_last > 180.0f) y_delta_last = 360.0f - y_delta_last;

        if (weapon_changed || p_delta_last > 45.0f || y_delta_last > 45.0f) {
            last_weapon = current_weapon_id;
            last_pitch  = current_pitch;
            last_yaw    = current_yaw;
        }

        matrix  ViewMatrix = player::view_matrix(LocalPlayer);
        Vector3 CameraPos  = player::camera_position(LocalPlayer);
        int     LocalTeam  = rpm<uint8_t>(LocalPlayer + offsets::player::team);

        uint64_t PlayerList = rpm<uint64_t>(PlayerManager + oxorany(0x28));
        if (!PlayerList) return;

        int PlayerCount = rpm<int>(PlayerList + oxorany(0x20));
        if (PlayerCount <= 0 || PlayerCount > 128) return;

        uint64_t ListBuffer = rpm<uint64_t>(PlayerList + oxorany(0x18));
        if (!ListBuffer) return;

        // ── DRAW FOV CIRCLE ───────────────────────────────────────────────────
        draw_fov();

        // ── AIMBOT MAIN LOOP ──────────────────────────────────────────────────
        // Аим всегда работает на 360° (включая спину).
        // fov_check=true  → цель принимается только если угловое отклонение
        //                   укладывается в px-радиус cfg::aim::fov.
        // fov_check=false → fov не ограничивает выбор цели, берём ближайшую
        //                   по углу из всех живых противников на любой стороне.
        //
        // visible_check и back_camera учитываются в обоих режимах.
        // Метрика выбора — угловое отклонение в градусах (меньше = лучше).

        float    best_angle  = 360.0f;  // текущий минимум углового отклонения
        uint64_t best_target = 0;
        Vector3  best_bone_pos{};

        // Порог в градусах для fov_check=true:
        // fov (px) → degrees через горизонтальный HFOV ~90° на пол-экрана
        const float deg_per_px  = (g_sw > 1.f) ? (90.0f / (g_sw * 0.5f)) : (90.0f / 960.0f);
        const float fov_deg_cap = cfg::aim::fov * deg_per_px;

        // Направление камеры — из view-matrix row0..row2 (column-major):
        // row 2 в row-major 4x4 = элементы [8],[9],[10] = forward Z
        // Нужен для расчёта угла к цели за экраном.
        const float* vm_raw = reinterpret_cast<const float*>(&ViewMatrix);

        // Вектор "вперёд" камеры в мировых координатах.
        // ViewMatrix строит clip-space, forward = -Z колонки 2 или row 2 col 0-2.
        // Используем pitch/yaw из AimingData для надёжного расчёта — они точные.
        const float cp = current_pitch * (3.14159265f / 180.0f);
        const float cy_rad = current_yaw   * (3.14159265f / 180.0f);
        const Vector3 cam_fwd = {
             cosf(cp) * sinf(cy_rad),   // X
            -sinf(cp),                  // Y (pitch down = negative Y)
             cosf(cp) * cosf(cy_rad)    // Z
        };

        for (int i = 0; i < PlayerCount; i++) {
            uint64_t Player = rpm<uint64_t>(ListBuffer + oxorany(0x30) + oxorany(0x18) * i);
            if (!Player || Player == LocalPlayer) continue;

            uint8_t PlayerTeam = rpm<uint8_t>(Player + offsets::player::team);
            if (PlayerTeam == static_cast<uint8_t>(LocalTeam)) continue;

            if (player::health(Player) <= 0) continue;

            // visible_check учитывается всегда
            if (cfg::aim::visible_check && !player::is_visible(Player)) continue;

            // back_camera: если включена и идёт возврат — не аимимся
            if (cfg::aim::back_camera && bc_returning) continue;

            player::bones_t bones;
            if (!player::get_bones(Player, bones)) continue;

            Vector3 target_pos;
            switch (cfg::aim::bone) {
                case 0:  target_pos = bones.head;   break;
                case 1:  target_pos = bones.neck;   break;
                case 2:  target_pos = bones.spine2; break;
                case 3:  target_pos = bones.pelvis; break;
                default: target_pos = bones.head;   break;
            }

            // Угловое расстояние до цели (работает на 360°, включая спину)
            Vector3 to_target = {
                target_pos.x - CameraPos.x,
                target_pos.y - CameraPos.y,
                target_pos.z - CameraPos.z
            };
            float len = sqrtf(to_target.x*to_target.x +
                              to_target.y*to_target.y +
                              to_target.z*to_target.z);
            if (len < 0.1f) continue;

            float dot = (cam_fwd.x * to_target.x +
                         cam_fwd.y * to_target.y +
                         cam_fwd.z * to_target.z) / len;
            dot = std::clamp(dot, -1.0f, 1.0f);
            float angle_deg = acosf(dot) * (180.0f / 3.14159265f);

            // fov_check=true → цель отсекается если угол > fov_deg_cap
            if (cfg::aim::fov_check && angle_deg > fov_deg_cap) continue;

            // Выбираем ближайшую по углу
            if (angle_deg < best_angle) {
                best_angle    = angle_deg;
                best_target   = Player;
                best_bone_pos = target_pos;
            }
        }

        // ── BACK CAMERA TICK ──────────────────────────────────────────────────
        // Вызывается после выбора цели: знает есть ли она.
        back_camera_tick(AimingData, current_pitch, current_yaw, best_target != 0);

        // ── TRIGGERBOT ────────────────────────────────────────────────────────
        triggerbot_tick(LocalPlayer, PlayerManager, ViewMatrix, CameraPos, LocalTeam);

        if (!best_target) return;

        // ── SMOOTH & WRITE ────────────────────────────────────────────────────
        Vector3 direction = best_bone_pos - CameraPos;
        float   distance  = direction.magnitude();
        if (distance < 0.1f) return;

        float pitch = -asinf(direction.y / distance) * Rad2Deg;
        float yaw   = atan2f(direction.x, direction.z) * Rad2Deg;
        while (yaw >  180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;

        float smooth_factor = 1.0f;
        float cfg_smooth    = cfg::aim::smooth;
        if (cfg_smooth <= 0.1f) {
            smooth_factor = 1.0f;
        } else if (cfg_smooth <= 1.0f) {
            smooth_factor = 0.7f + 0.3f * cfg_smooth;
        } else if (cfg_smooth <= 5.0f) {
            smooth_factor = 0.2f + 0.5f * (5.0f - cfg_smooth) / 4.0f;
        } else {
            float norm = (cfg_smooth - 5.0f) / 15.0f;
            if (norm > 1.f) norm = 1.f;
            smooth_factor = 0.2f * (1.0f - norm) + 0.01f * norm;
        }

        float p_delta = pitch      - current_pitch;
        float y_delta = yaw        - current_yaw;
        if (y_delta >  180.0f) y_delta -= 360.0f;
        if (y_delta < -180.0f) y_delta += 360.0f;

        float delta_len = sqrtf(p_delta*p_delta + y_delta*y_delta);
        if (cfg_smooth > 0.1f) {
            if      (delta_len > 10.f) smooth_factor *= 0.7f;
            else if (delta_len <  2.f) smooth_factor *= 0.9f;
        }

        float smoothed_p = current_pitch + p_delta * smooth_factor;
        float smoothed_y = current_yaw   + y_delta * smooth_factor;

        smoothed_p = std::clamp(smoothed_p, -89.0f, 89.0f);
        while (smoothed_y >  180.0f) smoothed_y -= 360.0f;
        while (smoothed_y < -180.0f) smoothed_y += 360.0f;

        if (valid_addr(AimingData) &&
            rpm<uint64_t>(AimController + offsets::aim::aiming_data) == AimingData)
        {
            wpm<float>(AimingData + 0x18, smoothed_p);
            wpm<float>(AimingData + 0x1C, smoothed_y);
            wpm<float>(AimingData + 0x24, smoothed_p);
            wpm<float>(AimingData + 0x28, smoothed_y);
            last_pitch = smoothed_p;
            last_yaw   = smoothed_y;
        }
    }

} // namespace aimbot

