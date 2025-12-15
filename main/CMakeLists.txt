idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES 
                        esp_netif esp_eth 
                        ethernet_init 
                        wifi_init bridge_core
                        driver 
                        nvs_flash 
                        esp_wifi 
                        esp_driver_uart
                        at_core
                       INCLUDE_DIRS ".")
