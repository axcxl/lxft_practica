#!/bin/bash

# All PEM / PSSWD = musca  -passout pass:musca

# Function to create OpenSSL config files
create_configs() {
    # CA config
    cat > ca.conf <<EOF
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_prj
OU = IT
CN = CA

[v3_ca]
basicConstraints = CA:TRUE
keyUsage = cRLSign, keyCertSign
EOF

    # Server/Broker config
    cat > broker.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_prj
OU = IT
CN = localhost
EOF

    # Client config
    cat > client.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_prj
OU = IT
CN = client
EOF

# ESP-Configs
    cat > client_esp1.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_1
OU = IT
CN = client_esp1
EOF

    cat > client_esp2.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_2
OU = IT
CN = client_esp2
EOF

    cat > client_esp3.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_3
OU = IT
CN = client_esp3
EOF


    # Client config
    cat > client_home_assistant.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no

[req_distinguished_name]
C = RO
ST = Bucharest
L = Bucharest
O = esp32_prj
OU = IT
CN = client_home_assistant
EOF
}

# Function to generate Certificate Authority - CA
generate_ca() {
    echo "#####  Generating a Certificate Authority (CA)  #####"
    openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt -config ca.conf -nodes || { echo "##### Failed to generate CA #####"; exit 1; }

    rm ../../main/certs/ca.crt
    cp ca.crt ../../main/certs
}

# Function to generate server certificates
generate_server() {
    echo "#####  Generating Server Key  #####"
    openssl genrsa -out broker.key 2048 || { echo "##### Failed to generate Server key #####"; exit 1; }

    echo "#####  Generating a Certificate Signing Request (CSR)  #####"
    openssl req -out broker.csr -key broker.key -new -config broker.conf || { echo "##### Failed to generate CSR #####"; exit 1; }

    echo "#####  Generate broker Certificate  #####"
    openssl x509 -req -in broker.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out broker.crt -days 365 || { echo "##### Failed to generate Generate Server Certificate #####"; exit 1; }
}

# Function to generate client certificates
generate_client() {
    echo "#####  Generating Client Key  #####"
    openssl genrsa -out client.key 2048 || { echo "##### Failed to generate Client key #####"; exit 1; }

    echo "#####  Generating a Certificate Signing Request (CSR)  #####"
    openssl req -out client.csr -key client.key -new -config client.conf || { echo "##### Failed to generate CSR #####"; exit 1; }

    echo "#####  Generate Client Certificate  #####"
    openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365 || { echo "##### Failed to generate Generate Client Certificate #####"; exit 1; }
}

generate_esp() {
    echo "#####  Generating ESP32 Certificates  #####"
    
    # Generate certificates for each ESP32 client
    for esp_num in 1 2 3; do
        echo "#####  Generating ESP32 ${esp_num} Key  #####"
        openssl genrsa -out client_esp${esp_num}.key 2048 || { echo "##### Failed to generate ESP32 ${esp_num} key #####"; exit 1; }

        echo "#####  Generating a Certificate Signing Request (CSR) for ESP32 ${esp_num}  #####"
        openssl req -out client_esp${esp_num}.csr -key client_esp${esp_num}.key -new -config client_esp${esp_num}.conf || { echo "##### Failed to generate CSR for ESP32 ${esp_num} #####"; exit 1; }

        echo "#####  Generate ESP32 ${esp_num} Certificate  #####"
        openssl x509 -req -in client_esp${esp_num}.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client_esp${esp_num}.crt -days 365 || { echo "##### Failed to generate ESP32 ${esp_num} Certificate #####"; exit 1; }

        # Copy to main/certs directory
        rm -f ../../main/certs/client_esp${esp_num}.crt ../../main/certs/client_esp${esp_num}.key
        cp client_esp${esp_num}.crt ../../main/certs
        cp client_esp${esp_num}.key ../../main/certs
    done
}


# Function to generate client certificates for home assistant
generate_home_assistant() {
     echo "#####  Generating Home Assistant Key  #####"
    openssl genrsa -out client_home_assistant.key 2048 || { echo "##### Failed to generate client_home_assistant key #####"; exit 1; }

    echo "#####  Generating a Certificate Signing Request (CSR)  #####"
    openssl req -out client_home_assistant.csr -key client_home_assistant.key -new -config client_home_assistant.conf || { echo "##### Failed to generate CSR #####"; exit 1; }

    echo "#####  Generate Home Assistant Certificate  #####"
    openssl x509 -req -in client_home_assistant.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client_home_assistant.crt -days 365 || { echo "##### Failed to generate Generate Home Assistant Certificate #####"; exit 1; }
}

# Create config files
create_configs

# Check arguments
case "$1" in
    -server)
        # Check if CA exists, if not generate it
        if [[ ! -f ca.crt || ! -f ca.key ]]; then
            generate_ca
        fi
        generate_server
        ;;
    -client)
        # Check if CA exists, if not generate it
        if [[ ! -f ca.crt || ! -f ca.key ]]; then
            generate_ca
        fi
        generate_client
        ;;
    -esp)
        # Check if CA exists, if not generate it
        if [[ ! -f ca.crt || ! -f ca.key ]]; then
            generate_ca
        fi
        generate_esp
        ;;
    -home_assistant)
        # Check if CA exists, if not generate it
        if [[ ! -f ca.crt || ! -f ca.key ]]; then
            generate_ca
        fi
        generate_home_assistant
        ;;
    -clean)
        echo "#####  Cleaning all generated certificates  #####"
        rm -f ca.crt ca.key ca.srl ca.conf
        rm -f broker.crt broker.key broker.csr broker.conf
        rm -f client.crt client.key client.csr client.conf
        echo "#####  All certificates and config files removed  #####"
        ;;
    *)
        echo "Usage: $0 [-server|-client|-clean]"
        echo "  -server: Generate server certificates"
        echo "  -client: Generate client certificates"
        echo "  -clean:  Remove all generated certificates and config files"
        exit 1
        ;;
esac