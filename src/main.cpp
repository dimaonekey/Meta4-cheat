#include "game/player.hpp"  // ← добавить эту строку
#include "Android_draw/draw.h"
#include "func/chams.hpp"
#include "func/matchams.hpp"
#include "func/autowin.hpp"
#include "func/weapon.hpp"
#include "ui/watermark.hpp"
#include "ui/theme/theme.hpp"
#include "ui/menu.hpp"
#include "ui/bar.hpp"
#include "other/memory.hpp"
#include "game/game.hpp"
#include "ui/splash.hpp"
#include "func/aimbot.hpp"
#include "func/fov_changer.hpp"
#include "func/test_loader.hpp"
#include "func/visuals.hpp"
#include "func/bighead.hpp"
#include "func/arms.hpp"
#include "func/mapcollider.hpp"
#include "func/maps_embedded.h"
#include "func/dmovement.hpp"
#include "protect/oxorany.hpp"
#include <cstdio>
#include <thread>
#include <chrono>

static void print_status(const char* status) {
    printf(oxorany("[@qqspk]"), status);
}

// Функция для запуска Standoff
static void launch_standoff() {
    system(oxorany("am start -n com.standoff/com.standoff.MainActivity"));
}

int main() {
    screen_config();

    int max_size = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    int min_size = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    g_sw = static_cast<float>(max_size);
    g_sh = static_cast<float>(min_size);

    native_window_screen_x = max_size;
    native_window_screen_y = max_size;

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) return -1;

    touch::init(max_size, min_size, (uint8_t)displayInfo.orientation);

    // Автоматический запуск Standoff
    print_status(oxorany("Игра найдена | Ach3ron.bl4ck"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    print_status(oxorany("Запуск чича.... | Ach3ron.bl4ck"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    launch_standoff();
    
    game::init();

    static float alpha = 0.f;
    static bool prev = false;
    static bool game_started = false;
    
    static constexpr float LD = 1.5f;   // длительность анимации (сек)
    static constexpr float HD = 0.3f;   // hold после завершения (сек)
    static float  sp_t    = 0.f;        // таймер сплэша
    static bool   sp_done = false;   

    while (true) {
        drawBegin();

        bool run = game::valid();

#if defined(__x86_64__)
        bool is_landscape = (displayInfo.orientation == 0 || displayInfo.orientation == 2);
#else
        bool is_landscape = (displayInfo.orientation == 1 || displayInfo.orientation == 3);
#endif

        if (run && !prev) {
            if (!game_started) {
                print_status(oxorany("Игра найдена | Ach3ron.bl4ck"));
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                print_status(oxorany("Запуск чича.... | Ach3ron.bl4ck"));
                game_started = true;
            } else {
                print_status(oxorany("Игра найдена | Ach3ron.bl4ck"));
            }
            prev = true;
        } else if (!run && prev) {
            print_status(oxorany("Игра не найдена | Ach3ron.bl4ck"));
            prev = false;
            game_started = false;
        }

        if (is_landscape) {
            ImGuiIO& io = ImGui::GetIO();
            float dt = io.DeltaTime;
            if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

            float target = run ? 1.f : 0.f;
            float spd = run ? 4.f : 6.f;

            if (alpha < target) {
                alpha += dt * spd;
                if (alpha > target) alpha = target;
            } else if (alpha > target) {
                alpha -= dt * spd;
                if (alpha < target) alpha = target;
            }

            ui::bar::set_game_alpha(alpha);

            if (alpha > 0.001f) {
                ui::menu::render();
                watermark::render();
            }

            if (run && proc::lib != 0) {
                game::check_lib(get_player_manager());
                
                // ── ПОЛУЧАЕМ МАТРИЦУ ВИДА ──────────────────────────────
                uint64_t PlayerManager = get_player_manager();
                uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));
                matrix ViewMatrix = player::view_matrix(LocalPlayer);
                
                // ── ГЛАВНАЯ ЛОГИКА ──────────────────────────────────────
                visuals::draw();
                
                if (cfg::world::collider_enabled && PlayerManager && LocalPlayer) {
                    matrix  vm  = player::view_matrix(LocalPlayer);
                    Vector3 cam = player::camera_position(LocalPlayer);
                    ImDrawList* dl = ImGui::GetBackgroundDrawList();
                    collider_esp::draw(dl, vm, cam);
                }
                
                // ── ОСТАЛЬНОЕ ───────────────────────────────────────────
                aimbot::handle();
                autowin::run();
                fov_changer::run();
                wallshot::run();
                strafe::run();
                bunny_hop::run();
                inf_ammo::run();
                norecoil::run();
                test_loader::run();
                inf_shop::run();
                fustknife::run();
                sigma::run();
                arms::run();
                weapon::run_all();
                bighead::run();
                crouch_speed::run();
                high_jump::run();
                chams::run();
                chams::handle();
                if (LocalPlayer) {
                    uint64_t weaponry = rpm<uint64_t>(LocalPlayer + oxorany(0x88ULL));
                    teleport_hack::run(weaponry);
                }
            }
            if(!sp_done) {
                sp_t+=dt;
                float sc2=g_sw/1920.f;
                float prog=ImClamp(sp_t/LD,0.f,1.f);
                if(prog>=1.f&&sp_t>=LD+HD) sp_done=true;
                ImDrawList* dl=ImGui::GetForegroundDrawList();
                splash(dl,g_sw,g_sh,sc2,prog,1.f);
            }
        }

        bool vis = ui::bar::g_open;
        drawEnd();
        usleep(vis ? 1500 : 4000);
    }

    shutdown();
    return 0;
}
