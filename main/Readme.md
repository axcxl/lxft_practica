# </> Code Description </>

> ### 🗒️ Core Files
> - **`main.c`** - Main application entry point, system initialization (with Watchdog integration)
> - **`CMakeLists.txt`** - Build configuration

> ### 📡 Network & Communication 🌐
> - **`wifi.c` / `wifi.h`** - WiFi management support
>   - **AP (Access Point) Mode:** Acts as a WiFi hotspot, creates a wireless network that other devices can connect to
>   - **STA (Station) Mode:** Acts as a WiFi client, connects to an existing wireless network.
> - **`dns_server.c` / `dns_server.h`**
>   - DNS server for captive portal functionality for the WiFi AP
> - **`http_server.c` / `http_server.h`**
>   - HTTP server implementation with configuration endpoints accessible through the WiFi AP
>   - If HTTPS is required, the certificates are already generated and included in the project through `CMakeLists.txt`
> - **`task_comms.c` / `task_comms.h`**
>   - The communication task module handles all network connectivity
>     - **Ethernet (Preferred):** Hardware-based connection
>     - **WIFI STA (Backup):** Automatic activation when Ethernet connection fails
>   - MQTTS configuration, initialization, and data transmission for the IoT system
>     - Secure SSL/TLS encrypted communication using embedded certificates
>     - Broker URL can be changed from the HTTP config page by connecting to the hotspot, or by accessing the SDK config menu

> ### 📊 Sensor Data Management
> - **`sensor_queue.h`** - Data structures for sensor queue management
> - **`task_sensors.c` / `task_sensors.h`** - Sensor data collection and processing

> ### 💡 Hardware Control
> - **`leds.c` / `leds.h`** - LED control functions for visual feedback
