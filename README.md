# Lily58 wireless ZMK configuration

The left half uses the stock nice!view screen. The right half uses the local
`config/boards/shields/nice_view_github` shield: the GitHub mark bobs by two
pixels every 3.2 seconds above a crisp, static `@pmartindev` bitmap and a
decorative contribution grid. Battery level, charging and split connection
indicators retain the stock peripheral drawing and event subscriptions.
No GitHub API, credentials, companion app, or firmware fork is involved.

## Build and flash

Push the branch to run the existing **build** Actions workflow, which builds
both halves. Download the successful run's `firmware` artifact and unzip it.
Connect the **right** half by USB, double-tap its reset button to enter the
UF2 bootloader, and copy
`lily58_right nice_view_adapter nice_view_github-nice_nano_v2-zmk.uf2`
to its bootloader drive. Do not flash this file to the left half.
The left UF2 remains
`lily58_left nice_view_adapter nice_view-nice_nano_v2-zmk.uf2`.

To revert, change only the right entry in `build.yaml` from `nice_view_github`
back to `nice_view`, rebuild, and flash the new **right** UF2. The local shield
can remain in the repository; it is not compiled when unselected.

## Display behavior

The artwork matches approved preview revision 4 in portrait 68 x 160 pixels:
48 x 48 mark at (10, 42), caption buffer 68 x 14 at (0, 100), and a 10-column,
7-row grid at (5, 117). Cells are 4 x 4 with two-pixel gaps; progression is
column-major, one step per 280 ms, with an 82-step fill/hold/reset cycle.
Empty cells keep black outlines and white interiors.

Images are pre-rotated to match stock nice!view's native 160 x 68 orientation.
There is no runtime font rendering or resampling of the handle. One LVGL loop
is created with the screen; battery and connection events only redraw status.
The existing display queue runs at 8 Hz and the grid is redrawn only when
visible cells change.

Stock nice!view behavior is retained: idle blanking is **off**. Continuous
animation costs more battery than static artwork; the actual impact needs
measurement. If desired, enable `CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE=y` in
`config/lily58.conf` (this shared setting affects both halves). ZMK stops its
display ticks while blanked; on wake the animation resumes at the current
phase, without starting another loop. Deep sleep remains controlled by the
existing `CONFIG_ZMK_SLEEP` option.

**Hardware acceptance is pending:** verify orientation, looping without the
old artwork, battery/charging and connection indicators, reconnect, and
sleep/wake on the physical keyboard.

## Provenance and checks

Display setup, status drawing and charging bolt derive from
[ZMK nice_view at 641514a](https://github.com/zmkfirmware/zmk/tree/641514a97db345f499dd50b0360e594270f008fe/app/boards/shields/nice_view)
(MIT). This peripheral-only implementation uses consistent byte buffers sized
with LVGL's canvas macro, rather than mixing upstream central/peripheral
widget declarations. It targets the LVGL 9 API in that revision.

The mark is
[Primer Octicons mark-github-16](https://github.com/primer/octicons/blob/9dc9906e43e131f4b5a65817d2432fa587055003/icons/mark-github-16.svg)
(MIT). Its approved 3x canvas raster was thresholded at 128 into a 48 x 48
one-bit bitmap. The handle uses the exact approved five-pixel glyphs.
`assets.h` records SHA-256 hashes of both unrotated grayscale rasters.
Upstream copyright notices and the MIT terms are retained in the shield's
`LICENSE`.

Run `python3 tests/check_animation.py` for the dependency-free asset and
animation checks (requires a C compiler). Firmware validation uses the existing
Actions matrix. The reusable workflow is hosted in `pmartindev/zmk`, but
`config/west.yml` fetches firmware from **zmkfirmware/zmk at main**; neither was
changed. Future upstream API changes may require updating this local shield.
