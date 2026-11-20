#include "vacuum_screen.h"
#include "vacuum_receiver.h"
#include <LilyGoLib.h>
#include <stdio.h>

void tools_screen_show();

static lv_obj_t *s_screen;
static lv_obj_t *s_pressure_label;
static lv_obj_t *s_unit_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_mbar_label;
static lv_obj_t *s_pa_label;
static lv_obj_t *s_gauge_voltage_label;
static lv_obj_t *s_gpio_voltage_label;

static lv_obj_t *make_value_label(lv_obj_t *parent, const char *text, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static void on_back(lv_event_t *)
{
    tools_screen_show();
}

static void on_update(lv_timer_t *)
{
    if (!vacuum_screen_is_active()) return;

    VacuumReading reading = {};
    vacuum_receiver_get_reading(&reading);
    if (!reading.valid) {
        lv_label_set_text(s_pressure_label, "--");
        lv_label_set_text(s_unit_label, "Torr");
        const char *error = vacuum_receiver_error();
        lv_label_set_text(s_status_label,
                          error[0] ? error : "Waiting for SuperBee ESP-NOW data");
        lv_obj_set_style_text_color(s_status_label,
                                    lv_color_make(0x88, 0x88, 0x88),
                                    LV_PART_MAIN);
        return;
    }

    uint32_t age_ms = millis() - reading.received_ms;
    if (age_ms > 3000) {
        lv_label_set_text(s_status_label, "NO SIGNAL");
        lv_obj_set_style_text_color(s_status_label,
                                    lv_color_make(0xFF, 0x66, 0x44),
                                    LV_PART_MAIN);
    } else {
        char status[40];
        snprintf(status, sizeof(status), "LIVE  %lu ms ago",
                 (unsigned long)age_ms);
        lv_label_set_text(s_status_label, status);
        lv_obj_set_style_text_color(s_status_label,
                                    lv_color_make(0x00, 0xCC, 0x66),
                                    LV_PART_MAIN);
    }

    char pressure[24];
    if (reading.pressure_torr < 1.0f) {
        snprintf(pressure, sizeof(pressure), "%.1f",
                 reading.pressure_torr * 1000.0f);
        lv_label_set_text(s_unit_label, "mTorr");
    } else {
        snprintf(pressure, sizeof(pressure), "%.2f", reading.pressure_torr);
        lv_label_set_text(s_unit_label, "Torr");
    }
    lv_label_set_text(s_pressure_label, pressure);

    char value[32];
    snprintf(value, sizeof(value), "mbar  %.4g", reading.pressure_mbar);
    lv_label_set_text(s_mbar_label, value);
    snprintf(value, sizeof(value), "Pa      %.4g", reading.pressure_pa);
    lv_label_set_text(s_pa_label, value);
    snprintf(value, sizeof(value), "Gauge  %.3f V", reading.gauge_voltage);
    lv_label_set_text(s_gauge_voltage_label, value);
    snprintf(value, sizeof(value), "Input    %.3f V", reading.gpio_voltage);
    lv_label_set_text(s_gpio_voltage_label, value);
}

static void on_gesture(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if (indev && lv_indev_get_gesture_dir(indev) == LV_DIR_TOP)
        tools_screen_show();
}

void vacuum_screen_create()
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_screen, 0, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "SUPERBEE");
    lv_obj_set_style_text_color(title, lv_color_make(0xFF, 0xCF, 0x24),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *back = lv_obj_create(s_screen);
    lv_obj_set_size(back, 72, 36);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 16, 10);
    lv_obj_set_style_bg_color(back, lv_color_make(0x22, 0x22, 0x22),
                              LV_PART_MAIN);
    lv_obj_set_style_radius(back, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(back, 0, LV_PART_MAIN);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "TOOLS");
    lv_obj_set_style_text_color(back_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(back_label);
    lv_obj_add_flag(back_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);

    s_status_label = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_16,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status_label, lv_color_make(0x88, 0x88, 0x88),
                                LV_PART_MAIN);
    lv_obj_set_width(s_status_label, 410);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 57);

    s_pressure_label = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_pressure_label,
                                lv_color_make(0xFF, 0xCF, 0x24),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pressure_label, &lv_font_montserrat_48,
                               LV_PART_MAIN);
    lv_obj_align(s_pressure_label, LV_ALIGN_CENTER, 0, -70);

    s_unit_label = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_unit_label, lv_color_make(0xCC, 0xCC, 0xCC),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(s_unit_label, &lv_font_montserrat_24,
                               LV_PART_MAIN);
    lv_obj_align(s_unit_label, LV_ALIGN_CENTER, 0, -24);

    s_mbar_label = make_value_label(s_screen, "mbar  --", 270);
    s_pa_label = make_value_label(s_screen, "Pa      --", 300);
    s_gauge_voltage_label = make_value_label(s_screen, "Gauge  -- V", 330);
    s_gpio_voltage_label = make_value_label(s_screen, "Input    -- V", 360);

    lv_obj_t *hint = lv_label_create(s_screen);
    lv_label_set_text(hint, "Readout stays live on the clock after opening");
    lv_obj_set_style_text_color(hint, lv_color_make(0x55, 0x55, 0x55),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_obj_add_event_cb(s_screen, on_gesture, LV_EVENT_GESTURE, NULL);
    lv_timer_create(on_update, 250, NULL);
}

void vacuum_screen_show()
{
    vacuum_receiver_start();
    lv_scr_load(s_screen);
    on_update(NULL);
}

bool vacuum_screen_is_active()
{
    return lv_screen_active() == s_screen;
}
