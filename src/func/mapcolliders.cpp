#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"
#include "maps_embedded.h"

#include "map.h"
#include "../game/game.hpp"
#include "imgui.h"

#include <sys/stat.h>
#include <errno.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <unordered_set>
#include <cctype>
#include <android/log.h>

#define MAP_LOG(fmt, ...) \
    __android_log_print(ANDROID_LOG_DEBUG, "map_service", fmt, ##__VA_ARGS__)

namespace ms = map_service_internal;

static const char* s_map_names[] = {
    "sandstone_colliders.glb",   "province_colliders.glb",  "dune_colliders.glb",
    "rust_colliders.glb",        "zone7_colliders.glb",     "breeze_colliders.glb",
    "hanami_colliders.glb",      "calypso_colliders.glb",   "hanari_colliders.glb",
    "favelas_colliders.glb",     "lakeside_colliders.glb",  "arena_colliders.glb",
    "cableway_colliders.glb",    "pipeline_colliders.glb",  "bridge.glb",
    "yard.glb",                  "block_colliders.glb",     "perimeter_colliders.glb",
    "pool_colliders.glb",        "prison_colliders.glb",    "sandyards_colliders.glb",
    "temple_colliders.glb",      "trainingoutside_colliders.glb", "village_colliders.glb",
};
static const char* s_display_names[] = {
    "sandstone", "province",  "dune",     "rust",
    "zone7",     "breeze",    "hanami",   "calypso",
    "hanari",    "favelas",   "lakeside", "arena",
    "cableway",  "pipeline",  "bridge",   "yard",
    "block",     "perimeter", "pool",     "prison",
    "sandyards", "temple",    "training", "village",
};
static constexpr int EMBEDDED_COUNT = 24;

const char* map_service::g_collider_map_names[]        = {
    "sandstone_colliders.glb",   "province_colliders.glb",     "dune_colliders.glb",
    "rust_colliders.glb",        "zone7_colliders.glb",        "breeze_colliders.glb",
    "hanami_colliders.glb",      "calypso_colliders.glb",      "hanari_colliders.glb",
    "favelas_colliders.glb",     "lakeside_colliders.glb",     "arena_colliders.glb",
    "cableway_colliders.glb",    "pipeline_colliders.glb",     "bridge.glb",
    "yard.glb",                  "block_colliders.glb",        "perimeter_colliders.glb",
    "pool_colliders.glb",        "prison_colliders.glb",       "sandyards_colliders.glb",
    "temple_colliders.glb",      "trainingoutside_colliders.glb", "village_colliders.glb",
};
const char* map_service::g_collider_map_display_names[] = {
    "sandstone", "province",  "dune",     "rust",
    "zone7",     "breeze",    "hanami",   "calypso",
    "hanari",    "favelas",   "lakeside", "arena",
    "cableway",  "pipeline",  "bridge",   "yard",
    "block",     "perimeter", "pool",     "prison",
    "sandyards", "temple",    "training", "village",
};

int map_service::g_collider_map_count =
    (int)(sizeof(s_map_names) / sizeof(s_map_names[0])); // 24
int  map_service::g_collider_map_index = 0;
bool map_service::g_enabled  = false;
unsigned int map_service::g_color = 0xFFFFFFFFu;
bool map_service::g_loaded   = false;
MapConfig map_service::g_config;
MapVector3 map_service::g_cam_pos = MapVector3(0, 0, 0);
float map_service::g_max_dist = 80.f;

static std::vector<MapTriangle>   s_rawTriangles;
static std::vector<BakedTriangle> s_bakedTriangles;
static std::unordered_map<long long, std::vector<int>> s_grid;
static std::mutex s_mutex;

static MapVector3 s_lastScale, s_lastRot, s_lastPos;
static bool s_lastFlipX = false, s_lastFlipY = false, s_lastFlipZ = false;

static const float CellSize = 5.0f;

static float DegToRad(float deg) {
    return deg * (3.14159265358979323846f / 180.0f);
}

static MapQuaternion CreateFromYawPitchRoll(float yaw, float pitch, float roll) {
    float cy = cosf(yaw   * 0.5f), sy = sinf(yaw   * 0.5f);
    float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    float cr = cosf(roll  * 0.5f), sr = sinf(roll  * 0.5f);
    MapQuaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = cr * sp * cy + sr * cp * sy;
    q.y = cr * cp * sy - sr * sp * cy;
    q.z = sr * cp * cy - cr * sp * sy;
    return q;
}

static MapVector3 TransformPoint(MapVector3 p, MapQuaternion q) {
    if (map_service::g_config.flipX) p.x = -p.x;
    if (map_service::g_config.flipY) p.y = -p.y;
    if (map_service::g_config.flipZ) p.z = -p.z;
    p.x *= map_service::g_config.mapScale.x;
    p.y *= map_service::g_config.mapScale.y;
    p.z *= map_service::g_config.mapScale.z;
    p = q * p;
    p.x += map_service::g_config.mapOffset.x;
    p.y += map_service::g_config.mapOffset.y;
    p.z += map_service::g_config.mapOffset.z;
    return p;
}

static long long GetGridHash(int x, int y, int z) {
    return ((long long)x * 73856093) ^ ((long long)y * 19349663) ^ ((long long)z * 83492791);
}

static void BakeMap() {
    s_bakedTriangles.clear();
    s_grid.clear();
    s_lastScale  = map_service::g_config.mapScale;
    s_lastRot    = map_service::g_config.mapRotation;
    s_lastPos    = map_service::g_config.mapOffset;
    s_lastFlipX  = map_service::g_config.flipX;
    s_lastFlipY  = map_service::g_config.flipY;
    s_lastFlipZ  = map_service::g_config.flipZ;

    MapQuaternion rotation = CreateFromYawPitchRoll(
        DegToRad(map_service::g_config.mapRotation.y),
        DegToRad(map_service::g_config.mapRotation.x),
        DegToRad(map_service::g_config.mapRotation.z)
    );

    s_bakedTriangles.reserve(s_rawTriangles.size());
    for (size_t i = 0; i < s_rawTriangles.size(); i++) {
        const MapTriangle& raw = s_rawTriangles[i];
        MapVector3 v1 = TransformPoint(raw.v1, rotation);
        MapVector3 v2 = TransformPoint(raw.v2, rotation);
        MapVector3 v3 = TransformPoint(raw.v3, rotation);
        MapVector3 minV(
            (std::min)({v1.x, v2.x, v3.x}),
            (std::min)({v1.y, v2.y, v3.y}),
            (std::min)({v1.z, v2.z, v3.z})
        );
        MapVector3 maxV(
            (std::max)({v1.x, v2.x, v3.x}),
            (std::max)({v1.y, v2.y, v3.y}),
            (std::max)({v1.z, v2.z, v3.z})
        );
        BakedTriangle bt;
        bt.v1 = v1; bt.v2 = v2; bt.v3 = v3;
        bt.min = minV; bt.max = maxV;
        bt.materialName = raw.materialName;
        s_bakedTriangles.push_back(bt);

        int minX = (int)floorf(minV.x / CellSize), maxX = (int)floorf(maxV.x / CellSize);
        int minY = (int)floorf(minV.y / CellSize), maxY = (int)floorf(maxV.y / CellSize);
        int minZ = (int)floorf(minV.z / CellSize), maxZ = (int)floorf(maxV.z / CellSize);
        for (int x = minX; x <= maxX; x++)
            for (int y = minY; y <= maxY; y++)
                for (int z = minZ; z <= maxZ; z++)
                    s_grid[GetGridHash(x, y, z)].push_back((int)i);
    }
}

static void ExtractMeshes(const tinygltf::Model& model) {
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) continue;
            auto itPos = primitive.attributes.find("POSITION");
            if (itPos == primitive.attributes.end()) continue;

            std::string matName;
            if (primitive.material >= 0 &&
                primitive.material < (int)model.materials.size())
                matName = model.materials[primitive.material].name;
            if (!mesh.name.empty())
                matName = mesh.name + (matName.empty() ? "" : " " + matName);

            const tinygltf::Accessor&   posAccessor = model.accessors[itPos->second];
            const tinygltf::BufferView& posView     = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer&     posBuffer   = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(
                &posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

            auto pushTri = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
                MapTriangle tri;
                tri.v1 = MapVector3(positions[i0*3], positions[i0*3+1], positions[i0*3+2]);
                tri.v2 = MapVector3(positions[i1*3], positions[i1*3+1], positions[i1*3+2]);
                tri.v3 = MapVector3(positions[i2*3], positions[i2*3+1], positions[i2*3+2]);
                tri.materialName = matName;
                s_rawTriangles.push_back(tri);
            };

            if (primitive.indices >= 0) {
                const tinygltf::Accessor&   ia = model.accessors[primitive.indices];
                const tinygltf::BufferView& iv = model.bufferViews[ia.bufferView];
                const tinygltf::Buffer&     ib = model.buffers[iv.buffer];
                const size_t idxOff = iv.byteOffset + ia.byteOffset;

                if (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* idx = reinterpret_cast<const uint16_t*>(&ib.data[idxOff]);
                    for (size_t i = 0; i + 2 < ia.count; i += 3)
                        pushTri(idx[i], idx[i+1], idx[i+2]);
                } else if (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* idx = reinterpret_cast<const uint32_t*>(&ib.data[idxOff]);
                    for (size_t i = 0; i + 2 < ia.count; i += 3)
                        pushTri(idx[i], idx[i+1], idx[i+2]);
                }
            } else {
                for (size_t i = 0; i + 2 < posAccessor.count; i += 3)
                    pushTri((uint32_t)i, (uint32_t)(i+1), (uint32_t)(i+2));
            }
        }
    }
}
bool map_service::LoadMap(const std::string& filename) {
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, filename))
        return false;

    std::lock_guard<std::mutex> lock(s_mutex);
    s_rawTriangles.clear();
    ExtractMeshes(model);
    BakeMap();
    g_loaded = true;
    return true;
}

