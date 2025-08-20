#!/bin/bash

cd utils/DHCP/
./dhcp.sh

cd ../home_assistant/
./run_dockers.sh

cd ../..