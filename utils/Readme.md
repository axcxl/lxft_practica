## DHCP

- **`dhcp.sh` + `dhcp.conf`** ~ Used for running a local DHCP server for the ESP32 board.
    > The ESP32 MAC address should be updated and, if needed, the IP network.
    > If the interface has another name than **`eth0`**, use this command to change its name:
    ```bash
    sudo ip link set <old_name> name eth0
    ```

---

## MQTTs with Docker

- **`broker.sh`** ~ Runs the MQTTs broker via Docker (based on the parameters set in `mosquitto.conf`).
    - If the config file is updated, restart the service:
        ```bash
        sudo service mosquitto restart
        ```
    - If it doesn't run, try stopping the already running mosquitto services and try again:
        ```bash
        sudo service mosquitto stop
        ```
    - If this error appears: `Unable to find image 'eclipse-mosquitto:latest' locally`, pull the image:
        ```bash
        docker pull eclipse-mosquitto:latest
        ```

- **`client.sh`** ~ Runs a client that connects to the broker.

- **`certs/certs_generator.sh`** ~ Runs an automatic configuration that generates the required certificates for the Certificate Authority, Broker (Server), and Client.
    > - The **Common Name** (CN) of the Server might need changes.
    > - ⚠️ **It has to be the same as the `-h` option from `client.sh`** or you must use the **`--insecure`** flag for the client to disable hostname verification.
    > - It uses relative paths, so run it from the `/docker-mosquitto/certs/` folder.

---

## Home Assistant

- **`run_docker.sh`** ~ Runs Home Assistant and the MQTTs broker.

- To access the web page, use a browser to navigate to:
    > http://localhost:8123
    > - **Username:** `admin`
    > - **Password:** `#dxc_lxft#`

- To stop and remove the Home Assistant Docker container:
    ```bash
    docker rm -f homeassistant
    ```

- To update the Home Assistant Docker container:
    ```bash
    docker compose pull
    docker compose up -d
    ```

- For email verification:
    ```bash
    # Get base64 encoded credentials
    echo -n 'email@outlook.com' | base64
    echo -n 'password' | base64

    # Connect to the SMTP server
    openssl s_client -connect smtp-mail.outlook.com:587 -starttls smtp -crlf

    # After the connection is established, enter the following commands:
    --> EHLO
    --> AUTH LOGIN
    ```