bool map_service::LoadMapFromMemory(const unsigned char* data, size_t size) {
    if (!data || size == 0 || size > 0x7FFFFFFFu) return false;
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromMemory(&model, &err, &warn, data, (unsigned int)size, ""))
        return false;

    std::lock_guard<std::mutex> lock(s_mutex);
    s_rawTriangles.clear();
    ExtractMeshes(model);
    BakeMap();
    g_loaded = true;
    return true;
}

static void ensure_maps_dir() {
    static const char* const k_dirs[] = {
        "/sdcard/NexuraWare",
        "/sdcard/NexuraWare/maps",
    };
    for (int i = 0; i < 2; i++) {
        if (mkdir(k_dirs[i], 0755) != 0 && errno != EEXIST) {
            MAP_LOG("[-] map_service: mkdir %s failed errno=%d", k_dirs[i], errno);
        }
    }
}

void map_service::LoadColliderMapByIndex(int index) {
    static bool s_dirs_ensured = false;
    if (!s_dirs_ensured) {
        ensure_maps_dir();
        // Extract every embedded map that is missing in /sdcard/NexuraWare/maps/
        int n = embedded_maps::ExtractAll("/sdcard/NexuraWare/maps");
        if (n > 0) MAP_LOG("[+] map_service: extracted %d embedded maps", n);
        s_dirs_ensured = true;
    }
    if (index < 0 || index >= g_collider_map_count) return;

    g_loaded = false;

    const char* name = g_collider_map_names[index];
    std::string path = std::string("/sdcard/NexuraWare/maps/") + name;

    if (LoadMap(path)) {
        MAP_LOG("[+] map_service: loaded %s from sdcard", name);
        return;
    }

    // Fallback: the file is missing/broken on disk — load straight from the
    // embedded resources and re-extract the maps folder for next time.
    unsigned char* data = nullptr;
    size_t size = embedded_maps::Decompress(index, &data);
    if (data && size > 0 && LoadMapFromMemory(data, size)) {
        MAP_LOG("[+] map_service: loaded %s from embedded resources", name);
        delete[] data;
        embedded_maps::ExtractAll("/sdcard/NexuraWare/maps");
        return;
    }
    delete[] data;

    MAP_LOG("[-] map_service: failed to load %s", name);
}

