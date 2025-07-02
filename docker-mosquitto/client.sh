#!/bin/bash
# Just a small client that subscribes to all topics on a server
# Can be used to quickly check that everything is working
#

docker run -it --rm efrecon/mqtt-client sub \
        -h 192.168.1.175 \
        -t "#" \
        -v
