/**
 * @file wifi_connect.c
 * @author 1)Yogita Sutar 
 * @brief Wi-Fi connection handling for ESP32 with SD card credentials.
 *
 * This module initializes Wi-Fi, manages events (connect/disconnect/IP),
 * retries connections, and can read Wi-Fi credentials from an SD card file.
 * @version 0.2
 * @date 2026-01-22
 */

#include "WIFIHandler.h"
#include "cJSON.h"

static EventGroupHandle_t s_wifi_event_group; /**< Wi-Fi event group for connection status */
static int IntRetryNum = 0;                   /**< Retry counter for Wi-Fi */

static bool wifi_got_ip = false;

/**
 * @brief Wi-Fi and IP event handler.
 *
 * Handles:
 * - Wi-Fi start: attempts to connect
 * - Wi-Fi disconnect: retries connection or restarts Wi-Fi
 * - Got IP: logs IP and sets connected bit
 *
 * @param arg User argument (unused)
 * @param event_base Event base (Wi-Fi or IP)
 * @param event_id Event ID
 * @param event_data Event-specific data
 */
static void event_handler(void* arg,esp_event_base_t event_base,int32_t event_id,void* event_data)
{
    /*WIFI DISCONNECTED*/
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAGWIFI, "WiFi disconnected");

        if (IntRetryNum < 3)
        {
            IntRetryNum++;
			
			wifi_got_ip = false;
			//SNTP_Stop();  
        
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    	xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);

            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK)
            {
                ESP_LOGW(TAGWIFI, "esp_wifi_connect failed: %s",
                         esp_err_to_name(err));
            }
        }
        else
        {
            ESP_LOGE(TAGWIFI, "WiFi retry failed");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }

    /*GOT IP*/
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        if (event_data == NULL)
        {
            ESP_LOGE(TAGWIFI, "IP event data NULL");
            return;
        }

        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;

        ESP_LOGI(TAGWIFI, "Got IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        IntRetryNum = 0;
		wifi_got_ip = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        //SNTP_Start();
    }
}

/**
 * @brief Initialize Wi-Fi (station mode).
 *
 * - Initializes NVS
 * - Creates default Wi-Fi STA
 * - Registers event handlers
 * - Starts Wi-Fi
 */
void InitWifiAll(void)
{
    esp_err_t ret;

    /*NVS*/
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /*Event Group*/
    if (s_wifi_event_group == NULL)
    {
        s_wifi_event_group = xEventGroupCreate();
    }

    /*Netif & Event Loop*/
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    /*WiFi Init*/
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /*Event Handlers*/
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    /*WiFi Mode*/
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	
	/*Start WiFi FIRST*/
	ESP_ERROR_CHECK(esp_wifi_start());
	
	/*Now connect*/
	WIFICridential();

}


/**
 * @brief Attempt to connect to a Wi-Fi network.
 *
 * Sets Wi-Fi config, clears previous status bits,
 * and waits up to 8 seconds for success/failure.
 *
 * @param ssid SSID of the network
 * @param password Password of the network
 * @return true if connected successfully, false otherwise
 */

bool WifiTryConnect(const char *ssid, const char *pass)
{
    wifi_config_t cfg = {0};

    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);

    /* ---------- TRY WPA3 FIRST ---------- */
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA3_PSK;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;
    

    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(5000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAGWIFI, "Connected using WPA3");
        return true;
    }

    /* ---------- FALLBACK TO WPA2 ---------- */
    ESP_LOGW(TAGWIFI, "WPA3 failed, retrying WPA2");

    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = false;
    cfg.sta.pmf_cfg.required = false;

    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(5000)
    );

    return (bits & WIFI_CONNECTED_BIT);
}

/**
 * @brief Connect using default Wi-Fi credentials.
 *
 * Uses compile-time defined SSID and password.
 */
