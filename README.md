# lxft_practica2025

## Table of Contents

1.  [Installation notes - LINUX](#installation-notes---linux)
2.  [Installation notes - WINDOWS](#installation-notes---windows)
3.  [Hardware](#hardware)

## Installation notes - LINUX

**Requirements:**

*   VSCode
    *   Devcontainer extension
*   Docker + Docker Compose
*   mosquitto
*   isc-dhcp-server
*   homeassistant (the docker folder should be placed in `/utils/home_assistant`)
*   Git

> **Note:** All scripts should be executed from their directory because they use relative paths.

> **Note:** For additional information check:
>
> *   [`utils/Readme.md`](utils/Readme.md)
> *   [`main/Readme.md`](main/Readme.md)

**Instructions:**

1.  Clone repo.
2.  Connect the board.
3.  Open with VSCode.
4.  Click "Re-open in devcontainer" button and wait for the container to be built.
    *   Recommended: install C/C++ extension in container.
5.  Connect ESP board, make sure `/dev/ttyUSB*` device appears in `dmesg` output. Select `/dev/ttyUSB*` device in VsCode.
6.  **First Setup Only:**
    *   Use `-conf` on the DHCP script to copy the config file.
    *   Generate the required certificates using the script from `utils/certs`.
    *   Connect to home assistant and Copy certificates in the MQTT config.
7.  Use `run.sh` to start the local dhcp server and home assistant + mosquitto services.
8.  Build, flash, monitor device.

## Installation notes - WINDOWS

Build environment can be run on Windows using WSL2. Tested on Windows 10.

**Requirements:**

*   VSCode
    *   Devcontainer extension
*   WSL2

> **NOTE:** Docker Desktop is not required! Do not install it! If already installed, the steps need to be modified.

**Instructions:**

1.  Install WSL2, instructions here: <https://learn.microsoft.com/en-us/windows/wsl/install> (NOTE: installed "Ubuntu" distro).
2.  Start WSL2 console, manually install Docker. Instructions for Ubuntu are here: <https://docs.docker.com/engine/install/ubuntu/>.
3.  To see the board serial inside WSL2 and the docker container, follow instructions here: <https://learn.microsoft.com/en-us/windows/wsl/connect-usb>.
4.  Connect board to laptop, attach it to WSL2 using instructions in page. This needs to be done before creating the container!
5.  Install VSCode. When started, it will auto-detect WSL2 and install proper extension. Also install Devcontainer extensions.
6.  Follow the [Linux Instructions](#installation-notes---linux).

## Hardware

![ESP32-POE-ISO GPIO Pinout](images/ESP32-POE-ISO-GPIO.png)

### Current Limitations

*   ⚠️ OTA functionality not working in the WIFI Backup mode, only with Ethernet connectivity.
*   Every ESP32 should have it's own mqtts certificate
*   If the URL broker is modified in the config, it is not saved on reboot