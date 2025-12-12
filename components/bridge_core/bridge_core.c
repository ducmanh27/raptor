#include "bridge_core.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bridge";

#define QUEUE_LENGTH 3
#define QUEUE_TIMEOUT_MS 1000

void bridge_init(bridge_state_t *bridge) {
    // Create queues
    bridge->queue_wifi_to_eth = xQueueCreate(QUEUE_LENGTH, sizeof(bridge_message_t));
    bridge->queue_eth_to_wifi = xQueueCreate(QUEUE_LENGTH, sizeof(bridge_message_t));
    
    if (bridge->queue_wifi_to_eth == NULL || bridge->queue_eth_to_wifi == NULL) {
        ESP_LOGE(TAG, "Failed to create queues!");
        return;
    }
    
    // Initialize WiFi client registry
    bridge->wifi_clients.count = 0;
    bridge->wifi_clients.mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
        bridge->wifi_clients.sockets[i] = -1;
    }
    
    // Initialize Ethernet client
    bridge->ethernet_client_sock = -1;
    bridge->eth_client_mutex = xSemaphoreCreateMutex();
    
    ESP_LOGI(TAG, "Bridge initialized successfully");
    ESP_LOGI(TAG, "Queue WiFi->Eth: %p", bridge->queue_wifi_to_eth);
    ESP_LOGI(TAG, "Queue Eth->WiFi: %p", bridge->queue_eth_to_wifi);
}

int bridge_send_to_ethernet(bridge_state_t *bridge, uint8_t *data, uint16_t len, int source_sock) {
    if (len > sizeof(((bridge_message_t*)0)->data)) {
        ESP_LOGE(TAG, "Data too large: %d bytes", len);
        return -1;
    }
    
    bridge_message_t msg;
    memcpy(msg.data, data, len);
    msg.len = len;
    msg.source_sock = source_sock;
    msg.source = FROM_WIFI;
    
    // Try to send with timeout
    // NOTE: Chỉnh timeout xuống 0 đảm bảo tính real-time trong trường hợp đầy thì bỏ 1 msg cũ nhất đi.
    if (xQueueSend(bridge->queue_wifi_to_eth, &msg, 0) != pdTRUE) {
        // Queue is full - evict oldest message and insert new one
        bridge_message_t discarded_msg;
        if (xQueueReceive(bridge->queue_wifi_to_eth, &discarded_msg, 0) == pdTRUE) {
            ESP_LOGW(TAG, "Queue WiFi->Eth full: dropped oldest message (%d bytes), keeping newest", 
                     discarded_msg.len);
            
            // Now send the new message (should succeed since we just freed a slot)
            if (xQueueSend(bridge->queue_wifi_to_eth, &msg, 0) != pdTRUE) {
                ESP_LOGE(TAG, "Failed to send after eviction - should not happen!");
                return -1;
            }
        } else {
            ESP_LOGE(TAG, "Queue full but cannot receive - should not happen!");
            return -1;
        }
    }
    
    ESP_LOGD(TAG, "Sent %d bytes from WiFi to Ethernet queue", len);
    return 0;
}

int bridge_send_to_wifi(bridge_state_t *bridge, uint8_t *data, uint16_t len, int source_sock) {
    if (len > sizeof(((bridge_message_t*)0)->data)) {
        ESP_LOGE(TAG, "Data too large: %d bytes", len);
        return -1;
    }
    
    bridge_message_t msg;
    memcpy(msg.data, data, len);
    msg.len = len;
    msg.source_sock = source_sock;
    msg.source = FROM_ETHERNET;
    
    // Try to send with no timeout (non-blocking)
    if (xQueueSend(bridge->queue_eth_to_wifi, &msg, 0) != pdTRUE) {
        // Queue is full - evict oldest message and insert new one
        bridge_message_t discarded_msg;
        
        if (xQueueReceive(bridge->queue_eth_to_wifi, &discarded_msg, 0) == pdTRUE) {
            ESP_LOGW(TAG, "Queue Eth->WiFi full: dropped oldest message (%d bytes), keeping newest", 
                     discarded_msg.len);
            
            // Now send the new message (should succeed since we just freed a slot)
            if (xQueueSend(bridge->queue_eth_to_wifi, &msg, 0) != pdTRUE) {
                ESP_LOGE(TAG, "Failed to send after eviction - should not happen!");
                return -1;
            }
        } else {
            ESP_LOGE(TAG, "Queue full but cannot receive - should not happen!");
            return -1;
        }
    }
    
    ESP_LOGD(TAG, "Sent %d bytes from Ethernet to WiFi queue", len);
    return 0;
}

int bridge_register_wifi_client(bridge_state_t *bridge, int sock) {
    if (xSemaphoreTake(bridge->wifi_clients.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for WiFi client registration");
        return -1;
    }
    
    int result = -1;
    for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
        if (bridge->wifi_clients.sockets[i] == -1) {
            bridge->wifi_clients.sockets[i] = sock;
            bridge->wifi_clients.count++;
            ESP_LOGI(TAG, "WiFi client registered: socket=%d, slot=%d, total=%d", 
                     sock, i, bridge->wifi_clients.count);
            result = 0;
            break;
        }
    }
    
    if (result != 0) {
        ESP_LOGW(TAG, "WiFi client registry full, cannot register socket %d", sock);
    }
    
    xSemaphoreGive(bridge->wifi_clients.mutex);
    return result;
}

void bridge_unregister_wifi_client(bridge_state_t *bridge, int sock) {
    if (xSemaphoreTake(bridge->wifi_clients.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for WiFi client unregistration");
        return;
    }
    
    for (int i = 0; i < MAX_WIFI_CLIENTS; i++) {
        if (bridge->wifi_clients.sockets[i] == sock) {
            bridge->wifi_clients.sockets[i] = -1;
            bridge->wifi_clients.count--;
            ESP_LOGI(TAG, "WiFi client unregistered: socket=%d, slot=%d, total=%d", 
                     sock, i, bridge->wifi_clients.count);
            break;
        }
    }
    
    xSemaphoreGive(bridge->wifi_clients.mutex);
}

void bridge_set_ethernet_client(bridge_state_t *bridge, int sock) {
    if (xSemaphoreTake(bridge->eth_client_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        bridge->ethernet_client_sock = sock;
        ESP_LOGI(TAG, "Ethernet client set: socket=%d", sock);
        xSemaphoreGive(bridge->eth_client_mutex);
    }
}

void bridge_clear_ethernet_client(bridge_state_t *bridge) {
    if (xSemaphoreTake(bridge->eth_client_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(TAG, "Ethernet client cleared: socket=%d", bridge->ethernet_client_sock);
        bridge->ethernet_client_sock = -1;
        xSemaphoreGive(bridge->eth_client_mutex);
    }
}