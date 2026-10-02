#include "visuals.hpp"
#include "chams2.hpp"
#include "snow.hpp"
#include "rain.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>

struct DamageMarker
{
    Vector3 pos;
    float spawn_time;
};

std::vector<DamageMarker> damage_markers;
static std::unordered_map<uint64_t, int> g_old_hp;

struct ArrowAnimState {
    ImVec2 target = ImVec2(0.0f, 0.0f);
    float alpha = 0.0f;
    float distance = 0.0f;
    int seen_frame = -1;
    bool offscreen = false;
};

static std::unordered_map<uint64_t, ArrowAnimState> g_arrow_anim_states;
static uint64_t g_arrow_player_manager = 0;

// ---- death particle system — chams dissolve ----

// One particle born on a chams limb surface
struct DeathParticle {
    Vector3 world_pos;   // spawn position on limb
    Vector3 velocity;    // world-space scatter direction
    float   life;
    float   maxLife;
    float   size;        // screen px at birth (scales with distance)
    // chams shape data for the "solid → dissolve" phase
    // index into the limb table so we know which segment it came from
    int     limb_idx;
    float   limb_t;      // 0..1 along that limb
    float   limb_hw;     // half-width at this point
};

// Cached bone screen positions at death moment
struct BoneSnap {
    Vector3 world;
    ImVec2  screen;
    bool    ok;
};

// 20 bone indices matching bones_t field order
// used to iterate them by pointer arithmetic below
static constexpr int BONE_COUNT = 20;

struct DeathEffect {
    // bones captured at death (world space)
    Vector3 bones[BONE_COUNT];
    bool    bone_ok[BONE_COUNT];

    // chams widths captured at death
    float ph;   // head→hip pixel height at spawn moment
    float base, tw, aw, lw;

    // captured chams colours
    ImVec4 col;
    ImVec4 col_gradient;
    int    type;

    float  timer;       // seconds since death
    bool   spawned;     // particles initialized

    std::vector<DeathParticle> particles;
};

// Duration breakdown
static constexpr float DEATH_SOLID_DUR   = 0.18f; // chams held solid
static constexpr float DEATH_DISSOLVE_DUR= 0.55f; // chams shrinks, particles born
static constexpr float DEATH_FLY_DUR     = 1.80f; // pure particle scatter

static std::unordered_map<uint64_t, DeathEffect> g_death_effects;

// device text position (follows distance/ping column)
static ImVec2 g_device_text_pos = ImVec2(0.f, 0.f);
static float  g_device_font_sz  = 14.f;

static std::mt19937 g_rng(std::random_device{}());

static float rand_float(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(g_rng);
}

static Vector3 random_dir()
{
    float x = rand_float(-1.f,1.f);
    float y = rand_float(-1.f,1.f);
    float z = rand_float(-1.f,1.f);

    float len = sqrtf(x*x+y*y+z*z);

    if(len < 0.01f)
        return {0,1,0};

    return {
        x/len,
        y/len,
        z/len
    };
}

struct TrailPoint
{
    Vector3 pos;
    float alpha;
};


static std::unordered_map<uint64_t, std::deque<TrailPoint>> g_trails;

extern ImFont* espFont;

// ================================================================
//  cfg::esp — НОВЫЕ ПОЛЯ (добавить в cfg.hpp / cfg.cpp)
// ================================================================
//
//  ImVec4 box_gradient_col   = { 0.f, 0.5f, 1.f, 1.f };   // второй цвет градиента бокса/3D/заливки
//  ImVec4 label_gradient_col = { 1.f, 0.5f, 0.f, 1.f };   // второй цвет градиента HP/armor/name/dist
//
// ================================================================

namespace visuals {

static const char* get_weapon_name(int id);
void dweapon(uint64_t player,
const ImVec2& box_max,
float cx,
float fontSize,
float a);

EnemyEntry g_enemies[MAX_ENEMIES];
    int        g_enemy_count = 0;
    float      g_local_yaw   = 0.f;

// ---------------- Forward ----------------
void draw_box3d(const Vector3& pos, const matrix& view, const ImVec4& col, float a);
void draw_box3d_fill(const Vector3& pos, const matrix& view, const ImVec4& col, float a);
void dbox_corner(const ImVec2& min, const ImVec2& max, float thickness, float a);
void darmor_bottom(int armor, const ImVec2& min, const ImVec2& max, float a);
void draw_text_outlined(ImDrawList* dl, ImFont* font, float size,
const ImVec2& pos, ImU32 color, const char* text);

// ================================================================
//  GRADIENT HELPERS
// ================================================================

// Линейная интерполяция между двумя ImVec4 с ультра-плавным easing
// t in [0..1], curve — степень easing (1 = линейный, 2 = квадратичный, ...)
static ImU32 lerp_col(const ImVec4& c0, const ImVec4& c1, float t, float alpha_mul = 1.f)
{
// smoothstep для невероятно плавного перехода
float s = t * t * (3.f - 2.f * t);   // smoothstep
// ещё один proход smoothstep → smootherstep Кена Перлина
s = s * s * (3.f - 2.f * s);

return IM_COL32(  
    (int)((c0.x + (c1.x - c0.x) * s) * 255.f),  
    (int)((c0.y + (c1.y - c0.y) * s) * 255.f),  
    (int)((c0.z + (c1.z - c0.z) * s) * 255.f),  
    (int)((c0.w + (c1.w - c0.w) * s) * 255.f * alpha_mul)  
);

}

// Вертикальный градиент через PrimReserve (quad)
// tl/tr = верхние вершины, bl/br = нижние
static void fill_quad_v_grad(ImDrawList* dl,
ImVec2 tl, ImVec2 tr, ImVec2 br, ImVec2 bl,
ImU32 col_top, ImU32 col_bot)
{
ImVec2 uv = dl->_Data->TexUvWhitePixel;
dl->PrimReserve(6, 4);
ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;
dl->PrimWriteVtx(tl, uv, col_top);
dl->PrimWriteVtx(tr, uv, col_top);
dl->PrimWriteVtx(br, uv, col_bot);
dl->PrimWriteVtx(bl, uv, col_bot);
dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+1); dl->PrimWriteIdx(idx+2);
dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+2); dl->PrimWriteIdx(idx+3);
}

// Горизонтальный градиент через PrimReserve (quad)
static void fill_quad_h_grad(ImDrawList* dl,
ImVec2 tl, ImVec2 tr, ImVec2 br, ImVec2 bl,
ImU32 col_left, ImU32 col_right)
{
ImVec2 uv = dl->_Data->TexUvWhitePixel;
dl->PrimReserve(6, 4);
ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;
dl->PrimWriteVtx(tl, uv, col_left);
dl->PrimWriteVtx(tr, uv, col_right);
dl->PrimWriteVtx(br, uv, col_right);
dl->PrimWriteVtx(bl, uv, col_left);
dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+1); dl->PrimWriteIdx(idx+2);
dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+2); dl->PrimWriteIdx(idx+3);
}

// Рисует многосегментную линию с вертикальным градиентом (для рёбер бокса)
// Разбивает линию на N сегментов и интерполирует цвет по Y
static void add_line_v_grad(ImDrawList* dl,
ImVec2 a, ImVec2 b,
const ImVec4& c0, const ImVec4& c1,
float alpha_mul, float thickness,
float y_min, float y_max,
int segments = 16)
{
float dy = y_max - y_min;
if (dy < 0.001f) dy = 0.001f;

ImVec2 prev = a;  
ImU32 prev_col = lerp_col(c0, c1,  
    std::clamp((a.y - y_min) / dy, 0.f, 1.f), alpha_mul);  

for (int i = 1; i <= segments; i++)  
{  
    float t_seg = (float)i / (float)segments;  
    ImVec2 cur(  
        a.x + (b.x - a.x) * t_seg,  
        a.y + (b.y - a.y) * t_seg  
    );  
    float t_col = std::clamp((cur.y - y_min) / dy, 0.f, 1.f);  
    ImU32 cur_col = lerp_col(c0, c1, t_col, alpha_mul);  

    // Тень  
    dl->AddLine(ImVec2(prev.x, prev.y),  
                ImVec2(cur.x,  cur.y),  
                IM_COL32(0,0,0,130), thickness + 1.5f);  
    // Цветная линия  
    dl->AddLine(prev, cur, cur_col, thickness);  

    prev     = cur;  
    prev_col = cur_col;  
}

}

// То же, но горизонтальный градиент (по X)
static void add_line_h_grad(ImDrawList* dl,
ImVec2 a, ImVec2 b,
const ImVec4& c0, const ImVec4& c1,
float alpha_mul, float thickness,
float x_min, float x_max,
int segments = 16)
{
float dx = x_max - x_min;
if (dx < 0.001f) dx = 0.001f;

ImVec2 prev = a;  
for (int i = 1; i <= segments; i++)  
{  
    float t_seg = (float)i / (float)segments;  
    ImVec2 cur(  
        a.x + (b.x - a.x) * t_seg,  
        a.y + (b.y - a.y) * t_seg  
    );  
    float t_col = std::clamp((cur.x - x_min) / dx, 0.f, 1.f);  
    ImU32 cur_col = lerp_col(c0, c1, t_col, alpha_mul);  

    dl->AddLine(ImVec2(prev.x, prev.y),  
                ImVec2(cur.x,  cur.y),  
                IM_COL32(0,0,0,130), thickness + 1.5f);  
    dl->AddLine(prev, cur, cur_col, thickness);  

    prev = cur;  
}

}

static ImU32 trail_lerp(const ImVec4& a, const ImVec4& b, float t, float alpha)
{
    float s = t * t * (3.f - 2.f * t);
    s = s * s * (3.f - 2.f * s);

    return IM_COL32(
        (int)((a.x + (b.x - a.x) * s) * 255.f),
        (int)((a.y + (b.y - a.y) * s) * 255.f),
        (int)((a.z + (b.z - a.z) * s) * 255.f),
        (int)((a.w + (b.w - a.w) * s) * 255.f * alpha)
    );
}

// ================================================================
//  HELPERS
// ================================================================

static void add_rect_outlined(ImDrawList* dl,
const ImVec2& min, const ImVec2& max,
ImU32 col, float rounding, float thickness)
{
dl->AddRect(
ImVec2(min.x - 1.f, min.y - 1.f),
ImVec2(max.x + 1.f, max.y + 1.f),
IM_COL32(0, 0, 0, 180),
rounding, 0, thickness + 1.f
);
dl->AddRect(
ImVec2(min.x + 1.f, min.y + 1.f),
ImVec2(max.x - 1.f, max.y - 1.f),
IM_COL32(0, 0, 0, 100),
std::max(0.f, rounding - 1.f), 0, 1.f
);
dl->AddRect(min, max, col, rounding, 0, thickness);
}

