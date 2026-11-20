#include "vacuum_receiver.h"
#include <WiFi.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "wifi_beacon_manager.h"
#include <math.h>
#include <string.h>

struct SenderVacuumPacket {
    float pressure_torr;
    float pressure_mbar;
    float pressure_pa;
    float gauge_voltage;
    float gpio_voltage;
};

static_assert(sizeof(SenderVacuumPacket) == 20,
              "SuperBee ESP-NOW packet must remain 20 bytes");

static portMUX_TYPE s_reading_mux = portMUX_INITIALIZER_UNLOCKED;
static VacuumReading s_reading = {};
static wifi_mode_t s_previous_wifi_mode = WIFI_OFF;
static bool s_running = false;
static bool s_previous_mode_valid = false;
static const char *s_error = nullptr;
static uint8_t s_channel = 1;
static uint32_t s_last_channel_scan_ms = 0;

static void on_vacuum_packet(const uint8_t *mac, const uint8_t *data, int length)
{
    if (!data || length != (int)sizeof(SenderVacuumPacket)) return;

    SenderVacuumPacket packet;
    memcpy(&packet, data, sizeof(packet));
    if (!isfinite(packet.pressure_torr) ||
        !isfinite(packet.pressure_mbar) ||
        !isfinite(packet.pressure_pa) ||
        !isfinite(packet.gauge_voltage) ||
        !isfinite(packet.gpio_voltage) ||
        packet.pressure_torr < 0.0f ||
        packet.pressure_torr > 1000.0f ||
        packet.pressure_mbar < 0.0f ||
        packet.pressure_pa < 0.0f)
        return;

    portENTER_CRITICAL(&s_reading_mux);
    s_reading.pressure_torr = packet.pressure_torr;
    s_reading.pressure_mbar = packet.pressure_mbar;
    s_reading.pressure_pa = packet.pressure_pa;
    s_reading.gauge_voltage = packet.gauge_voltage;
    s_reading.gpio_voltage = packet.gpio_voltage;
    s_reading.received_ms = millis();
    if (mac) memcpy(s_reading.sender_mac, mac, 6);
    s_reading.valid = true;
    portEXIT_CRITICAL(&s_reading_mux);
}

bool vacuum_receiver_start()
{
    if (s_running) return true;
    s_error = nullptr;

    if (wifi_beacon_active()) {
        s_error = "Stop WiFi scanning before receiving";
        return false;
    }

    s_previous_wifi_mode = WiFi.getMode();
    s_previous_mode_valid = true;
    if (!WiFi.mode(WIFI_STA)) {
        s_error = "Could not start WiFi radio";
        return false;
    }
    if (WiFi.status() != WL_CONNECTED &&
        esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        s_error = "Could not set WiFi channel";
        WiFi.mode(s_previous_wifi_mode);
        return false;
    }

    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
        s_error = "ESP-NOW initialization failed";
        WiFi.mode(s_previous_wifi_mode);
        return false;
    }
    result = esp_now_register_recv_cb(on_vacuum_packet);
    if (result != ESP_OK) {
        s_error = "ESP-NOW receiver setup failed";
        esp_now_deinit();
        WiFi.mode(s_previous_wifi_mode);
        return false;
    }

    portENTER_CRITICAL(&s_reading_mux);
    memset(&s_reading, 0, sizeof(s_reading));
    portEXIT_CRITICAL(&s_reading_mux);
    s_running = true;
    return true;
}

void vacuum_receiver_stop()
{
    if (s_running) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        s_running = false;
    }
    if (s_previous_mode_valid) {
        WiFi.mode(s_previous_wifi_mode);
        s_previous_mode_valid = false;
    }
}

void vacuum_receiver_poll()
{
    if (!s_running || WiFi.status() == WL_CONNECTED || wifi_beacon_active())
        return;

    uint32_t now = millis();
    if (now - s_last_channel_scan_ms < 650) return;

    bool stale = true;
    portENTER_CRITICAL(&s_reading_mux);
    if (s_reading.valid && now - s_reading.received_ms <= 3000)
        stale = false;
    portEXIT_CRITICAL(&s_reading_mux);
    if (!stale) return;

    s_last_channel_scan_ms = now;
    uint8_t next = s_channel == 13 ? 1 : (uint8_t)(s_channel + 1);
    if (esp_wifi_set_channel(next, WIFI_SECOND_CHAN_NONE) == ESP_OK)
        s_channel = next;
}

bool vacuum_receiver_is_running()
{
    return s_running;
}

const char *vacuum_receiver_error()
{
    return s_error ? s_error : "";
}

void vacuum_receiver_get_reading(VacuumReading *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_reading_mux);
    *out = s_reading;
    portEXIT_CRITICAL(&s_reading_mux);
}
