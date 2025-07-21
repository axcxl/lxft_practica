# lxft_practica2024

## Table of Contents

 1. [Installation notes - LINUX](#subheading-1)
 2. [Installation notes - WINDOWS](#subheading-2)


## Installation notes - LINUX

Requirements:
- VSCode 
    - Devcontainer extension
- Docker
- Git

Instructions:
1. Clone repo
2. Open with VSCode
3. Click "Re-open in devcontainer" button and wait for the container to be built.
4. Connect ESP board, make sure /dev/ttyUSB* device appears in dmesg output. Select /dev/ttyUSB* device in VsCode
5. Build, flash, monitor device.


## Installation notes - WINDOWS

Build environment can be run on Windows using WSL2. Tested on Windows 10. 

<span style="color:red">**CURRENT LIMITATION: not sure how to run MQTT broker/client from WSL2 and make it available on the network**</span>

Requirements:
- VSCode 
    - Devcontainer extension
- WSL2

**NOTE: Docker Desktop is not required! Do not install it! If already installed, the steps need to be modified.**

Instructions:
1. Install WSL2, instructions here: https://learn.microsoft.com/en-us/windows/wsl/install (NOTE: installed "Ubuntu" distro)
2. Start WSL2 console, manually install Docker. Instructions for Ubuntu are here: https://docs.docker.com/engine/install/ubuntu/
3. To see the board serial inside WSL2 and the docker container, follow instructions here: https://learn.microsoft.com/en-us/windows/wsl/connect-usb
4. Connect board to laptop, attach it to WSL2 using instructions in page. This needs to be done before creating the container!
3. Install VSCode. When started, it will auto-detect WSL2 and install proper extension. Also install Devcontainer extensions.
4. Clone repo.
5. Click "Re-open in devcontainer". Wait for container to be built.
6. Build, flash, monitor device.

# TODOs
- Home Assistant Setup + Docker Compose
- HTTPs server info on OTA and configs ERRORS
- Wifi - Custom Hotspot Setup