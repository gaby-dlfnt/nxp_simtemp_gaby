# NXP Simulated Temperature Driver

A Linux kernel module that provides a simulated temperature device with configurable operating modes and real-time monitoring capabilities.

---

## Features

- **Multiple Temperature Modes:**
  - **Normal:** Stable temperature readings
  - **Noisy:** Temperature with random variations
  - **Ramp:** Gradually increasing/decreasing temperature
- **Real-time Monitoring:** Continuous temperature sampling
- **Configurable Parameters:** Adjustable sampling rate and thresholds
- **SysFS Interface:** User-space configuration via sysfs
- **Character Device:** Application interface via `/dev/simtemp`

---

## Installation

### Prerequisites

```bash
sudo apt update
sudo apt install linux-source build-essential libncurses-dev flex bison libssl-dev
```

---

### Build from Source

#### 1. Extract Kernel Source
```bash
cd /usr/src
sudo tar xjf linux-source-*.tar.bz2
cd linux-source-*
```

#### 2. Add Driver to Kernel Tree
```bash
# Create driver directory
sudo mkdir drivers/misc/nxp_simtemp

# Copy driver files to kernel tree
sudo cp nxp_simtemp.c nxp_simtemp.h nxp_simtemp_ioctl.h Kconfig Makefile drivers/misc/nxp_simtemp/
```

#### 3. Update Kernel Configuration

Edit `drivers/misc/Makefile` and add:
```makefile
obj-$(CONFIG_NXP_SIMTEMP) += nxp_simtemp/
```

Edit `drivers/misc/Kconfig` and add:
```kconfig
source "drivers/misc/nxp_simtemp/Kconfig"
```

#### 4. Configure and Build
```bash
make menuconfig
# Navigate to: Device Drivers → Misc devices → Simulated Temperature device driver
# Set to <M> for module

make modules -j$(nproc)
```

---

## Usage

### Loading the Module
```bash
# Check if module is loaded
lsmod | grep nxp_simtemp

# Load the module
sudo insmod drivers/misc/nxp_simtemp/nxp_simtemp.ko

# Verify device creation
ls /dev/simtemp

# Check kernel messages
sudo dmesg | tail

# Unload module
sudo rmmod nxp_simtemp
```

---

### System Configuration via SysFS
```bash
# View current settings
cat /sys/class/misc/simtemp/sampling_ms
cat /sys/class/misc/simtemp/threshold_mC
cat /sys/class/misc/simtemp/mode

# Set permissions for user configuration
sudo chmod 666 /sys/class/misc/simtemp/sampling_ms
sudo chmod 666 /sys/class/misc/simtemp/threshold_mC
sudo chmod 666 /sys/class/misc/simtemp/mode

# Configure parameters
echo 200 > /sys/class/misc/simtemp/sampling_ms        # Set sampling to 200ms
echo 40000 > /sys/class/misc/simtemp/threshold_mC     # Set threshold to 40°C

# Change temperature mode
echo normal > /sys/class/misc/simtemp/mode
echo noisy > /sys/class/misc/simtemp/mode
echo ramp > /sys/class/misc/simtemp/mode
```

---

## Application Interface

### Compile Client Application
```bash
cd /user/cli
gcc client.c -o client
sudo chmod 666 /dev/simtemp
./client
```

### Example Client Usage

The client application can:
- Read current temperature  
- Set configuration parameters  
- Monitor temperature changes  
- Handle threshold alerts  

---

## Configuration Parameters

| Parameter     | Description                                 | Default | Range        |
|----------------|---------------------------------------------|----------|--------------|
| sampling_ms    | Sampling interval in milliseconds            | 1000     | 10–5000      |
| threshold_mC   | Temperature threshold in millidegrees Celsius| 50000    | 0–100000     |
| mode           | Temperature generation mode                  | normal   | normal, noisy, ramp |

---

## Temperature Modes

- **normal:** Stable temperature at ~25°C  
- **noisy:** Temperature with ±2°C random noise  
- **ramp:** Temperature ramping from 20°C to 30°C and back  

---

## System Integration

### Module Installation
```bash
# Install module system-wide
sudo make modules_install
sudo depmod -a

# Load on boot
echo "nxp_simtemp" | sudo tee -a /etc/modules
```

### udev Rules (Optional)
Create `/etc/udev/rules.d/99-nxp-simtemp.rules`:
```
KERNEL=="simtemp", MODE="0666", GROUP="users"
```

---

## Debugging

### Kernel Messages
```bash
# View module loading messages
sudo dmesg | grep simtemp

# Monitor real-time messages
sudo dmesg -w
```

### Module Information
```bash
# Module details
modinfo nxp_simtemp

# Module parameters
sudo cat /sys/module/nxp_simtemp/parameters/*
```

---

## Files Created

- **Device File:** `/dev/simtemp`  
- **SysFS Directory:** `/sys/class/misc/simtemp/`  
- **SysFS Attributes:**
  - `sampling_ms` – Sampling interval  
  - `threshold_mC` – Temperature threshold  
  - `mode` – Operating mode  

---

## License

This driver is licensed under the **GPL v2**, as required for Linux kernel modules.

---

## Contributing

1. Fork the repository  
2. Create a feature branch  
3. Commit your changes  
4. Submit a pull request  

---

## Troubleshooting

### Common Issues

**Module won't load:**
- Check kernel version compatibility  
- Verify all dependencies are met  
- Examine `dmesg` for error messages  

**Device file not created:**
- Check if module loaded successfully  
- Verify udev rules (if used)  
- Check permissions on `/dev/simtemp`  

**SysFS attributes missing:**
- Ensure module compiled with debug support  
- Check kernel configuration  

---

## Support

For issues and questions:
- Check kernel messages with `dmesg`  
- Verify module loading with `lsmod`  
- Ensure proper file permissions  

---

## Quick Start Example

```bash
# 1. Build and load module
sudo insmod nxp_simtemp.ko

# 2. Set permissions
sudo chmod 666 /dev/simtemp
sudo chmod 666 /sys/class/misc/simtemp/*

# 3. Configure
echo "noisy" > /sys/class/misc/simtemp/mode
echo "500" > /sys/class/misc/simtemp/sampling_ms

# 4. Use with application
./client

# 5. Monitor
sudo dmesg | tail -20
```