void map_service::Update() {
    if (!g_loaded) return;
    bool changed =
        s_lastScale.x != g_config.mapScale.x    ||
        s_lastScale.y != g_config.mapScale.y    ||
        s_lastScale.z != g_config.mapScale.z    ||
        s_lastRot.x   != g_config.mapRotation.x ||
        s_lastRot.y   != g_config.mapRotation.y ||
        s_lastRot.z   != g_config.mapRotation.z ||
        s_lastPos.x   != g_config.mapOffset.x   ||
        s_lastPos.y   != g_config.mapOffset.y   ||
        s_lastPos.z   != g_config.mapOffset.z   ||
        s_lastFlipX   != g_config.flipX         ||
        s_lastFlipY   != g_config.flipY         ||
        s_lastFlipZ   != g_config.flipZ;
    if (changed) {
        std::lock_guard<std::mutex> lock(s_mutex);
        BakeMap();
    }
}

static bool world_to_screen_ms(const matrix& m, float sw, float sh,
                                const MapVector3& pos, ImVec2& out) {
    float sx = m.m11 * pos.x + m.m21 * pos.y + m.m31 * pos.z + m.m41;
    float sy = m.m12 * pos.x + m.m22 * pos.y + m.m32 * pos.z + m.m42;
    float sw_ = m.m14 * pos.x + m.m24 * pos.y + m.m34 * pos.z + m.m44;
    if (sw_ <= 0.0001f) return false;
    float iw = 1.0f / sw_;
    out.x = (sx * iw + 1.0f) * 0.5f * sw;
    out.y = (1.0f - sy * iw) * 0.5f * sh;
    return true;
}

