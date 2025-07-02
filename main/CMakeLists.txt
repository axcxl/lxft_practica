
idf_component_register(SRCS "task_server.c" "main.c" "task_comms.c" "task_sensors.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES "certs/servercert.pem"
                                   "certs/prvtkey.pem")