static void add_line_outlined(ImDrawList* dl,
const ImVec2& a, const ImVec2& b,
ImU32 col, float thickness = 1.5f)
{
dl->AddLine(a, b, IM_COL32(0, 0, 0, 160), thickness + 1.5f);
dl->AddLine(a, b, col, thickness);
}

static void skel_line(ImDrawList* dl, ImU32 col,
                      const ImVec2& a, const ImVec2& b)
{
    dl->AddLine(a, b, IM_COL32(0,0,0,200), 2.8f);
    dl->AddLine(a, b, col,                  1.6f);
}

void update_trail(uint64_t player, const Vector3& pos)
{
    if (!cfg::esp::trail)
        return;


    auto& dq = g_trails[player];


    dq.push_front(
    {
        pos,
        1.f
    });


    int max_points =
        (int)(cfg::esp::trail_dist * 12.f);


    if(max_points < 2)
        max_points = 2;


    if((int)dq.size() > max_points)
        dq.pop_back();
}

void draw_trail(ImDrawList* dl,
                uint64_t player,
                const matrix& view)
{
    if(!cfg::esp::trail)
        return;


    auto it = g_trails.find(player);

    if(it == g_trails.end())
        return;


    auto& dq = it->second;


    if(dq.size() < 2)
        return;



    for(size_t i = 1; i < dq.size(); i++)
    {

        ImVec2 a,b;


        if(!world_to_screen(
            dq[i-1].pos,
            view,
            a))
            continue;


        if(!world_to_screen(
            dq[i].pos,
            view,
            b))
            continue;



        float alpha =
            dq[i].alpha;



            ImU32 col = IM_COL32(
            (int)(cfg::esp::trail_col.x * 255),
            (int)(cfg::esp::trail_col.y * 255),
            (int)(cfg::esp::trail_col.z * 255),
            (int)(cfg::esp::trail_col.w * 255 * alpha)
        );



        dl->AddLine(
            a,
            b,
            col,
            cfg::esp::trail_thick
        );
    }



    // плавное исчезновение хвоста
    for(auto& p : dq)
    {
        p.alpha *= 0.94f;
    }
}


// ================================================================
//  ARMOR BAR  — горизонтальный градиент (box_col → box_gradient_col)
// ================================================================
void darmor_bottom(int armor, const ImVec2& min, const ImVec2& max, float a)
{
if (a < 0.01f || armor <= 0) return;
ImDrawList* dl = ImGui::GetBackgroundDrawList();

armor = std::clamp(armor, 0, 100);  
float pct = armor / 100.f;  

float w = max.x - min.x;  
float bar_h = 3.f;  
float y = max.y + 4.f;  

// фон  
dl->AddRectFilled(  
    ImVec2(min.x - 2.f, y - 2.f),  
    ImVec2(max.x + 2.f, y + bar_h + 2.f),  
    IM_COL32(0,0,0,120), 1.f);  
dl->AddRectFilled(  
    ImVec2(min.x - 1.f, y - 1.f),  
    ImVec2(max.x + 1.f, y + bar_h + 1.f),  
    IM_COL32(0,0,0,200), 0.f);  

// заполненная часть с горизонтальным градиентом  
float fill_x1 = min.x;  
float fill_x2 = min.x + w * pct;  

ImVec2 tl(fill_x1, y);  
ImVec2 tr(fill_x2, y);  
ImVec2 br(fill_x2, y + bar_h);  
ImVec2 bl(fill_x1, y + bar_h);  

ImU32 col_l = lerp_col(cfg::esp::armor_col, cfg::esp::armorgradient_col, 0.f, a);  
ImU32 col_r = lerp_col(cfg::esp::armor_col, cfg::esp::armorgradient_col, 1.f, a);  

fill_quad_h_grad(dl, tl, tr, br, bl, col_l, col_r);

}

void add_damage_marker(Vector3 pos)
{
    DamageMarker m;

    m.pos = pos;
    m.spawn_time = ImGui::GetTime();

    damage_markers.push_back(m);
}

void on_player_damage(Vector3 enemy_pos)
{
    add_damage_marker(enemy_pos);
}

void draw_damage_marker(const matrix& view)
{
    float now = ImGui::GetTime();

    for(auto it = damage_markers.begin(); it != damage_markers.end(); )
    {
        float age = now - it->spawn_time;

        if(age >= cfg::esp::marker_time)
        {
            it = damage_markers.erase(it);
            continue;
        }

        ImVec2 screen;

        if(world_to_screen(it->pos, view, screen))
        {
            float alpha = 1.f - (age / cfg::esp::marker_time);

            ImColor col = cfg::esp::marker_col;
            col.Value.w *= alpha;

            float size = 8.f;

            ImGui::GetBackgroundDrawList()->AddLine(
                {screen.x - size, screen.y - size},
                {screen.x + size, screen.y + size},
                col,
                2.f
            );

            ImGui::GetBackgroundDrawList()->AddLine(
                {screen.x + size, screen.y - size},
                {screen.x - size, screen.y + size},
                col,
                2.f
            );
        }

        ++it;
    }
}

// ----------------------------------------------------------------
//  Helper: get bones_t as flat Vector3 array (matches BONE_COUNT=20)
//  Order must match the DeathEffect::bones[] index used in draw.
// ----------------------------------------------------------------
static void bones_to_flat(const player::bones_t& b, Vector3 out[BONE_COUNT]) {
    out[0]  = b.head;
    out[1]  = b.neck;
    out[2]  = b.spine2;
    out[3]  = b.spine1;
    out[4]  = b.spine;
    out[5]  = b.pelvis;
    out[6]  = b.l_shoulder;
    out[7]  = b.l_arm;
    out[8]  = b.l_forearm;
    out[9]  = b.l_hand;
    out[10] = b.r_shoulder;
    out[11] = b.r_arm;
    out[12] = b.r_forearm;
    out[13] = b.r_hand;
    out[14] = b.l_thigh;
    out[15] = b.l_knee;
    out[16] = b.l_foot;
    out[17] = b.r_thigh;
    out[18] = b.r_knee;
    out[19] = b.r_foot;
}

// Limb table: pairs of bone indices + width multiplier
// index → { bone_a, bone_b, hw_mul (relative to base) }
struct LimbDef { int a, b; float hw; };
static constexpr LimbDef LIMBS[] = {
    // torso
    {1,  2,  3.00f}, {2,  3,  2.55f}, {3,  4,  2.25f}, {4,  5,  2.40f},
    // shoulders
    {1,  6,  1.56f}, {1, 10,  1.56f},
    // left arm
    {6,  7,  1.30f}, {7,  8,  1.11f}, {8,  9,  0.91f},
    // right arm
    {10,11,  1.30f}, {11,12,  1.11f}, {12,13,  0.91f},
    // left leg
    {5, 14,  1.44f}, {14,15,  1.80f}, {15,16,  1.26f},
    // right leg
    {5, 17,  1.44f}, {17,18,  1.80f}, {18,19,  1.26f},
};
static constexpr int LIMB_COUNT = (int)(sizeof(LIMBS) / sizeof(LIMBS[0]));

// ----------------------------------------------------------------
//  spawn_death_effect — called once on death
// ----------------------------------------------------------------
static void spawn_death_effect(uint64_t player_ptr, const matrix& vm)
{
    if (!cfg::esp::death) return;

    player::bones_t raw;
    if (!player::get_bones(player_ptr, raw)) return;

    DeathEffect e{};
    bones_to_flat(raw, e.bones);

    // mark which bones projected OK (just check if non-zero)
    for (int i = 0; i < BONE_COUNT; i++)
        e.bone_ok[i] = (e.bones[i].x != 0.f || e.bones[i].y != 0.f || e.bones[i].z != 0.f);

    // compute chams scale — head(0) → pelvis(5)
    ImVec2 sh, sf;
    e.ph = 0.f;
    if (world_to_screen(e.bones[0], vm, sh) && world_to_screen(e.bones[5], vm, sf)) {
        float dx = sh.x - sf.x, dy = sh.y - sf.y;
        e.ph = sqrtf(dx*dx + dy*dy);
    }
    if (e.ph < 1.f) e.ph = 80.f; // fallback

    e.base = e.ph * 0.12f;
    e.tw   = e.base * 3.0f;
    e.aw   = e.base * 1.3f;
    e.lw   = e.base * 1.8f;

    // snapshot chams colour
    e.col      = cfg::chams::col;
    e.col_gradient = cfg::chams::col_gradient;
    e.type     = cfg::chams::type;
    e.timer    = 0.f;
    e.spawned  = false;

    g_death_effects[player_ptr] = std::move(e);
}

// ----------------------------------------------------------------
//  Seed particles on all limbs — called once at dissolve start
// ----------------------------------------------------------------
static void seed_particles(DeathEffect& e, const matrix& vm)
{
    e.particles.clear();
    e.particles.reserve(320);

    int per_limb = (int)(cfg::esp::death_count / LIMB_COUNT);
    if (per_limb < 4) per_limb = 4;

    for (int li = 0; li < LIMB_COUNT; li++) {
        const LimbDef& ld = LIMBS[li];
        if (!e.bone_ok[ld.a] || !e.bone_ok[ld.b]) continue;

        const Vector3& wa = e.bones[ld.a];
        const Vector3& wb = e.bones[ld.b];

        float hw = e.base * ld.hw;

        for (int n = 0; n < per_limb; n++) {
            float t  = rand_float(0.f, 1.f);
            // point along limb axis
            Vector3 along(
                wa.x + (wb.x - wa.x) * t,
                wa.y + (wb.y - wa.y) * t,
                wa.z + (wb.z - wa.z) * t);

            // limb tangent
            float dx = wb.x - wa.x, dy = wb.y - wa.y, dz = wb.z - wa.z;
            float ln = sqrtf(dx*dx + dy*dy + dz*dz);
            if (ln < 0.01f) continue;
            // random scatter direction biased outward from center of mass
            Vector3 scatter = random_dir();
            float speed = rand_float(2.f, 8.f) * (e.ph / 80.f);

            DeathParticle p{};
            p.world_pos = along;
            p.velocity  = { scatter.x * speed, scatter.y * speed, scatter.z * speed };
            p.life      = rand_float(DEATH_FLY_DUR * 0.5f, DEATH_FLY_DUR);
            p.maxLife   = p.life;
            p.size      = hw * rand_float(0.3f, 0.9f);
            p.limb_idx  = li;
            p.limb_t    = t;
            p.limb_hw   = hw;
            e.particles.push_back(p);
        }
    }

    e.spawned = true;
}

