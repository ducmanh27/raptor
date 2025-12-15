#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

// ============ ETHERNET CONFIG ============
#define ETH_STATIC_IP      "192.168.24.249"
#define ETH_NETMASK        "255.255.255.0"
#define ETH_GATEWAY        "192.168.24.1"
#define ETH_TCP_PORT       8888

// ============ WIFI AP CONFIG ============
#define WIFI_SSID          "MTB-052"
#define WIFI_PASSWORD      "12345687"
#define WIFI_CHANNEL       1
#define WIFI_MAX_CONN      5

#define WIFI_AP_IP         "192.168.10.1"
#define WIFI_AP_GATEWAY    "192.168.10.1"
#define WIFI_AP_NETMASK    "255.255.255.0"
#define WIFI_TCP_PORT      9999

// ============ TCP SERVER CONFIG ============
#define KEEPALIVE_IDLE     5
#define KEEPALIVE_INTERVAL 5
#define KEEPALIVE_COUNT    3

#endif // NETWORK_CONFIG_H