// DualHair: two-tone hair (Omega style) for chosen player characters.
//
// The native creature renderer queues the hair model at one call site with a
// material colour of base * hair colour. For a configured character this mod
// queues a recoloured copy of that hair model instead: the copy is made with
// the game's own Sprite copy constructor, its voxels get a top-to-bottom
// gradient from the character's hair colour into the second colour, and the
// game's own mesher rebuilds it. The hair tint is divided out of the queued
// colour, so the top of the hair matches vanilla. Everything after the queue
// (lighting, shadows, TAA, other mods) sees an ordinary native model draw.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "cwsdk.h"
#include "DualHairRules.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dualhair {
// Native addresses (RVAs) in the inspected Steam build.
constexpr size_t HAIR_QUEUE_CALL = 0x2813a; // call QueueModel for the hair sprite
constexpr size_t QUEUE_MODEL = 0xee2f0;
constexpr size_t SPRITE_COPY = 0x16e790; // Sprite::Sprite(const Sprite &)
constexpr size_t SPRITE_MESH = 0x170700; // Sprite::BuildMesh(this, 0); replaces its buffers
constexpr size_t SPRITE_VTABLE = 0x44e790;
constexpr size_t GAME_NEW = 0x392bac; // operator new used by the native scalar deleter
constexpr size_t SPRITE_SIZE = 0xa8;
// Sprite fields: voxel colours (3 bytes each, x fastest, then y, then z) and size.
constexpr size_t SPRITE_VOXELS = 0x68, SPRITE_SIZE_X = 0x88, SPRITE_SIZE_Y = 0x8c,
                 SPRITE_SIZE_Z = 0x90, SPRITE_GFX = 0x60;
constexpr size_t CREATURE_HAIR_COLOR = 0x7a; // Appearance::hair_color (RGBA bytes)
constexpr size_t GAME_POINTER = 0x551a80, CHAT_PRINT = 0x26bf10;
// A clone unused this long cannot be in a pending frame queue and is freed.
constexpr ULONGLONG RETIRE_MS = 10000;
constexpr size_t CACHE_LIMIT = 64;

using QueueModel = void (*)(void *, void *, void *, float *, float, float, float, float, float);
using SpriteCopy = void *(*)(void *, void *);
using SpriteMesh = void (*)(void *, int);
using GameNew = void *(*)(size_t);

static const wchar_t *INI = L".\\Mods\\DualHair.ini";
static bool enabled = true;
static Shape shape;
static void *creatureSlot = nullptr; // Filled by the stub: r14 (renderer state; [r14] = creature).

static void log(const char *text) {
    FILE *f = nullptr;
    fopen_s(&f, "Mods\\DualHair.log", "a");
    if (f) {
        fprintf(f, "%s\n", text);
        fclose(f);
    }
}
static unsigned char *base() {
    return (unsigned char *)GetModuleHandleW(nullptr);
}
template <typename T> static T field(void *object, size_t offset) {
    T value;
    memcpy(&value, (unsigned char *)object + offset, sizeof(T));
    return value;
}

