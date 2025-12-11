/* 
 * Author: Phan Duc Manh
 * License: MIT License
 * Date: 8/12/2025
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "ethernet_init.h"
#include "wifi_init.h"
#include "sdkconfig.h"
#include "network_config.h"
#include "bridge_core.h"

static const char *TAG = "main";

static EventGroupHandle_t eth_event_group;
const int ETH_CONNECTED_BIT = BIT0;

static EventGroupHandle_t wifi_event_group;
const int WIFI_READY_BIT = BIT1;

// Global bridge state
bridge_state_t g_bridge;

void wifi_ap_ready_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_START) {
        xEventGroupSetBits(wifi_event_group, WIFI_READY_BIT);
        ESP_LOGI(TAG, "WiFi AP is ready for TCP server");
    }
}

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
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

    xEventGroupSetBits(eth_event_group, ETH_CONNECTED_BIT);
}

// ========== ETHERNET TX TASK (Gửi đến Ethernet Client) ==========
static void ethernet_tx_task(void *pvParameters) {
    int sock = (int)pvParameters;
    bridge_message_t msg;
    const char *TASK_TAG = "eth_tx";
    
    ESP_LOGI(TASK_TAG, "Ethernet TX task started for socket %d", sock);
    
    while (1) {
        // Đợi message từ WiFi
        if (xQueueReceive(g_bridge.queue_wifi_to_eth, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TASK_TAG, "[WiFi->ETH] Forwarding %d bytes to Ethernet client", msg.len);
            
            // Gửi đến Ethernet client
            int to_write = msg.len;
            uint8_t *ptr = msg.data;
            
            while (to_write > 0) {
                int written = send(sock, ptr, to_write, 0);
                
                if (written < 0) {
                    ESP_LOGE(TASK_TAG, "send failed: errno %d", errno);
                    goto EXIT;
                }
                
                to_write -= written;
                ptr += written;
            }
            
            ESP_LOGI(TASK_TAG, "Successfully sent %d bytes to Ethernet client", msg.len);
        }
    }
    
EXIT:
    ESP_LOGI(TASK_TAG, "Ethernet TX task ended");
    vTaskDelete(NULL);
}

// ========== ETHERNET SERVER TASK ==========
static void tcp_server_ethernet_task(void *pvParameters)
{
    const char *TASK_TAG = "eth_server";
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    
    ESP_LOGI(TASK_TAG, "Waiting for Ethernet connection...");
    xEventGroupWaitBits(eth_event_group, ETH_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TASK_TAG, "Ethernet connected! Starting TCP server...");

    while (1) {
        struct sockaddr_storage dest_addr;

        if (addr_family == AF_INET) {
            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
            dest_addr_ip4->sin_addr.s_addr = inet_addr(ETH_STATIC_IP);
            dest_addr_ip4->sin_family = AF_INET;
            dest_addr_ip4->sin_port = htons(ETH_TCP_PORT);
            ip_protocol = IPPROTO_IP;
        }

        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            ESP_LOGE(TASK_TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "Socket unable to bind: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        ESP_LOGI(TASK_TAG, "Socket bound to port %d", ETH_TCP_PORT);

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "listen failed: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TASK_TAG, "TCP Server listening on %s:%d", ETH_STATIC_IP, ETH_TCP_PORT);

        // Accept loop - chỉ accept 1 client
        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        
        if (sock < 0) {
            ESP_LOGE(TASK_TAG, "accept failed: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Set keepalive
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TASK_TAG, "Ethernet client connected from: %s", addr_str);
        
        bridge_set_ethernet_client(&g_bridge, sock);
        
        TaskHandle_t tx_task_handle = NULL;
        
        xTaskCreate(ethernet_tx_task, "eth_tx", 4096, (void*)sock, 5, &tx_task_handle);
        
        char rx_buffer[256];
        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            
            if (len < 0) {
                ESP_LOGE(TASK_TAG, "recv failed: errno %d", errno);
                break;
            }
            
            if (len == 0) {
                ESP_LOGI(TASK_TAG, "Ethernet client disconnected");
                break;
            }
            
            rx_buffer[len] = 0;
            ESP_LOGI(TASK_TAG, "[ETH->WiFi] Received %d bytes: %s", len, rx_buffer);
            
            if (bridge_send_to_wifi(&g_bridge, (uint8_t*)rx_buffer, len, sock) != 0) {
                ESP_LOGW(TASK_TAG, "Failed to send to WiFi queue");
            }
        }
        
        ESP_LOGI(TASK_TAG, "Cleaning up client connection...");

        close(sock);
        
        // Delele TX task
        if (tx_task_handle != NULL) {
            vTaskDelete(tx_task_handle);
            tx_task_handle = NULL;
        }
        
        bridge_clear_ethernet_client(&g_bridge);
        
        close(listen_sock);
        
        ESP_LOGI(TASK_TAG, "Client disconnected, restarting server...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}

// ========== WIFI CLIENT HANDLER TASK ==========
static void wifi_client_handler_task(void *pvParameters) {
    int sock = (int)pvParameters;
    char rx_buffer[256];
    const char *TASK_TAG = "wifi_client";
    int keepAlive = 1;
    
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    
    // Đăng ký client vào bridge
    if (bridge_register_wifi_client(&g_bridge, sock) != 0) {
        ESP_LOGE(TASK_TAG, "Failed to register WiFi client, closing connection");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TASK_TAG, "WiFi client handler started for socket %d", sock);
    
    while (1) {
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        
        if (len < 0) {
            ESP_LOGE(TASK_TAG, "recv failed: errno %d", errno);
            break;
        }

        if (len == 0) {
            ESP_LOGI(TASK_TAG, "WiFi client disconnected");
            break;
        }

        rx_buffer[len] = 0;
        ESP_LOGI(TASK_TAG, "[WiFi->ETH] Received %d bytes: %s", len, rx_buffer);
        
        // Gửi vào queue để forward đến Ethernet
        if (bridge_send_to_ethernet(&g_bridge, (uint8_t*)rx_buffer, len, sock) != 0) {
            ESP_LOGW(TASK_TAG, "Failed to send to Ethernet queue");
        }
    }
    
    // Cleanup
    bridge_unregister_wifi_client(&g_bridge, sock);
    close(sock);
    ESP_LOGI(TASK_TAG, "WiFi client handler ended for socket %d", sock);
    vTaskDelete(NULL);
}

// ========== WIFI BROADCAST TASK ==========
static void wifi_broadcast_task(void *pvParameters) {
    bridge_message_t msg;
    const char *TASK_TAG = "wifi_broadcast";
    
    ESP_LOGI(TASK_TAG, "WiFi broadcast task started");
    
    while (1) {
        // Đợi message từ Ethernet
        if (xQueueReceive(g_bridge.queue_eth_to_wifi, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TASK_TAG, "[ETH->WiFi] Broadcasting %d bytes to all WiFi clients", msg.len);
            
            // Lấy mutex để access client list
            if (xSemaphoreTake(g_bridge.wifi_clients.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                int sent_count = 0;
                int failed_count = 0;
                
                // Broadcast đến tất cả WiFi clients
                for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
                    int sock = g_bridge.wifi_clients.sockets[i];
                    if (sock != -1) {
                        int to_write = msg.len;
                        uint8_t *ptr = msg.data;
                        
                        while (to_write > 0) {
                            int written = send(sock, ptr, to_write, 0);
                            
                            if (written < 0) {
                                ESP_LOGE(TASK_TAG, "send failed to socket %d: errno %d", sock, errno);
                                failed_count++;
                                break;
                            }
                            
                            to_write -= written;
                            ptr += written;
                        }
                        
                        if (to_write == 0) {
                            sent_count++;
                        }
                    }
                }
                
                xSemaphoreGive(g_bridge.wifi_clients.mutex);
                
                ESP_LOGI(TASK_TAG, "Broadcast complete: sent=%d, failed=%d, total_clients=%d", 
                         sent_count, failed_count, g_bridge.wifi_clients.count);
            } else {
                ESP_LOGW(TASK_TAG, "Failed to acquire mutex for broadcast");
            }
        }
    }
    
    vTaskDelete(NULL);
}

// ========== WIFI SERVER TASK ==========
static void tcp_server_wifi_task(void *pvParameters)
{
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;

    const char *TASK_TAG = "wifi_server";
    
    ESP_LOGI(TASK_TAG, "Waiting for WiFi AP to start...");
    xEventGroupWaitBits(wifi_event_group, WIFI_READY_BIT, false, true, portMAX_DELAY);
    
    vTaskDelay(pdMS_TO_TICKS(500));
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
        
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "Socket unable to bind: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        ESP_LOGI(TASK_TAG, "Socket bound to %s:%d", WIFI_AP_IP, WIFI_TCP_PORT);

        err = listen(listen_sock, 5);  // Tăng backlog lên 5
        if (err != 0) {
            ESP_LOGE(TASK_TAG, "listen failed: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TASK_TAG, "WiFi TCP Server listening on %s:%d", WIFI_AP_IP, WIFI_TCP_PORT);

        // Accept loop - accept nhiều clients
        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t addr_len = sizeof(source_addr);
            
            int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TASK_TAG, "accept failed: errno %d", errno);
                break;
            }

            // Set keepalive
            setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
            setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
            
            char addr_str[128];
            if (source_addr.ss_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, 
                           addr_str, sizeof(addr_str) - 1);
            }

            ESP_LOGI(TASK_TAG, "New WiFi client connected from: %s", addr_str);
            
            // Tạo task xử lý client
            char task_name[32];
            snprintf(task_name, sizeof(task_name), "wifi_cli_%d", sock);
            xTaskCreate(wifi_client_handler_task, task_name, 4096, (void*)sock, 5, NULL);
        }

        close(listen_sock);
        ESP_LOGI(TASK_TAG, "Restarting WiFi TCP server...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}

// ========== APP MAIN ==========
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize bridge
    bridge_init(&g_bridge);

    eth_event_group = xEventGroupCreate();
    wifi_event_group = xEventGroupCreate();

    // GPIO setup for Ethernet
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_16, 1));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "External oscillator enabled on GPIO 16");

    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *eth_netifs[eth_port_cnt];
    esp_eth_netif_glue_handle_t eth_netif_glues[eth_port_cnt];

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netifs[0] = esp_netif_new(&cfg);
    eth_netif_glues[0] = esp_eth_new_netif_glue(eth_handles[0]);

    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton(ETH_STATIC_IP);  
    ip_info.netmask.addr = esp_ip4addr_aton(ETH_NETMASK);
    ip_info.gw.addr = esp_ip4addr_aton(ETH_GATEWAY); 

    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netifs[0]));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netifs[0], &ip_info));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netifs[0], eth_netif_glues[0]));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, wifi_ap_ready_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));

    // Initialize WiFi
    wifi_softap_config_t wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
        .channel = WIFI_CHANNEL,
        .max_conn = WIFI_MAX_CONN,
        .ip = WIFI_AP_IP,
        .gateway = WIFI_AP_GATEWAY,
        .netmask = WIFI_AP_NETMASK
    };
    wifi_init_softap(&wifi_config);

    // Start TCP servers
    xTaskCreate(tcp_server_ethernet_task, "tcp_eth_server", 4096, NULL, 5, NULL);
    xTaskCreate(tcp_server_wifi_task, "tcp_wifi_server", 4096, NULL, 5, NULL);

    // Start broadcast task
    xTaskCreate(wifi_broadcast_task, "wifi_broadcast", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "Bridge/Gateway initialized successfully");
    ESP_LOGI(TAG, "Ethernet: %s:%d", ETH_STATIC_IP, ETH_TCP_PORT); 
    ESP_LOGI(TAG, "WiFi AP:  %s:%d", WIFI_AP_IP, WIFI_TCP_PORT); 
    ESP_LOGI(TAG, "===========================================");
}
