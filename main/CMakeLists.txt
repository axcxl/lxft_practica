idf_component_register(SRCS "leds.c" "wifi.c" "main.c" "task_comms.c" "task_sensors.c" "dns_server.c" "http_server.c" 
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES
                        "certs/servercert.pem"
                        "certs/prvtkey.pem"
                        "certs/client.key"
                        "certs/client.crt"
                        "certs/ca.crt")