#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace lilies {
constexpr unsigned FlowerModel = 44, PadModel = 45;
constexpr unsigned CandidateCount = 40, MaximumPads = 32;
constexpr int64_t Dots = 65536;
struct Candidate {
    int64_t x = 0, y = 0;
    float scale = .06f, yaw = 0.f;
    unsigned model = PadModel;
    bool enabled = false;
};
inline uint64_t mix(uint64_t n) {
    n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ull;
    n = (n ^ (n >> 27)) * 0x94d049bb133111ebull;
    return n ^ (n >> 31);
}
inline std::array<Candidate, CandidateCount> candidates(uint32_t seed, int zx, int zy,
                                                        unsigned density, unsigned flowers) {
    std::array<Candidate, CandidateCount> result{};
    auto zone = mix(seed ^ mix(uint32_t(zx)) ^ (mix(uint32_t(zy)) << 1));
    for (unsigned patch = 0; patch < 4; ++patch) {
        auto key = mix(zone ^ (uint64_t(patch) + 1) * 0x9e3779b97f4a7c15ull);
        bool enabled = key % 100 < std::min(density, 100u);
        // Four loose clusters, distributed across the chunk rather than a grid
        // of individual pads. All anchors stay in the owner's inner 62 blocks.
        int cx = int((patch & 1) * 32) + 10 + int((key >> 8) % 12);
        int cy = int((patch >> 1) * 32) + 10 + int((key >> 20) % 12);
        unsigned members = 6 + unsigned((key >> 32) % 5);
        for (unsigned member = 0; member < 10; ++member) {
            auto roll = mix(key ^ mix(member + 1));
            auto &p = result[patch * 10 + member];
            int64_t dx = int64_t((roll >> 8) % (12 * Dots)) - 6 * Dots;
            int64_t dy = int64_t((roll >> 32) % (12 * Dots)) - 6 * Dots;
            p.x = (int64_t(zx) * 64 + cx) * Dots + dx;
            p.y = (int64_t(zy) * 64 + cy) * Dots + dy;
            p.scale = .055f + float((roll >> 52) & 255) / 255.f * .020f;
            p.yaw = float((roll >> 16) % 360);
            p.model = roll % 100 < std::min(flowers, 100u) ? FlowerModel : PadModel;
            p.enabled = enabled && member < members;
        }
    }
    return result;
}
inline int64_t block(int64_t dots) {
    return dots / Dots - (dots < 0 && dots % Dots != 0);
}
inline bool inlandBiome(int biome) {
    return biome >= 0 && biome <= 13 && biome != 1 && biome != 6 && biome != 7 && biome != 11 &&
           biome != 12 && biome != 13;
}
template <class Type>
inline bool findSurface(int ground, unsigned maximumDepth, int &waterTop, Type type) {
    int depth = int(std::clamp(maximumDepth, 1u, 16u));
    // Native water can be implicit above voxel storage. The supplied native
    // getter remains authoritative, including TerrainVariance's water ceiling.
    for (int z = ground + depth + 2; z >= ground - 2; --z)
        if (type(z) == 2 && type(z + 1) == 0 && type(z + 2) == 0 && type(z + 3) == 0 &&
            type(z - depth) != 2) {
            waterTop = z;
            return true;
        }
    return false;
}
template <class Type> inline bool footprint(int top, Type type) {
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            if (type(x, y, top) != 2 || type(x, y, top + 1) != 0 || type(x, y, top + 2) != 0 ||
                type(x, y, top + 3) != 0)
                return false;
    return true;
}
struct Decoration {
    unsigned model = PadModel, reserved = 0;
    int64_t x = 0, y = 0, z = 0;
    float scale = .06f, yaw = 0.f, light = 255.f;
    float tint[3]{1.f, 1.f, 1.f};
    unsigned flags = 2, padding = 0;
};
static_assert(sizeof(Decoration) == 0x40);
inline bool lily(unsigned model) {
    return model == FlowerModel || model == PadModel;
}
inline bool overlaps(const Decoration &a, const Candidate &b) {
    // Preserve existing lilies and keep their native meshes from overlapping.
    if (!lily(a.model))
        return false;
    auto dx = (long double)a.x - b.x, dy = (long double)a.y - b.y;
    auto distance = 1.05L * Dots;
    return dx * dx + dy * dy < distance * distance;
}
template <class Occupied, class Locate, class Append>
inline unsigned populate(const std::array<Candidate, CandidateCount> &plan, unsigned maximum,
                         Occupied occupied, Locate locate, Append append) {
    unsigned placed = 0, selected = 0;
    for (const auto &p : plan) {
        if (!p.enabled || selected >= std::min(maximum, MaximumPads))
            continue;
        if (occupied(p)) {
            ++selected; // Existing pads consume their slot on a chunk reload.
            continue;
        }
        int top = 0;
        if (!locate(p, top))
            continue;
        Decoration pad;
        pad.model = p.model;
        pad.x = p.x;
        pad.y = p.y;
        // water-lily02's green pad occupies voxel layer 1; layer 0 is empty.
        // The flower model starts at layer 0. Align those bases to the surface
        // instead of placing each model's padded cub origin at the same height.
        auto baseOffset = p.model == PadModel ? int64_t(p.scale * Dots) : 0;
        pad.z = (int64_t(top) + 1) * Dots - baseOffset + int64_t(.01 * Dots);
        pad.scale = p.scale;
        pad.yaw = p.yaw;
        if (append(pad)) {
            ++placed;
            ++selected;
        }
    }
    return placed;
}
} // namespace lilies