static bool RayHitAABB(const MapVector3& origin, const MapVector3& dir,
                        const MapVector3& boxMin, const MapVector3& boxMax, float maxT) {
    float tmin = 0.0f, tmax = maxT;
    const float* o  = &origin.x;
    const float* d  = &dir.x;
    const float* mn = &boxMin.x;
    const float* mx = &boxMax.x;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(d[i]) < 1e-7f) {
            if (o[i] < mn[i] || o[i] > mx[i]) return false;
            continue;
        }
        float t1 = (mn[i] - o[i]) / d[i], t2 = (mx[i] - o[i]) / d[i];
        if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax || tmax < 0.0f) return false;
    }
    return tmin <= tmax && tmax >= 0.0f && tmin <= maxT;
}

static bool IntersectRayTriangle(const MapVector3& origin, const MapVector3& dir,
                                  const BakedTriangle& tri, float& t) {
    using namespace ms;
    const float EPSILON = 1e-7f;
    MapVector3 edge1 = tri.v2 - tri.v1;
    MapVector3 edge2 = tri.v3 - tri.v1;
    MapVector3 h     = Cross(dir, edge2);
    float      a     = Dot(edge1, h);
    if (a > -EPSILON && a < EPSILON) return false;
    float      f = 1.0f / a;
    MapVector3 s = origin - tri.v1;
    float      u = f * Dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;
    MapVector3 q = Cross(s, edge1);
    float      v = f * Dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;
    t = f * Dot(edge2, q);
    return t > EPSILON;
}


