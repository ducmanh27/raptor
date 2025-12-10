/* 
 * Author: Phan Duc Manh
 * License: MIT License
 * Date: 10/12/2025
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_mac.h"
#include "nvs_flash.h"
#include "wifi_init.h"
#include "network_config.h"

static const char *TAG = "main";

static EventGroupHandle_t eth_event_group;
const int ETH_CONNECTED_BIT = BIT0;

static EventGroupHandle_t wifi_event_group;
const int WIFI_READY_BIT = BIT1;

void wifi_ap_ready_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_START) {
        xEventGroupSetBits(wifi_event_group, WIFI_READY_BIT);
        ESP_LOGI(TAG, "WiFi AP is ready for TCP server");
    }
}


/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    // Set bit để báo Ethernet đã có IP
    xEventGroupSetBits(eth_event_group, ETH_CONNECTED_BIT);
}

int write_data(int sock, char *rx_buffer, int len) {
    int to_write = len;
    
    while (to_write > 0) {
        int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
        
        if (written < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            return -1;
        }
        
        to_write -= written;
    }
    
    return 0;
}


static void tcp_server_ethernet_task(void *pvParameters)
{
    const char *TASK_TAG = "ethernet_tcp_server";
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    
    // Đợi Ethernet có IP
    ESP_LOGI(TASK_TAG, "[Ethernet] Waiting for Ethernet connection...");
    xEventGroupWaitBits(eth_event_group, ETH_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TASK_TAG, "[Ethernet] Ethernet connected! Starting TCP server...");

    while (1) {
        struct sockaddr_storage dest_addr;

        if (addr_family == AF_INET) {
            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
            dest_addr_ip4->sin_family = AF_INET;
            dest_addr_ip4->sin_port = htons(ETH_TCP_PORT);  // <--- SỬA DÒNG NÀY
            ip_protocol = IPPROTO_IP;
        }

        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            ESP_LOGE(TASK_TAG, "[Ethernet] Unable to create socket: errno %d", errno);
            break;
        }
        
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        ESP_LOGI(TASK_TAG, "[Ethernet] Socket created");

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "[Ethernet] Socket unable to bind: errno %d", errno);
            ESP_LOGE(TASK_TAG, "[Ethernet] IPPROTO: %d", addr_family);
            goto CLEAN_UP;
        }
        ESP_LOGI(TASK_TAG, "[Ethernet] Socket bound, port %d", ETH_TCP_PORT);

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "[Ethernet] Error occurred during listen: errno %d", errno);
            goto CLEAN_UP;
        }

        ESP_LOGI(TASK_TAG, "TCP Server listening on %s:%d", 
                 ETH_STATIC_IP, ETH_TCP_PORT);

        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t addr_len = sizeof(source_addr);
            int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TAG, "[Ethernet] Unable to accept connection: errno %d", errno);
                break;
            }

            // Set tcp keepalive option
            setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

            // Convert ip address to string
            if (source_addr.ss_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            }

            ESP_LOGI(TASK_TAG, "[Ethernet] Socket accepted from IP:%s", addr_str);

            while (1) {
                int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                
                if (len < 0) {
                    ESP_LOGE(TASK_TAG, "[Ethernet] recv failed: errno %d", errno);
                    break;
                } else if (len == 0) {
                    ESP_LOGI(TASK_TAG, "[Ethernet] Connection closed");
                    break;
                } else {
                    rx_buffer[len] = 0; // Null-terminate
                    ESP_LOGI(TASK_TAG, "[Ethernet] Received %d bytes: %s", len, rx_buffer);

                    // test echo back
                    // if (write_data(sock, rx_buffer, len) < 0) {
                    //     ESP_LOGE(TAG, "Failed to send data back, cleaning up...");
                    //     goto CLIENT_CLEANUP;
                    // }
                }
            }

// CLIENT_CLEANUP:
//             shutdown(sock, 0);
//             close(sock);
//             ESP_LOGI(TAG, "Client disconnected");
        }

CLEAN_UP:
        close(listen_sock);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}

// TCP Server Task cho WiFi
static void tcp_server_wifi_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    
    const char *TASK_TAG = "wifi_tcp_server";
    
    // Đợi WiFi SoftAP sẵn sàng
    ESP_LOGI(TASK_TAG, "Waiting for WiFi AP to start...");
    xEventGroupWaitBits(wifi_event_group, WIFI_READY_BIT, false, true, portMAX_DELAY);
    
    // Đợi thêm 1 chút để đảm bảo interface hoàn toàn ready
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TASK_TAG, "WiFi AP ready! Starting TCP server...");

    while (1) {
        struct sockaddr_storage dest_addr;

        if (addr_family == AF_INET) {
            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
            dest_addr_ip4->sin_addr.s_addr = inet_addr(WIFI_AP_IP);
            dest_addr_ip4->sin_family = AF_INET;
            dest_addr_ip4->sin_port = htons(WIFI_TCP_PORT);
            ip_protocol = IPPROTO_IP;
        }

        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            ESP_LOGE(TASK_TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        // Set socket options
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        ESP_LOGI(TASK_TAG, "Socket created");

        // Bind socket vào địa chỉ WiFi AP
        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "Socket unable to bind: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        ESP_LOGI(TASK_TAG, "Socket bound to %s:%d", 
                 WIFI_AP_IP, WIFI_TCP_PORT);

        // Listen for connections
        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "Error occurred during listen: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TASK_TAG, "WiFi TCP Server listening on %s:%d", 
                 WIFI_AP_IP, WIFI_TCP_PORT);

        // Accept connections loop
        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t addr_len = sizeof(source_addr);
            
            int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TASK_TAG, "Unable to accept connection: errno %d", errno);
                break;
            }

            // Set TCP keepalive options
            setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

            // Convert client IP address to string
            if (source_addr.ss_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, 
                           addr_str, sizeof(addr_str) - 1);
            }

            ESP_LOGI(TASK_TAG, "[WiFi] Client connected from IP: %s", addr_str);

            // Receive data loop
            while (1) {
                int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                
                if (len < 0) {
                    ESP_LOGE(TASK_TAG, "recv failed: errno %d", errno);
                    break;
                } else if (len == 0) {
                    ESP_LOGI(TASK_TAG, "[WiFi] Connection closed by client");
                    break;
                } else {
                    rx_buffer[len] = 0; // Null-terminate
                    ESP_LOGI(TASK_TAG, "[WiFi] Received %d bytes: %s", len, rx_buffer);

                    // Echo back (tùy chọn)
                    int to_write = len;
                    while (to_write > 0) {
                        int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                        if (written < 0) {
                            ESP_LOGE(TASK_TAG, "Error sending data: errno %d", errno);
                            break;
                        }
                        to_write -= written;
                    }
                }
            }

            // Cleanup client connection
            shutdown(sock, 0);
            close(sock);
            ESP_LOGI(TASK_TAG, "[WiFi] Client disconnected");
        }

        // Cleanup listening socket
        close(listen_sock);
        ESP_LOGI(TASK_TAG, "Restarting WiFi TCP server...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    eth_event_group = xEventGroupCreate();
    wifi_event_group = xEventGroupCreate();

    // Enable external oscillator for LAN8720A (GPIO 16)
    // GPIO 16 is pulled down at boot to allow IO0 strapping
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_16, 1));
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for oscillator to stabilize
    
    ESP_LOGI(TAG, "External oscillator enabled on GPIO 16");
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *eth_netifs[eth_port_cnt];
    esp_eth_netif_glue_handle_t eth_netif_glues[eth_port_cnt];

    // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
    // default esp-netif configuration parameters.
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netifs[0] = esp_netif_new(&cfg);
    eth_netif_glues[0] = esp_eth_new_netif_glue(eth_handles[0]);

    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton(ETH_STATIC_IP);  
    ip_info.netmask.addr = esp_ip4addr_aton(ETH_NETMASK);
    ip_info.gw.addr = esp_ip4addr_aton(ETH_GATEWAY); 

    // Cấu hình IP tĩnh cho giao diện Ethernet
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netifs[0]));  // Dừng DHCP Client
    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netifs[0], &ip_info));

    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[0], eth_netif_glues[0]));
    

    // Register user defined event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, wifi_ap_ready_handler, NULL));
    // Start Ethernet driver state machine
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));

    wifi_softap_config_t wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
        .channel = WIFI_CHANNEL,
        .max_conn = WIFI_MAX_CONN,
        .ip = WIFI_AP_IP,
        .gateway = WIFI_AP_GATEWAY,
        .netmask = WIFI_AP_NETMASK
    };
    // Start WiFi SoftAP
    wifi_init_softap(&wifi_config);

    xTaskCreate(tcp_server_ethernet_task, "tcp_eth_server", 4096, NULL, 5, NULL);
    xTaskCreate(tcp_server_wifi_task, "tcp_wifi_server", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Both TCP servers started");
    ESP_LOGI(TAG, "Ethernet: %s:%d", ETH_STATIC_IP, ETH_TCP_PORT); 
    ESP_LOGI(TAG, "WiFi AP:  %s:%d", WIFI_AP_IP, WIFI_TCP_PORT); 
}
