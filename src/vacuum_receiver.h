#pragma once
#include <stdint.h>

struct VacuumReading {
    float pressure_torr;
    float pressure_mbar;
    float pressure_pa;
    float gauge_voltage;
    float gpio_voltage;
    uint32_t received_ms;
    bool valid;
};

bool vacuum_receiver_start();
void vacuum_receiver_stop();
void vacuum_receiver_poll();
bool vacuum_receiver_is_running();
const char *vacuum_receiver_error();
void vacuum_receiver_get_reading(VacuumReading *out);
