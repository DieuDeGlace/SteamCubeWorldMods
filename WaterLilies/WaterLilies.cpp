#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "cwsdk.h"
#include "../NativeScene.h"
#include "PlacementRules.h"
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace waterLilies {
using lilies::Decoration;
struct Array {
    Decoration *begin, *end, *capacity;
};
struct Stamp {
    cube::World *world;
    IntVector2 position;
    int seed;
};
static bool enabled = false;
static unsigned density = 50, flowers = 25, maximum = 18, depth = 10;
static std::mutex cacheMutex;
static std::unordered_map<cube::Zone *, Stamp> processed;
static std::atomic<uint64_t> decorated{0}, totalPads{0};
static uintptr_t base;
static_assert(offsetof(cube::Zone, gap18) == 0x18);
static_assert(offsetof(cube::SpriteManager, environment_sprites) == 0x38);
static_assert(offsetof(Decoration, x) == 8 && offsetof(Decoration, flags) == 0x38);
static_assert(sizeof(Array) == 24);

static void log(const char *words) {
    FILE *file = nullptr;
    fopen_s(&file, "Mods/WaterLilies.log", "a");
    if (file) {
        fprintf(file, "%s\n", words);
        fclose(file);
    }
}
static bool valid(const Array &a) {
    auto b = uintptr_t(a.begin), e = uintptr_t(a.end), c = uintptr_t(a.capacity);
    if (!b)
        return !e && !c;
    return e >= b && c >= e && (e - b) % sizeof(Decoration) == 0 &&
           (c - b) % sizeof(Decoration) == 0 && c - b <= 16384 * sizeof(Decoration);
}
static Array array(cube::Zone *zone) {
    Array value{};
    std::memcpy(&value, reinterpret_cast<unsigned char *>(zone) + 0x18, sizeof(value));
    return value;
}
static bool assets(cube::World *world) {
    auto manager = world->spritemanager;
    if (!manager || manager->environment_sprites.size() <= lilies::PadModel ||
        manager->sprites.size() <= 0xd24)
        return false;
    for (unsigned model : {lilies::FlowerModel, lilies::PadModel}) {
        auto sprite = manager->environment_sprites[model];
        auto native = manager->sprites[model == lilies::FlowerModel ? 0xd23 : 0xd24];
        if (!sprite || sprite != native || sprite->lowering_this_makes_plants_go_fast != 4 ||
            sprite->something_with_offset_1 != (model == lilies::FlowerModel ? 13 : 9) ||
            sprite->something_with_offset_2 != (model == lilies::FlowerModel ? 14 : 9))
            return false;
    }
    return true;
}
static int type(cube::Zone *zone, int x, int y, int z) {
    cube::Block value{};
    using Lookup = cube::Block *(*)(cube::Zone *, cube::Block *, int, int, int);
    reinterpret_cast<Lookup>(base + 0xeb90)(zone, &value, x, y, z);
    return int(value.type);
}
static void forget(cube::Zone *zone) {
    std::lock_guard<std::mutex> guard(cacheMutex);
    processed.erase(zone);
}
static bool claim(cube::Zone *zone) {
    std::lock_guard<std::mutex> guard(cacheMutex);
    auto found = processed.find(zone);
    if (found != processed.end() && found->second.world == zone->world &&
        found->second.position == zone->position && found->second.seed == zone->world->seed)
        return false;
    // Lifecycle callbacks clear normal residency. Bound the cache even if a
    // different loader omits a destruction callback; placement remains deduped.
    if (processed.size() >= 4096)
        processed.clear();
    processed[zone] = {zone->world, zone->position, zone->world->seed};
    return true;
}
static void place(cube::Zone *zone) {
    if (!enabled || !zone || !zone->world || !maximum || !density || !assets(zone->world))
        return;
    auto records = array(zone);
    if (!valid(records) || !claim(zone))
        return;
    int64_t originX = int64_t(zone->position.x) * 64, originY = int64_t(zone->position.y) * 64;
    if (originX < INT_MIN + 64LL || originX > INT_MAX - 64LL || originY < INT_MIN + 64LL ||
        originY > INT_MAX - 64LL)
        return;
    IntVector2 owner{};
    using Region = void (*)(cube::World *, IntVector2 *, int, int);
    reinterpret_cast<Region>(base + 0x2ada20)(zone->world, &owner, int(originX + 32),
                                              int(originY + 32));
    using Biome = int (*)(cube::World *, int, int);
    if (!lilies::inlandBiome(
            reinterpret_cast<Biome>(base + 0x2b9080)(zone->world, owner.x, owner.y)))
        return;
    const auto plan = lilies::candidates(uint32_t(zone->world->seed), zone->position.x,
                                         zone->position.y, density, flowers);
    auto occupied = [&](const lilies::Candidate &candidate) {
        records = array(zone); // The native append helper can move its storage.
        if (!valid(records))
            return true;
        for (auto p = records.begin; p && p != records.end; ++p)
            if (lilies::overlaps(*p, candidate))
                return true;
        return false;
    };
    auto locate = [&](const lilies::Candidate &candidate, int &top) {
        auto x = lilies::block(candidate.x), y = lilies::block(candidate.y);
        int lx = int(x - originX), ly = int(y - originY);
        if (lx < 1 || ly < 1 || lx >= 63 || ly >= 63)
            return false;
        auto height = zone->fields[lx * 64 + ly].field_8;
        if (!std::isfinite(height) || std::abs(height) > 100000.f)
            return false;
        return lilies::findSurface(int(std::floor(height)), depth, top,
                                   [&](int z) { return type(zone, int(x), int(y), z); }) &&
               lilies::footprint(top, [&](int dx, int dy, int z) {
                   return type(zone, int(x) + dx, int(y) + dy, z);
               });
    };
    auto append = [&](const Decoration &pad) {
        // Use Steam's POD decoration-vector append, including its own allocator.
        // No Creature, block edits, occupancy/collision or render hooks are added.
        using Append = void (*)(void *, const Decoration *);
        auto target = reinterpret_cast<unsigned char *>(zone) + 0x18;
        reinterpret_cast<Append>(base + 0x69b30)(target, &pad);
        return true;
    };
    unsigned placed = lilies::populate(plan, maximum, occupied, locate, append);
    if (placed) {
        auto chunks = decorated.fetch_add(1, std::memory_order_relaxed) + 1;
        auto pads = totalPads.fetch_add(placed, std::memory_order_relaxed) + placed;
        if (chunks == 1 || chunks % 32 == 0) {
            char message[160];
            sprintf_s(message,
                      "Native scenery: decorated chunks=%llu, lily pads=%llu; no actors spawned.",
                      (unsigned long long)chunks, (unsigned long long)pads);
            log(message);
        }
    }
}
static void initialize() {
    base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto setting = [](const wchar_t *name, unsigned fallback) {
        return GetPrivateProfileIntW(L"WaterLilies", name, fallback, L".\\Mods\\WaterLilies.ini");
    };
    density = std::min(setting(L"PatchDensity", 50), 100u);
    flowers = std::min(setting(L"FlowerPercent", 25), 100u);
    maximum = std::min(setting(L"MaxPadsPerChunk", 18), lilies::MaximumPads);
    depth = std::clamp(setting(L"MaximumDepth", 10), 1u, 16u);
    constexpr unsigned char entry[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
                                       0xec, 0x20, 0x48, 0x8b, 0x41, 0x08, 0x48, 0x8b};
    enabled = setting(L"Enabled", 1) && nativeScene::supported() &&
              std::memcmp(reinterpret_cast<void *>(base + 0x69b30), entry, sizeof(entry)) == 0;
    log(enabled ? "WaterLilies enabled: native freshwater scenery, stable chunk clusters."
                : "WaterLilies disabled or native decoration append signature unavailable.");
}
} // namespace waterLilies

class WaterLilies : public GenericMod {
  public:
    void Initialize() override {
        waterLilies::initialize();
    }
    void OnChunkRemesh(cube::Zone *zone) override {
        waterLilies::place(zone);
    }
    void OnZoneGenerated(cube::Zone *zone) override {
        waterLilies::forget(zone);
    }
    void OnZoneDestroy(cube::Zone *zone) override {
        waterLilies::forget(zone);
    }
};
static_assert(sizeof(GenericMod) == 0x428);
EXPORT GenericMod *MakeMod() {
    return new WaterLilies();
}
EXPORT void ModPreInitialize() {}
EXPORT int ModMajorVersion() {
    return 7;
}
EXPORT int ModMinorVersion() {
    return 3;
}