float map_service::RaycastWallWithMaterial(MapVector3 origin, MapVector3 dir,
                                            float maxDist, std::string* outMaterial) {
    if (outMaterial) outMaterial->clear();
    if (!g_loaded) return FLT_MAX;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_bakedTriangles.empty()) return FLT_MAX;
    float  minT      = FLT_MAX;
    size_t bestIndex = (size_t)-1;
    const float minWallDist = 0.4f;
    for (size_t i = 0; i < s_bakedTriangles.size(); i++) {
        const BakedTriangle& tri = s_bakedTriangles[i];
        if (!RayHitAABB(origin, dir, tri.min, tri.max, maxDist)) continue;
        float t;
        if (!IntersectRayTriangle(origin, dir, tri, t)) continue;
        if (t > minWallDist && t < maxDist && t < minT) { minT = t; bestIndex = i; }
    }
    if (outMaterial && bestIndex != (size_t)-1)
        *outMaterial = s_bakedTriangles[bestIndex].materialName;
    return minT;
}

float map_service::RaycastClosestWithName(MapVector3 origin, MapVector3 dir,
                                           float maxDist, std::string* outMaterial) {
    if (outMaterial) outMaterial->clear();
    if (!g_loaded) return FLT_MAX;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_bakedTriangles.empty()) return FLT_MAX;
    float  minT      = FLT_MAX;
    size_t bestIndex = (size_t)-1;
    const float minDist = 0.02f;
    for (size_t i = 0; i < s_bakedTriangles.size(); i++) {
        const BakedTriangle& tri = s_bakedTriangles[i];
        if (!RayHitAABB(origin, dir, tri.min, tri.max, maxDist)) continue;
        float t;
        if (!IntersectRayTriangle(origin, dir, tri, t)) continue;
        if (t <= minDist || t >= maxDist || t >= minT) continue;
        minT = t; bestIndex = i;
    }
    if (outMaterial && bestIndex != (size_t)-1)
        *outMaterial = s_bakedTriangles[bestIndex].materialName;
    return minT;
}

float map_service::RaycastWallExcluding(MapVector3 origin, MapVector3 dir,
                                         float maxDist,
                                         const std::vector<std::string>& excludeSubstrings) {
    if (!g_loaded) return FLT_MAX;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_bakedTriangles.empty()) return FLT_MAX;
    float minT = FLT_MAX;
    const float minWallDist = 0.4f;
    for (size_t i = 0; i < s_bakedTriangles.size(); i++) {
        const BakedTriangle& tri = s_bakedTriangles[i];
        if (!RayHitAABB(origin, dir, tri.min, tri.max, maxDist)) continue;
        float t;
        if (!IntersectRayTriangle(origin, dir, tri, t)) continue;
        if (t <= minWallDist || t >= maxDist || t >= minT) continue;
        std::string nameLower = tri.materialName;
        for (char& c : nameLower) c = (char)std::tolower((unsigned char)c);
        bool skip = false;
        for (const auto& sub : excludeSubstrings) {
            std::string subL = sub;
            for (char& c : subL) c = (char)std::tolower((unsigned char)c);
            if (nameLower.find(subL) != std::string::npos) { skip = true; break; }
        }
        if (!skip) minT = t;
    }
    return minT;
}

float map_service::RaycastWallClosest(MapVector3 origin, MapVector3 dir, float maxDist) {
    if (!g_loaded) return FLT_MAX;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_bakedTriangles.empty()) return FLT_MAX;
    float minT = FLT_MAX;
    const float minWallDist = 0.02f;
    for (size_t i = 0; i < s_bakedTriangles.size(); i++) {
        const BakedTriangle& tri = s_bakedTriangles[i];
        if (!RayHitAABB(origin, dir, tri.min, tri.max, maxDist)) continue;
        float t;
        if (!IntersectRayTriangle(origin, dir, tri, t)) continue;
        if (t <= minWallDist || t >= maxDist || t >= minT) continue;
        minT = t;
    }
    return minT;
}

