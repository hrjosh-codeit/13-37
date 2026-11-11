#pragma once
#include <lvgl.h>

void wifi_screen_create();
void wifi_screen_show();
bool wifi_screen_is_active();
void wifi_screen_set_low_power_mode(bool enabled);
