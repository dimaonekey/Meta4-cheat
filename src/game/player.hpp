#pragma once

#include "game.hpp"
#include "offsets.hpp"
#include "../other/vector3.h"
#include "../other/string.h"
#include "../protect/oxorany.hpp"
#include <cstring>
#include <string>
#include <cmath>
#include <vector>
#include <unordered_map>

namespace player {

    struct TransformEntry {
        Vector4 position;
        Vector4 rotation;
        Vector4 scale;
    };

    inline bool likely_ptr(uint64_t p) noexcept {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    inline bool sane_world_pos(const Vector3& v) noexcept {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
               std::fabs(v.x) < 100000.f && std::fabs(v.y) < 100000.f && std::fabs(v.z) < 100000.f &&
               !(v.x == 0.f && v.y == 0.f && v.z == 0.f);
    }

    inline bool sane_bone_pos(const Vector3& v, const Vector3& base) noexcept {
        if (!sane_world_pos(v)) return false;
        if (!sane_world_pos(base)) return true;

        float dx = v.x - base.x;
        float dy = v.y - base.y;
        float dz = v.z - base.z;
        return (dx * dx + dy * dy + dz * dz) <= 100.f;
    }

    inline Vector3 rotate_by_quat(const Vector3& v, const Vector4& q) noexcept {
        float x2 = q.x + q.x;
        float y2 = q.y + q.y;
        float z2 = q.z + q.z;

        float xx = q.x * x2;
        float yy = q.y * y2;
        float zz = q.z * z2;
        float xy = q.x * y2;
        float xz = q.x * z2;
        float yz = q.y * z2;
        float wx = q.w * x2;
        float wy = q.w * y2;
        float wz = q.w * z2;

        return Vector3(
            (1.f - (yy + zz)) * v.x + (xy - wz) * v.y + (xz + wy) * v.z,
            (xy + wz) * v.x + (1.f - (xx + zz)) * v.y + (yz - wx) * v.z,
            (xz - wy) * v.x + (yz + wx) * v.y + (1.f - (xx + yy)) * v.z
        );
    }

    inline bool transform_position_from_data(uint64_t transform_data, int index, Vector3& out) noexcept {
        if (!likely_ptr(transform_data) || index < 0 || index > 100000) return false;

        uint64_t transform_array = rpm<uint64_t>(transform_data + oxorany(OFF_TRANSFORM_DATA_ARRAY));
        uint64_t transform_indices = rpm<uint64_t>(transform_data + oxorany(OFF_TRANSFORM_DATA_INDICES));
        if (!likely_ptr(transform_array) || !likely_ptr(transform_indices)) return false;

        TransformEntry entry = rpm<TransformEntry>(transform_array + (sizeof(TransformEntry) * static_cast<uint64_t>(index)));
        Vector3 result(entry.position.x, entry.position.y, entry.position.z);

        int parent = rpm<int>(transform_indices + (sizeof(int) * static_cast<uint64_t>(index)));
        int depth = 0;

        while (parent >= 0 && parent <= 100000 && depth++ < 64) {
            TransformEntry parent_entry = rpm<TransformEntry>(transform_array + (sizeof(TransformEntry) * static_cast<uint64_t>(parent)));
            Vector3 scaled(result.x * parent_entry.scale.x, result.y * parent_entry.scale.y, result.z * parent_entry.scale.z);
            Vector3 rotated = rotate_by_quat(scaled, parent_entry.rotation);

            result = Vector3(parent_entry.position.x, parent_entry.position.y, parent_entry.position.z) + rotated;
            parent = rpm<int>(transform_indices + (sizeof(int) * static_cast<uint64_t>(parent)));
        }

        if (!sane_world_pos(result)) return false;

        out = result;
        return true;
    }

    inline bool transform_position(uint64_t transform, Vector3& out) noexcept {
        if (!likely_ptr(transform)) return false;

        uint64_t native_transform = rpm<uint64_t>(transform + oxorany(OFF_TRANSFORM_NATIVE));
        if (!likely_ptr(native_transform)) return false;

        uint64_t transform_data = rpm<uint64_t>(native_transform + oxorany(OFF_NATIVE_TRANSFORM_DATA));
        int index = rpm<int>(native_transform + oxorany(OFF_NATIVE_TRANSFORM_INDEX));

        if (transform_position_from_data(transform_data, index, out)) return true;

        uint64_t nested_data = likely_ptr(transform_data) ? rpm<uint64_t>(transform_data + oxorany(OFF_TRANSFORM_DATA_ARRAY)) : 0;
        if (transform_position_from_data(nested_data, index, out)) return true;

        Vector3 direct = rpm<Vector3>(native_transform + oxorany(OFF_NATIVE_TRANSFORM_DIRECT));
        if (sane_world_pos(direct)) {
            out = direct;
            return true;
        }

        return false;
    }

    inline Vector3 position(uint64_t p) noexcept {
        uint64_t MovementController = rpm<uint64_t>(p + oxorany(OFF_PLAYER_MOVEMENT_CTRL));
        if (!MovementController) return Vector3(0, 0, 0);

        uint64_t TransformData = rpm<uint64_t>(MovementController + oxorany(OFF_MC_TRANSFORM_DATA));
        if (!TransformData) return Vector3(0, 0, 0);

        return rpm<Vector3>(TransformData + oxorany(OFF_TD_POSITION));
    }

    inline uint64_t biped_map(uint64_t p) noexcept {
        uint64_t view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_1));
        if (likely_ptr(view)) {
            uint64_t map = rpm<uint64_t>(view + oxorany(OFF_VIEW_BIPED_MAP));
            if (likely_ptr(map)) return map;
        }

