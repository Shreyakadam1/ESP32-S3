#ifndef OTA_H
#define OTA_H

#include <stdbool.h>
#include <time.h>
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <esp_ota_ops.h>
#include "nvs_flash.h"

// Local OTA server configuration
#define OTA_BASE_URL        "https://192.168.21.85/ota"
#define OTA_VERSION_URL     OTA_BASE_URL "/version.json"
#define CURRENT_FIRMWARE_VERSION  "V1.0"

// Log tag
#define OTATAG "OTA"

// Buffer sizes
#define JSON_BUFFER_SIZE  4096
#define OTA_BUFFER_SIZE   8192

// NTP sync interval
#define NTP_SYNC_INTERVAL_HRS  1

// Minimum valid unix time
//#define MIN_VALID_UNIX_TIME 1672531200 // Jan 1, 2023

typedef struct {
    int hour;
    int minute;
    int second;
    int repeat_hours;
    bool is_fixed_time;
} ota_schedule_t;

// Function prototypes
bool IsNewVersion(const char *currentversion, const char *latestversion);
bool CheckLatestVersion(char *latestversion, char *downloadURL);
void PerformOTAUpdate(const char *downloadUrl);
ota_schedule_t ParseOTASchedule(const char *str);
bool ShouldTriggerOTA(struct tm *currenttime, ota_schedule_t *schedule);
void OTATask(void *params);
int CompareVersions(const char *currentVersion, const char *targetVersion);
bool FetchRollbackJSON(char *targetVersion, bool *rollback);
void CheckRemoteRollback(void);
void RemoteRollbackTask(void *params);

// Embedded self-signed certificate
//extern const unsigned char _etc_ssl_certs_ota_crt[];
//extern const size_t _etc_ssl_certs_ota_crt_len;

#endif