// ----------------------------------------------------------------
//  draw_death_effect — call every frame
//  death_type 0 = chams dissolve → particle scatter  (existing)
//  death_type 1 = chams pure alpha fade-out, no particles
// ----------------------------------------------------------------
static void draw_death_effect(const matrix& view)
{
    if (!cfg::esp::death) return;
    if (g_death_effects.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float dt       = ImGui::GetIO().DeltaTime;

    // per-type total duration
    const float FADE_DUR = DEATH_SOLID_DUR + DEATH_DISSOLVE_DUR * 2.5f;
    float total_dur = (cfg::esp::death_type == 1)
                      ? FADE_DUR
                      : DEATH_SOLID_DUR + DEATH_DISSOLVE_DUR + DEATH_FLY_DUR;

    for (auto it = g_death_effects.begin(); it != g_death_effects.end(); ) {
        DeathEffect& e = it->second;
        e.timer += dt;

        if (e.timer >= total_dur) {
            it = g_death_effects.erase(it);
            continue;
        }

        // Project all bones this frame
        BoneSnap snap[BONE_COUNT];
        for (int i = 0; i < BONE_COUNT; i++) {
            snap[i].world = e.bones[i];
            snap[i].ok    = e.bone_ok[i] && world_to_screen(e.bones[i], view, snap[i].screen);
        }

        // gradient Y range — same as live chams
        float y_min = snap[0].ok ? snap[0].screen.y : 0.f;
        float y_max = snap[5].ok ? snap[5].screen.y : y_min + e.ph;
        if (snap[16].ok) y_max = std::max(y_max, snap[16].screen.y);
        if (snap[19].ok) y_max = std::max(y_max, snap[19].screen.y);

        // helper: gradient col using captured chams colours
        auto gc = [&](float y, float a) -> ImU32 {
            float t = (y_max > y_min) ? (y - y_min) / (y_max - y_min) : 0.f;
            t = std::clamp(t, 0.f, 1.f);
            t = t * t * (3.f - 2.f * t);
            const ImVec4& tc = e.col;
            const ImVec4& bc = e.col_gradient;
            return IM_COL32(
                (int)((tc.x + (bc.x-tc.x)*t)*255),
                (int)((tc.y + (bc.y-tc.y)*t)*255),
                (int)((tc.z + (bc.z-tc.z)*t)*255),
                (int)((tc.w + (bc.w-tc.w)*t)*255*a));
        };

        // simple fading limb draw — gradient quad, no chams type logic
        auto draw_fade_limb = [&](const ImVec2& sa, const ImVec2& sb, float hw, float a) {
            float dx=sb.x-sa.x, dy=sb.y-sa.y, len=sqrtf(dx*dx+dy*dy);
            if (hw < 0.3f || len < 0.5f) return;
            float nx=-dy/len, ny=dx/len;
            ImVec2 p1={sa.x+nx*hw,sa.y+ny*hw}, p2={sa.x-nx*hw,sa.y-ny*hw};
            ImVec2 p3={sb.x-nx*hw,sb.y-ny*hw}, p4={sb.x+nx*hw,sb.y+ny*hw};
            ImDrawList* d=ImGui::GetBackgroundDrawList();
            d->PrimReserve(6,4);
            ImVec2 uv=d->_Data->TexUvWhitePixel;
            ImDrawIdx idx=(ImDrawIdx)d->_VtxCurrentIdx;
            d->PrimWriteVtx(p1,uv,gc(p1.y,a)); d->PrimWriteVtx(p2,uv,gc(p2.y,a));
            d->PrimWriteVtx(p3,uv,gc(p3.y,a)); d->PrimWriteVtx(p4,uv,gc(p4.y,a));
            d->PrimWriteIdx(idx+0);d->PrimWriteIdx(idx+1);d->PrimWriteIdx(idx+2);
            d->PrimWriteIdx(idx+0);d->PrimWriteIdx(idx+2);d->PrimWriteIdx(idx+3);
        };

        // ================================================================
        //  DEATH TYPE 1 — pure chams alpha fade  (no particles)
        // ================================================================
        if (cfg::esp::death_type == 1) {
            float t_hold  = e.timer;
            float t_fade  = e.timer - DEATH_SOLID_DUR;

            float a = 1.f;
            if (t_fade > 0.f) {
                float d = t_fade / (DEATH_DISSOLVE_DUR * 2.5f);
                // smoothstep fade: fast at start, slow tail
                d = std::clamp(d, 0.f, 1.f);
                a = 1.f - d*d*(3.f - 2.f*d);
            }
            if (a < 0.005f) { ++it; continue; }

            for (int li = 0; li < LIMB_COUNT; li++) {
                const LimbDef& ld = LIMBS[li];
                if (!snap[ld.a].ok || !snap[ld.b].ok) continue;
                float hw = e.base * ld.hw;
                draw_fade_limb(snap[ld.a].screen, snap[ld.b].screen, hw, a);
            }

            // head
            if (snap[0].ok) {
                float hrx = e.base * 1.8f, hry = e.base * 2.5f;
                // fan-fill ellipse with gradient
                const int N=20;
                ImVec2 sc=snap[0].screen;
                for (int i=0;i<N;i++){
                    float a0=IM_PI*2.f*i/N, a1=IM_PI*2.f*(i+1)/N;
                    ImVec2 pa={sc.x+cosf(a0)*hrx,sc.y+sinf(a0)*hry};
                    ImVec2 pb={sc.x+cosf(a1)*hrx,sc.y+sinf(a1)*hry};
                    float my=(sc.y+pa.y+pb.y)/3.f;
                    dl->AddTriangleFilled(sc,pa,pb,gc(my,a));
                }
            }

            ++it;
            continue;
        }

        // ================================================================
        //  DEATH TYPE 0 — chams dissolve → particle scatter
        // ================================================================
        float t_solid    = e.timer;
        float t_dissolve = e.timer - DEATH_SOLID_DUR;
        float t_fly      = e.timer - DEATH_SOLID_DUR - DEATH_DISSOLVE_DUR;

        // ── PHASE 1: solid chams (held) ──
        if (t_solid < DEATH_SOLID_DUR) {
            for (int li = 0; li < LIMB_COUNT; li++) {
                const LimbDef& ld = LIMBS[li];
                if (!snap[ld.a].ok || !snap[ld.b].ok) continue;
                draw_fade_limb(snap[ld.a].screen, snap[ld.b].screen, e.base * ld.hw, 1.f);
            }
            if (snap[0].ok) {
                float hrx=e.base*1.8f, hry=e.base*2.5f;
                const int N=20; ImVec2 sc=snap[0].screen;
                for (int i=0;i<N;i++){
                    float a0=IM_PI*2.f*i/N,a1=IM_PI*2.f*(i+1)/N;
                    ImVec2 pa={sc.x+cosf(a0)*hrx,sc.y+sinf(a0)*hry};
                    ImVec2 pb={sc.x+cosf(a1)*hrx,sc.y+sinf(a1)*hry};
                    dl->AddTriangleFilled(sc,pa,pb,gc((sc.y+pa.y+pb.y)/3.f,1.f));
                }
            }
        }

        // ── PHASE 2: dissolve — shrinks + particles born ──
        else if (t_dissolve >= 0.f && t_dissolve < DEATH_DISSOLVE_DUR) {
            if (!e.spawned) seed_particles(e, view);

            float d      = t_dissolve / DEATH_DISSOLVE_DUR;
            float shrink = 1.f - d;
            float a      = shrink;

            for (int li = 0; li < LIMB_COUNT; li++) {
                const LimbDef& ld = LIMBS[li];
                if (!snap[ld.a].ok || !snap[ld.b].ok) continue;
                draw_fade_limb(snap[ld.a].screen, snap[ld.b].screen,
                               e.base * ld.hw * shrink, a);
            }
            if (snap[0].ok) {
                float hrx=e.base*1.8f*shrink, hry=e.base*2.5f*shrink;
                if (hrx > 0.3f) {
                    const int N=20; ImVec2 sc=snap[0].screen;
                    for (int i=0;i<N;i++){
                        float a0=IM_PI*2.f*i/N,a1=IM_PI*2.f*(i+1)/N;
                        ImVec2 pa={sc.x+cosf(a0)*hrx,sc.y+sinf(a0)*hry};
                        ImVec2 pb={sc.x+cosf(a1)*hrx,sc.y+sinf(a1)*hry};
                        dl->AddTriangleFilled(sc,pa,pb,gc((sc.y+pa.y+pb.y)/3.f,a));
                    }
                }
            }

            // particles emerge
            for (auto& p : e.particles) {
                p.world_pos.x += p.velocity.x * dt * d * 0.3f;
                p.world_pos.y += p.velocity.y * dt * d * 0.3f;
                p.world_pos.z += p.velocity.z * dt * d * 0.3f;
                ImVec2 sc;
                if (!world_to_screen(p.world_pos, view, sc)) continue;
                float pw = p.limb_hw * shrink * rand_float(0.4f,1.f);
                dl->AddCircleFilled(sc, std::max(1.f,pw*.5f), gc(sc.y, d), 8);
            }
        }

        // ── PHASE 3: fly — pure particles ──
        else if (t_fly >= 0.f) {
            if (!e.spawned) seed_particles(e, view);
            float fly_t = t_fly / DEATH_FLY_DUR;

            for (auto& p : e.particles) {
                if (p.life <= 0.f) continue;
                p.life -= dt;
                if (p.life <= 0.f) continue;
                p.world_pos.x += p.velocity.x * dt;
                p.world_pos.y += p.velocity.y * dt;
                p.world_pos.z += p.velocity.z * dt;
                p.velocity.x *= 0.94f; p.velocity.y *= 0.94f; p.velocity.z *= 0.94f;

                ImVec2 sc;
                if (!world_to_screen(p.world_pos, view, sc)) continue;
                float pa = p.life / p.maxLife;
                float sz = std::max(1.f, p.limb_hw * pa * 0.45f);
                dl->AddCircleFilled(sc, sz, gc(sc.y, pa), 6);
            }
        }

        ++it;
    }
}


static uint64_t g_last_weapon = 0;
static int g_last_shot_count = 0;

void dskeleton(ImDrawList* dl, const ImRect& r,
                        uint64_t player, const matrix& vm)
{
    if (!cfg::esp::skeleton) return;

    player::bones_t bones;
    if (!player::get_bones(player, bones)) return;

    float thick = cfg::esp::skeleton_thickness;

    ImVec4 skeleton_start = cfg::esp::skeleton_col;
    ImVec4 skeleton_end   = cfg::esp::skeleton_gradient_col;

    // Вертикальный градиент как у Box ESP: сверху вниз
    auto draw_line = [&](const Vector3& p1, const Vector3& p2) {
        ImVec2 s1, s2;
        if (world_to_screen(p1, vm, s1) && world_to_screen(p2, vm, s2)) {

            float y = (s1.y + s2.y) * 0.5f;
            float gradient = (y - r.Min.y) / (r.Max.y - r.Min.y);
            gradient = ImClamp(gradient, 0.0f, 1.0f);

            ImU32 col = lerp_col(
                skeleton_start,
                skeleton_end,
                gradient
            );

            dl->AddLine(s1, s2, IM_COL32(0,0,0,200), thick + 1.2f);
            dl->AddLine(s1, s2, col,                  thick);
        }
    };

    // Позвоночник
    draw_line(bones.head,   bones.neck);
    draw_line(bones.neck,   bones.spine2);
    draw_line(bones.spine2, bones.spine1);
    draw_line(bones.spine1, bones.spine);
    draw_line(bones.spine,  bones.pelvis);

    // Левая рука
    draw_line(bones.neck,       bones.l_shoulder);
    draw_line(bones.l_shoulder, bones.l_arm);
    draw_line(bones.l_arm,      bones.l_forearm);
    draw_line(bones.l_forearm,  bones.l_hand);

    // Правая рука
    draw_line(bones.neck,       bones.r_shoulder);
    draw_line(bones.r_shoulder, bones.r_arm);
    draw_line(bones.r_arm,      bones.r_forearm);
    draw_line(bones.r_forearm,  bones.r_hand);

    // Левая нога
    draw_line(bones.pelvis,  bones.l_thigh);
    draw_line(bones.l_thigh, bones.l_knee);
    draw_line(bones.l_knee,  bones.l_foot);

    // Правая нога
    draw_line(bones.pelvis,  bones.r_thigh);
    draw_line(bones.r_thigh, bones.r_knee);
    draw_line(bones.r_knee,  bones.r_foot);
}


// ================================================================
//  TEXT OUTLINED
// ================================================================
void draw_text_outlined(ImDrawList* dl, ImFont* font, float size,
const ImVec2& pos, ImU32 color, const char* text)
{
if (!dl || !font || !text) return;
int aa = (color >> IM_COL32_A_SHIFT) & 0xFF;
dl->AddText(font, size, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0,0,0,(int)(aa * 0.6f)), text);
dl->AddText(font, size, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0,0,0,(int)(aa * 0.3f)), text);
dl->AddText(font, size, pos, color, text);
}

