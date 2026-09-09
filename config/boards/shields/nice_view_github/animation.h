/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* LCM of the 3.2-second bob and the 82 * 280 ms grid loop. */
#define GITHUB_LOOP_MS 918400

static inline int github_bob(uint32_t elapsed) {
    /* Integer-ms transitions of round(2 * sin(elapsed * 2 * PI / 3200)). */
    static const uint16_t transitions[] = {129, 432, 1169, 1472, 1729, 2032, 2769, 3072};
    static const int8_t offsets[] = {0, 1, 2, 1, 0, -1, -2, -1, 0};
    uint32_t phase = elapsed % 3200;
    unsigned i = 0;
    while (i < sizeof(transitions) / sizeof(transitions[0]) && phase >= transitions[i]) {
        ++i;
    }
    return offsets[i];
}

static inline unsigned github_grid_step(uint32_t elapsed) {
    return (elapsed / 280) % 82;
}

static inline bool github_cell_filled(unsigned index, unsigned step) {
    return index < step && (index * 37 + 11) % 10 < 6;
}
