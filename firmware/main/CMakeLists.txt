idf_component_register(SRCS "main.c" "ui.c" "network.c" "telemetry.c" "control.c" INCLUDE_DIRS "." REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_client esp_timer json)
