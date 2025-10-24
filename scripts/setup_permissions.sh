#!/bin/bash
# setup_permissions.sh - Sets up permissions for simtemp device and files

echo "Setting up permissions for simtemp temperature monitoring system..."

# Compile the client program
#echo "Compiling client.c..."
#gcc client.c -o client
#if [ $? -eq 0 ]; then
#    echo "Client compiled successfully"
#else
#    echo "Error: Failed to compile client.c"
#    exit 1
#fi

# Set permissions for the device
echo "Setting permissions for /dev/simtemp..."
sudo chmod 666 /dev/simtemp

# Set permissions for sysfs entries
echo "Setting permissions for sysfs entries..."
sudo chmod 666 /sys/class/misc/simtemp/sampling_ms
sudo chmod 666 /sys/class/misc/simtemp/threshold_mC
sudo chmod 666 /sys/class/misc/simtemp/mode
sudo chmod 666 /sys/class/misc/simtemp/reset_alerts

echo "All permissions set successfully!"
echo "You can now run: ./client_control.sh"