static bool supported() {
    auto b = base();
    auto dos = (IMAGE_DOS_HEADER *)b;
    if (!b || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    auto nt = (IMAGE_NT_HEADERS64 *)(b + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.TimeDateStamp == 0x5d93ab0d &&
           (nt->OptionalHeader.SizeOfImage == 0x5b7000 || // PC (SteamStub .bind section)
            nt->OptionalHeader.SizeOfImage == 0x58a000);  // GameNative (section stripped)
}

// ---------------------------------------------------------------------------
// Settings: per-character second colours, keyed by character name.
// ---------------------------------------------------------------------------
struct Entry {
    std::string name;
    Rgb second;
};
static std::vector<Entry> characters;

static std::wstring wide(const std::string &s) {
    return std::wstring(s.begin(), s.end());
}
static void loadSettings() {
    enabled = GetPrivateProfileIntW(L"DualHair", L"Enabled", 1, INI) != 0;
    shape.start =
        std::clamp((int)GetPrivateProfileIntW(L"DualHair", L"StartPercent", 40, INI), 0, 100) * .01f;
    shape.blend =
        std::clamp((int)GetPrivateProfileIntW(L"DualHair", L"BlendPercent", 35, INI), 0, 100) * .01f;
    characters.clear();
    std::vector<wchar_t> buffer(32768);
    DWORD length = GetPrivateProfileSectionW(L"Characters", buffer.data(), (DWORD)buffer.size(), INI);
    for (const wchar_t *line = buffer.data(); line < buffer.data() + length && *line;
         line += wcslen(line) + 1) {
        const wchar_t *equals = wcschr(line, L'=');
        if (!equals || *line == L';')
            continue;
        Rgb color;
        if (!parseColor(equals + 1, color))
            continue;
        std::string name;
        for (const wchar_t *c = line; c < equals; ++c)
            name += (char)*c;
        while (!name.empty() && name.back() == ' ')
            name.pop_back();
        if (!name.empty())
            characters.push_back({name, color});
    }
}
static const Entry *entryFor(const char *name) {
    for (auto &e : characters)
        if (_stricmp(e.name.c_str(), name) == 0)
            return &e;
    return nullptr;
}
static std::string nameOf(cube::Creature *c) {
    char name[17]{};
    memcpy(name, c->entity_data.name, 16);
    return name;
}

// ---------------------------------------------------------------------------
// Recoloured hair models.
// ---------------------------------------------------------------------------
struct Clone {
    void *original, *originalVoxels, *gfx;
    int size[3];
    Rgb first, second;
    Shape shape;
    void *sprite;
    ULONGLONG used;
};
static std::vector<Clone> clones;

static void recolor(void *sprite, Rgb first, Rgb second, const Shape &s) {
    int sx = field<int>(sprite, SPRITE_SIZE_X), sy = field<int>(sprite, SPRITE_SIZE_Y),
        sz = field<int>(sprite, SPRITE_SIZE_Z);
    auto voxels = field<uint8_t *>(sprite, SPRITE_VOXELS);
    int top = -1, bottom = sz;
    for (int z = 0; z < sz; z++)
        for (int i = 0; i < sx * sy; i++) {
            auto v = voxels + 3 * ((size_t)z * sx * sy + i);
            if (!emptyVoxel({v[0], v[1], v[2]})) {
                top = std::max(top, z);
                bottom = std::min(bottom, z);
            }
        }
    if (top < 0)
        return;
    float span = (float)std::max(1, top - bottom);
    for (int z = 0; z < sz; z++) {
        float share = secondShare((top - z) / span, s);
        for (int i = 0; i < sx * sy; i++) {
            auto v = voxels + 3 * ((size_t)z * sx * sy + i);
            Rgb voxel{v[0], v[1], v[2]};
            if (emptyVoxel(voxel))
                continue;
            auto out = shadeVoxel(voxel, first, second, share);
            v[0] = out.r, v[1] = out.g, v[2] = out.b;
        }
    }
}

static void retire(Clone &c) {
    // Native scalar deleting destructor: frees the buffers, voxels and object.
    auto vtable = *(void (***)(void *, int))c.sprite;
    vtable[0](c.sprite, 1);
}

static void *cloneFor(void *original, Rgb first, Rgb second) {
    auto voxels = field<void *>(original, SPRITE_VOXELS);
    auto gfx = field<void *>(original, SPRITE_GFX);
    int size[3] = {field<int>(original, SPRITE_SIZE_X), field<int>(original, SPRITE_SIZE_Y),
                   field<int>(original, SPRITE_SIZE_Z)};
    if (*(void **)original != base() + SPRITE_VTABLE || !voxels || !gfx || size[0] <= 0 ||
        size[1] <= 0 || size[2] <= 0 || size[0] * size[1] * size[2] > 256 * 256 * 256)
        return nullptr;
    auto now = GetTickCount64();
    for (auto &c : clones)
        if (c.original == original && c.originalVoxels == voxels && c.gfx == gfx &&
            !memcmp(c.size, size, sizeof size) && c.first == first && c.second == second &&
            c.shape.start == shape.start && c.shape.blend == shape.blend) {
            c.used = now;
            return c.sprite;
        }
    // Only clones idle long enough to be outside any pending draw queue are freed.
    for (size_t i = 0; i < clones.size();)
        if (now - clones[i].used > RETIRE_MS) {
            retire(clones[i]);
            clones.erase(clones.begin() + i);
        } else {
            ++i;
        }
    if (clones.size() >= CACHE_LIMIT)
        return nullptr;
    auto memory = ((GameNew)(base() + GAME_NEW))(SPRITE_SIZE);
    if (!memory)
        return nullptr;
    auto sprite = ((SpriteCopy)(base() + SPRITE_COPY))(memory, original);
    if (!field<void *>(sprite, SPRITE_VOXELS)) {
        auto vtable = *(void (***)(void *, int))sprite;
        vtable[0](sprite, 1);
        return nullptr;
    }
    recolor(sprite, first, second, shape);
    ((SpriteMesh)(base() + SPRITE_MESH))(sprite, 0);
    clones.push_back({original, voxels, gfx, {size[0], size[1], size[2]}, first, second, shape,
                      sprite, now});
    return sprite;
}

// Replaces the native hair queue call (same arguments; rdx = hair sprite,
// r9 = final material colour).
static void queueHair(void *queue, void *sprite, void *matrix, float *color, float a, float b,
                      float c, float d, float e) {
    auto original = (QueueModel)(base() + QUEUE_MODEL);
    void *state = creatureSlot;
    auto creature = state ? *(cube::Creature **)state : nullptr;
    if (enabled && sprite && color && creature &&
        creature->entity_data.hostility_type == 0 /* player */) {
        if (auto entry = entryFor(nameOf(creature).c_str())) {
            auto hair = (uint8_t *)creature + CREATURE_HAIR_COLOR;
            Rgb first{hair[0], hair[1], hair[2]};
            if (auto recolored = cloneFor(sprite, first, entry->second)) {
                float untinted[4] = {color[0], color[1], color[2], color[3]};
                untint(untinted, first);
                original(queue, recolored, matrix, untinted, a, b, c, d, e);
                return;
            }
        }
    }
    original(queue, sprite, matrix, color, a, b, c, d, e);
}

// Call-site stub near the module: saves r14 for queueHair, then jumps to it.
static bool install() {
    auto site = base() + HAIR_QUEUE_CALL;
    // movss xmm6,[rbp+0x2cc8]; movss [rsp+0x20],xmm6; call QueueModel
    static const unsigned char expected[] = {0xf3, 0x0f, 0x10, 0xb5, 0xc8, 0x2c, 0x00,
                                             0x00, 0xf3, 0x0f, 0x11, 0x74, 0x24, 0x20,
                                             0xe8, 0xb1, 0x61, 0x0c, 0x00};
    static const unsigned char hairSprite[] = {0x49, 0x8b, 0x96, 0xe0, 0x00, 0x00, 0x00};
    static const unsigned char copyPrologue[] = {0x4c, 0x8b, 0xdc, 0x49, 0x89, 0x4b, 0x08, 0x55};
    static const unsigned char meshPrologue[] = {0x48, 0x8b, 0xc4, 0x55, 0x56, 0x57, 0x41, 0x54};
    static const unsigned char newPrologue[] = {0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b};
    if (memcmp(site - 14, expected, sizeof expected) ||
        memcmp(base() + 0x280f2, hairSprite, sizeof hairSprite) ||
        memcmp(base() + SPRITE_COPY, copyPrologue, sizeof copyPrologue) ||
        memcmp(base() + SPRITE_MESH, meshPrologue, sizeof meshPrologue) ||
        memcmp(base() + GAME_NEW, newPrologue, sizeof newPrologue)) {
        log("Native hair draw does not match the inspected build (or another mod changed it); "
            "DualHair is off.");
        return false;
    }
    // Find executable memory within rel32 range of the call site.
    unsigned char *stub = nullptr;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    auto granularity = (uintptr_t)info.dwAllocationGranularity;
    for (uintptr_t distance = granularity; distance < 0x70000000 && !stub; distance += granularity)
        for (int sign : {-1, 1}) {
            auto at = ((uintptr_t)site + sign * (intptr_t)distance) & ~(granularity - 1);
            stub = (unsigned char *)VirtualAlloc((void *)at, 4096, MEM_RESERVE | MEM_COMMIT,
                                                 PAGE_EXECUTE_READWRITE);
            if (stub)
                break;
        }
    if (!stub) {
        log("No memory near the game for the hook stub; DualHair is off.");
        return false;
    }
    unsigned char code[] = {
        0x49, 0xbb, 0, 0, 0, 0, 0, 0, 0, 0, // mov r11, &creatureSlot
        0x4d, 0x89, 0x33,                   // mov [r11], r14
        0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, // mov rax, queueHair
        0xff, 0xe0,                         // jmp rax
    };
    auto slot = &creatureSlot;
    auto target = &queueHair;
    memcpy(code + 2, &slot, 8);
    memcpy(code + 15, &target, 8);
    memcpy(stub, code, sizeof code);
    FlushInstructionCache(GetCurrentProcess(), stub, sizeof code);
    DWORD old;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old))
        return false;
    int32_t rel = (int32_t)(stub - (site + 5));
    memcpy(site + 1, &rel, 4);
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return true;
}