// ================================================================
//  MAIN ESP DRAW
// ================================================================

// -------- CHINA HAT (standalone visual) --------
static void draw_china_hat(const Vector3& worldPos, const matrix& view)
{
    if (!cfg::esp::china)
        return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const int segments = 48;
    const float radius = 0.38f;
    const float height = 0.55f;

    Vector3 headPos(worldPos.x, worldPos.y + 1.67f, worldPos.z);
    Vector3 apex3(headPos.x, headPos.y + height, headPos.z);

    ImVec2 apex;
    if (!world_to_screen(apex3, view, apex))
        return;

    float rot = (float)ImGui::GetTime() * 0.8f;

    std::vector<ImVec2> ring;
    ring.reserve(segments);

    for (int i = 0; i < segments; i++)
    {
        float a = (IM_PI * 2.f * i / segments) + rot;

        Vector3 p(
            headPos.x + cosf(a) * radius,
            headPos.y,
            headPos.z + sinf(a) * radius
        );

        ImVec2 screen;
        if (world_to_screen(p, view, screen))
            ring.push_back(screen);
    }

    if (ring.size() < 3)
        return;

    for (int i = 0; i < (int)ring.size() - 1; i++)
    {
        float t = ((float)i / (float)segments);
        float side = (sinf(t * IM_PI * 2.f + rot) + 1.f) * 0.5f;

        ImVec4 grad = ImLerp(
            cfg::esp::china1,
            cfg::esp::china2,
            side
        );

        dl->AddTriangleFilled(
            apex,
            ring[i],
            ring[i + 1],
            ImGui::ColorConvertFloat4ToU32(grad)
        );
    }
}

void draw() {

if (!cfg::esp::esp_arrows) {
        g_arrow_anim_states.clear();
    }
    
bool any =
cfg::esp::box ||
cfg::esp::name ||
cfg::esp::health ||
cfg::esp::distance ||
cfg::esp::ping ||
cfg::esp::fill ||
cfg::esp::box3d ||
cfg::esp::fill3d ||
cfg::esp::line ||
cfg::esp::skeleton ||
cfg::esp::head_circle ||
cfg::esp::weapon ||
cfg::esp::esp_arrows ||
cfg::esp::trail ||
cfg::esp::death ||
cfg::esp::marker ||
cfg::esp::device_text ||
cfg::esp::china ||
cfg::chams::enabled;

if (!any) return;  


uint64_t PlayerManager = get_player_manager();  
if (!PlayerManager) return;  

uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  
if (!LocalPlayer) return;  

matrix ViewMatrix = player::view_matrix(LocalPlayer);  

Vector3 LocalPos  = player::position(LocalPlayer);  
int     LocalTeam = rpm<uint8_t>(LocalPlayer + oxorany(0x79));  

uint64_t PlayerList = rpm<uint64_t>(PlayerManager + oxorany(0x28));  
if (!PlayerList) return;  

int PlayerCount = rpm<int>(PlayerList + oxorany(0x20));  
if (PlayerCount <= 0 || PlayerCount > 64) return;  

uint64_t ListBuffer = rpm<uint64_t>(PlayerList + oxorany(0x18));  
if (!ListBuffer) return;  

ImDrawList* dl = ImGui::GetBackgroundDrawList();  

for (int i = 0; i < PlayerCount; i++) {  

    uint64_t Player = rpm<uint64_t>(ListBuffer + oxorany(0x30) + oxorany(0x18) * i);  
    if (!Player || Player == LocalPlayer) continue;  

    if (rpm<uint8_t>(Player + oxorany(0x79)) == LocalTeam)  
        continue;  

    Vector3 pos = player::position(Player);
    draw_china_hat(pos, ViewMatrix);
    if (!pos.x && !pos.y && !pos.z) continue;  

    int hp    = player::health(Player);
int armor = player::armor(Player);

bool alive = hp > 0;

auto oldHp = g_old_hp.find(Player);

if(oldHp != g_old_hp.end())
{
    if(hp < oldHp->second && hp > 0)
    {
        // только если игрок виден
        if(player::visibility_state(Player) == 2)
        {
            Vector3 hitPos = player::position(Player);

            add_damage_marker(hitPos);
        }
    }
}

g_old_hp[Player] = hp;

// proверка смерти ДО continue
{
    static std::unordered_map<uint64_t, bool> s_was_alive;
    bool& was = s_was_alive[Player];
    if (was && !alive) {
        if (player::visibility_state(Player) == 2) {
            spawn_death_effect(Player, ViewMatrix);
        }
    }
    was = alive;
}

// теперь можно пропускать мёртвых
if (!alive)
    continue;  

    float dist = calculate_distance(pos, LocalPos);  
    if (dist > 500.f) continue;  
    
    // -------- ARROWS STATE UPDATE --------
if (cfg::esp::esp_arrows)
{
    Vector3 chest(pos.x, pos.y + 0.9f, pos.z);

    float x =
        ViewMatrix.m11 * chest.x +
        ViewMatrix.m21 * chest.y +
        ViewMatrix.m31 * chest.z +
        ViewMatrix.m41;

    float y =
        ViewMatrix.m12 * chest.x +
        ViewMatrix.m22 * chest.y +
        ViewMatrix.m32 * chest.z +
        ViewMatrix.m42;

    float w =
        ViewMatrix.m14 * chest.x +
        ViewMatrix.m24 * chest.y +
        ViewMatrix.m34 * chest.z +
        ViewMatrix.m44;


    bool offscreen = false;

    float sx = 0;
    float sy = 0;


    if (w > 0.001f)
    {
        float inv = 1.0f / w;

        sx = (x * inv + 1.0f) * 0.5f * g_sw;
        sy = (1.0f - y * inv) * 0.5f * g_sh;

        offscreen =
            sx < 0 ||
            sx > g_sw ||
            sy < 0 ||
            sy > g_sh;
    }
    else
    {
        // игрок за камерой
        offscreen = true;

        // переворачиваем направление правильно
        sx = g_sw * 0.5f - x;
        sy = g_sh * 0.5f + y;
    }


    if (offscreen)
    {
        ArrowAnimState& state = g_arrow_anim_states[Player];

        // направление относительно центра экрана
        ImVec2 center(
            g_sw * 0.5f,
            g_sh * 0.5f
        );


        float dx = sx - center.x;
        float dy = sy - center.y;

        float len = sqrtf(dx * dx + dy * dy);

        if (len > 0.001f)
        {
            dx /= len;
            dy /= len;
        }


        // точка на окружности экрана
        float radius = cfg::esp::esp_arrows_distance;

        state.target = ImVec2(
            center.x + dx * radius,
            center.y + dy * radius
        );


        state.distance = dist;
        state.seen_frame = ImGui::GetFrameCount();
        state.offscreen = true;
    }
}

    Vector3 head(pos.x, pos.y + 1.67f, pos.z);

    ImVec2 head2d, foot2d;  
    if (!world_to_screen(head, ViewMatrix, head2d) ||  
        !world_to_screen(pos,  ViewMatrix, foot2d))  
        continue;  
        
        update_trail(Player, pos);

    float top    = std::min(head2d.y, foot2d.y);  
    float bot    = std::max(head2d.y, foot2d.y);  
    float height = bot - top;  
    float width  = height * 0.25f;  
    float cx     = (head2d.x + foot2d.x) * 0.5f;  

    ImVec2 bmin(cx - width, top);  
    ImVec2 bmax(cx + width, bot);  
    ImRect rect(bmin, bmax);  

    float font_sz = std::clamp(400.f / (dist + 0.1f), 10.f, 18.f);  

    // -------- BOX --------  
    if (cfg::esp::box) {  
        if (cfg::esp::box_type == 0)  
            dbox(bmin, bmax, 1.f);  
        else  
            dbox_corner(bmin, bmax, 1.5f, 1.f);  
    }  

    // -------- FILL 2D --------  
    if (cfg::esp::fill) {  
        ImU32 col_t = IM_COL32(  
            (int)(cfg::esp::fill_col.x * 255),  
            (int)(cfg::esp::fill_col.y * 255),  
            (int)(cfg::esp::fill_col.z * 255),  
            (int)(cfg::esp::fill_col.w * 120));  
        ImU32 col_b = IM_COL32(  
            (int)(cfg::esp::fillgradient_col.x * 255),  
            (int)(cfg::esp::fillgradient_col.y * 255),  
            (int)(cfg::esp::fillgradient_col.z * 255),  
            (int)(cfg::esp::fillgradient_col.w * 120));  

        fill_quad_v_grad(dl,  
            bmin, ImVec2(bmax.x, bmin.y),  
            bmax, ImVec2(bmin.x, bmax.y),  
            col_t, col_b);  
    }  

    // -------- 3D --------  
    if (cfg::esp::box3d)  
        draw_box3d(pos, ViewMatrix, cfg::esp::box3d_col, 1.f);  

    if (cfg::esp::fill3d) {  
        if (cfg::esp::fill3d_type == 0)  
            draw_box3d_fill_base(pos, ViewMatrix, cfg::esp::fill3d_col, 1.f);  
        else  
            draw_box3d_fill(pos, ViewMatrix, cfg::esp::fill3d_col, 0.35f);  
    }  

    // -------- HEALTH --------  
    if (cfg::esp::health)  
        dhp(hp, bmin, bmax, height, 1.f);  

    // -------- ARMOR --------  
    if (cfg::esp::armor && armor > 0)  
        darmor_bottom(armor, bmin, bmax, 1.f);  

// -------- NAME --------  
    if (cfg::esp::name) {  
        auto name = player::name(Player).as_utf8();  
        if (!name.empty())  
            dnick(name.c_str(), bmin, bmax, cx, font_sz, 1.f);  
    }  
      
    // -------- WEAPON --------
if (cfg::esp::weapon)
{
    float weapon_y = bmax.y + 2.f;

    // если армор бар включён и есть армор — weapon уплывает под бар
    if (cfg::esp::armor && armor > 0)
    {
        // bmax.y + 4 (отступ до бара) + 3 (bar_h) + 2 (нижний padding фона)
        weapon_y = bmax.y + 4.f + 3.f + 2.f + 2.f;
    }

    dweapon(Player, ImVec2(bmax.x, weapon_y), cx, font_sz, 1.f);
}

// -------- DISTANCE --------  
    int ping = player::ping(Player);

float base_x = bmax.x + 6.f;
float base_y = bmin.y + 2.f;
float line_h = font_sz + 2.f;

// distance + ping колонка
if (cfg::esp::distance)
{
ddist(dist, ImVec2(base_x, base_y), font_sz, 1.f);
base_y += line_h;
}

if (cfg::esp::ping)
{
dping(ping, ImVec2(base_x, base_y), font_sz, 1.f);
base_y += line_h;
}
if (cfg::esp::device_text)
{
    g_device_text_pos = ImVec2(base_x, base_y);
    g_device_font_sz  = font_sz;
    ddevice(Player, bmin, bmax, 1.f);
}

// -------- LINE --------
if (cfg::esp::line) {
    ImU32 col = IM_COL32(
        (int)(cfg::esp::line_col.x * 255),
        (int)(cfg::esp::line_col.y * 255),
        (int)(cfg::esp::line_col.z * 255),
        (int)(cfg::esp::line_col.w * 255)
    );

    ImVec2 screen_top_center(ImGui::GetIO().DisplaySize.x * 0.5f, 0.f);
    add_line_outlined(dl, screen_top_center, head2d, col, 1.f);
}

if (cfg::esp::trail)
{
    bool isEnemy = (rpm<uint8_t>(Player + oxorany(0x79)) != LocalTeam);
    bool isSelf  = (Player == LocalPlayer);

    if ((cfg::esp::trail_enemy && isEnemy) ||
        (cfg::esp::trail_self && isSelf))
    {
        draw_trail(dl, Player, ViewMatrix);
    }
}
if(cfg::esp::marker)
{
    draw_damage_marker(ViewMatrix);
}

if (cfg::esp::skeleton)
{
    dskeleton(dl, rect, Player, ViewMatrix);
}

if (cfg::chams::enabled)
    chams::draw_chams(Player, ViewMatrix);

int arrow_frame = ImGui::GetFrameCount();

// -------- ARROWS ANIMATION RENDERING --------
if (cfg::esp::esp_arrows) {
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

    for (auto it = g_arrow_anim_states.begin(); it != g_arrow_anim_states.end();) {
        ArrowAnimState& state = it->second;
        bool visible = state.seen_frame == arrow_frame && state.offscreen;
        float target_alpha = visible ? 1.0f : 0.0f;

        if (cfg::esp::esp_arrows_in_animated) {
            float step = dt * 6.0f;
            if (state.alpha < target_alpha)
                state.alpha = std::min(target_alpha, state.alpha + step);
            else if (state.alpha > target_alpha)
                state.alpha = std::max(target_alpha, state.alpha - step);
        } else {
            state.alpha = target_alpha;
        }

        if (state.alpha > 0.001f)
            visuals::darrow(state.target.x, state.target.y, state.alpha, state.distance);

        if (!visible && state.alpha <= 0.001f)
            it = g_arrow_anim_states.erase(it);
        else
            ++it;
    }
}

SnowEffect(ViewMatrix, LocalPos);

draw_death_effect(ViewMatrix);

RainEffect(ViewMatrix, LocalPos);
}
}