        view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_2));
        if (likely_ptr(view)) {
            uint64_t map = rpm<uint64_t>(view + oxorany(OFF_VIEW_BIPED_MAP));
            if (likely_ptr(map)) return map;
        }

        return 0;
    }

    inline bool read_biped_bone(uint64_t map, uint64_t offset, const Vector3& base, Vector3& out) noexcept {
        if (!likely_ptr(map)) return false;

        uint64_t transform = rpm<uint64_t>(map + offset);
        if (!likely_ptr(transform)) return false;

        Vector3 pos{};
        if (!transform_position(transform, pos)) return false;
        if (!sane_bone_pos(pos, base)) return false;

        out = pos;
        return true;
    }

    inline bool bone_position(uint64_t p, int bone_mode, const Vector3& base, Vector3& out) noexcept {
        uint64_t map = biped_map(p);
        if (!likely_ptr(map)) return false;

        if (bone_mode == 1) {
            return read_biped_bone(map, oxorany(OFF_BONE_HEAD_1), base, out) ||
                   read_biped_bone(map, oxorany(OFF_BONE_HEAD_2), base, out);
        }

        if (bone_mode == 2) {
            return read_biped_bone(map, oxorany(OFF_BONE_CHEST_1), base, out) ||
                   read_biped_bone(map, oxorany(OFF_BONE_CHEST_2), base, out) ||
                   read_biped_bone(map, oxorany(OFF_BONE_CHEST_3), base, out) ||
                   read_biped_bone(map, oxorany(OFF_BONE_CHEST_4), base, out);
        }

        return read_biped_bone(map, oxorany(OFF_BONE_HEAD_2), base, out) ||
               read_biped_bone(map, oxorany(OFF_BONE_HEAD_1), base, out);
    }

    inline bool bone_position(uint64_t p, int bone_mode, Vector3& out) noexcept {
        return bone_position(p, bone_mode, position(p), out);
    }

    // Welodium is_visible: primary — occlusion (visibilityState==2 && occlusionState!=1),
    // fallback — view+0x30 bool. Строже чем старый visibility_state (нет false-positive от next==2).
    inline bool is_visible(uint64_t p) noexcept {
        if (!p) return false;

        uint64_t occlusion = rpm<uint64_t>(p + oxorany(OFF_PLAYER_OCCLUSION));
        if (likely_ptr(occlusion)) {
            int visState = rpm<int>(occlusion + oxorany(OFF_OCCLUSION_CURRENT));
            int occState = rpm<int>(occlusion + oxorany(OFF_OCCLUSION_NEXT));
            return (visState == 2 && occState != 1);
        }

        // Fallback: view+0x30 bool (Welodium)
        uint64_t view = rpm<uint64_t>(p + oxorany(OFF_PLAYER_VIEW_1));
        if (likely_ptr(view)) {
            return rpm<bool>(view + oxorany(0x30));
        }

        return false;
    }

    // Совместимость с вызовами trigger_visible_only в combat.cpp
    inline int visibility_state(uint64_t p) noexcept {
        return is_visible(p) ? 2 : 0;
    }

    inline uint64_t photon_ptr(uint64_t p) noexcept {
        return rpm<uint64_t>(p + oxorany(OFF_PLAYER_PHOTON_PTR));
    }

    template<typename T>
    inline T property(uint64_t p, const char* tag) noexcept {
        T result{};
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer) return result;

        uint64_t PropertiesRegistry = rpm<uint64_t>(PhotonPlayer + oxorany(OFF_PHOTON_PROPS_REG));
        if (!PropertiesRegistry) return result;

        int Count = rpm<int>(PropertiesRegistry + oxorany(OFF_PROPS_COUNT));
        uint64_t PropertiesList = rpm<uint64_t>(PropertiesRegistry + oxorany(OFF_PROPS_LIST));

        for (int i = 0; i < Count; i++) {
            uint64_t Key = rpm<uint64_t>(PropertiesList + oxorany(OFF_PROPS_KEY_BASE) + oxorany(OFF_LIST_ENTRY_STRIDE) * i);
            uint64_t Value = rpm<uint64_t>(PropertiesList + oxorany(OFF_PROPS_VAL_BASE) + oxorany(OFF_LIST_ENTRY_STRIDE) * i);

            if (!Key) continue;

            std::string KeyString = rpm<read_string>(Key).as_utf8();
            if (strstr(KeyString.c_str(), tag)) {
                result = rpm<T>(Value + oxorany(OFF_PROPS_VALUE_DATA));
                break;
            }
        }

        return result;
    }

    inline int health(uint64_t p) noexcept {
        return property<int>(p, oxorany("health"));
    }
    
    inline int armor(uint64_t p) noexcept {
    return property<int>(p, oxorany("armor"));
    }
    
    inline int ping(uint64_t p) noexcept {
    return property<int>(p, oxorany("ping"));
    }

    inline read_string name(uint64_t p) noexcept {
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer) return {};
        return rpm<read_string>(rpm<uint64_t>(PhotonPlayer + oxorany(OFF_PHOTON_NAME)));
    }

    inline matrix view_matrix(uint64_t p) noexcept {
        uint64_t PlayerMainCamera = rpm<uint64_t>(p + oxorany(OFF_PLAYER_MAIN_CAMERA));
        if (!PlayerMainCamera) return {};

        uint64_t CameraTransform = rpm<uint64_t>(PlayerMainCamera + oxorany(OFF_CAM_TRANSFORM));
        if (!CameraTransform) return {};

        uint64_t CameraMatrix = rpm<uint64_t>(CameraTransform + oxorany(OFF_CAM_TRANSFORM_MATRIX));
        if (!CameraMatrix) return {};

        return rpm<matrix>(CameraMatrix + oxorany(OFF_CAM_MATRIX_DATA));
    }

    // ── Bones (перенесено из Welo) ────────────────────────────────────────────
    struct bones_t {
        Vector3 head;
        Vector3 neck;
        Vector3 spine;
        Vector3 spine1;
        Vector3 spine2;
        Vector3 l_shoulder;
        Vector3 l_arm;
        Vector3 l_forearm;
        Vector3 l_hand;
        Vector3 r_shoulder;
        Vector3 r_arm;
        Vector3 r_forearm;
        Vector3 r_hand;
        Vector3 pelvis;
        Vector3 l_thigh;
        Vector3 l_knee;
        Vector3 l_foot;
        Vector3 l_toe;
        Vector3 r_thigh;
        Vector3 r_knee;
        Vector3 r_foot;
        Vector3 r_toe;

        Vector3& operator[](int i) noexcept {
            return ((Vector3*)this)[i];
        }
    };

    // Применяет дефолтную T-позу относительно root
    inline void apply_default_pose(bones_t& b, const Vector3& root) noexcept {
        b.head      = root + Vector3(0,     1.75f, 0);
        b.neck      = root + Vector3(0,     1.6f,  0);
        b.spine2    = root + Vector3(0,     1.45f, 0);
        b.spine1    = root + Vector3(0,     1.25f, 0);
        b.spine     = root + Vector3(0,     1.05f, 0);
        b.pelvis    = root + Vector3(0,     0.9f,  0);
        b.l_shoulder= root + Vector3(-0.2f, 1.55f, 0);
        b.l_arm     = root + Vector3(-0.4f, 1.5f,  0);
        b.l_forearm = root + Vector3(-0.4f, 1.2f,  0);
        b.l_hand    = root + Vector3(-0.4f, 1.0f,  0);
        b.r_shoulder= root + Vector3(0.2f,  1.55f, 0);
        b.r_arm     = root + Vector3(0.4f,  1.5f,  0);
        b.r_forearm = root + Vector3(0.4f,  1.2f,  0);
        b.r_hand    = root + Vector3(0.4f,  1.0f,  0);
        b.l_thigh   = root + Vector3(-0.15f,0.9f,  0);
        b.l_knee    = root + Vector3(-0.15f,0.45f, 0);
        b.l_foot    = root + Vector3(-0.15f,0.05f, 0);
        b.l_toe     = root + Vector3(-0.15f,0.0f,  0.1f);
        b.r_thigh   = root + Vector3(0.15f, 0.9f,  0);
        b.r_knee    = root + Vector3(0.15f, 0.45f, 0);
        b.r_foot    = root + Vector3(0.15f, 0.05f, 0);
        b.r_toe     = root + Vector3(0.15f, 0.0f,  0.1f);
    }

    // Читает мировую позицию кости через hierarchical TransformEntry-цепочку
    inline Vector3 get_bone_world_pos(
        const std::vector<TransformEntry>& matrices,
        const std::vector<int>& parents,
        uint32_t index) noexcept
    {
        uint32_t count = (uint32_t)matrices.size();
        if (index >= count) return {0,0,0};

        const TransformEntry& tm = matrices[index];
        Vector3 result = {tm.position.x, tm.position.y, tm.position.z};
        int p_idx = parents[index];
        int depth = 0;

        while (p_idx >= 0 && p_idx < (int)count && depth < 64) {
            const TransformEntry& p_tm = matrices[p_idx];
            float rx = p_tm.rotation.x, ry = p_tm.rotation.y,
                  rz = p_tm.rotation.z, rw = p_tm.rotation.w;
            float sx = result.x * p_tm.scale.x;
            float sy = result.y * p_tm.scale.y;
            float sz = result.z * p_tm.scale.z;

            result.x = p_tm.position.x + sx
                + sx * (ry*ry*-2.f - rz*rz*2.f)
                + sy * (rw*rz*-2.f - ry*rx*-2.f)
                + sz * (rz*rx*2.f  - rw*ry*-2.f);
            result.y = p_tm.position.y + sy
                + sx * (rx*ry*2.f  - rw*rz*-2.f)
                + sy * (rz*rz*-2.f - rx*rx*2.f)
                + sz * (rw*rx*-2.f - rz*ry*-2.f);
            result.z = p_tm.position.z + sz
                + sx * (rw*ry*-2.f - rx*rz*-2.f)
                + sy * (ry*rz*2.f  - rw*rx*-2.f)
                + sz * (rx*rx*-2.f - ry*ry*2.f);

            p_idx = parents[p_idx];
            depth++;
        }
        return result;
    }

    // Полный get_bones с кешем и fallback на дефолтную позу
    // Требует <vector> и <unordered_map> — уже подключены выше
    struct bone_cache_entry_t {
        Vector3 offsets[OFF_BIPED_MAP_BONE_COUNT];
        bool valid = false;
    };
    inline std::unordered_map<uint64_t, bone_cache_entry_t> g_bone_cache;
    inline uint64_t g_bone_cache_pm = 0;

    inline bool get_bones(uint64_t p, bones_t& b) noexcept {
        uint64_t cur_pm = get_player_manager();
        if (cur_pm != g_bone_cache_pm) {
            g_bone_cache.clear();
            g_bone_cache_pm = cur_pm;
        }

        Vector3 root = position(p);
        if (root.x == 0.f && root.y == 0.f && root.z == 0.f) return false;

        auto fallback = [&]() -> bool {
            auto it = g_bone_cache.find(p);
            if (it != g_bone_cache.end() && it->second.valid) {
                for (int i = 0; i < OFF_BIPED_MAP_BONE_COUNT; i++)
                    b[i] = root + it->second.offsets[i];
                return true;
            }
            apply_default_pose(b, root);
            return true;
        };

        // Welodium: только строгий is_visible открывает полный read костей.
        // Невидимый игрок → кеш или дефолтная поза (кости не читаем).
        if (!is_visible(p)) return fallback();

        uint64_t map = biped_map(p);
        if (!likely_ptr(map)) return fallback();

        uint64_t ptrs[OFF_BIPED_MAP_BONE_COUNT];
        if (!mem_read(map + oxorany(OFF_BIPED_MAP_PTRS_BASE),
                      ptrs, sizeof(ptrs))) return fallback();

        uint32_t transform_indices[OFF_BIPED_MAP_BONE_COUNT];
        uint64_t matrix_list = 0, matrix_indices_ptr = 0;
        uint32_t max_index = 0;

        for (int i = 0; i < OFF_BIPED_MAP_BONE_COUNT; i++) {
            transform_indices[i] = 0xFFFFFFFF;
            if (!likely_ptr(ptrs[i])) continue;
            uint64_t native = rpm<uint64_t>(
                ptrs[i] + oxorany(OFF_TRANSFORM_NATIVE));
            if (!likely_ptr(native)) continue;
            uint32_t idx = rpm<uint32_t>(native + oxorany(BONE_TRANSFORM_IDX));
            transform_indices[i] = idx;
            if (idx > max_index && idx < 10000) max_index = idx;
            if (!matrix_list) {
                uint64_t mdata = rpm<uint64_t>(native + oxorany(BONE_MATRIX_DATA));
                if (likely_ptr(mdata)) {
                    matrix_list        = rpm<uint64_t>(mdata + oxorany(BONE_MATRIX_LIST));
                    matrix_indices_ptr = rpm<uint64_t>(mdata + oxorany(BONE_MATRIX_INDICES));
                }
            }
        }

        if (!matrix_list || !matrix_indices_ptr || max_index == 0) return fallback();

        uint32_t count = max_index + 1;
        if (count > 10000) return fallback();

        std::vector<TransformEntry> all_matrices(count);
        std::vector<int>            all_parents(count);
        if (!mem_read(matrix_list,        all_matrices.data(), count * sizeof(TransformEntry))) return fallback();
        if (!mem_read(matrix_indices_ptr, all_parents.data(),  count * sizeof(int)))            return fallback();

        bool any_valid = false;
        for (int i = 0; i < OFF_BIPED_MAP_BONE_COUNT; i++) {
            if (transform_indices[i] != 0xFFFFFFFF) {
                b[i] = get_bone_world_pos(all_matrices, all_parents, transform_indices[i]);
                if (b[i].x != 0.f || b[i].y != 0.f) any_valid = true;
            } else {
                b[i] = {0, 0, 0};
            }
        }

        if (any_valid) {
            float d = (b[1] - root).magnitude();
            if (d > 0.1f && d < 3.0f) {
                auto& cache = g_bone_cache[p];
                for (int i = 0; i < OFF_BIPED_MAP_BONE_COUNT; i++)
                    cache.offsets[i] = b[i] - root;
                cache.valid = true;
                return true;
            }
        }
        return fallback();
    }

    // Позиция камеры (Welodium-логика: p+0x28 как прямой transform → main_camera chain → position)
    inline Vector3 get_transform_position_full(uint64_t transform) noexcept {
        if (!likely_ptr(transform)) return {0,0,0};
        uint64_t native = rpm<uint64_t>(transform + oxorany(OFF_TRANSFORM_NATIVE));
        if (!likely_ptr(native)) return {0,0,0};
        uint64_t mdata = rpm<uint64_t>(native + oxorany(BONE_MATRIX_DATA));
        if (!likely_ptr(mdata)) return {0,0,0};
        uint64_t mlist = rpm<uint64_t>(mdata + oxorany(BONE_MATRIX_LIST));
        uint64_t midx  = rpm<uint64_t>(mdata + oxorany(BONE_MATRIX_INDICES));
        if (!likely_ptr(mlist) || !likely_ptr(midx)) return {0,0,0};
        uint32_t idx = rpm<uint32_t>(native + oxorany(BONE_TRANSFORM_IDX));
        if (idx >= 10000) return {0,0,0};
        uint32_t count = idx + 1;
        std::vector<TransformEntry> mats(count);
        std::vector<int>            pars(count);
        if (!mem_read(mlist, mats.data(), count * sizeof(TransformEntry))) return {0,0,0};
        if (!mem_read(midx,  pars.data(), count * sizeof(int)))            return {0,0,0};
        return get_bone_world_pos(mats, pars, idx);
    }

    inline Vector3 camera_position(uint64_t p) noexcept {
        if (!p) return {0,0,0};

        // Путь 1 (Welodium): p+0x28 как прямой transform-ptr
        {
            uint64_t transform = rpm<uint64_t>(p + oxorany(0x28));
            if (likely_ptr(transform)) {
                Vector3 r = get_transform_position_full(transform);
                if (sane_world_pos(r)) return r;
            }
        }

        // Путь 2: main_camera → transform chain
        {
            uint64_t cam = rpm<uint64_t>(p + oxorany(OFF_PLAYER_MAIN_CAMERA));
            if (likely_ptr(cam)) {
                uint64_t cam_transform = rpm<uint64_t>(cam + oxorany(OFF_CAM_TRANSFORM));
                if (likely_ptr(cam_transform)) {
                    Vector3 r = get_transform_position_full(cam_transform);
                    if (sane_world_pos(r)) return r;
                }
            }
        }

        return position(p);
    }
    inline int platform(uint64_t p) noexcept {
    return property<int>(p, oxorany("pl"));
    }
}
