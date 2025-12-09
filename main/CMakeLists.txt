idf_component_register(SRCS "ethernet_example_main.c"
                       PRIV_REQUIRES esp_netif esp_eth ethernet_init driver esp_wifi nvs_flash
                       INCLUDE_DIRS ".")
