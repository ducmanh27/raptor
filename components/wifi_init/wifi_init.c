// wifi_init.c

#include "wifi_init.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"

static const char *TAG = "wifi_init";

// Hàm này xử lý các sự kiện WiFi
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}
// Cấu hình và khởi động WiFi SoftAP
void wifi_init_softap(const wifi_softap_config_t *config) {
    ESP_LOGI(TAG, "Starting WiFi SoftAP...");

    esp_netif_t *wifi_ap_netif = esp_netif_create_default_wifi_ap();

    // ===== CẤU HÌNH IP TĨNH CHO WIFI AP =====
    esp_netif_ip_info_t ip_info;
    
    // Dừng DHCP server trước khi set IP
    esp_netif_dhcps_stop(wifi_ap_netif);
    
    // Set IP cho WiFi AP
    ip_info.ip.addr = esp_ip4addr_aton(config->ip);
    ip_info.gw.addr = esp_ip4addr_aton(config->gateway);
    ip_info.netmask.addr = esp_ip4addr_aton(config->netmask);
    
    ESP_ERROR_CHECK(esp_netif_set_ip_info(wifi_ap_netif, &ip_info));
    
    // Khởi động lại DHCP server với cấu hình mới
    ESP_ERROR_CHECK(esp_netif_dhcps_start(wifi_ap_netif));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(config->ssid),
            .channel = config->channel,
            .max_connection = config->max_conn,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, config->ssid, sizeof(wifi_config.ap.ssid));
    strncpy((char *)wifi_config.ap.password, config->password, sizeof(wifi_config.ap.password));


    if (strlen(config->password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi SoftAP started: SSID=%s channel:%d", 
             config->ssid, config->channel);
}
