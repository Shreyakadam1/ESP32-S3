#include "OTA.h"
#include "RTCHandler.h"
#include "SDCardHandler.h"
#include <stdlib.h>

#define CERT_PATH   MOUNT_POINT "/OTACER.txt"
#define CERT_MAX_SIZE 2048

static char cert_buffer[CERT_MAX_SIZE];

char OTAschedulestr[] = "120600,1"; // Example: daily at 00:00:00, repeat every 1 hour


bool IsNewVersion(const char *currentversion, const char *latestversion)
{
    if(!currentversion || !latestversion) 
    	return false;
    return (CompareVersions(currentversion, latestversion) < 0);
}



static int ReadHTTPResponse(esp_http_client_handle_t client, char *httpresponsebuffer, int buffermaxlength)
{
    int totalbytesread = 0;

    while (1) {
        int bytesread = esp_http_client_read(client, httpresponsebuffer + totalbytesread, buffermaxlength - totalbytesread - 1);
        if(bytesread <= 0) 
        	break;

        totalbytesread += bytesread;
        if(totalbytesread >= buffermaxlength - 1) 
        	break;
    }

    httpresponsebuffer[totalbytesread] = '\0';
    return totalbytesread;
}

bool CheckLatestVersion(char *latestversion, char *downloadURL)
{
    ESP_LOGI(OTATAG, "Checking local OTA server for latest firmware...");

    esp_http_client_config_t config = {
        .url = OTA_VERSION_URL,
        .timeout_ms = 15000,
        .crt_bundle_attach = NULL,
        .cert_pem = cert_buffer, 
        .buffer_size = 4096,
        .buffer_size_tx = 4096
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if(!client) {
        ESP_LOGE(OTATAG, "Failed to init HTTP client");
        return false;
    }

    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(OTATAG, "Connection failed");
        esp_http_client_cleanup(client);
        return false;
    }

	esp_http_client_fetch_headers(client);

    char *jsonbuffer = heap_caps_malloc(JSON_BUFFER_SIZE, MALLOC_CAP_8BIT);
    if(!jsonbuffer) {
        ESP_LOGE(OTATAG, "Heap alloc failed (JSON buffer)");
        esp_http_client_cleanup(client);
        return false;
    }
    
	memset(jsonbuffer, 0, JSON_BUFFER_SIZE);

   int length = ReadHTTPResponse(client, jsonbuffer, JSON_BUFFER_SIZE);
    esp_http_client_cleanup(client);

    if (length <= 0) {
        ESP_LOGE(OTATAG, "JSON read failed");
        heap_caps_free(jsonbuffer);
        return false;
    } 

	ESP_LOGI(OTATAG, "JSON RECEIVED:\n%s", jsonbuffer);

    cJSON *jsonroot = cJSON_Parse(jsonbuffer);
    if (!jsonroot) {
        ESP_LOGE(OTATAG, "JSON parse error");
        heap_caps_free(jsonbuffer);
        return false;
    }

    cJSON *ver = cJSON_GetObjectItem(jsonroot, "version");
    cJSON *fw = cJSON_GetObjectItem(jsonroot, "firmwareURL");

    if (!ver || !fw || !cJSON_IsString(ver) || !cJSON_IsString(fw)) {
        cJSON_Delete(jsonroot);
        return false;
    }

    strncpy(latestversion, ver->valuestring, 32);
    strncpy(downloadURL, fw->valuestring, 256);

    ESP_LOGI(OTATAG, "Latest Version = %s", latestversion);
    ESP_LOGI(OTATAG, "Firmware URL = %s", downloadURL);

    cJSON_Delete(jsonroot);
    heap_caps_free(jsonbuffer);
    
    return true;
}

void PerformOTAUpdate(const char *downloadUrl)
{
    ESP_LOGW(OTATAG, "Starting OTA update...");

    esp_http_client_config_t http_config = {
        .url = downloadUrl,
        .timeout_ms = 20000,
        .crt_bundle_attach = NULL,
        .cert_pem = cert_buffer,
        .buffer_size = OTA_BUFFER_SIZE,
        .buffer_size_tx = OTA_BUFFER_SIZE,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
        .partial_http_download = false
    };
    
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(OTATAG, "OTA SUCCESS!");
        esp_ota_mark_app_valid_cancel_rollback();
        //ResetCrashCounter();
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(OTATAG, "OTA FAILED: 0x%x", ret);
    }
}