// ================================================================
//  2D BOX — вертикальный градиент по рёбрам
//  Каждое ребро разбивается на сегменты, цвет интерполируется по Y
// ================================================================
void dbox(const ImVec2& t, const ImVec2& b, float a)
{
if (a < 0.01f) return;
ImDrawList* dl = ImGui::GetBackgroundDrawList();

const ImVec4& c0 = cfg::esp::box_col;  
const ImVec4& c1 = cfg::esp::boxgradient_col;  
float r = cfg::esp::box_rounding;  

float y_min = t.y;  
float y_max = b.y;  

// Тень (стандартная обводка с тенью)  
dl->AddRect(  
    ImVec2(t.x - 1.f, t.y - 1.f),  
    ImVec2(b.x + 1.f, b.y + 1.f),  
    IM_COL32(0, 0, 0, 180), r, 0, 2.5f);  
dl->AddRect(  
    ImVec2(t.x + 1.f, t.y + 1.f),  
    ImVec2(b.x - 1.f, b.y - 1.f),  
    IM_COL32(0, 0, 0, 100),  
    std::max(0.f, r - 1.f), 0, 1.f);  

// Рёбра бокса с вертикальным градиентом  
// Если rounding = 0 — рисуем 4 стороны сегментированными линиями  
// Если rounding > 0 — рисуем AddRect с одним цветом (упрощение; закругления и градиент — дорого)  
if (r < 0.5f) {  
    // TOP  (горизонтальное ребро — цвет = top gradient)  
    {  
        ImU32 col_top = lerp_col(c0, c1, 0.f, a);  
        dl->AddLine(ImVec2(t.x, t.y), ImVec2(b.x, t.y), col_top, 1.5f);  
    }  
    // BOTTOM  
    {  
        ImU32 col_bot = lerp_col(c0, c1, 1.f, a);  
        dl->AddLine(ImVec2(t.x, b.y), ImVec2(b.x, b.y), col_bot, 1.5f);  
    }  
    // LEFT (вертикаль — полный диапазон градиента)  
    add_line_v_grad(dl, ImVec2(t.x, t.y), ImVec2(t.x, b.y),  
                    c0, c1, a, 1.5f, y_min, y_max, 24);  
    // RIGHT  
    add_line_v_grad(dl, ImVec2(b.x, t.y), ImVec2(b.x, b.y),  
                    c0, c1, a, 1.5f, y_min, y_max, 24);  
} else {  
    // При rounding используем просто lerp цвета посередине как fallback  
    ImU32 col_mid = lerp_col(c0, c1, 0.5f, a);  
    dl->AddRect(t, b, col_mid, r, 0, 1.5f);  
}

}

