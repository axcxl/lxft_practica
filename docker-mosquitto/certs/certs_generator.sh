#!/bin/bash

# All PEM / PSSWD = musca

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

    # Server config
    cat > server.conf <<EOF
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
}

# Function to generate Certificate Authority - CA
generate_ca() {
    echo "#####  Generating a Certificate Authority (CA)  #####"
    openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt -config ca.conf -passout pass:musca || { echo "##### Failed to generate CA #####"; exit 1; }
}

# Function to generate server certificates
generate_server() {
    echo "#####  Generating Server Key  #####"
    openssl genrsa -aes256 -out server.key -passout pass:musca 2048 || { echo "##### Failed to generate Server key #####"; exit 1; }

    echo "#####  Generating a Certificate Signing Request (CSR)  #####"
    openssl req -out server.csr -key server.key -new -config server.conf -passin pass:musca || { echo "##### Failed to generate CSR #####"; exit 1; }

    echo "#####  Generate Server Certificate  #####"
    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -passin pass:musca || { echo "##### Failed to generate Generate Server Certificate #####"; exit 1; }
}

# Function to generate client certificates
generate_client() {
    echo "#####  Generating Client Key  #####"
    openssl genrsa -aes256 -out client.key -passout pass:musca 2048 || { echo "##### Failed to generate Client key #####"; exit 1; }

    echo "#####  Generating a Certificate Signing Request (CSR)  #####"
    openssl req -out client.csr -key client.key -new -config client.conf -passin pass:musca || { echo "##### Failed to generate CSR #####"; exit 1; }

    echo "#####  Generate Client Certificate  #####"
    openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365 -passin pass:musca || { echo "##### Failed to generate Generate Client Certificate #####"; exit 1; }
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
    *)
        echo "Usage: $0 [-server|-client]"
        echo "  -server: Generate server certificates"
        echo "  -client: Generate client certificates"
        exit 1
        ;;
esac