ota_schedule_t ParseOTASchedule(const char *schedulestr)
{
    ota_schedule_t schedule = {0};

    char schedulecopy[20];
	strncpy(schedulecopy, schedulestr, sizeof(schedulecopy) - 1);
	schedulecopy[sizeof(schedulecopy) - 1] = '\0';

    char *commaposition = strchr(schedulecopy, ',');

    *commaposition = '\0';
    char *timestr = schedulecopy;
    char *repeatstr = commaposition + 1;

    long timevalue = atol(timestr);
    schedule.repeat_hours = atoi(repeatstr);

    if (timevalue <= 0)
    {
        schedule.is_fixed_time = false;  // frequency mode
    }
    else
    {
        schedule.is_fixed_time = true;
        schedule.hour   = (timevalue / 10000) % 100;
        schedule.minute = (timevalue / 100)   % 100;
        schedule.second = timevalue % 100;
    }

    return schedule;
}

bool ShouldTriggerOTA(struct tm *currenttime, ota_schedule_t *OTAschedule)
{
    if (OTAschedule->is_fixed_time)
    {
        return (currenttime->tm_hour == OTAschedule->hour && currenttime->tm_min  == OTAschedule->minute && currenttime->tm_sec  == OTAschedule->second);
    }

    int elapsedminutestoday = currenttime->tm_hour * 60 + currenttime->tm_min;
    int intervalMinutes = OTAschedule->repeat_hours * 60;

    if (intervalMinutes <= 0) 
    	return false;

    return (elapsedminutestoday % intervalMinutes == 0);
}

int CompareVersions(const char *currentVersion, const char *targetVersion)
{
    int major1, minor1;
    int major2, minor2;
    
    sscanf(currentVersion, "V%d.%d", &major1, &minor1);
    sscanf(targetVersion, "V%d.%d", &major2, &minor2);

    if (major1 != major2) 
    	return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) 
    	return (minor1 > minor2) ? 1 : -1;
    	
    return 0;
}

void OTATask(void *params)
{
    struct tm rtctime;
    char latestversionstr[32];
    char firmwaredownloadURL[256];
    time_t lastNtpsyncepoch = 0;
    
    if (!GSdMount) {
    	ESP_LOGE(OTATAG, "SD not mounted");
	}

    bool read_ok = SD_ReadFile(CERT_PATH, cert_buffer, CERT_MAX_SIZE);

   	if (!read_ok) {
       	ESP_LOGE(OTATAG, "Certificate read failed");
   	}
   	else {
    	ESP_LOGI(OTATAG, "Certificate loaded successfully");			// Print certificate content
    	ESP_LOGI(OTATAG, "Certificate Content:\n%s", cert_buffer);
    }
    
    ota_schedule_t OTAschedule = ParseOTASchedule(OTAschedulestr);

    ESP_LOGW(OTATAG, "Parsed Schedule: fixed=%d, time=%02d:%02d:%02d repeat=%d",
             OTAschedule.is_fixed_time,
             OTAschedule.hour, OTAschedule.minute, OTAschedule.second,
             OTAschedule.repeat_hours);

    while(1)
    {
        time_t  currentepoch;
        time(& currentepoch);
       
        if (lastNtpsyncepoch==0)
        	lastNtpsyncepoch =  currentepoch;

       if ((currentepoch - lastNtpsyncepoch) >= (NTP_SYNC_INTERVAL_HRS * 3600))
        {
			ESP_LOGW(OTATAG, "Performing scheduled NTP sync...");
			SyncNTPAndUpdateRTC();
				time_t 	NTPtime;
				time(&NTPtime);

				if (NTPtime > MIN_VALID_UNIX_TIME) 
					lastNtpsyncepoch = NTPtime;
			
		}

        if (GetTimeRTC(&rtctime) != RETURN_NUM_RTC_READSUCCESSFULLY) {
            ESP_LOGE(OTATAG,"RTC read failed");
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }

        ESP_LOGI(OTATAG,"RTC %02d:%02d:%02d", rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);

        if (ShouldTriggerOTA(&rtctime,&OTAschedule))
        {
			ESP_LOGW(OTATAG, "OTA Time reached → Checking update...");
			
            memset(latestversionstr,0,sizeof(latestversionstr));
            memset(firmwaredownloadURL,0,sizeof(firmwaredownloadURL));
            
            if (CheckLatestVersion(latestversionstr,firmwaredownloadURL))
            {
                if (IsNewVersion(CURRENT_FIRMWARE_VERSION,latestversionstr))
           		{
					ESP_LOGW(OTATAG, "New firmware found → downloading...");
                    PerformOTAUpdate(firmwaredownloadURL);
                }
                else
				{
                    ESP_LOGI(OTATAG, "Already up to date.");
                }
            } 
            else 
            {
            	ESP_LOGE(OTATAG,"Version check failed");
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}