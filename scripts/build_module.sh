#!/bin/bash

echo "=== Building Kernel Module for FRDM-IMX93 ==="

KERNEL_SRC="$HOME/linux-imx"
MODULE_DIR="$HOME/Documents/NXP_challenge/simtemp/kernel"
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Check toolchain
echo "1. Checking toolchain..."
which ${CROSS_COMPILE}gcc || { echo "Toolchain not found!"; exit 1; }

# Prepare kernel build
echo "2. Preparing kernel build..."
make -C $KERNEL_SRC prepare

echo "3. Building kernel scripts..."
make -C $KERNEL_SRC scripts

# Build the module
echo "4. Building kernel module..."
make -C $KERNEL_SRC M=$MODULE_DIR modules

if [ $? -eq 0 ]; then
    echo "Module built successfully!"
    echo "File info:"
    file $MODULE_DIR/nxp_simtemp.ko
else
    echo "Module build failed!"
    exit 1
fi

echo "Build complete!"