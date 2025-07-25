#!/bin/bash

# Stop any active instance
docker rm -f homeassistant
sudo service mosquitto stop

# Apply modifications made to the config file
sudo cp configuration.yaml ./docker

# Start Dockers
docker compose up -d