"""Check approved raster pixels, native orientation, and the actual C animation helpers."""

import hashlib
import math
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SHIELD = ROOT / "config/boards/shields/nice_view_github"


def portrait_pixels(name, width, height):
    source = (SHIELD / "assets.h").read_text()
    array = re.search(rf"{name}_map\[\] = \{{(.*?)\}};", source, re.S).group(1)
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", array))
    stride = (height + 7) // 8
    assert len(data) == 8 + stride * width
    assert data[:8] == bytes([255, 255, 255, 255, 0, 0, 0, 255])
    return bytes(
        0 if data[8 + x * stride + (height - 1 - y) // 8]
        & (1 << (7 - (height - 1 - y) % 8)) else 255
        for y in range(height) for x in range(width)
    )


mark = portrait_pixels("github_mark", 48, 48)
caption = portrait_pixels("github_caption", 68, 14)
# Independent hashes taken directly from approved revision 4's browser canvases.
assert hashlib.sha256(mark).hexdigest() == "4b4d3c9f938646f575e348d07fd447e0eb34af2fb75ec59b36fa6bdc564061df"
assert hashlib.sha256(caption).hexdigest() == "3bd31c909e5802782eea44af581951bfa809bc139b707b5c444c0a9fa1cc39e1"

glyphs = [
    [14, 17, 23, 21, 23, 16, 14, 0],
    [0, 0, 30, 17, 17, 30, 16, 16],
    [0, 0, 26, 21, 21, 21, 21, 0],
    [0, 0, 14, 1, 15, 17, 15, 0],
    [0, 0, 22, 25, 16, 16, 16, 0],
    [4, 4, 14, 4, 4, 5, 2, 0],
    [4, 0, 12, 4, 4, 4, 14, 0],
    [0, 0, 30, 17, 17, 17, 17, 0],
    [1, 1, 15, 17, 17, 17, 15, 0],
    [0, 0, 14, 17, 31, 16, 14, 0],
    [0, 0, 17, 17, 17, 10, 4, 0],
]
expected = bytearray([255] * (68 * 14))
for index, glyph in enumerate(glyphs):
    for y, row in enumerate(glyph):
        for x in range(5):
            if row & (1 << (4 - x)):
                expected[(3 + y) * 68 + 1 + index * 6 + x] = 0
assert caption == expected

with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / "check.c"
    binary = Path(directory) / "check"
    source.write_text(r"""
#include <assert.h>
#include <stdio.h>
#include "animation.h"
int main(void) {
    assert(GITHUB_LOOP_MS % 3200 == 0);
    assert(GITHUB_LOOP_MS % (280 * 82) == 0);
    for (unsigned t = 0; t < GITHUB_LOOP_MS * 2; ++t) {
        assert(github_bob(t) == github_bob(t % GITHUB_LOOP_MS));
        assert(github_grid_step(t) == (t / 280) % 82);
    }
    for (unsigned step = 0; step < 82; ++step) {
        unsigned filled = 0;
        for (unsigned index = 0; index < 70; ++index) {
            bool expected = index < step && (index * 37 + 11) % 10 < 6;
            assert(github_cell_filled(index, step) == expected);
            filled += expected;
        }
        if (step == 0) assert(filled == 0);
        if (step >= 70) assert(filled == 42);
    }
    for (unsigned t = 0; t < 3200; ++t) printf("%d\n", github_bob(t));
}
""")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(SHIELD),
                    str(source), "-o", str(binary)], check=True)
    offsets = list(map(int, subprocess.check_output([str(binary)], text=True).split()))
    assert offsets == [math.floor(2 * math.sin(t * 2 * math.pi / 3200) + 0.5)
                       for t in range(3200)]

# All five bob positions stay below the status region and above the caption.
for offset in set(offsets):
    assert 32 <= 42 + offset and 42 + offset + 48 <= 100
    assert 0 <= 70 - offset and 70 - offset + 48 <= 128
assert 100 + 14 <= 117
assert 5 + 9 * 6 + 4 <= 68
assert 117 + 6 * 6 + 4 <= 160
print("Approved assets, rotation, glyphs, motion bounds, fill/hold/reset and C timing passed.")
