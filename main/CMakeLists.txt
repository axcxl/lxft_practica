idf_component_register(SRCS "leds.c" "wifi.c" "main.c" "task_comms.c" "task_sensors.c" "dns_server.c" "http_server.c" 
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES
                        "certs/servercert.pem"
                        "certs/prvtkey.pem"
                        "certs/client_esp1.key"
                        "certs/client_esp1.crt"
                        "certs/client_esp2.key"
                        "certs/client_esp2.crt"
                        "certs/client_esp3.key"
                        "certs/client_esp3.crt"
                        "certs/ca.crt")