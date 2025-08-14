#!/bin/bash

# --- Configuration ---
# ESP32's IP address
ESP_IP="192.168.111.25" 
ESP_PORT="443"
ESP_PORT="80"
HOTSPOT_PASSWORD="esp32_config_psswd"

FIRMWARE_PATH=$1

# Check if the firmware file exists
if [ ! -f "$FIRMWARE_PATH" ]; then
    echo "Warning: Firmware file not found at '$FIRMWARE_PATH'"
    FIRMWARE_PATH="./../build/esp32-mqtt-ethernet.bin"
    echo "Using default firmware: $FIRMWARE_PATH"
    if [ ! -f "$FIRMWARE_PATH" ]; then
        echo "Error: Default firmware file not found at '$FIRMWARE_PATH'"
        exit 1
    fi
fi



# ________________________ Functions used ________________________
# Function to scan for ESP32 hotspot
find_esp32_hotspot() {
    echo "Scanning for ESP32 hotspot..."
    # Scan for networks and look for ESP32 prefix
    local networks=$(nmcli dev wifi list | grep "$ESP_HOTSPOT_SSID_PREFIX" | head -1 | awk '{print $2}')
    echo "$networks"
}

# Function to get current connection
get_current_connection() {
    nmcli -t -f NAME connection show --active | head -1
}

# Function to connect to ESP32 hotspot
connect_to_esp32() {
    local ssid=$1
    echo "Attempting to connect to ESP32 hotspot: $ssid"
    
    if [ -z "$HOTSPOT_PASSWORD" ]; then
        # Open network
        nmcli dev wifi connect "$ssid" 2>/dev/null
    else
        # Password protected network
        nmcli dev wifi connect "$ssid" password "$HOTSPOT_PASSWORD" 2>/dev/null
    fi
    
    if [ $? -eq 0 ]; then
        echo "✅ Connected to ESP32 hotspot"
        # Wait a moment for connection to stabilize
        sleep 2
        return 0
    else
        echo "❌ Failed to connect to ESP32 hotspot"
        return 1
    fi
}

# Function to restore original connection
restore_connection() {
    local original_conn=$1
    if [ -n "$original_conn" ]; then
        echo "Restoring original connection: $original_conn"
        nmcli connection up "$original_conn" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "✅ Original connection restored"
        else
            echo "⚠️  Could not restore original connection. Please reconnect manually."
        fi
    fi
}

# Function to check if ESP32 is reachable
check_esp32_reachable() {
    echo "Checking if ESP32 is reachable at $ESP_IP..."
    curl -s --connect-timeout 5 "http://${ESP_IP}:${ESP_PORT}/" > /dev/null 2>&1
    return $?
}



# ________________________ Connect to Hotspot ________________________
echo "====================================================="
echo "ESP32 OTA Update via Hotspot"
echo "====================================================="
echo "Target IP:      ${ESP_IP}"
echo "Firmware File:  ${FIRMWARE_PATH}"
echo "====================================================="

# Store current connection
ORIGINAL_CONNECTION=$(get_current_connection)
echo "Current connection: ${ORIGINAL_CONNECTION:-"None detected"}"

# Check if ESP32 is already reachable (maybe already connected)
if check_esp32_reachable; then
    echo "✅ ESP32 is already reachable, proceeding with update..."
else
    # Scan for ESP32 hotspot
    ESP32_SSID=$(find_esp32_hotspot)
    
    if [ -z "$ESP32_SSID" ]; then
        echo "❌ ESP32 hotspot not found. Make sure:"
        echo "   1. ESP32 is powered on"
        echo "   2. ESP32 hotspot is active"
        echo "   3. You're within WiFi range"
        exit 1
    fi
    
    echo "Found ESP32 hotspot: $ESP32_SSID"
    
    # Connect to ESP32 hotspot
    if ! connect_to_esp32 "$ESP32_SSID"; then
        echo "❌ Cannot connect to ESP32 hotspot"
        exit 1
    fi
    
    # Verify ESP32 is reachable
    if ! check_esp32_reachable; then
        echo "❌ ESP32 not reachable after connecting to hotspot"
        restore_connection "$ORIGINAL_CONNECTION"
        exit 1
    fi
fi

# Construct the URL (HTTP, not HTTPS)
URL="http://${ESP_IP}:${ESP_PORT}/ota"

echo ""
echo "Upload URL:     ${URL}"
echo "Starting firmware upload..."



# ________________________ Send the POST Request ________________________
curl -X POST \
     -F "firmware=@${FIRMWARE_PATH}" \
     --connect-timeout 10 \
     --max-time 300 \
     --verbose \
     "${URL}"

CURL_EXIT_CODE=$?

# Restore original connection if we connected to hotspot
if [ -n "$ORIGINAL_CONNECTION" ] && [ "$ESP32_SSID" ]; then
    echo ""
    restore_connection "$ORIGINAL_CONNECTION"
fi

echo ""
echo "====================================================="
if [ $CURL_EXIT_CODE -eq 0 ]; then
    echo "✅ OTA Update completed successfully!"
    echo "ESP32 should now be running the new firmware."
else
    echo "❌ OTA Update failed!"
    echo "Check ESP32 status and try again."
fi
echo "====================================================="

exit $CURL_EXIT_CODE
