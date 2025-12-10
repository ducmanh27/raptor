idf_component_register(SRCS "bridge_core.c" "main.c"
                       PRIV_REQUIRES esp_netif esp_eth ethernet_init wifi_init driver nvs_flash esp_wifi
                       INCLUDE_DIRS ".")