// ================================================================
//  CORNER BOX — вертикальный градиент (угловые скобки)
// ================================================================
void dbox_corner(const ImVec2& min, const ImVec2& max, float thickness, float a)
{
if (a < 0.01f) return;
ImDrawList* dl = ImGui::GetBackgroundDrawList();

const ImVec4& c0 = cfg::esp::box_col;  
const ImVec4& c1 = cfg::esp::boxgradient_col;  

ImU32 shadow = IM_COL32(0, 0, 0, (int)(180 * a));  

float w  = max.x - min.x;  
float h  = max.y - min.y;  
float lw = w * 0.25f;  
float lh = h * 0.25f;  
float r  = std::clamp(cfg::esp::box_rounding, 0.0f, std::min(lw, lh));  

float ts = thickness + 1.5f;  
float y_min = min.y;  
float y_max = max.y;  

// Цвета по позиции Y  
ImU32 col_top = lerp_col(c0, c1, 0.f,  a);  
ImU32 col_mid = lerp_col(c0, c1, 0.25f, a);  
ImU32 col_mid2= lerp_col(c0, c1, 0.75f, a);  
ImU32 col_bot = lerp_col(c0, c1, 1.f,  a);  

// Лямбда для дуги (дуги остаются однотонными — ImGui не поддерживает per-vertex дуги легко)  
auto corner_arc_grad = [&](ImVec2 center, float start, float end, ImU32 col)  
{  
    if (r > 0.5f) {  
        dl->PathArcTo(center, r + 0.75f, start, end, 6);  
        dl->PathStroke(shadow, 0, ts);  
        dl->PathArcTo(center, r, start, end, 6);  
        dl->PathStroke(col, 0, thickness);  
    }  
};  

// ---- ТЕНИ ----  
// TOP LEFT  
dl->AddLine(ImVec2(min.x + r, min.y), ImVec2(min.x + lw, min.y), shadow, ts);  
dl->AddLine(ImVec2(min.x, min.y + r), ImVec2(min.x, min.y + lh), shadow, ts);  
// TOP RIGHT  
dl->AddLine(ImVec2(max.x - lw, min.y), ImVec2(max.x - r, min.y), shadow, ts);  
dl->AddLine(ImVec2(max.x, min.y + r),  ImVec2(max.x, min.y + lh), shadow, ts);  
// BOTTOM LEFT  
dl->AddLine(ImVec2(min.x + r, max.y), ImVec2(min.x + lw, max.y), shadow, ts);  
dl->AddLine(ImVec2(min.x, max.y - lh), ImVec2(min.x, max.y - r), shadow, ts);  
// BOTTOM RIGHT  
dl->AddLine(ImVec2(max.x - lw, max.y), ImVec2(max.x - r, max.y), shadow, ts);  
dl->AddLine(ImVec2(max.x, max.y - lh),  ImVec2(max.x, max.y - r), shadow, ts);  

// ---- ОСНОВНЫЕ ЛИНИИ с градиентом ----  
// TOP LEFT  — горизонталь берёт col_top, вертикаль идёт от top к mid  
dl->AddLine(ImVec2(min.x + r, min.y), ImVec2(min.x + lw, min.y), col_top, thickness);  
add_line_v_grad(dl, ImVec2(min.x, min.y + r), ImVec2(min.x, min.y + lh),  
                c0, c1, a, thickness, y_min, y_max, 12);  
corner_arc_grad(ImVec2(min.x + r, min.y + r), IM_PI, IM_PI * 1.5f, col_top);  

// TOP RIGHT  
dl->AddLine(ImVec2(max.x - lw, min.y), ImVec2(max.x - r, min.y), col_top, thickness);  
add_line_v_grad(dl, ImVec2(max.x, min.y + r), ImVec2(max.x, min.y + lh),  
                c0, c1, a, thickness, y_min, y_max, 12);  
corner_arc_grad(ImVec2(max.x - r, min.y + r), IM_PI * 1.5f, IM_PI * 2.0f, col_top);  

// BOTTOM LEFT  
dl->AddLine(ImVec2(min.x + r, max.y), ImVec2(min.x + lw, max.y), col_bot, thickness);  
add_line_v_grad(dl, ImVec2(min.x, max.y - lh), ImVec2(min.x, max.y - r),  
                c0, c1, a, thickness, y_min, y_max, 12);  
corner_arc_grad(ImVec2(min.x + r, max.y - r), IM_PI * 0.5f, IM_PI, col_bot);  

// BOTTOM RIGHT  
dl->AddLine(ImVec2(max.x - lw, max.y), ImVec2(max.x - r, max.y), col_bot, thickness);  
add_line_v_grad(dl, ImVec2(max.x, max.y - lh), ImVec2(max.x, max.y - r),  
                c0, c1, a, thickness, y_min, y_max, 12);  
corner_arc_grad(ImVec2(max.x - r, max.y - r), 0.0f, IM_PI * 0.5f, col_bot);

}

// ================================================================
//  HEALTH BAR — вертикальный градиент
// ================================================================
void dhp(int hp, const ImVec2& t, const ImVec2& b, float box_h, float a)
{
if (a < 0.01f) return;
ImDrawList* dl = ImGui::GetBackgroundDrawList();

hp = std::clamp(hp, 0, 100);  
float pct = hp / 100.f;  

float bx      = roundf(t.x - 6.f);  
float bw      = 3.f;  
float bot_y   = b.y;  
float top_y   = t.y;  
float bh      = roundf(bot_y - top_y);  
float fh      = bh * pct;  
float fill_top = roundf(bot_y - fh);  

// фон  
dl->AddRectFilled(ImVec2(bx - 2.f, top_y - 2.f), ImVec2(bx + bw + 2.f, bot_y + 2.f),  
                  IM_COL32(0,0,0,120), 1.f);  
dl->AddRectFilled(ImVec2(bx - 1.f, top_y - 1.f), ImVec2(bx + bw + 1.f, bot_y + 1.f),  
                  IM_COL32(0,0,0,200), 0.f);  

// Вертикальный градиент HP: верх = health_col, низ = healthgradient_col  
// fill_top — верхняя граница заполненной части, bot_y — нижняя (всегда основание бара)  
// Нормализуем позицию fill_top внутри полного бара [top_y..bot_y],  
// чтобы цвет градиента соответствовал реальной позиции в бокселе, а не всегда начинался с нуля  
float t_norm_top = std::clamp((fill_top - top_y) / (bh + 0.001f), 0.f, 1.f);  
float t_norm_bot = 1.f;  

ImU32 col_t = lerp_col(cfg::esp::health_col, cfg::esp::healthgradient_col, t_norm_top, a);  
ImU32 col_b = lerp_col(cfg::esp::health_col, cfg::esp::healthgradient_col, t_norm_bot, a);  

fill_quad_v_grad(dl,  
    ImVec2(bx, fill_top), ImVec2(bx + bw, fill_top),  
    ImVec2(bx + bw, bot_y), ImVec2(bx, bot_y),  
    col_t, col_b);

}

// ================================================================
//  NAME — горизонтальный градиент текста (через два перекрытых рендера
//  с ImGui clip rect'ами левой и правой половины)
// ================================================================
void dnick(const char* name,
const ImVec2& min, const ImVec2& max,
float cx, float sz, float a)
{
if (!espFont || !name || a < 0.01f) return;
ImDrawList* dl = ImGui::GetBackgroundDrawList();

ImVec2 ts  = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);  
ImVec2 pos(cx - ts.x * 0.5f, min.y - ts.y - 3.f);  

// Тень текста  
dl->AddText(espFont, sz, ImVec2(pos.x + 1, pos.y + 1),  
            IM_COL32(0,0,0, (int)(0.6f * a * 255)), name);  
dl->AddText(espFont, sz, ImVec2(pos.x + 2, pos.y + 2),  
            IM_COL32(0,0,0, (int)(0.3f * a * 255)), name);  

// Горизонтальный градиент через clip rect:  
// Левая половина — name_col, правая — label_gradient_col  
// Плавный переход достигается через N срезов по X  
//  
// Реализация: рисуем текст N раз, каждый раз с узким clip rect  
// и цветом, интерполированным по X-позиции среза.  
const int SLICES = 32;  
float x_start = pos.x;  
float x_end   = pos.x + ts.x;  
float slice_w  = ts.x / SLICES;  

for (int s = 0; s < SLICES; s++)  
{  
    float t = ((float)s + 0.5f) / (float)SLICES;  
    ImU32 slice_col = lerp_col(cfg::esp::name_col, cfg::esp::namegradient_col, t, a);  

    float clip_x0 = x_start + s * slice_w;  
    float clip_x1 = clip_x0 + slice_w + 0.5f; // +0.5 против зазоров  

    dl->PushClipRect(  
        ImVec2(clip_x0, pos.y - 1.f),  
        ImVec2(clip_x1, pos.y + ts.y + 1.f),  
        true);  
    dl->AddText(espFont, sz, pos, slice_col, name);  
    dl->PopClipRect();  
}

}

// ================================================================
//  DISTANCE — горизонтальный градиент (аналогично name, но проще —
//  текст короткий, достаточно 2 цветов через твою мертвую матуху)
// ================================================================
void ddist(float dist, const ImVec2& pos, float sz, float a)
{
if (!espFont || a < 0.01f) return;

ImDrawList* dl = ImGui::GetBackgroundDrawList();  

char txt[16];  
snprintf(txt, sizeof(txt), "%dm", (int)dist);  

ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, txt);  

// shadow  
dl->AddText(espFont, sz,  
    ImVec2(pos.x + 1, pos.y + 1),  
    IM_COL32(0,0,0, (int)(0.6f * a * 255)), txt);  

dl->AddText(espFont, sz,  
    ImVec2(pos.x + 2, pos.y + 2),  
    IM_COL32(0,0,0, (int)(0.3f * a * 255)), txt);  

const int SLICES = 16;  
float slice_w = ts.x / SLICES;  

for (int s = 0; s < SLICES; s++)  
{  
    float t = ((float)s + 0.5f) / (float)SLICES;  

    ImU32 col = lerp_col(  
        cfg::esp::distance_col,  
        cfg::esp::distancegradient_col,  
        t,  
        a  
    );  

    float x0 = pos.x + s * slice_w;  
    float x1 = x0 + slice_w + 0.5f;  

    dl->PushClipRect(  
        ImVec2(x0, pos.y - 1.f),  
        ImVec2(x1, pos.y + ts.y + 1.f),  
        true  
    );  

    dl->AddText(espFont, sz, pos, col, txt);  
    dl->PopClipRect();  
}

}

void dping(int ping, const ImVec2& pos, float sz, float a)
{
if (!espFont || a < 0.01f) return;

ImDrawList* dl = ImGui::GetBackgroundDrawList();  

char txt[16];  
snprintf(txt, sizeof(txt), "%dms", ping);  

ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, txt);  

// shadow  
dl->AddText(espFont, sz,  
    ImVec2(pos.x + 1, pos.y + 1),  
    IM_COL32(0,0,0, (int)(0.6f * a * 255)), txt);  

dl->AddText(espFont, sz,  
    ImVec2(pos.x + 2, pos.y + 2),  
    IM_COL32(0,0,0, (int)(0.3f * a * 255)), txt);  

const int SLICES = 16;  
float slice_w = ts.x / SLICES;  

for (int s = 0; s < SLICES; s++)  
{  
    float t = ((float)s + 0.5f) / (float)SLICES;  

    ImU32 col = lerp_col(  
        cfg::esp::ping_col,  
        cfg::esp::pinggradient_col,  
        t,  
        a  
    );  

    float x0 = pos.x + s * slice_w;  
    float x1 = x0 + slice_w + 0.5f;  

    dl->PushClipRect(  
        ImVec2(x0, pos.y - 1.f),  
        ImVec2(x1, pos.y + ts.y + 1.f),  
        true  
    );  

    dl->AddText(espFont, sz, pos, col, txt);  
    dl->PopClipRect();  
}

}

