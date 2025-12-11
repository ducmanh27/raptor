# Ethernet WiFi Bridge Gateway

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

## Overview

This project implements a **bidirectional bridge/gateway** between Ethernet and WiFi networks using ESP32. It demonstrates advanced usage of:
- **Ethernet driver** with static IP configuration
- **WiFi SoftAP** mode for multiple client connections
- **FreeRTOS queues** for inter-task communication
- **Multi-threaded TCP server** architecture

### Key Features

- ✅ **Bidirectional data forwarding** between Ethernet and WiFi
- ✅ **Multiple WiFi clients** support (up to 10 concurrent connections)
- ✅ **Single Ethernet client** connection
- ✅ **Broadcast capability**: Data from Ethernet → All WiFi clients
- ✅ **Aggregation**: Data from any WiFi client → Ethernet client
- ✅ **Thread-safe** message queue architecture
- ✅ **Automatic client management** with registration/unregistration

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Gateway                            │
│                                                             │
│  WiFi Side (Multiple clients)    Ethernet Side (1 client)   │
│  ┌─────────────────┐             ┌──────────────────┐       │
│  │ WiFi Client 1   │             │                  │       │
│  │ WiFi Client 2   │────────┐    │  Ethernet Client │       │
│  │ WiFi Client 3   │        │    │                  │       │
│  │      ...        │        │    │                  │       │
│  └─────────────────┘        │    └──────────────────┘       │
│         │                   │             │                 │
│         ▼                   ▼             ▼                 │
│  ┌─────────────┐      ┌─────────────┐   ┌──────────┐        │
│  │  WiFi Task  │◄────►│   Bridge    │◄─►│ Eth Task │        │
│  │  (Server)   │      │   Queues    │   │ (Server) │        │
│  └─────────────┘      └─────────────┘   └──────────┘        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Business Logic

### Data Flow

**1. WiFi → Ethernet:**
```
WiFi Client sends data
    ↓
WiFi Client Handler Task receives data
    ↓
bridge_send_to_ethernet() → queue_wifi_to_eth
    ↓
Ethernet TX Task receives from queue
    ↓
send() to Ethernet Client
```

**2. Ethernet → WiFi (Broadcast):**
```
Ethernet Client sends data
    ↓
Ethernet Server Task receives data
    ↓
bridge_send_to_wifi() → queue_eth_to_wifi
    ↓
WiFi Broadcast Task receives from queue
    ↓
Loop through ALL registered WiFi clients
    ↓
send() to each WiFi Client
```

### Task Architecture

| Task Name | Quantity | Purpose |
|-----------|----------|---------|
| `tcp_server_ethernet_task` | 1 | Accept Ethernet client, handle RX |
| `ethernet_tx_task` | 1 (per client) | Forward data from WiFi → Ethernet |
| `tcp_server_wifi_task` | 1 | Accept WiFi clients |
| `wifi_client_handler_task` | Multiple | Handle each WiFi client RX |
| `wifi_broadcast_task` | 1 | Broadcast data from Ethernet → All WiFi |

### Connection Management

**Ethernet Client:**
- Only **1 client** allowed at a time
- When client connects:
  - RX handled directly in server task
  - TX handled in separate `ethernet_tx_task`
- When client disconnects:
  - TX task is deleted using `vTaskDelete()`
  - Socket closed
  - Server restarts and waits for new client

**WiFi Clients:**
- Up to **10 concurrent clients**
- Each client gets its own handler task
- Clients are registered in `wifi_client_registry_t`
- Automatic unregistration on disconnect

## Network Configuration

Edit `components/network_config/network_config.h`:

```c
// Ethernet Configuration
#define ETH_STATIC_IP      "192.168.49.53"
#define ETH_NETMASK        "255.255.255.0"
#define ETH_GATEWAY        "192.168.49.1"
#define ETH_TCP_PORT       8888

// WiFi AP Configuration
#define WIFI_SSID          "MTB-052"
#define WIFI_PASSWORD      "12345687"
#define WIFI_CHANNEL       1
#define WIFI_MAX_CONN      5

#define WIFI_AP_IP         "192.168.10.1"
#define WIFI_AP_GATEWAY    "192.168.10.1"
#define WIFI_AP_NETMASK    "255.255.255.0"
#define WIFI_TCP_PORT      9999
```

## Hardware Required

- ESP32 development board with Ethernet support
- Supported Ethernet PHY: `LAN8720`, `IP101`, `DP83848`, `RTL8201`
- Or SPI Ethernet modules: `DM9051`, `W5500`, `KSZ8851SNL`

### Pin Assignment

