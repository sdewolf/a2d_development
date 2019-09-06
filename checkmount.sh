#!/bin/bash

if mount | grep /home/avn4/Data > /dev/null; then
    echo "yay"
else
    mount -v 192.168.1.100:/exports/Data /home/avn4/Data
fi