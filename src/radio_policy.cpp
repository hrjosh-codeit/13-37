#include "radio_policy.h"
#include "ble_scan_manager.h"
#include "analyze_screen.h"
#include "nfc_screen.h"
#include "pager.h"
#include "pingsweep.h"
#include "portscan.h"
#include "tpms.h"
#include "wifi_beacon_manager.h"
#include "wifi_screen.h"

static bool s_low_power_mode;

bool radio_policy_low_power_mode()
{
    return s_low_power_mode;
}

void radio_policy_set_low_power_mode(bool enabled)
{
    if (s_low_power_mode == enabled) return;
    s_low_power_mode = enabled;

    ble_scan_manager_set_low_power_mode(enabled);
    wifi_beacon_manager_set_low_power_mode(enabled);
    wifi_screen_set_low_power_mode(enabled);
    analyze_screen_set_low_power_mode(enabled);
    nfc_screen_set_low_power_mode(enabled);

    if (!enabled) return;
    pingsweep_stop();
    portscan_stop();
    if (pager_is_scanning_all()) pager_stop();
    if (tpms_is_running()) tpms_stop();
}