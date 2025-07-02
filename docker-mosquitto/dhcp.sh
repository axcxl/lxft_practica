#!/bin/bash

if [[ "$1" == "-conf" ]]; then
    sudo touch /etc/dhcp/dhcpd.conf
    sudo cp dhcp.conf /etc/dhcp/dhcpd.conf
    cat /etc/dhcp/dhcpd.conf
    echo ""
fi

# Tell NetworkManager to ignore eth0 to prevent it from overriding the manual IP configuration
sudo nmcli dev set eth0 managed no

sudo ip addr flush dev eth0
sudo ip addr add 192.168.100.1/24 dev eth0
ip a s eth0
echo ""

sudo service isc-dhcp-server restart

sudo service isc-dhcp-server status
