#include <stddef.h>
#include <stdio.h>
#include "command_check_version.h"
#include "esp_log.h"
const char* TAG = "at_gmr";

void check_version_information(char **argv, uint8_t argc)
{
    ESP_LOGI(TAG, "Response: AT version:1.0.0.0");
    ESP_LOGI(TAG, "OK");
}



