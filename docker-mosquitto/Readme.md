# MQTTs with Docker

- **dhcp.sh + dhcp.conf** ~ used for running a local DHCP server for the esp32 board
    - the esp32 mac adress should be updated and if needed the IP network
    - if the interface has another name than **eth0** use this command to change it's name
    ```bash
        sudo ip link set <old_name> name eth0
    ```

- **broker.sh** ~ runs via docker the MQTTs broker (based on the parameters set on mosquitto.conf)
    - if the config file is updated
    ```bash
        sudo service mosquitto restart
    ```
    - if it doesn't run try stopping the already running mosquitto servicies and try again:
    ```bash
        sudo service mosquitto stop
    ```

- **client.sh** ~ runs a client that connects to the broker
- **certs/certs_generator.sh** ~ runs an automatic config that generates the required certificates for the Certificate Authority, Brocker(Server) and Client
    - The **Common Name** (CN) of the Server might need changes
    - ⚠️ **It has to be the same as the -h option from client.sh** or otherwise use **--insecure** for the client to disable hostname verification
    - it uses relative paths, so run it from **/docker-mosquitto/certs/** folder

