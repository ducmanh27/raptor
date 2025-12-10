#ifndef BRIDGE_CORE_H
#define BRIDGE_CORE_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Message format for bridge communication
typedef struct {
    uint8_t data[256];      // Payload data
    uint16_t len;           // Data length
    int source_sock;        // Source socket (for tracing)
    enum {
        FROM_WIFI,
        FROM_ETHERNET
    } source;
} bridge_message_t;

// Client registry for WiFi clients
#define MAX_WIFI_CLIENTS 5

typedef struct {
    int sockets[MAX_WIFI_CLIENTS];
    int count;
    SemaphoreHandle_t mutex;
} wifi_client_registry_t;

// Global bridge state
typedef struct {
    QueueHandle_t queue_wifi_to_eth;
    QueueHandle_t queue_eth_to_wifi;
    wifi_client_registry_t wifi_clients;
    int ethernet_client_sock;
    SemaphoreHandle_t eth_client_mutex;
} bridge_state_t;

// Bridge functions
void bridge_init(bridge_state_t *bridge);
int bridge_send_to_ethernet(bridge_state_t *bridge, uint8_t *data, uint16_t len, int source_sock);
int bridge_send_to_wifi(bridge_state_t *bridge, uint8_t *data, uint16_t len, int source_sock);
int bridge_register_wifi_client(bridge_state_t *bridge, int sock);
void bridge_unregister_wifi_client(bridge_state_t *bridge, int sock);
void bridge_set_ethernet_client(bridge_state_t *bridge, int sock);
void bridge_clear_ethernet_client(bridge_state_t *bridge);

#endif // BRIDGE_CORE_H