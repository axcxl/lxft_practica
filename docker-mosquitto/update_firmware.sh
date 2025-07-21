#!/bin/bash

# --- Configuration ---
# ESP32's IP address
ESP_IP="192.168.111.25" 
ESP_PORT="443"

FIRMWARE_PATH=$1

# Check if the firmware file exists
if [ ! -f "$FIRMWARE_PATH" ]; then
    echo "Error: Firmware file not found at '$FIRMWARE_PATH'"
    exit 1
fi

# Construct the URL
URL="https://${ESP_IP}:${ESP_PORT}/ota"

echo "====================================================="
echo "Target IP:      ${ESP_IP}"
echo "Firmware File:  ${FIRMWARE_PATH}"
echo "Upload URL:     ${URL}"
echo "====================================================="
echo ""
echo "Starting firmware upload..."

# Use curl to send the file
# --insecure / -k:      This tells curl to allow connections to SSL
#                       sites without verifying the certificate. Needed for
#                       the self-signed cert on the ESP32.
curl -X POST -F "firmware=@${FIRMWARE_PATH}" --insecure --verbose "${URL}"

# Check the exit code of curl
if [ $? -eq 0 ]; then
    echo -e "\n\n✅ Curl command executed successfully."
    echo "Check the output above for the server's response."
    echo "If successful, the ESP32 should be rebooting."
else
    echo -e "\n\n❌ Curl command failed. Please check the error messages above."
fi
