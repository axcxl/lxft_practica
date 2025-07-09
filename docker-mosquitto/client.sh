#!/bin/bash

# Client that subscribes to all topics on a server
docker run -it --rm --network host      \
        -v ./certs/ca.crt:/certs/ca.crt \
        -v ./certs/client.crt:/certs/client.crt \
        -v ./certs/client.key:/certs/client.key \
        eclipse-mosquitto mosquitto_sub \
        --cafile /certs/ca.crt          \
        --cert /certs/client.crt        \
        --key /certs/client.key         \
        --tls-version tlsv1.2           \
        -h localhost                    \
        -p 8883                         \
        -t "#"                          \
        -v --debug