See common pin assignments for Ethernet examples from [ESP-IDF Ethernet examples](https://github.com/espressif/esp-idf/tree/master/examples/ethernet).

**Default for LAN8720:**
- GPIO16: External oscillator enable
- RMII interface connected to ESP32

## How to Use

### Build and Flash

```bash
idf.py build
idf.py -p PORT flash monitor
```

### Test the Bridge

**1. Connect Ethernet Client:**
```bash
# From a machine on 192.168.49.0/24 network
nc 192.168.49.53 8888
```

**2. Connect WiFi Clients:**
```bash
# Connect to WiFi AP: MTB-052 (password: 12345687)
# Your device will get IP: 192.168.10.2 or higher
# Windows: Dùng hercules terminal mở TCP Client đến 192.168.10.1 9999 và gửi dữ liệu
```

**3. Test Bidirectional Communication:**

**From WiFi Client:**
```
Type: Hello from WiFi
→ Should appear on Ethernet Client
```

**From Ethernet Client:**
```
Type: Hello from Ethernet
→ Should appear on ALL connected WiFi Clients
```

## Example Output

```
I (2256) main: Ethernet Started
I (2256) wifi_init: Starting WiFi SoftAP...
I (2266) main: Ethernet Got IP Address
I (2266) main: ETHIP:192.168.49.53
I (2276) main: ETHMASK:255.255.255.0
I (2276) main: ETHGW:192.168.49.1

I (2456) wifi:mode : softAP (5c:01:3b:f3:fc:99)
I (2466) main: WiFi AP is ready for TCP server
I (2476) wifi_init: WiFi SoftAP started: SSID=MTB-052 channel:1
I (2486) esp_netif_lwip: DHCP server started with IP: 192.168.10.1

I (2496) eth_server: TCP Server listening on 192.168.49.53:8888
I (3006) wifi_server: WiFi TCP Server listening on 192.168.10.1:9999
I (2516) wifi_broadcast: WiFi broadcast task started

I (2526) main: ===========================================
I (2526) main: Bridge/Gateway initialized successfully
I (2526) main: Ethernet: 192.168.49.53:8888
I (2536) main: WiFi AP:  192.168.10.1:9999
I (2536) main: ===========================================

// WiFi client connects
I (24436) wifi:station: e0:2e:0b:92:f9:41 join, AID=1
I (25856) esp_netif_lwip: DHCP assigned IP to client: 192.168.10.2
I (86206) wifi_server: New WiFi client connected from: 192.168.10.2
I (86206) bridge: WiFi client registered: socket=56, slot=0, total=1

// Ethernet client connects
I (145156) eth_server: Ethernet client connected from: 192.168.49.52
I (145156) bridge: Ethernet client set: socket=56
I (145156) eth_tx: Ethernet TX task started for socket 56

// Data transfer: WiFi → Ethernet
I (158346) wifi_client: [WiFi->ETH] Received 4 bytes: XXXX
I (158346) eth_tx: [WiFi->ETH] Forwarding 4 bytes to Ethernet client
I (158346) eth_tx: Successfully sent 4 bytes to Ethernet client

// Data transfer: Ethernet → WiFi (broadcast)
I (158346) eth_server: [ETH->WiFi] Received 4 bytes: XXXX
I (158356) wifi_broadcast: [ETH->WiFi] Broadcasting 4 bytes to all WiFi clients
I (158366) wifi_broadcast: Broadcast complete: sent=1, failed=0, total_clients=1
```

## Performance Characteristics

- **Latency**: < 5ms for small packets
- **Throughput**: Limited by TCP socket buffers (256 bytes per message)
- **WiFi Clients**: Tested with up to 10 concurrent connections
- **Message Queue Depth**: 10 messages per queue
- **RAM Usage**: ~6KB for queues + 4KB per WiFi client task

## Code Structure

```
.
├── main/
│   ├── main.c                   # Main application, TCP servers
│   └── network_config.h         # Centralized network configuration
├── components/
│   ├── bridge_core/
│   │   ├── bridge_core.h        # Bridge data structures
│   │   └── bridge_core.c        # Bridge logic, queue management
│   ├── wifi_init/
│   │   ├── wifi_init.h
│   │   └── wifi_init.c          # WiFi SoftAP initialization
│   └── ethernet_init/
│       ├── ethernet_init.h
│       └── ethernet_init.c      # Ethernet driver initialization
└── README.md
```

## Key Implementation Details

### Thread-Safe Queue Communication

```c
// WiFi → Ethernet
bridge_send_to_ethernet(&g_bridge, data, len, sock);
    ↓
xQueueSend(queue_wifi_to_eth, &msg, timeout);
    ↓
xQueueReceive(queue_wifi_to_eth, &msg, portMAX_DELAY);
    ↓
send(ethernet_sock, data, len);
```

### Client Registry Protection

All WiFi client operations are protected by mutex:
```c
xSemaphoreTake(wifi_clients.mutex, timeout);
// Add/remove/iterate clients
xSemaphoreGive(wifi_clients.mutex);
```

### Task Cleanup on Disconnect

When Ethernet client disconnects:
```c
// 1. Delete TX task safely
if (tx_task_handle != NULL) {
    vTaskDelete(tx_task_handle);
    tx_task_handle = NULL;
}

// 2. Close socket
close(sock);

// 3. Clear bridge state
bridge_clear_ethernet_client(&g_bridge);
```

## Troubleshooting

**WiFi clients not receiving data:**
- Check WiFi client is properly registered (see logs: "WiFi client registered")
- Verify Ethernet client is connected
- Check queue is not full (see warning: "Queue is full")

**Ethernet connection fails:**
- Verify Ethernet cable is connected
- Check PHY chip is properly powered
- Verify IP configuration matches your network

**Memory issues:**
- Reduce `MAX_WIFI_CLIENTS` if RAM is limited
- Decrease `QUEUE_LENGTH` if needed
- Monitor task stack watermarks

## Advanced Features

### Scalability

To support more WiFi clients, edit `bridge_core.h`:
```c
#define MAX_WIFI_CLIENTS 20  // Increase limit
```

**RAM Impact:** +4KB per additional client task

### Protocol Extensions

Current implementation uses raw TCP byte streams. Can be extended to:
- Add protocol headers (length, checksum, sequence numbers)
- Implement packet fragmentation for large messages
- Add encryption layer
- Implement QoS prioritization

## License

MIT License

## Author

Phan Duc Manh

Date: 8/12/2025

## References

- [ESP-IDF Ethernet Examples](https://github.com/espressif/esp-idf/tree/master/examples/ethernet)
- [ESP-IDF TCP Server Example](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/tcp_server)
- [FreeRTOS Queue Documentation](https://www.freertos.org/a00018.html)