# DHCP

- **dhcp.sh + dhcp.conf** ~ used for running a local DHCP server for the esp32 board
    - the esp32 mac adress should be updated and if needed the IP network
    - if the interface has another name than **eth0** use this command to change it's name
    ```bash
        sudo ip link set <old_name> name eth0
    ```

# MQTTs with Docker

- **broker.sh** ~ runs via docker the MQTTs broker (based on the parameters set on mosquitto.conf)
    - if the config file is updated
    ```bash
        sudo service mosquitto restart
    ```
    - if it doesn't run try stopping the already running mosquitto servicies and try again:
    ```bash
        sudo service mosquitto stop
    ```
    -if this error apears: "Unable to find image 'eclipse-mosquitto:latest' locally"
    ```bash
        docker pull eclipse-mosquitto:latest
    ```

- **client.sh** ~ runs a client that connects to the broker
- **certs/certs_generator.sh** ~ runs an automatic config that generates the required certificates for the Certificate Authority, Brocker(Server) and Client
    - The **Common Name** (CN) of the Server might need changes
    - ⚠️ **It has to be the same as the -h option from client.sh** or otherwise use **--insecure** for the client to disable hostname verification
    - it uses relative paths, so run it from **/docker-mosquitto/certs/** folder


# Home Assistant

- **run_docker.sh** ~ runs home assistant and the MQTTS broker
- To acces the web page via browser use this:  
        http://localhost:8123  
        - Username: admin  
        - Password: #dxc_lxft#
- for stopping and removing the docker run:
```bash
    docker rm -f homeassistant
```

- for updating home assistant docker:
```bash
    docker compose pull
    docker compose up -d
```

- for email verification:
```bash
    echo -n 'email@outlook.com' | base64
    echo -n 'password' | base64
    openssl s_client -connect smtp-mail.outlook.com:587 -starttls smtp -crlf
    # after the connection is established:
    --> EHLO
    --> AUTH LOGIN
```
