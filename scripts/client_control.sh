#!/bin/bash

# client_control.sh - Interactive control for simtemp temperature monitoring

KERNEL_MODULE="nxp_simtemp.ko"
MODULE_NAME="nxp_simtemp"

show_help() {
    echo "=== NXP SimTemp Control Script ==="
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  1              Change threshold"
    echo "  2              Change sampling period"
    echo "  3              Change mode"
    echo "  4              Show current values"
    echo "  5              Run client program"
    echo "  6              Reset alarm count"
    echo "  7              Show device info"
    echo "  8              Load kernel module"
    echo "  9              Unload kernel module"
    echo "  -h, --help     Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 1           # Change threshold"
    echo "  $0 3           # Change mode"
    echo "  $0 4           # Show current values"
    echo "  $0 8           # Load module with default params"
}

show_current_values() {
    echo "=== Current SimTemp Values ==="
    if [ -f "/sys/class/misc/simtemp/sampling_ms" ]; then
        echo "Sampling period: $(cat /sys/class/misc/simtemp/sampling_ms) ms"
    else
        echo "Sampling period: Not available"
    fi
    
    if [ -f "/sys/class/misc/simtemp/threshold_mC" ]; then
        echo "Threshold: $(cat /sys/class/misc/simtemp/threshold_mC) mC"
    else
        echo "Threshold: Not available"
    fi
    
    if [ -f "/sys/class/misc/simtemp/mode" ]; then
        echo "Mode: $(cat /sys/class/misc/simtemp/mode)"
    else
        echo "Mode: Not available"
    fi
    
    if [ -f "/sys/class/misc/simtemp/stats" ]; then
        echo "Stats: $(cat /sys/class/misc/simtemp/stats)"
    else
        echo "Stats: Not available"
    fi
    echo ""
}

check_device_available() {
    if [ ! -c "/dev/simtemp" ]; then
        echo "Error: /dev/simtemp device not found!"
        echo "The kernel module may not be loaded."
        echo "Use option 8 to load the module first."
        return 1
    fi
    return 0
}

load_kernel_module() {
    echo "Loading kernel module..."
    
    # Check if module is already loaded
    if lsmod | grep -q "$MODULE_NAME"; then
        echo "Module is already loaded. Unloading first..."
        sudo rmmod $MODULE_NAME
    fi
    
    # Ask for parameters or use defaults
    read -p "Enter sampling period [200]: " sampling
    sampling=${sampling:-200}
    
    read -p "Enter threshold [40000]: " threshold
    threshold=${threshold:-40000}
    
    echo "Available modes:"
    echo "  0) normal"
    echo "  1) noisy"
    echo "  2) ramp"
    read -p "Select mode [1]: " mode
    mode=${mode:-1}
    
    echo "Loading module with: sampling_ms=$sampling, threshold_mC=$threshold, default_mode=$mode"
    
    sudo insmod $KERNEL_MODULE sampling_ms=$sampling threshold_mC=$threshold default_mode=$mode
    
    if [ $? -eq 0 ]; then
        echo "Module loaded successfully!"
        # Auto-setup permissions after loading
        ./setup_permissions.sh
    else
        echo "Failed to load module"
        dmesg | tail -5
    fi
}

unload_kernel_module() {
    echo "Unloading kernel module..."
    
    if lsmod | grep -q "$MODULE_NAME"; then
        sudo rmmod $MODULE_NAME
        if [ $? -eq 0 ]; then
            echo "Module unloaded successfully!"
        else
            echo "Failed to unload module"
            dmesg | tail -5
        fi
    else
        echo "Module is not loaded"
    fi
}

change_threshold() {
    if ! check_device_available; then
        return 1
    fi
    
    show_current_values
    read -p "Enter new threshold value in mC (e.g., 40000): " threshold
    if [[ $threshold =~ ^[0-9]+$ ]]; then
        echo $threshold > /sys/class/misc/simtemp/threshold_mC
        if [ $? -eq 0 ]; then
            echo "Threshold set to $threshold mC"
        else
            echo "Error: Failed to set threshold"
        fi
    else
        echo "Error: Please enter a valid number"
    fi
}

change_sampling() {
    if ! check_device_available; then
        return 1
    fi
    
    show_current_values
    read -p "Enter new sampling period in ms (e.g., 200): " sampling
    if [[ $sampling =~ ^[0-9]+$ ]]; then
        echo $sampling > /sys/class/misc/simtemp/sampling_ms
        if [ $? -eq 0 ]; then
            echo "Sampling period set to $sampling ms"
        else
            echo "Error: Failed to set sampling period"
        fi
    else
        echo "Error: Please enter a valid number"
    fi
}

