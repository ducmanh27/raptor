idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES esp_netif esp_eth ethernet_init wifi_init driver nvs_flash esp_wifi
                       INCLUDE_DIRS ".")
