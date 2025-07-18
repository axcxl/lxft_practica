#!/bin/bash
# Script for running a local DHCP server
# ARGUMENTS:
#   "-conf" = update the config file

sudo service isc-dhcp-server stop

if [[ "$1" == "-conf" ]]; then
    echo "~~~ Updating Config File ~~~"
    sudo rm /etc/dhcp/dhcpd.conf
    sudo cp dhcp.conf /etc/dhcp/dhcpd.conf
    cat /etc/dhcp/dhcpd.conf
    echo ""
fi


echo "~~~ Setting up the Interface ~~~"
# Tell NetworkManager to ignore the interface to prevent it from overriding the manual IP configuration
sudo nmcli dev set eth0 managed no

sudo ip addr flush dev eth0
sudo ip addr add 192.168.111.1/24 dev eth0
ip a s eth0
echo ""


echo "~~~ Start DHCP server  ~~~"
sudo service isc-dhcp-server restart

sudo service isc-dhcp-server status