void dweapon(uint64_t player,
             const ImVec2& box_max,
             float cx,
             float fontSize,
             float a)
{
    uint64_t weaponry = rpm<uint64_t>(player + oxorany(0x88));
    if (!player::likely_ptr(weaponry)) return;

    uint64_t weapon = rpm<uint64_t>(weaponry + oxorany(0xA0));
    if (!player::likely_ptr(weapon))
        weapon = rpm<uint64_t>(weaponry + oxorany(0x98));
    if (!player::likely_ptr(weapon)) return;

    uint64_t params = rpm<uint64_t>(weapon + oxorany(0xA8));
    if (!player::likely_ptr(params))
        params = rpm<uint64_t>(weapon + oxorany(0xA0));
    if (!player::likely_ptr(params)) return;

    int id = rpm<int>(params + oxorany(0x18));
    const char* name = get_weapon_name(id);
    if (!name) return;

    if (!espFont || a < 0.01f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // используем ОДИН размер шрифта везде — никакого realSize/fontSize рассинхрона
    ImVec2 ts = espFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, name);

    // центр строго под боксом / под армор баром (box_max.y уже учитывает сдвиг)
    ImVec2 pos(cx - ts.x * 0.5f, box_max.y);

    // shadow
    dl->AddText(espFont, fontSize,
        ImVec2(pos.x + 1, pos.y + 1),
        IM_COL32(0,0,0, (int)(0.6f * a * 255)), name);
    dl->AddText(espFont, fontSize,
        ImVec2(pos.x + 2, pos.y + 2),
        IM_COL32(0,0,0, (int)(0.3f * a * 255)), name);

    // градиент срезами — слегка расширяем общий clip, чтобы хвост не "чернел"
    const int SLICES = 24;
    float slice_w = ts.x / SLICES;

    for (int s = 0; s < SLICES; s++)
    {
        float t = ((float)s + 0.5f) / (float)SLICES;

        ImU32 col = lerp_col(
            cfg::esp::weapon_col,
            cfg::esp::weapongradient_col,
            t,
            a
        );

        float x0 = pos.x + s * slice_w;
        // последний срез растягиваем до конца текста, чтобы не оставалось непрокрашенного хвоста
        float x1 = (s == SLICES - 1) ? (pos.x + ts.x + 1.f) : (x0 + slice_w + 0.5f);

        dl->PushClipRect(
            ImVec2(x0, pos.y - 1.f),
            ImVec2(x1, pos.y + ts.y + 1.f),
            true
        );

        dl->AddText(espFont, fontSize, pos, col, name);
        dl->PopClipRect();
    }
}

// ================================================================
//  3D BOX — вертикальный градиент яиц
// ================================================================
void draw_box3d(const Vector3& pos, const matrix& view, const ImVec4& col, float a)
{
ImDrawList* dl = ImGui::GetBackgroundDrawList();

float w = 0.3f, h = 1.7f, d = 0.3f;  

Vector3 corners[8] = {  
    { pos.x - w, pos.y,     pos.z - d },  
    { pos.x - w, pos.y,     pos.z + d },  
    { pos.x + w, pos.y,     pos.z + d },  
    { pos.x + w, pos.y,     pos.z - d },  
    { pos.x - w, pos.y + h, pos.z - d },  
    { pos.x - w, pos.y + h, pos.z + d },  
    { pos.x + w, pos.y + h, pos.z + d },  
    { pos.x + w, pos.y + h, pos.z - d },  
};  

ImVec2 s[8];  
for (int i = 0; i < 8; i++)  
    if (!world_to_screen(corners[i], view, s[i])) return;  

// y_min / y_max в screen space (меньший Y = верх экрана = "верх" градиента)  
float y_min = FLT_MAX, y_max = -FLT_MAX;  
for (auto& v : s) {  
    y_min = std::min(y_min, v.y);  
    y_max = std::max(y_max, v.y);  
}  

const ImVec4& c1 = cfg::esp::box3dgradient_col;  

constexpr int edges[12][2] = {  
    {0,1},{1,2},{2,3},{3,0},  
    {4,5},{5,6},{6,7},{7,4},  
    {0,4},{1,5},{2,6},{3,7}  
};  

// Каждое ребро — градиент по Y  
for (auto& e : edges)  
    add_line_v_grad(dl, s[e[0]], s[e[1]],  
                    col, c1, a, 1.5f, y_min, y_max, 16);

}

// ================================================================
//  3D BOX FILL GLASS грызть какащкэ
// ================================================================
void draw_box3d_fill(const Vector3& pos, const matrix& view, const ImVec4& col, float a)
{
ImDrawList* dl = ImGui::GetBackgroundDrawList();

float w = 0.3f, h = 1.7f, d = 0.3f;  

Vector3 corners[8] = {  
    { pos.x - w, pos.y,     pos.z - d },  
    { pos.x - w, pos.y,     pos.z + d },  
    { pos.x + w, pos.y,     pos.z + d },  
    { pos.x + w, pos.y,     pos.z - d },  
    { pos.x - w, pos.y + h, pos.z - d },  
    { pos.x - w, pos.y + h, pos.z + d },  
    { pos.x + w, pos.y + h, pos.z + d },  
    { pos.x + w, pos.y + h, pos.z - d },  
};  

ImVec2 s[8];  
for (int i = 0; i < 8; i++)  
    if (!world_to_screen(corners[i], view, s[i])) return;  

ImVec2 uv = dl->_Data->TexUvWhitePixel;  

const ImVec4& c1 = cfg::esp::fill3dgradient_col;  

// c_base: верх = col (основной), низ = gradient_col  
ImU32 c_dark  = IM_COL32(0, 0, 0, (int)(80 * a));  
ImU32 c_edge_top = IM_COL32(  
    (int)(col.x * 255), (int)(col.y * 255),  
    (int)(col.z * 255), (int)(col.w * 200 * a));  
ImU32 c_edge_bot = IM_COL32(  
    (int)(c1.x * 255), (int)(c1.y * 255),  
    (int)(c1.z * 255), (int)(c1.w * 200 * a));  

struct Face { int tl, tr, bl, br; };  
Face faces[4] = {  
    {4, 5, 0, 1},  
    {5, 6, 1, 2},  
    {6, 7, 2, 3},  
    {7, 4, 3, 0},  
};  

for (auto& f : faces)  
{  
    ImVec2 tl = s[f.tl], tr = s[f.tr];  
    ImVec2 bl = s[f.bl], br = s[f.br];  

    float y_top = (tl.y + tr.y) * 0.5f;  
    float y_bot = (bl.y + br.y) * 0.5f;  
    if (y_top > y_bot) {  
        std::swap(tl, bl);  
        std::swap(tr, br);  
    }  

    // 1. тёмная подложка  
    ImVec2 v_dark[4] = { tl, tr, br, bl };  
    dl->AddConvexPolyFilled(v_dark, 4, c_dark);  

    // 2. базовый fill — вертикальный градиент (верх = col, низ = c1)  
    {  
        ImU32 base_top = IM_COL32(  
            (int)(col.x*255),(int)(col.y*255),(int)(col.z*255),(int)(col.w*40*a));  
        ImU32 base_bot = IM_COL32(  
            (int)(c1.x*255),(int)(c1.y*255),(int)(c1.z*255),(int)(c1.w*40*a));  
        fill_quad_v_grad(dl, tl, tr, br, bl, base_top, base_bot);  
    }  

    // 3. per-vertex белый градиент (блик верх → прозрачный)  
    {  
        ImU32 bright = IM_COL32(255, 255, 255, (int)(90 * a));  
        ImU32 trans  = IM_COL32(255, 255, 255, 0);  
        dl->PrimReserve(6, 4);  
        ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;  
        dl->PrimWriteVtx(tl, uv, bright);  
        dl->PrimWriteVtx(tr, uv, bright);  
        dl->PrimWriteVtx(br, uv, trans);  
        dl->PrimWriteVtx(bl, uv, trans);  
        dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+1); dl->PrimWriteIdx(idx+2);  
        dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+2); dl->PrimWriteIdx(idx+3);  
    }  

    // 4. блик-полоса (верхняя треть грани)  
    {  
        float t = 0.28f;  
        ImVec2 ml = ImVec2(tl.x + (bl.x - tl.x) * t, tl.y + (bl.y - tl.y) * t);  
        ImVec2 mr = ImVec2(tr.x + (br.x - tr.x) * t, tr.y + (br.y - tr.y) * t);  
        ImU32 hl_top = IM_COL32(255, 255, 255, (int)(210 * a));  
        ImU32 hl_bot = IM_COL32(255, 255, 255, 0);  
        dl->PrimReserve(6, 4);  
        ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;  
        dl->PrimWriteVtx(tl, uv, hl_top);  
        dl->PrimWriteVtx(tr, uv, hl_top);  
        dl->PrimWriteVtx(mr, uv, hl_bot);  
        dl->PrimWriteVtx(ml, uv, hl_bot);  
        dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+1); dl->PrimWriteIdx(idx+2);  
        dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+2); dl->PrimWriteIdx(idx+3);  
    }  

    // 5. контур по рёбрам грани  
    dl->AddLine(tl, tr, IM_COL32(255, 255, 255, (int)(180 * a)), 1.5f);  
    dl->AddLine(bl, br, IM_COL32(0, 0, 0, (int)(120 * a)), 1.0f);  
}  

// 6. рёбра бокса с вертикальным градиентом  
float y_min = FLT_MAX, y_max = -FLT_MAX;  
for (auto& v : s) { y_min = std::min(y_min, v.y); y_max = std::max(y_max, v.y); }  

constexpr int edges[12][2] = {  
    {0,1},{1,2},{2,3},{3,0},  
    {4,5},{5,6},{6,7},{7,4},  
    {0,4},{1,5},{2,6},{3,7}  
};  
for (auto& e : edges)  
    add_line_v_grad(dl, s[e[0]], s[e[1]],  
                    col, c1, a, 1.0f, y_min, y_max, 16);

}

// ================================================================
//  3D BOX FILL BASE ням
// ================================================================
void draw_box3d_fill_base(const Vector3& pos, const matrix& view, const ImVec4& col, float a)
{
ImDrawList* dl = ImGui::GetBackgroundDrawList();

float w = 0.3f, h = 1.7f, d = 0.3f;  

Vector3 corners[8] = {  
    { pos.x - w, pos.y,     pos.z - d },  
    { pos.x - w, pos.y,     pos.z + d },  
    { pos.x + w, pos.y,     pos.z + d },  
    { pos.x + w, pos.y,     pos.z - d },  
    { pos.x - w, pos.y + h, pos.z - d },  
    { pos.x - w, pos.y + h, pos.z + d },  
    { pos.x + w, pos.y + h, pos.z + d },  
    { pos.x + w, pos.y + h, pos.z - d },  
}; // гпт солушионс  

ImVec2 s[8];  
for (int i = 0; i < 8; i++)  
    if (!world_to_screen(corners[i], view, s[i])) return;  

const ImVec4& c1 = cfg::esp::fill3dgradient_col;  

// Для каждой грани определяем верх/низ в screen space и рисуем градиент  
struct Face { int tl, tr, bl, br; };  
Face faces[4] = {  
    {4, 5, 0, 1},  
    {5, 6, 1, 2},  
    {6, 7, 2, 3},  
    {7, 4, 3, 0},  
};  

for (auto& f : faces)  
{  
    ImVec2 tl = s[f.tl], tr = s[f.tr];  
    ImVec2 bl = s[f.bl], br = s[f.br];  

    float y_top = (tl.y + tr.y) * 0.5f;  
    float y_bot = (bl.y + br.y) * 0.5f;  
    if (y_top > y_bot) {  
        std::swap(tl, bl);  
        std::swap(tr, br);  
    }  

    ImU32 c_top = IM_COL32(  
        (int)(col.x*255),(int)(col.y*255),(int)(col.z*255),(int)(col.w*120*a));  
    ImU32 c_bot = IM_COL32(  
        (int)(c1.x*255),(int)(c1.y*255),(int)(c1.z*255),(int)(c1.w*120*a)); // тоже самое  

    fill_quad_v_grad(dl, tl, tr, br, bl, c_top, c_bot);  
}

}

