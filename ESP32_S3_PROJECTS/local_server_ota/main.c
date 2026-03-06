#include "OTA.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "WIFIHandler.h"
#include "I2CHandler.h"

void app_main(void)
{
   InitWifiAll();
    I2cMasterInit();
    InitSdCard();
    xTaskCreate(OTATask, "OTA Task", 8192, NULL, 5, NULL);
}