// ---------------------------------------------------------------------------
// Chat: /hair2 R G B | #RRGGBB | off | start N | blend N
// ---------------------------------------------------------------------------
static cube::Game *game() {
    return *(cube::Game **)(base() + GAME_POINTER);
}
static cube::Creature *player() {
    auto g = game();
    return g && g->world ? g->world->local_creature : nullptr;
}
static void say(const std::wstring &text) {
    auto g = game();
    if (!g || !g->gui.chat_widget)
        return;
    using Print = void (*)(void *, std::wstring *, FloatRGBA *);
    std::wstring line = text + L"\n";
    FloatRGBA color(.85f, .75f, 1.f, 1.f);
    ((Print)(base() + CHAT_PRINT))(g->gui.chat_widget, &line, &color);
}
static void setPercent(const wchar_t *key, const wchar_t *value) {
    int n = std::clamp(_wtoi(value), 0, 100);
    WritePrivateProfileStringW(L"DualHair", key, std::to_wstring(n).c_str(), INI);
    loadSettings();
    say(std::wstring(L"DualHair: ") + key + L" set to " + std::to_wstring(n) + L"%.");
}
static bool chat(const std::wstring &message) {
    static const wchar_t command[] = L"/hair2";
    if (_wcsnicmp(message.c_str(), command, 6) != 0 || (message.size() > 6 && message[6] != L' '))
        return false;
    auto self = player();
    if (!self)
        return true;
    auto name = wide(nameOf(self));
    std::wstring args = message.size() > 7 ? message.substr(7) : L"";
    while (!args.empty() && args.back() == L' ')
        args.pop_back();
    if (args.empty()) {
        auto entry = entryFor(nameOf(self).c_str());
        wchar_t text[160];
        if (entry)
            swprintf_s(text, L"DualHair: %ls has second hair colour %d,%d,%d (start %d%%, blend %d%%).",
                       name.c_str(), entry->second.r, entry->second.g, entry->second.b,
                       (int)std::lround(shape.start * 100), (int)std::lround(shape.blend * 100));
        else
            swprintf_s(text, L"DualHair: off for %ls. Use /hair2 R G B or /hair2 #RRGGBB.",
                       name.c_str());
        say(text);
        return true;
    }
    if (_wcsicmp(args.c_str(), L"off") == 0) {
        WritePrivateProfileStringW(L"Characters", name.c_str(), nullptr, INI);
        loadSettings();
        say(L"DualHair: second hair colour removed for " + name + L".");
        return true;
    }
    if (_wcsnicmp(args.c_str(), L"start ", 6) == 0) {
        setPercent(L"StartPercent", args.c_str() + 6);
        return true;
    }
    if (_wcsnicmp(args.c_str(), L"blend ", 6) == 0) {
        setPercent(L"BlendPercent", args.c_str() + 6);
        return true;
    }
    Rgb color;
    if (!parseColor(args.c_str(), color)) {
        say(L"DualHair: use /hair2 R G B (0-255), /hair2 #RRGGBB, /hair2 off, "
            L"/hair2 start N or /hair2 blend N.");
        return true;
    }
    wchar_t value[16];
    swprintf_s(value, L"%d,%d,%d", color.r, color.g, color.b);
    WritePrivateProfileStringW(L"Characters", name.c_str(), value, INI);
    loadSettings();
    say(L"DualHair: second hair colour for " + name + L" set to " + value + L".");
    return true;
}
} // namespace dualhair

class DualHair : public GenericMod {
  public:
    void Initialize() override {
        if (!dualhair::supported()) {
            dualhair::log("Unsupported Cube World executable; DualHair is off.");
            return;
        }
        dualhair::loadSettings();
        if (dualhair::install())
            dualhair::log("DualHair attached to the native hair draw; /hair2 sets a second colour.");
    }
    int OnChat(std::wstring *message) override {
        return message && dualhair::chat(*message) ? 1 : 0;
    }
};
static_assert(sizeof(GenericMod) == 0x428);
EXPORT GenericMod *MakeMod() {
    return new DualHair();
}
EXPORT void ModPreInitialize() {}
EXPORT int ModMajorVersion() {
    return 7;
}
EXPORT int ModMinorVersion() {
    return 3;
}
