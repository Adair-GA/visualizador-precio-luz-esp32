# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
idf_component_register(SRCS "visualizador_precio_luz.cpp" "time_sync.c" "display.cpp" "tasks.cpp"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp-tls esp_wifi esp_timer nvs_flash esp_http_client CalEPD
                    EMBED_TXTFILES server_root_cert.pem)
