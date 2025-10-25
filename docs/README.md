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
sudo apt-get install gcc-aarch64-linux-gnu
```

---

### Build from Source

#### 1. Enter to the Kernel Source
```bash
cd /usr/src
#Find your linux folder
cd linux-hwe-6.8-6.8.0
# or extract it 
sudo apt install linux-source
sudo tar xjf linux-source-*.tar.bz2
cd linux-source-*
```

#### 2. Add Driver to Kernel Tree
```bash
# Create driver directory
sudo mkdir drivers/misc/nxp_simtemp

# Copy driver files to kernel tree
sudo cp kernel/* /usr/src/linux-hwe-6.8-6.8.0/drivers/misc/nxp_simtemp/
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
#On your linux foilder
make menuconfig
# Navigate to: Device Drivers → Misc devices → Simulated Temperature device driver
# Set to <M> for module

make modules -j$(nproc)

#or compile just the folder
sudo make M=drivers/misc modules
```

#### 5. Compile the dtb for the frdm 
```bash
#on linux-imx
$ sudo apt-get install gcc-aarch64-linux-gnu
$ make -j $(nproc --all) imx_v8_defconfig ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
$ make -j $(nproc --all) freescale/imx93-11x11-frdm-simtemp.dtb ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```
This generates the custom DTB at:
arch/arm64/boot/dts/freescale/imx93-11x11-frdm-simtemp.dtb

#### 6. Compile Your Kernel Module

Now compile your driver using the cross-compiler:
```bash
$ cd ~/linux-imx
$ make M=/home/gaby/Documents/NXP_challenge/simtemp/kernel modules ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

# Or, from within your module directory:

$ cd /home/gaby/Documents/NXP_challenge/simtemp/kernel
$ make -C ~/linux-imx M=$(pwd) modules ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

# Alternatively, use the helper script:

$ ./build_module.sh
```
#### 6.1 Verify the Cross-Compiled Module
```bash
$ file nxp_simtemp.ko
#Expected output: ELF 64-bit LSB relocatable, ARM aarch64, version 1 (SYSV), not stripped

$ aarch64-linux-gnu-readelf -h nxp_simtemp.ko | grep Machine
#Expected output: Machine: AArch64
```

5. Prepare for Deployment
```bash
$ mkdir -p frdm_deployment
$ cp /home/gaby/Documents/NXP_challenge/simtemp/kernel/nxp_simtemp.ko frdm_deployment/
$ cp ~/linux-imx/arch/arm64/boot/dts/freescale/imx93-11x11-frdm-simtemp.dtb frdm_deployment/
$ ls -la frdm_deployment/
```

6. Deploy to FRDM Board

```bash
sudo cp frdm_deployment/imx93-11x11-frdm-simtemp.dtb /media/boot/your-sd-card/
sudo cp frdm_deployment/nxp_simtemp.ko /home/root/
```

---

## Testing on the FRDM Board

After booting with the new DTB:

### Loading the Module
```bash
insmod nxp_simtemp.ko
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

# 2. Setup permissions
./setup_permissions.sh

# 3. Control the temperature monitoring system
./client_control.sh

# 4. Use with application
./client

# 5. Monitor
sudo dmesg | tail -20
```