change_mode() {
    if ! check_device_available; then
        return 1
    fi
    
    show_current_values
    echo "Available modes:"
    echo "  1) normal"
    echo "  2) noisy" 
    echo "  3) ramp"
    read -p "Select mode (1-3): " mode_choice
    
    case $mode_choice in
        1) 
            echo "normal" > /sys/class/misc/simtemp/mode
            echo "Mode set to normal"
            ;;
        2) 
            echo "noisy" > /sys/class/misc/simtemp/mode
            echo "Mode set to noisy"
            ;;
        3) 
            echo "ramp" > /sys/class/misc/simtemp/mode
            echo "Mode set to ramp"
            ;;
        *) 
            echo "Error: Invalid selection"
            ;;
    esac
}

run_client() {
    if ! check_device_available; then
        return 1
    fi
    
    if [ ! -f "./client" ]; then
        echo "Error: client program not found!"
        echo "Please make sure client.c is compiled to 'client'"
        return 1
    fi
    
    echo "Starting client program..."
    echo "Press Ctrl+C to stop the client"
    ./client
}

reset_alarms() {
    if ! check_device_available; then
        return 1
    fi
    
    if [ ! -f "/sys/class/misc/simtemp/reset_alerts" ]; then
        echo "Error: reset_alerts sysfs file not found!"
        echo "Make sure the module has the reset functionality"
        return 1
    fi
    
    echo "Resetting alarm count..."
    echo 1 > /sys/class/misc/simtemp/reset_alerts
    if [ $? -eq 0 ]; then
        echo "Alarm count reset successfully!"
        show_current_values
    else
        echo "Error: Failed to reset alarm count"
    fi
}

show_device_info() {
    echo "=== Device Information ==="
    
    # Check if module is loaded
    if lsmod | grep -q "$MODULE_NAME"; then
        echo "Module: $MODULE_NAME - Loaded"
        echo "Module parameters:"
        if [ -d "/sys/module/$MODULE_NAME/parameters" ]; then
            for param in /sys/module/$MODULE_NAME/parameters/*; do
                param_name=$(basename $param)
                param_value=$(cat $param 2>/dev/null)
                echo "  $param_name = $param_value"
            done
        fi
    else
        echo "Module: $MODULE_NAME - Not loaded"
    fi
    
    # Check if device exists
    if [ -c "/dev/simtemp" ]; then
        echo "Device: /dev/simtemp - Present"
        ls -la /dev/simtemp
    else
        echo "Device: /dev/simtemp - Not found"
    fi
    
    echo ""
    echo "=== SysFS Entries ==="
    for entry in sampling_ms threshold_mC mode stats reset_alerts; do
        if [ -f "/sys/class/misc/simtemp/$entry" ]; then
            echo "/sys/class/misc/simtemp/$entry - Present"
        else
            echo "/sys/class/misc/simtemp/$entry - Not found"
        fi
    done
}

# Main script logic
if [ $# -eq 0 ]; then
    # Interactive mode if no arguments provided
    while true; do
        echo ""
        echo "=== NXP SimTemp Control Menu ==="
        echo "1) Change threshold"
        echo "2) Change sampling period" 
        echo "3) Change mode"
        echo "4) Show current values"
        echo "5) Run client program"
        echo "6) Reset alarm count"
        echo "7) Show device info"
        echo "8) Load kernel module"
        echo "9) Unload kernel module"
        echo "10) Exit"
        read -p "Select option (1-10): " choice
        
        case $choice in
            1) change_threshold ;;
            2) change_sampling ;;
            3) change_mode ;;
            4) show_current_values ;;
            5) run_client ;;
            6) reset_alarms ;;
            7) show_device_info ;;
            8) load_kernel_module ;;
            9) unload_kernel_module ;;
            10) echo "Goodbye!"; exit 0 ;;
            *) echo "Invalid option. Please try again." ;;
        esac
    done
else
    # Command-line argument mode
    case $1 in
        1) change_threshold ;;
        2) change_sampling ;;
        3) change_mode ;;
        4) show_current_values ;;
        5) run_client ;;
        6) reset_alarms ;;
        7) show_device_info ;;
        8) load_kernel_module ;;
        9) unload_kernel_module ;;
        -h|--help) show_help ;;
        *) 
            echo "Error: Invalid option '$1'"
            echo ""
            show_help 
            exit 1 
            ;;
    esac
fi