float map_service::RaycastWallClosestWithNormal(MapVector3 origin, MapVector3 dir, float maxDist, MapVector3* outNormal) {
    if (!g_loaded) return FLT_MAX;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_bakedTriangles.empty()) return FLT_MAX;
    float minT = FLT_MAX;
    size_t bestIndex = (size_t)-1;
    const float minWallDist = 0.02f;
    for (size_t i = 0; i < s_bakedTriangles.size(); i++) {
        const BakedTriangle& tri = s_bakedTriangles[i];
        if (!RayHitAABB(origin, dir, tri.min, tri.max, maxDist)) continue;
        float t;
        if (!IntersectRayTriangle(origin, dir, tri, t)) continue;
        if (t <= minWallDist || t >= maxDist || t >= minT) continue;
        minT = t;
        bestIndex = i;
    }
    if (outNormal && bestIndex != (size_t)-1) {
        const BakedTriangle& tri = s_bakedTriangles[bestIndex];
        MapVector3 edge1 = tri.v2 - tri.v1;
        MapVector3 edge2 = tri.v3 - tri.v1;
        MapVector3 normal = ms::Cross(edge1, edge2);
        float len = sqrtf(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
        if (len > 1e-7f) {
            normal.x /= len;
            normal.y /= len;
            normal.z /= len;
        }
        *outNormal = normal;
    }

    return minT;
}

bool map_service::GetColliderAABB(const char* nameSubstring,
                                   MapVector3* outMin, MapVector3* outMax) {
    if (!nameSubstring || !outMin || !outMax || !g_loaded) return false;
    std::string sub(nameSubstring);
    for (char& c : sub) c = (char)std::tolower((unsigned char)c);
    std::lock_guard<std::mutex> lock(s_mutex);
    bool  found = false;
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
    for (const BakedTriangle& tri : s_bakedTriangles) {
        std::string nl = tri.materialName;
        for (char& c : nl) c = (char)std::tolower((unsigned char)c);
        if (nl.find(sub) == std::string::npos) continue;
        found = true;
        minX = (std::min)(minX, tri.min.x); minY = (std::min)(minY, tri.min.y); minZ = (std::min)(minZ, tri.min.z);
        maxX = (std::max)(maxX, tri.max.x); maxY = (std::max)(maxY, tri.max.y); maxZ = (std::max)(maxZ, tri.max.z);
    }
    if (!found) return false;
    outMin->x = minX; outMin->y = minY; outMin->z = minZ;
    outMax->x = maxX; outMax->y = maxY; outMax->z = maxZ;
    return true;
}