static const char* get_weapon_name(int id)
{
switch (id)
{
case 11: return "G22";
case 12: return "USP";
case 13: return "P350";
case 15: return "Deagle";
case 16: return "Tec9";
case 17: return "Five-Seven";
case 18: return "Berettas";

case 32: return "UMP45";  
    case 33: return "Akimbo Uzi";  
    case 34: return "MP7";  
    case 35: return "P90";  
    case 36: return "MP5";  
    case 37: return "MAC10";  

    case 42: return "VAL";  
    case 43: return "M4A1";  
    case 44: return "AKR";  
    case 45: return "AKR12";  
    case 46: return "M4";  
    case 47: return "M16";  
    case 48: return "FAMAS";  
    case 49: return "FN FAL";  

    case 51: return "AWM";  
    case 52: return "M40";  
    case 53: return "M110";  

    case 62: return "SM1014";  
    case 63: return "FabM";  
    case 64: return "M60";  
    case 65: return "SPAS";  

    case 70: return "Knife";  
    case 71: return "Bayonet";  
    case 72: return "Karambit";  
    case 73: return "Kommando";  
    case 75: return "Butterfly";  
    case 77: return "Flip Knife";  
    case 78: return "Kunai";  
    case 79: return "Scorpion";  
    case 80: return "Tanto";  
    case 81: return "Dagger";  
    case 82: return "Kukri";  
    case 83: return "Stilet";  
    case 85: return "Mantis";  
    case 86: return "Fang";  
    case 88: return "Sting";  
    case 89: return "Hands";  

    case 91: return "HE";  
    case 92: return "Smoke";  
    case 93: return "Flash";  
    case 94: return "Molotov";  
    case 95: return "Incendiary";  
    case 100: return "Bomb";  
}  

return "Unknown";

}

// ====== RGB COLOR FUNCTION ======
static ImU32 get_color_with_rgb(bool rgb_enabled, float rgb_speed, ImVec4 color, float alpha) {
    if (!rgb_enabled) {
        return IM_COL32(color.x * 255, color.y * 255, color.z * 255, color.w * 255 * alpha);
    }
    
    float time = ImGui::GetTime();
    float r = (sinf(time * rgb_speed) + 1.0f) * 0.5f;
    float g = (sinf(time * rgb_speed + 2.0f) + 1.0f) * 0.5f;
    float b = (sinf(time * rgb_speed + 4.0f) + 1.0f) * 0.5f;
    
    return IM_COL32(r * 255, g * 255, b * 255, color.w * 255 * alpha);
}

void darrow(float target_scrx, float target_scry, float alpha, float distance) {
    if (!cfg::esp::esp_arrows || alpha <= 0.001f) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float cx = g_sw * 0.5f;
    float cy = g_sh * 0.5f;
    float dx = target_scrx - cx;
    float dy = target_scry - cy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) {
        dx = 0.0f;
        dy = -1.0f;
    } else {
        dx /= len;
        dy /= len;
    }

    float sc = g_sh / 757.0f;                   // масштаб под разрешение
    float radius = cfg::esp::esp_arrows_distance * sc;   // радиус от прицела
    float size   = cfg::esp::esp_arrows_size * sc;
    if (cfg::esp::esp_arrows_in_animated)
        size *= 0.85f + 0.15f * alpha;

    float margin = size * (1.5f + cfg::esp::esp_arrows_glow * 0.15f);
    float max_radius_x = fabsf(dx) > 0.001f ? (cx - margin) / fabsf(dx) : radius;
    float max_radius_y = fabsf(dy) > 0.001f ? (cy - margin) / fabsf(dy) : radius;
    radius = std::max(0.0f, std::min(radius, std::min(max_radius_x, max_radius_y)));

    ImVec2 base(cx + dx * radius, cy + dy * radius);     // центр стрелки на окружности
    float px = -dy, py = dx;                    // перпендикуляр

    ImVec2 tip(base.x + dx * size,            base.y + dy * size);            // остриё наружу
    ImVec2 w1(base.x - dx * size * 0.6f + px * size * 0.6f,
              base.y - dy * size * 0.6f + py * size * 0.6f);
    ImVec2 w2(base.x - dx * size * 0.6f - px * size * 0.6f,
              base.y - dy * size * 0.6f - py * size * 0.6f);

    ImU32 col = get_color_with_rgb(cfg::esp::esp_arrows_rgb, cfg::esp::esp_arrows_rgb_speed, cfg::esp::esp_arrows_col, alpha);

    bool draw_distance = cfg::esp::esp_arrows_show_distance && espFont;
    char distance_text[16] = {};
    ImVec2 distance_pos;
    float distance_font_size = cfg::visuals::esp_font_size * sc;
    if (draw_distance) {
        snprintf(distance_text, sizeof(distance_text), "%dm", static_cast<int>(distance));
        ImVec2 text_size = espFont->CalcTextSizeA(distance_font_size, FLT_MAX, 0.0f, distance_text);
        float gap = 5.0f * sc;
        float radial_half = 0.5f * (fabsf(dx) * text_size.x + fabsf(dy) * text_size.y);
        ImVec2 text_center(base.x - dx * (size * 0.6f + gap + radial_half),
                           base.y - dy * (size * 0.6f + gap + radial_half));
        distance_pos = ImVec2(text_center.x - text_size.x * 0.5f,
                              text_center.y - text_size.y * 0.5f);
    }

    int glow_layers = (int)ceilf(cfg::esp::esp_arrows_glow);
    if (glow_layers > 0) {
        ImVec2 center((tip.x + w1.x + w2.x) / 3.0f, (tip.y + w1.y + w2.y) / 3.0f);
        ImVec4 glow_color = ImGui::ColorConvertU32ToFloat4(col);

        for (int i = glow_layers; i >= 1; --i) {
            float t = (float)i / (float)(glow_layers + 1);
            float expand = 1.0f + 0.10f * i;
            ImVec2 glow_tip(center.x + (tip.x - center.x) * expand, center.y + (tip.y - center.y) * expand);
            ImVec2 glow_w1(center.x + (w1.x - center.x) * expand, center.y + (w1.y - center.y) * expand);
            ImVec2 glow_w2(center.x + (w2.x - center.x) * expand, center.y + (w2.y - center.y) * expand);
            ImVec4 layer_color(glow_color.x, glow_color.y, glow_color.z,
                               glow_color.w * 0.18f * (1.0f - t));
            dl->AddTriangleFilled(glow_tip, glow_w1, glow_w2,
                                  ImGui::ColorConvertFloat4ToU32(layer_color));

            if (draw_distance) {
                float blur = i * sc;
                ImU32 text_glow = ImGui::ColorConvertFloat4ToU32(layer_color);
                dl->AddText(espFont, distance_font_size, ImVec2(distance_pos.x - blur, distance_pos.y), text_glow, distance_text);
                dl->AddText(espFont, distance_font_size, ImVec2(distance_pos.x + blur, distance_pos.y), text_glow, distance_text);
                dl->AddText(espFont, distance_font_size, ImVec2(distance_pos.x, distance_pos.y - blur), text_glow, distance_text);
                dl->AddText(espFont, distance_font_size, ImVec2(distance_pos.x, distance_pos.y + blur), text_glow, distance_text);
            }
        }
    }

    dl->AddTriangleFilled(tip, w1, w2, col);
    dl->AddTriangle(tip, w1, w2,
                    IM_COL32(0, 0, 0, (int)(160 * alpha * cfg::esp::esp_arrows_col.w)),
                    1.2f * sc);
    if (draw_distance)
        draw_text_outlined(dl, espFont, distance_font_size, distance_pos, col, distance_text);
}

void ddevice(uint64_t player, const ImVec2& box_min, const ImVec2& box_max, float a) {
    if (a < 0.01f) return;
    if (!cfg::esp::device_text) return;
    if (!espFont) return;

    int platformVal = player::platform(player);
    if (platformVal == 0) return;

    const char* label = platformVal == 1 ? "and." : "ios";
    ImDrawList* dl    = ImGui::GetBackgroundDrawList();

    // Позиция берётся из g_device_text_pos — устанавливается прямо перед вызовом
    // в том же цикле игрока, поэтому всегда актуальна для текущего Player.
    ImVec2 p = ImVec2(roundf(g_device_text_pos.x), roundf(g_device_text_pos.y));

    // Размер шрифта — тот же что у dist/ping для этого игрока
    float sz = g_device_font_sz;

    ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, label);

    // shadow
    dl->AddText(espFont, sz, ImVec2(p.x + 1, p.y + 1),
        IM_COL32(0, 0, 0, (int)(0.6f * a * 255)), label);
    dl->AddText(espFont, sz, ImVec2(p.x + 2, p.y + 2),
        IM_COL32(0, 0, 0, (int)(0.3f * a * 255)), label);

    // 16-slice горизонтальный градиент: device_col → device_textgradient_col
    const int SLICES = 16;
    float slice_w = ts.x / SLICES;

    for (int s = 0; s < SLICES; s++) {
        float t = ((float)s + 0.5f) / (float)SLICES;

        ImU32 col = lerp_col(
            cfg::esp::device_col,
            cfg::esp::device_textgradient_col,
            t,
            a
        );

        float x0 = p.x + s * slice_w;
        float x1 = x0 + slice_w + 0.5f;

        dl->PushClipRect(
            ImVec2(x0, p.y - 1.f),
            ImVec2(x1, p.y + ts.y + 1.f),
            true
        );
        dl->AddText(espFont, sz, p, col, label);
        dl->PopClipRect();
    }
}
}//немепаст визуалс🤑