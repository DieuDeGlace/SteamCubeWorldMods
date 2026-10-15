#pragma once
// Pure colour rules for DualHair, shared by the mod and its rule test.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <string>

namespace dualhair {
struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb &o) const {
        return r == o.r && g == o.g && b == o.b;
    }
};

// Gradient shape: the second colour begins `start` of the way down the hair
// and is fully reached after `blend` more (both 0-1; blend 0 = hard split).
struct Shape {
    float start = .4f, blend = .35f;
};

// Accepts "R,G,B", "R G B" (0-255 each) or "#RRGGBB"/"RRGGBB".
inline bool parseColor(const wchar_t *text, Rgb &out) {
    while (*text && iswspace(*text))
        ++text;
    if (*text == L'#')
        ++text;
    size_t hexDigits = 0;
    while (iswxdigit(text[hexDigits]))
        ++hexDigits;
    if (hexDigits == 6 && (text[6] == 0 || iswspace(text[6]))) {
        auto value = wcstoul(std::wstring(text, 6).c_str(), nullptr, 16);
        out = {uint8_t(value >> 16), uint8_t(value >> 8), uint8_t(value)};
        return true;
    }
    int parts[3];
    for (int i = 0; i < 3; i++) {
        while (*text && (iswspace(*text) || *text == L','))
            ++text;
        if (!iswdigit(*text))
            return false;
        wchar_t *end = nullptr;
        long value = wcstol(text, &end, 10);
        if (value < 0 || value > 255)
            return false;
        parts[i] = (int)value;
        text = end;
    }
    while (*text && iswspace(*text))
        ++text;
    if (*text)
        return false;
    out = {uint8_t(parts[0]), uint8_t(parts[1]), uint8_t(parts[2])};
    return true;
}

// Share of the second colour at `depth` (0 = top of the hair, 1 = lowest voxel).
inline float secondShare(float depth, const Shape &shape) {
    if (shape.blend <= 0)
        return depth >= shape.start ? 1.f : 0.f;
    float s = std::clamp((depth - shape.start) / shape.blend, 0.f, 1.f);
    return s * s * (3 - 2 * s);
}

// A hair voxel shaded by the gradient. The native draw multiplies the whole
// model by the hair colour; the mod removes that tint for a recoloured model,
// so the first colour is baked in here too and the top matches vanilla.
// Results never become empty (0,0,0) or a pure marker colour (a single 255
// channel with the others 0), which the native mesher treats specially.
inline Rgb shadeVoxel(Rgb voxel, Rgb first, Rgb second, float share) {
    auto mix = [&](uint8_t v, uint8_t a, uint8_t b) {
        float tint = (a + (b - a) * share) / 255.f;
        return (int)std::lround(v * tint);
    };
    int c[3] = {mix(voxel.r, first.r, second.r), mix(voxel.g, first.g, second.g),
                mix(voxel.b, first.b, second.b)};
    for (int &x : c)
        x = std::clamp(x, 1, 254);
    return {uint8_t(c[0]), uint8_t(c[1]), uint8_t(c[2])};
}

inline bool emptyVoxel(Rgb v) {
    return v.r == 0 && v.g == 0 && v.b == 0;
}

// Removes the native hair tint from a final model colour. A zero hair channel
// leaves no information, so it uses the mean of the recoverable channels.
inline void untint(float color[3], Rgb hair) {
    const uint8_t h[3] = {hair.r, hair.g, hair.b};
    float sum = 0;
    int known = 0;
    for (int i = 0; i < 3; i++)
        if (h[i]) {
            color[i] *= 255.f / h[i];
            sum += color[i];
            known++;
        }
    float fallback = known ? sum / known : 1.f;
    for (int i = 0; i < 3; i++)
        if (!h[i])
            color[i] = fallback;
}
} // namespace dualhair
