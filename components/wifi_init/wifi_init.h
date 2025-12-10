// wifi_init.h

#ifndef WIFI_INIT_H
#define WIFI_INIT_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

typedef struct {
    const char *ssid;
    const char *password;
    uint8_t channel;
    uint8_t max_conn;
    const char *ip;
    const char *gateway;
    const char *netmask;
} wifi_softap_config_t;

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);

void wifi_init_softap(const wifi_softap_config_t *config);

#endif // WIFI_INIT_H
