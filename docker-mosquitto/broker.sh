#!/bin/bash
#Steps to install:
# - run "docker run -it -p 1883:1883 -p 9001:9001 eclipse-mosquitto" to get a default config
# - copy config file from docker: "docker cp <instance name>:/mosquitto/config/mosquitto.conf mosquitto.conf
# - modify things in mosquitto.conf
# - run the following command which uses the config we modified + volumes to persist data
# - for system checking run :               sudo systemctl status mosquitto
# - if the config file is modified run:     sudo service mosquitto restart

docker run -it --rm -p 1883:1883 -p 9001:9001 -p 8883:8883      \
    -v ./mosquitto.conf:/mosquitto/config/mosquitto.conf        \
    -v ./certs/ca.crt:/mosquitto/certs/ca.crt                   \
    -v ./certs/broker.key:/mosquitto/certs/broker.key           \
    -v ./certs/broker.crt:/mosquitto/certs/broker.crt           \
    -v /mosquitto/log:/mosquitto/log                            \
    -v /mosquitto/data:/mosquitto/data                          \
    eclipse-mosquitto
    