void WifiConnectDefault(void)
{
    ESP_LOGW(TAGWIFI, "Using default WiFi credentials...");

    if (!WIFI_SSID || !WIFI_PASS || strlen(WIFI_SSID) == 0)
    {
        ESP_LOGE(TAGWIFI, "Default WiFi credentials invalid");
        return;
    }

    bool ok = WifiTryConnect(WIFI_SSID, WIFI_PASS);
    if (!ok)
    {
        ESP_LOGE(TAGWIFI, "Default WiFi connection failed!");
    }
}


/**
 * @brief Read Wi-Fi credentials from an SD card file.
 *
 * Each line must be in format:
 * ```
 * SSID,PASSWORD
 * ```
 *
 * Attempts each SSID in order until successful.
 *
 * @param FilePath Path to Wi-Fi credentials file
 */
void ReadFromSDCard(const char *path)
{
    ESP_LOGI(TAGWIFI, "Reading WiFi JSON file...");

    if (!ReadWifiFromJson(path))
    {
        ESP_LOGW(TAGWIFI, "JSON WiFi failed, using default");
        WifiConnectDefault();
    }
}

/**
 * @brief Parse WiFi credentials from JSON file and try connecting.
 *
 * JSON format:
 * {
 *   "wifi": [
 *     { "ssid": "SSID1", "password": "PASS1" },
 *     { "ssid": "SSID2", "password": "PASS2" }
 *   ]
 * }
 *
 * Each SSID is tried sequentially until a successful connection.
 *
 * @param path Path to JSON file.
 * @return true if connected to any WiFi network.
 * @return false if all attempts fail.
 */
bool ReadWifiFromJson(const char *path)
{
    if (!path)
        return false;

    FILE *f = fopen(path, "r");
    if (!f)
    {
        ESP_LOGW(TAGWIFI, "WiFi JSON file not found");
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0 || size > 2048)
    {
        fclose(f);
        ESP_LOGE(TAGWIFI, "Invalid JSON size");
        return false;
    }

    char *json_buf = malloc(size + 1);
    if (!json_buf)
    {
        fclose(f);
        ESP_LOGE(TAGWIFI, "JSON malloc failed");
        return false;
    }

    size_t len = fread(json_buf, 1, size, f);
    fclose(f);
    json_buf[len] = '\0';

    cJSON *root = cJSON_Parse(json_buf);
    free(json_buf);

    if (!root)
    {
        ESP_LOGE(TAGWIFI, "JSON parse error");
        return false;
    }

    cJSON *wifi_arr = cJSON_GetObjectItem(root, "wifi");
    if (!cJSON_IsArray(wifi_arr))
    {
        ESP_LOGE(TAGWIFI, "wifi array missing");
        cJSON_Delete(root);
        return false;
    }

    cJSON *entry;
    cJSON_ArrayForEach(entry, wifi_arr)
    {
        cJSON *ssid = cJSON_GetObjectItem(entry, "ssid");
        cJSON *pass = cJSON_GetObjectItem(entry, "password");

        if (cJSON_IsString(ssid) && cJSON_IsString(pass))
        {
            ESP_LOGI(TAGWIFI, "Trying SSID='%s'", ssid->valuestring);

            if (WifiTryConnect(ssid->valuestring,
                               pass->valuestring))
            {
                ESP_LOGI(TAGWIFI, "Connected to %s",
                         ssid->valuestring);
                cJSON_Delete(root);
                return true;
            }
        }
    }

    cJSON_Delete(root);
    return false;
}

/**
 * @brief Manage WiFi credentials source.
 *
 * If SD card is mounted, WiFi credentials are read from JSON file.
 * Otherwise, default compile-time credentials are used.
 */
void WIFICridential(void)
{
    if (GSdMount)
    {
        const char *path = MOUNT_POINT "/CONFIG.txt";
        
        ReadFromSDCard(path);
        return;
    }

    WifiConnectDefault();
}

/**
 * @brief Check current WiFi connection status.
 *
 * @return true if WiFi is connected.
 * @return false otherwise.
 */

bool GetWifiConnect(void)
{
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// simple safe version inside WIFIHandler.c
bool IsWifiGotIP(void)
{
    return wifi_got_ip;   // or esp_netif_get_ip_info based flag
}
