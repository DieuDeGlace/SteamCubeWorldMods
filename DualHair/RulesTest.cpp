#include "DualHairRules.h"
#include <cassert>
#include <cstdio>
using namespace dualhair;
int main() {
    Rgb c;
    assert((parseColor(L"40,120,255", c) && c == Rgb{40, 120, 255}));
    assert((parseColor(L" 1 2 3 ", c) && c == Rgb{1, 2, 3}));
    assert((parseColor(L"#2E6BFF", c) && c == Rgb{0x2e, 0x6b, 0xff}));
    assert((parseColor(L"a0b1c2", c) && c == Rgb{0xa0, 0xb1, 0xc2}));
    assert(!parseColor(L"256,0,0", c));
    assert(!parseColor(L"1,2", c));
    assert(!parseColor(L"1,2,3,4", c));
    assert(!parseColor(L"#12345", c));
    assert(!parseColor(L"blue", c));

    Shape s{.4f, .2f};
    assert(secondShare(0, s) == 0 && secondShare(.4f, s) == 0);
    assert(secondShare(.6f, s) == 1 && secondShare(1, s) == 1);
    float mid = secondShare(.5f, s);
    assert(mid > .45f && mid < .55f);
    Shape hard{.5f, 0};
    assert(secondShare(.49f, hard) == 0 && secondShare(.5f, hard) == 1);

    Rgb purple{120, 40, 200}, blue{30, 110, 255};
    // Top keeps the native look (voxel * hair colour); bottom is the second colour.
    Rgb top = shadeVoxel({255, 255, 255}, purple, blue, 0);
    assert((top == Rgb{120, 40, 200}));
    Rgb bottom = shadeVoxel({200, 200, 200}, purple, blue, 1);
    assert(bottom.r == 24 && bottom.g == 86 && bottom.b == 200);
    // Never empty or a pure marker colour.
    Rgb dark = shadeVoxel({10, 10, 10}, {0, 0, 0}, {0, 0, 0}, .5f);
    assert(!emptyVoxel(dark));
    Rgb red = shadeVoxel({255, 255, 255}, {255, 0, 0}, {255, 0, 0}, 0);
    assert((!(red == Rgb{255, 0, 0})));

    float color[3] = {120 / 255.f * .8f, 40 / 255.f * .8f, 0};
    untint(color, {120, 40, 0});
    assert(color[0] > .79f && color[0] < .81f && color[1] > .79f && color[1] < .81f);
    assert(color[2] > .79f && color[2] < .81f);
    puts("DualHair rules PASS");
}
