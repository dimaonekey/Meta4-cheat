#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "bar.hpp"
#include "cfg.hpp"
#include "cfg_holy.hpp"
#include "theme/theme.hpp"
#include "neverlose/gui.hpp"
#include "neverlose/hashes.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "Android_draw/draw.h"
#include "../func/mapcollider.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <thread>
#include <climits>

namespace ui::menu {
    using ui::style::popup_open;

    static float sc = 1.f;
    static float menu_alpha = 0.f;
    static bool first_pos = true;
    static bool prev_col = false;
    static std::atomic<bool> g_map_loading{false};

    static void load_collider_map_async(int map_index) {
        if (g_map_loading.load()) return;

        g_map_loading.store(true);
        std::thread([map_index]() {
            cfg::world::collider_map_id = map_index;
            map_service::g_collider_map_index = map_index;
            map_service::LoadColliderMapByIndex(map_index);
            g_map_loading.store(false);
        }).detach();
    }

    static bool custom_combo(const char* label, int* current_item, const char* const items[], int items_count) {
        if (!current_item || !items || items_count <= 0) return false;
        if (*current_item < 0 || *current_item >= items_count) *current_item = 0;

        bool value_changed = false;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f, 0.07f, 0.11f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.09f, 0.15f, 0.25f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.24f, 0.40f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.90f, 0.98f, 1.f));

        if (ImGui::BeginCombo(label, items[*current_item], ImGuiComboFlags_HeightLargest)) {
            for (int i = 0; i < items_count; i++) {
                const bool is_selected = (i == *current_item);
                if (ImGui::Selectable(items[i], is_selected)) {
                    *current_item = i;
                    value_changed = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::PopStyleColor(4);
        return value_changed;
    }

    static void color_picker(const char* label, ImVec4* color) {
        ImGui::PushID(label);

        ImGuiColorEditFlags f = ImGuiColorEditFlags_AlphaPreview;
        if (ImGui::ColorButton("##cb", *color, f, ImVec2(40.f * sc, 20.f * sc))) {
            ImGui::OpenPopup("##pk_pop");
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);

        if (ImGui::BeginPopup("##pk_pop")) {
            ImGui::ColorPicker4("##pk", (float*)color,
                f | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoOptions);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    static void ragebot_general() {
        const float group_w = ImGui::GetWindowWidth() * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;

        gui.group_box(ICON_FA_FEATHER " Automatic", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::Checkbox("Aim", &cfg::aim::enabled);
            if (cfg::aim::enabled) {
                ImGui::Checkbox("Visible Check", &cfg::aim::visible_check);
                ImGui::Checkbox("Back Camera", &cfg::aim::back_camera);
                ImGui::Checkbox("Auto fire", &cfg::aim::triggerbot);
                if (cfg::aim::triggerbot) {
                    ImGui::SliderFloat("AF Shot Delay", &cfg::aim::trigger_delay, 0.0f, 1.0f, "%.2fs");
                    ImGui::SliderFloat("AF Max Distance", &cfg::aim::trigger_range, 1.0f, 1000.f, "%.0fm");

                    const char* trigger_types[] = { "Head Only", "Head & Neck", "Upper Body", "All" };
                    custom_combo("AF Hitbox", &cfg::aim::trigger_bone_mask, trigger_types, 4);

                    ImGui::Checkbox("AF Visible Only", &cfg::aim::trigger_visible_only);
                }
                ImGui::Checkbox("Fov Check", &cfg::aim::fov_check);
                if (cfg::aim::fov_check) {
                    ImGui::Checkbox("Draw Fov", &cfg::aim::fov_draw);
                    ImGui::SliderFloat("Fov", &cfg::aim::fov, 1.f, 360.f, "%.0f°");
                    if (cfg::aim::fov_draw) {
                        color_picker("Fov", &cfg::aim::fov_color);
                    }
                }
                ImGui::SliderFloat("Smooth", &cfg::aim::smooth, 1.f, 20.f, "%.1f");

                const char* targets[] = { "Head", "Neck", "Chest", "Pelvis" };
                custom_combo("Target Bone", &cfg::aim::bone, targets, 4);
            }

        } gui.end_group_box();

        ImGui::SameLine();

        gui.group_box(ICON_FA_CROWN " Crown (D)", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::Checkbox("Fast Knife", &cfg_holy::fustknife::enabled);
            ImGui::Checkbox("Invisible", &cfg::test::invisible);
            ImGui::Checkbox("Fire Rate", &cfg::fire_rate::enabled);
            ImGui::Checkbox("Auto Win", &cfg::misc::autowin);
            ImGui::Checkbox("Damage hack", &cfg_holy::sigma::enabled);

            if (cfg_holy::sigma::enabled) {
                ImGui::SliderInt("Damage", &cfg_holy::sigma::damage, 1, 1000);
            }

            ImGui::Checkbox("No recoil", &cfg::norecoil::enabled);
            ImGui::Checkbox("Infinity ammo", &cfg::inf_ammo::enabled);
            ImGui::Checkbox("Inf Shop", &cfg_holy::inf_shop::enabled);

            if (cfg_holy::inf_shop::enabled) {
                ImGui::SliderInt("Shop value", &cfg_holy::inf_shop::value, 100, 30000);
            }

            ImGui::Checkbox("Wallshot", &cfg::wallshot::enabled);
            ImGui::Checkbox("Bunny Hop", &cfg::bunny_hop::enabled);
            ImGui::Checkbox("Crouch Speed", &cfg::crouch_speed::enabled);
            ImGui::Checkbox("High Jump", &cfg::high_jump::enabled);
            if (cfg::high_jump::enabled) {
                ImGui::SliderFloat("Jump Value", &cfg::high_jump::height, 1.0f, 5.0f, "%.1f");
            }
            ImGui::Checkbox("Teleport Enabled", &cfg::movement::teleport::enabled);
            if (cfg::movement::teleport::enabled) {
                ImGui::Checkbox("Active##tp", &cfg::movement::teleport::active);
            }

        } gui.end_group_box();
    }

    static void players_content() {
        const float group_w = ImGui::GetWindowWidth() * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;

        gui.group_box(ICON_FA_USER " Players", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::BeginChild("##players_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            ImGui::Checkbox("Box", &cfg::esp::box);
            if (cfg::esp::box) {
                const char* btypes[] = { "Full", "Corner" };
                custom_combo("Box type", &cfg::esp::box_type, btypes, 2);
                ImGui::SliderFloat("Box rounding", &cfg::esp::box_rounding, 0.f, 10.f, "%.0f");
            }
            ImGui::Checkbox("Box fill", &cfg::esp::fill);
            ImGui::Checkbox("3d box", &cfg::esp::box3d);
            ImGui::Checkbox("3d box fill", &cfg::esp::fill3d);
            if (cfg::esp::fill3d) {
                const char* fill_types[] = { "Base", "Glass" };
                custom_combo("3D Fill Type", &cfg::esp::fill3d_type, fill_types, 2);
            }
            ImGui::Checkbox("Name", &cfg::esp::name);
            ImGui::Checkbox("Health bar", &cfg::esp::health);
            ImGui::Checkbox("Armor bar", &cfg::esp::armor);
            ImGui::Checkbox("Line", &cfg::esp::line);
            ImGui::Checkbox("Distance", &cfg::esp::distance);
            ImGui::Checkbox("Weapon", &cfg::esp::weapon);
            ImGui::Checkbox("Skeleton", &cfg::esp::skeleton);
            ImGui::Checkbox("China hat", &cfg::esp::china);
            ImGui::Checkbox("Ping", &cfg::esp::ping);
            ImGui::Checkbox("Device", &cfg::esp::device_text);
            ImGui::Checkbox("Chams", &cfg::chams::enabled);
            if (cfg::chams::enabled) {
                const char* chams_types[] = { "Solid", "Shaded", "Outline", "Filled outline", "Wireframe", "Glass", "Blur", "Capsule", "Glow" };
                custom_combo("Chams Type", &cfg::chams::type, chams_types, 9);
            }
            ImGui::Checkbox("Material Chams", &cfg::matchams::enabled);
            if (cfg::matchams::enabled) {
                ImGui::Checkbox("Team check (skip allies)", &cfg::matchams::team_check);
            }
            ImGui::Checkbox("Light body", &cfg::chams2::enabled);

            ImGui::EndChild();

        } gui.end_group_box();

        ImGui::SameLine();

        gui.group_box(ICON_FA_PALETTE " Colors", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::BeginChild("##players_colors_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            color_picker("Box", &cfg::esp::box_col);
            color_picker("Box gradient", &cfg::esp::boxgradient_col);
            color_picker("Fill", &cfg::esp::fill_col);
            color_picker("Fill gradient", &cfg::esp::fillgradient_col);
            color_picker("3d box", &cfg::esp::box3d_col);
            color_picker("3d box gradient", &cfg::esp::box3dgradient_col);
            color_picker("3d fill", &cfg::esp::fill3d_col);
            color_picker("3d fill gradient", &cfg::esp::fill3dgradient_col);
            color_picker("Name", &cfg::esp::name_col);
            color_picker("Name gradient", &cfg::esp::namegradient_col);
            color_picker("Health", &cfg::esp::health_col);
            color_picker("Health gradient", &cfg::esp::healthgradient_col);
            color_picker("Armor", &cfg::esp::armor_col);
            color_picker("Armor gradient", &cfg::esp::armorgradient_col);
            color_picker("Line", &cfg::esp::line_col);
            color_picker("Distance", &cfg::esp::distance_col);
            color_picker("Distance gradient", &cfg::esp::distancegradient_col);
            color_picker("Weapon", &cfg::esp::weapon_col);
            color_picker("Weapon gradient", &cfg::esp::weapongradient_col);
            color_picker("Skeleton", &cfg::esp::skeleton_col);
            color_picker("Skeleton gradient", &cfg::esp::skeleton_gradient_col);
            color_picker("China hat", &cfg::esp::china1);
            color_picker("China hat gradient", &cfg::esp::china2);
            color_picker("Device", &cfg::esp::device_col);
            color_picker("Device gradient", &cfg::esp::device_textgradient_col);
            color_picker("Ping", &cfg::esp::ping_col);
            color_picker("Ping gradient", &cfg::esp::pinggradient_col);
            color_picker("Chams", &cfg::chams::col);
            color_picker("Chams gradient", &cfg::chams::col_gradient);

            ImGui::EndChild();

        } gui.end_group_box();
    }

    static void world_content() {
        const float group_w = ImGui::GetWindowWidth() * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;

        gui.group_box(ICON_FA_GLOBE " World", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::BeginChild("##world_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            ImGui::Checkbox("Arms Position", &cfg_holy::arms::enabled);
            if (cfg_holy::arms::enabled) {
                ImGui::SliderFloat("Arms X", &cfg_holy::arms::pos_x, -2.f, 2.f, "%.2f");
                ImGui::SliderFloat("Arms Y", &cfg_holy::arms::pos_y, -2.f, 2.f, "%.2f");
                ImGui::SliderFloat("Arms Z", &cfg_holy::arms::pos_z, -2.f, 2.f, "%.2f");
            }

            ImGui::Checkbox("Hit marker", &cfg::esp::marker);
            if (cfg::esp::marker) {
                ImGui::SliderFloat("Marker hiding time", &cfg::esp::marker_time, 0.5f, 5.0f, "%.1f");
            }

            ImGui::Checkbox("Offscreen arrows", &cfg::esp::esp_arrows);

            ImGui::Checkbox("Death effect", &cfg::esp::death);
            if (cfg::esp::death) {
                const char* death_types[] = { "Soul Essence", "Fade" };
                custom_combo("Effect Type", &cfg::esp::death_type, death_types, 2);
            }

            ImGui::Checkbox("Trail", &cfg::esp::trail);
            if (cfg::esp::trail) {
                ImGui::SliderFloat("Thickness", &cfg::esp::trail_thick, 1.5f, 5.0f, "%.1f");
            }

            ImGui::Checkbox("Snow", &cfg::esp::snow);
            ImGui::Checkbox("Rain", &cfg::esp::rain);
            ImGui::Checkbox("FOV", &cfg::fov_changer::enabled);
            if (cfg::fov_changer::enabled) {
                ImGui::SliderFloat("FOV Value", &cfg::fov_changer::value, 40.f, 90.f, "%.0f");
            }
            ImGui::Checkbox("Head Scale", &cfg::bighead::enabled);
            if (cfg::bighead::enabled) {
                ImGui::SliderFloat("Scale Value", &cfg::bighead::scale, 1.5f, 8.0f, "%.1f");
            }

            ImGui::Checkbox("Map Collider", &cfg::world::collider_enabled);

            if (cfg::world::collider_enabled && !prev_col) {
                int map_index = cfg::world::collider_map_id;
                if (map_index < 0 || map_index >= map_service::g_collider_map_count) {
                    map_index = 0;
                }
                load_collider_map_async(map_index);
            }
            prev_col = cfg::world::collider_enabled;

            if (cfg::world::collider_enabled) {
                ImGui::SliderFloat("Collider alpha", &cfg::world::collider_alpha, 0.f, 1.f, "%.2f");
                ImGui::SliderFloat("Max distance (m)", &cfg::world::collider_max_dist, 10.f, 200.f, "%.0f");
                color_picker("Collider color", &cfg::world::collider_color);

                ImGui::TextDisabled("Select map:");

                const int count = map_service::g_collider_map_count;
                int cur = cfg::world::collider_map_id;

                if (count > 0) {
                    if (cur < 0 || cur >= count) cur = 0;

                    if (ImGui::Combo("##map_pick", &cur, map_service::g_collider_map_display_names, count)) {
                        load_collider_map_async(cur);
                    }
                }

                if (g_map_loading.load()) {
                    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.f, 1.f), "Loading GLB in background...");
                } else if (!map_service::g_loaded) {
                    ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "No GLB loaded. Put files in /sdcard/NexuraWare/maps/");
                } else {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.f, 1.f), "GLB loaded OK.");
                }
            }

            ImGui::EndChild();

        } gui.end_group_box();

        ImGui::SameLine();

        gui.group_box(ICON_FA_PALETTE " Colors", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::BeginChild("##world_colors_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            color_picker("Marker", &cfg::esp::marker_col);
            color_picker("Arrows", &cfg::esp::esp_arrows_col);
            color_picker("Trail", &cfg::esp::trail_col);

            ImGui::EndChild();

        } gui.end_group_box();
    }

    static void main_content() {
        const float group_w = ImGui::GetWindowWidth() * 0.5f - ImGui::GetStyle().ItemSpacing.x * 0.5f;

        gui.group_box(ICON_FA_HAMMER " System", ImVec2(group_w, ImGui::GetWindowHeight())); {

            char buf[64];
            snprintf(buf, sizeof(buf), "screen: %.0f x %.0f", g_sw, g_sh);
            ImGui::TextUnformatted(buf);

            snprintf(buf, sizeof(buf), "fps: %.0f", ImGui::GetIO().Framerate);
            ImGui::TextUnformatted(buf);

            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.10f, 0.16f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.22f, 0.36f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.35f, 0.58f, 1.f));

            if (ImGui::Button("Close Menu", ImVec2(ImGui::GetContentRegionAvail().x, 30.f * sc))) {
                bar::g_open = false;
            }

            ImGui::PopStyleColor(3);

        } gui.end_group_box();

        ImGui::SameLine();

        gui.group_box(ICON_FA_INFO_CIRCLE " About", ImVec2(group_w, ImGui::GetWindowHeight())); {

            ImGui::TextUnformatted("Meta4 release");
            ImGui::TextDisabled("External 0.39.2");
            ImGui::TextDisabled("t.me/qqspk | sheeted o_0");

        } gui.end_group_box();
    }

    void render() {
        bar::render();

        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

        float tgt = bar::g_open ? 1.f : 0.f;
        if (menu_alpha < tgt) menu_alpha = ImMin(menu_alpha + dt * 12.f, tgt);
        else if (menu_alpha > tgt) menu_alpha = ImMax(menu_alpha - dt * 12.f, tgt);

        if (menu_alpha < 0.01f) {
            popup_open = false;
            return;
        }

        ui::style::tick();

        sc = g_sw / 1920.f;
        if (sc < 0.6f) sc = 0.6f;
        if (sc > 2.f) sc = 2.f;
        gui_sc = sc;

        ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
        int da = (int)(150 * menu_alpha * bar::game_alpha());
        bgdl->AddRectFilled(ImVec2(0, 0), ImVec2(g_sw, g_sh), IM_COL32(3, 5, 8, da));

        ImGui::PushFont(fontMenu);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menu_alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f * sc, 8.f * sc));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f * sc);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f * sc);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f * sc);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.f * sc);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.f * sc);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.05f, 0.08f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.11f, 0.18f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.16f, 0.26f, 1.f));

        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.18f, 0.49f, 0.96f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.30f, 0.62f, 1.00f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.22f, 0.54f, 1.00f, 1.f));

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.04f, 0.05f, 0.08f, 0.98f));

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.08f, 0.14f, 0.24f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.13f, 0.24f, 0.40f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.35f, 0.60f, 1.f));

        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.49f, 0.96f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.08f, 0.13f, 1.f));

        float mw = 690.f * sc;
        float mh = 500.f * sc;

        ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);
        if (first_pos) {
            ImGui::SetNextWindowPos(ImVec2((g_sw - mw) * 0.5f, (g_sh - mh) * 0.5f), ImGuiCond_Once);
            first_pos = false;
        }

        ImGuiWindowFlags wf =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar;

        if (ImGui::Begin("##meta4", nullptr, wf)) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImDrawList* draw = window->DrawList;
            ImVec2 pos = window->Pos;
            ImVec2 size = window->Size;

            gui.m_anim = ImMin(1.f, ImLerp(gui.m_anim, 1.f, 1.f - std::exp(-10.f * dt)));

            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::InvisibleButton("##drag_zone", ImVec2(200.f * sc, 60.f * sc), ImGuiButtonFlags_MouseButtonLeft);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                window->Pos += ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }

            ImVec2 title_size = fontTitle->CalcTextSizeA(fontTitle->FontSize, FLT_MAX, 0.f, "META4");
            draw->AddText(fontTitle, fontTitle->FontSize,
                pos + ImVec2(170.f * sc / 2.f - title_size.x / 2.f + 1.f, 20.f * sc + 1.f),
                IM_COL32(10, 30, 80, 200), "META4");
            draw->AddText(fontTitle, fontTitle->FontSize,
                pos + ImVec2(170.f * sc / 2.f - title_size.x / 2.f, 20.f * sc),
                IM_COL32(75, 155, 255, 255), "META4");

            draw->AddLine(pos + ImVec2(0, size.y - 50.f * sc), pos + ImVec2(170.f * sc, size.y - 50.f * sc),
                IM_COL32(45, 125, 245, 60));

            draw->AddText(pos + ImVec2(50.f * sc, size.y - 40.f * sc), gui.text.to_im_color(), "Meta4 release");
            draw->AddText(pos + ImVec2(50.f * sc, size.y - 25.f * sc), gui.text_disabled.to_im_color(), "Till:");
            draw->AddText(pos + ImVec2(50.f * sc + ImGui::CalcTextSize("Till: ").x, size.y - 25.f * sc),
                IM_COL32(45, 125, 245, 255), "Lifetime (Free)");

            ImGui::SetCursorPos(ImVec2(10.f * sc, 70.f * sc));
            ImGui::BeginChild("##tabs", ImVec2(150.f * sc, size.y - 120.f * sc));

            gui.group_title("Aimbot");
            if (gui.tab(ICON_FA_CROSSHAIRS, "Ragebot", gui.m_tab == 0) && gui.m_tab != 0) {
                gui.m_tab = 0;
                gui.m_anim = 0.f;
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            gui.group_title("Visuals");
            if (gui.tab(ICON_FA_USER, "Players", gui.m_tab == 1) && gui.m_tab != 1) {
                gui.m_tab = 1;
                gui.m_anim = 0.f;
            }
            if (gui.tab(ICON_FA_PALLET, "World", gui.m_tab == 2) && gui.m_tab != 2) {
                gui.m_tab = 2;
                gui.m_anim = 0.f;
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            gui.group_title("Miscellaneous");
            if (gui.tab(ICON_FA_HAMMER, "Main", gui.m_tab == 3) && gui.m_tab != 3) {
                gui.m_tab = 3;
                gui.m_anim = 0.f;
            }

            ImGui::EndChild();

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, gui.m_anim);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f * sc, 8.f * sc));

            ImGui::SetCursorPos(ImVec2(185.f * sc, 20.f * sc));
            ImGui::BeginChild("##childs", ImVec2(size.x - 200.f * sc, size.y - 50.f * sc));

            switch (gui.m_tab) {
            case 0:
                ragebot_general();
                break;
            case 1:
                players_content();
                break;
            case 2:
                world_content();
                break;
            case 3:
                main_content();
                break;
            default:
                break;
            }

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
        }
        ImGui::End();

        popup_open = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        ImGui::PopStyleColor(12);
        ImGui::PopStyleVar(9);
        ImGui::PopFont();
    }
}
