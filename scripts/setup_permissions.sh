#!/bin/bash

# setup_permissions.sh - Sets up permissions for simtemp temperature monitoring system

echo "Setting up permissions for simtemp temperature monitoring system..."

# Check if module is loaded
if ! lsmod | grep -q "nxp_simtemp"; then
    echo "Warning: nxp_simtemp module is not loaded!"
    echo "You can load it with: sudo insmod nxp_simtemp.ko sampling_ms=200 threshold_mC=40000 default_mode=1"
    echo "Or use the client_control.sh script to load it automatically"
fi

# Set permissions for the device (if it exists)
echo "Setting permissions for /dev/simtemp..."
if [ -c "/dev/simtemp" ]; then
    sudo chmod 666 /dev/simtemp
    echo "✓ /dev/simtemp permissions set"
else
    echo "✗ /dev/simtemp not found (module may not be loaded)"
fi

# Set permissions for sysfs entries (if they exist)
echo "Setting permissions for sysfs entries..."
SYSFS_PATH="/sys/class/misc/simtemp"

if [ -d "$SYSFS_PATH" ]; then
    for entry in sampling_ms threshold_mC mode stats reset_alerts; do
        if [ -f "$SYSFS_PATH/$entry" ]; then
            sudo chmod 666 "$SYSFS_PATH/$entry"
            echo "$SYSFS_PATH/$entry permissions set"
        else
            echo "$SYSFS_PATH/$entry not found"
        fi
    done
else
    echo "SysFS directory not found (module may not be loaded)"
fi

echo ""
echo "=== Summary ==="
if [ -c "/dev/simtemp" ]; then
    echo "Device: /dev/simtemp - Present"
    ls -la /dev/simtemp
else
    echo "Device: /dev/simtemp - Not found (load module first)"
fi

echo ""
echo "Setup completed!"
echo "If module is not loaded, use: ./client_control.sh 8"