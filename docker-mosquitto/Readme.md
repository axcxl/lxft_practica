# MQTTs with Docker

- **dhcp.sh + dhcp.conf** ~ used for running a local DHCP server for the esp32 board
    - the esp32 mac adress should be updated and if needed the IP network
- **broker.sh** ~ runs via docker the MQTTs broker based on the parameters set on mosquitto.conf
- **client.sh** ~ runs a client that connects to the broker
- **certs/certs_generator.sh** ~ runs an automatic config that generates the required certificates for the Certificate Authority, Brocker(Server) and Client
    - The **Common Name** (CN) of the Server might need changes
    - ⚠️ **It has to be the same as the -h option from client.sh** or otherwise use --insecure for the client to disable hostname verification
