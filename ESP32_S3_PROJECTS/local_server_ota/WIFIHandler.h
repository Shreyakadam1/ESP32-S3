
/**
 * @file wifi_connect.h
 * @author
 *   1) Yogita Sutar
 * @brief WiFi connection handler for ESP32.
 *
 * This module manages WiFi connectivity on ESP32.
 * - Reads credentials from SD card (if available).
 * - Provides fallback to default SSID/PASS.
 * - Attempts auto-reconnection if WiFi fails.
 */
#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "Common.h"
#include "SDCardHandler.h"

/** @brief Logging tag for WiFi module */
#define TAGWIFI  "wifi_connect"

/** @brief Event bit set when WiFi is connected */
#define WIFI_CONNECTED_BIT BIT0
/** @brief Event bit set when WiFi connection fails */
#define WIFI_FAIL_BIT      BIT1

/** @brief Default WiFi SSID (used if SD card credentials are missing) */
#define WIFI_SSID "SM-0CD5-4-4G"  //"dlink-A915"
//"Test_MQTT_Router_EXT"
// "vivoY21T"

/** @brief Default WiFi Password */
#define WIFI_PASS "05022026-4"  //qtvnr74538"
//"begumpura123$"
//"Yogitasutar"

/** @brief Maximum number of WiFi networks stored */
#define MAX_WIFI 10

/** @brief Global flag: SD card mounted status */
extern bool GSdMount;



/**
 * @brief Try connecting to one WiFi network
 * 
 * @param ssid     WiFi SSID
 * @param password WiFi password
 * @return true if connected, false otherwise
 */
typedef struct {
    char ssid[32];
    char pass[64];
} wifi_creds_t;

extern wifi_creds_t creds_list[MAX_WIFI];
extern int total_networks;


/**
 * @brief Initialize WiFi from all available sources (SD card / defaults).
 *
 * Reads WiFi credentials from SD card file (if mounted).
 * If not available, connects using default SSID/PASS.
 */

void InitWifiAll(void);

/**
 * @brief Reads credentials and attempts connection.
 */
void WIFICridential(void);

/**
 * @brief Try connecting to one WiFi network.
 *
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return true if connected, false otherwise
 */
bool WifiTryConnect(const char *ssid, const char *password);

/**
 * @brief Connect to default WiFi network (defined in macros).
 */
void WifiConnectDefault(void);

/**
 * @brief Read WiFi credentials from SD card file.
 *
 * @param FilePath Path to WiFi credentials file
 *                 (each line in format "SSID,PASS")
 */
void ReadFromSDCard(const char *FilePath);

bool ReadWifiFromJson(const char *path);

/**
 * @brief Check WiFi connection status.
 *
 * @return true if connected, false otherwise
 */
bool GetWifiConnect(void);

bool IsWifiGotIP(void);


#endif // WIFI_CONNECT_H
