#include "AlphaCoordinates.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    alphacoords::Mapping frame;
    frame.steamAnchor = {33554432, 33554432};
    const auto home = frame.point(33554432, 33554432);
    assert(home && home->x == alphacoords::HOME && home->y == alphacoords::HOME);
    assert(!frame.point(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
    assert(!frame.zone(std::numeric_limits<int32_t>::max(), 0));

    // Four parents must cover all 4096 source columns exactly once.
    frame = {{0, 0}, {235, 235}};
    const auto split = frame.zone(0, 0);
    assert(split && split->count == 4);
    std::array<int, 64 * 64> visited{};
    for (size_t i = 0; i < split->count; ++i) {
        const auto &p = split->patches[i];
        assert(p.alphaX >= 0 && p.alphaY >= 0 && p.alphaX + p.width <= 256 && p.alphaY + p.height <= 256);
        for (int x = 0; x < p.width; ++x)
            for (int y = 0; y < p.height; ++y) {
                const int sx = p.sourceX + x, sy = p.sourceY + y;
                assert(sx >= 0 && sy >= 0 && sx < 64 && sy < 64);
                ++visited[sx + 64 * sy];
                const auto point = frame.point(sx, sy);
                assert(point && point->x == p.zoneX * 256 + p.alphaX + x && point->y == p.zoneY * 256 + p.alphaY + y);
            }
    }
    for (auto count : visited)
        assert(count == 1);

    frame = {{1, 1}, {0, 0}};
    const auto clipped = frame.zone(0, 0);
    assert(clipped && clipped->count == 1);
    const auto &patch = clipped->patches[0];
    assert(patch.sourceX == 1 && patch.sourceY == 1 && patch.width == 63 && patch.height == 63);
    const auto outside = frame.zone(-1, -1);
    assert(outside && outside->count == 0);
    assert(!frame.point(0, 0));
    frame = {{0, 0}, {alphacoords::WORLD_SIDE - 1, alphacoords::WORLD_SIDE - 1}};
    const auto last = frame.zone(0, 0);
    assert(last && last->count == 1 && last->patches[0].width == 1 && last->patches[0].height == 1);
    assert(frame.point(0, 0) && !frame.point(1, 1));
    frame = {{0, 0}, {235, 235}};
    const auto negative = frame.zone(-1, -1);
    assert(negative && negative->count == 1 && frame.point(-1, -1)->x == 234);
    frame.alphaAnchor = {-1, 0};
    assert(!frame.zone(0, 0) && !frame.point(0, 0));
    std::puts("Alpha coordinate mapping: PASS");
}
