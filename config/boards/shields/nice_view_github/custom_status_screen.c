/*
 * Copyright (c) 2023, 2025 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Peripheral status drawing/listeners adapted from nice_view at
 * 641514a97db345f499dd50b0360e594270f008fe.
 */

#include <lvgl.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>

#include "animation.h"
#include "assets.h"

#define STATUS_SIZE 68
#define CANVAS_FORMAT LV_COLOR_FORMAT_L8
#define BUFFER_SIZE(w, h) \
    LV_CANVAS_BUF_SIZE(w, h, LV_COLOR_FORMAT_GET_BPP(CANVAS_FORMAT), LV_DRAW_BUF_STRIDE_ALIGN)

static lv_obj_t *screen;
static lv_obj_t *status;
static lv_obj_t *logo;
static lv_obj_t *grid;
static uint8_t status_buffer[BUFFER_SIZE(68, 68)];
static uint8_t rotation_buffer[BUFFER_SIZE(68, 68)];
static uint8_t grid_buffer[BUFFER_SIZE(40, 58)];
static int previous_step = -1;

static struct {
    uint8_t battery;
    bool charging;
    bool connected;
} state;

static void rect(lv_layer_t *layer, int x, int y, int w, int h, bool black) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = black ? lv_color_black() : lv_color_white();
    lv_area_t area = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &dsc, &area);
}

static void draw_status(void) {
    lv_canvas_fill_bg(status, lv_color_white(), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(status, &layer);

    /* Keep the stock peripheral battery, charging bolt, and connection symbol. */
    rect(&layer, 0, 2, 29, 12, true);
    rect(&layer, 1, 3, 27, 10, false);
    int fill = (state.battery + 2) / 4;
    if (fill > 0) {
        rect(&layer, 2, 4, fill, 8, true);
    }
    rect(&layer, 30, 5, 3, 6, true);
    rect(&layer, 31, 6, 1, 4, false);
    if (state.charging) {
        lv_draw_image_dsc_t image;
        lv_draw_image_dsc_init(&image);
        image.src = &bolt;
        lv_area_t area = {9, -1, 19, 16};
        lv_draw_image(&layer, &image, &area);
    }

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.color = lv_color_black();
    label.font = &lv_font_montserrat_16;
    label.align = LV_TEXT_ALIGN_RIGHT;
    label.text = state.connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE;
    lv_area_t area = {0, 0, STATUS_SIZE, STATUS_SIZE};
    lv_draw_label(&layer, &label, &area);
    lv_canvas_finish_layer(status, &layer);

    uint8_t *data = lv_canvas_get_draw_buf(status)->data;
    memcpy(rotation_buffer, data, sizeof(rotation_buffer));
    uint32_t stride = lv_draw_buf_width_to_stride(STATUS_SIZE, CANVAS_FORMAT);
    lv_draw_sw_rotate(rotation_buffer, data, STATUS_SIZE, STATUS_SIZE, stride, stride,
                      LV_DISPLAY_ROTATION_270, CANVAS_FORMAT);
    lv_obj_invalidate(status);
}

struct battery_state {
    uint8_t level;
    bool charging;
};

static struct battery_state battery_get_state(const zmk_event_t *eh) {
    return (struct battery_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .charging = zmk_usb_is_powered(),
#endif
    };
}

static void battery_update(struct battery_state battery) {
    state.battery = battery.level;
    state.charging = battery.charging;
    draw_status();
}

ZMK_DISPLAY_WIDGET_LISTENER(github_battery, struct battery_state, battery_update, battery_get_state)
ZMK_SUBSCRIPTION(github_battery, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(github_battery, zmk_usb_conn_state_changed);
#endif

static bool connection_get_state(const zmk_event_t *eh) {
    return zmk_split_bt_peripheral_is_connected();
}

static void connection_update(bool connected) {
    state.connected = connected;
    draw_status();
}

ZMK_DISPLAY_WIDGET_LISTENER(github_connection, bool, connection_update, connection_get_state)
ZMK_SUBSCRIPTION(github_connection, zmk_split_peripheral_status_changed);

static void animate(void *obj, int32_t elapsed) {
    lv_obj_set_x(logo, 70 - github_bob(elapsed));
    unsigned step = github_grid_step(elapsed);
    if (previous_step == (int)step) {
        return;
    }

    /* Only invalidate the grid when a visible cell changes, including reset. */
    bool changed = previous_step < 0;
    for (unsigned i = 0; i < 70 && !changed; ++i) {
        changed = github_cell_filled(i, step) != github_cell_filled(i, previous_step);
    }
    previous_step = step;
    if (!changed) {
        return;
    }

    lv_canvas_fill_bg(grid, lv_color_white(), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(grid, &layer);
    for (unsigned column = 0; column < 10; ++column) {
        for (unsigned row = 0; row < 7; ++row) {
            int x = 36 - row * 6;
            int y = column * 6;
            rect(&layer, x, y, 4, 4, true);
            if (!github_cell_filled(column * 7 + row, step)) {
                rect(&layer, x + 1, y + 1, 2, 2, false);
            }
        }
    }
    lv_canvas_finish_layer(grid, &layer);
}

static int32_t elapsed_path(const lv_anim_t *animation) {
    /* Avoid the default linear path's 1024-step quantization on this long loop. */
    return animation->act_time;
}

static void plain_object(lv_obj_t *obj, int x, int y, int w, int h) {
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *zmk_display_status_screen(void) {
    if (screen != NULL) {
        return screen;
    }
    screen = lv_obj_create(NULL);
    plain_object(screen, 0, 0, 160, 68);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    status = lv_canvas_create(screen);
    plain_object(status, 92, 0, 68, 68);
    lv_canvas_set_buffer(status, status_buffer, 68, 68, CANVAS_FORMAT);

    /* Portrait (x,y,w,h) maps to native (160-y-h,x,h,w), like stock nice!view. */
    lv_obj_t *art = lv_obj_create(screen);
    plain_object(art, 0, 0, 128, 68);
    lv_obj_set_style_bg_color(art, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(art, LV_OPA_COVER, 0);

    logo = lv_image_create(art);
    lv_image_set_src(logo, &github_mark);
    lv_obj_set_pos(logo, 70, 10);
    lv_obj_t *caption = lv_image_create(art);
    lv_image_set_src(caption, &github_caption);
    lv_obj_set_pos(caption, 46, 0);

    grid = lv_canvas_create(art);
    plain_object(grid, 3, 5, 40, 58);
    lv_canvas_set_buffer(grid, grid_buffer, 40, 58, CANVAS_FORMAT);

    github_battery_init();
    github_connection_init();

    /* One LVGL-owned loop: display blanking stops ZMK's existing display ticks. */
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, screen);
    lv_anim_set_exec_cb(&animation, animate);
    lv_anim_set_path_cb(&animation, elapsed_path);
    lv_anim_set_values(&animation, 0, GITHUB_LOOP_MS);
    lv_anim_set_duration(&animation, GITHUB_LOOP_MS);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation);
    return screen;
}