float map_service::RaycastColliderByName(MapVector3 origin, MapVector3 dir,
                                          float maxDist, const char* nameSubstring) {
    if (!nameSubstring || !g_loaded) return FLT_MAX;
    std::string sub(nameSubstring);
    for (char& c : sub) c = (char)std::tolower((unsigned char)c);
    std::lock_guard<std::mutex> lock(s_mutex);
    float minT = FLT_MAX;
    const float minDist = 0.02f;
    for (const BakedTriangle& tri : s_bakedTriangles) {
        std::string nl = tri.materialName;
        for (char& c : nl) c = (char)std::tolower((unsigned char)c);
        if (nl.find(sub) == std::string::npos) continue;
        if (!RayHitAABB(origin, dir, tri.min, tri.max, maxDist)) continue;
        float t;
        if (!IntersectRayTriangle(origin, dir, tri, t)) continue;
        if (t <= minDist || t >= maxDist || t >= minT) continue;
        minT = t;
    }
    return minT;
}
void map_service::RenderColliderHighlight(ImDrawList* draw_list, const matrix& vm,
                                           float screen_w, float screen_h,
                                           const char* nameSubstring, unsigned int color) {
    if (!draw_list || !nameSubstring || !g_loaded) return;
    std::string sub(nameSubstring);
    for (char& c : sub) c = (char)std::tolower((unsigned char)c);
    std::lock_guard<std::mutex> lock(s_mutex);
    for (const BakedTriangle& tri : s_bakedTriangles) {
        std::string nl = tri.materialName;
        for (char& c : nl) c = (char)std::tolower((unsigned char)c);
        if (nl.find(sub) == std::string::npos) continue;
        float cx = (tri.v1.x + tri.v2.x + tri.v3.x) * (1.f / 3.f);
        float cy = (tri.v1.y + tri.v2.y + tri.v3.y) * (1.f / 3.f);
        float cz = (tri.v1.z + tri.v2.z + tri.v3.z) * (1.f / 3.f);
        float w  = vm.m14*cx + vm.m24*cy + vm.m34*cz + vm.m44;
        if (w < 0.05f) continue;
        ImVec2 s1, s2, s3;
        if (world_to_screen_ms(vm, screen_w, screen_h, tri.v1, s1) &&
            world_to_screen_ms(vm, screen_w, screen_h, tri.v2, s2) &&
            world_to_screen_ms(vm, screen_w, screen_h, tri.v3, s3)) {
            draw_list->AddLine(s1, s2, (ImU32)color);
            draw_list->AddLine(s2, s3, (ImU32)color);
            draw_list->AddLine(s3, s1, (ImU32)color);
        }
    }
}
void map_service::Render(ImDrawList* draw_list, const matrix& vm,
                          float screen_w, float screen_h,
                          const std::string* filterMaterialName) {
    if (!g_enabled || !g_loaded || !draw_list) return;
    std::lock_guard<std::mutex> lock(s_mutex);
    ImU32 col       = (ImU32)g_color;
    const bool filt = (filterMaterialName && !filterMaterialName->empty());

    const float slack = 200.f;
    const float xmin = -slack, xmax = screen_w + slack;
    const float ymin = -slack, ymax = screen_h + slack;

    const float md2 = g_max_dist * g_max_dist;

    for (const BakedTriangle& tri : s_bakedTriangles) {
        if (filt && tri.materialName != *filterMaterialName) continue;

        float cx = (tri.v1.x + tri.v2.x + tri.v3.x) * (1.f / 3.f);
        float cy = (tri.v1.y + tri.v2.y + tri.v3.y) * (1.f / 3.f);
        float cz = (tri.v1.z + tri.v2.z + tri.v3.z) * (1.f / 3.f);

        {
            float dx = cx - g_cam_pos.x;
            float dy = cy - g_cam_pos.y;
            float dz = cz - g_cam_pos.z;
            if (dx*dx + dy*dy + dz*dz > md2) continue;
        }

        float w  = vm.m14*cx + vm.m24*cy + vm.m34*cz + vm.m44;
        if (w < 0.05f) continue;

        ImVec2 s1, s2, s3;
        if (!world_to_screen_ms(vm, screen_w, screen_h, tri.v1, s1)) continue;
        if (!world_to_screen_ms(vm, screen_w, screen_h, tri.v2, s2)) continue;
        if (!world_to_screen_ms(vm, screen_w, screen_h, tri.v3, s3)) continue;

        bool any_on = (s1.x > xmin && s1.x < xmax && s1.y > ymin && s1.y < ymax) ||
                      (s2.x > xmin && s2.x < xmax && s2.y > ymin && s2.y < ymax) ||
                      (s3.x > xmin && s3.x < xmax && s3.y > ymin && s3.y < ymax);
        if (!any_on) continue;
        ImVec2 pts[3] = { s1, s2, s3 };
        draw_list->AddPolyline(pts, 3, col, ImDrawFlags_Closed, 1.0f);
    }
}
