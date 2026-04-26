#!/bin/bash

set -e

echo "=================================="
echo " CRuntime Setup and Test Script"
echo "=================================="
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Error: This script must be run as root"
    exit 1
fi

# Check kernel version
KERNEL_VERSION=$(uname -r | cut -d. -f1-2)
echo "Kernel version: $(uname -r)"

# Check cgroups v2
echo
echo "Checking cgroups v2..."
if [ -d "/sys/fs/cgroup/cgroup.controllers" ]; then
    echo "✓ cgroups v2 is available"
    cat /sys/fs/cgroup/cgroup.controllers
else
    echo "✗ cgroups v2 not found"
    echo "  You may need to enable it with: systemd.unified_cgroup_hierarchy=1"
    exit 1
fi

# Check required tools
echo
echo "Checking required tools..."
MISSING_TOOLS=0

check_tool() {
    if command -v $1 &> /dev/null; then
        echo "✓ $1"
    else
        echo "✗ $1 not found"
        MISSING_TOOLS=1
    fi
}

check_tool ip
check_tool iptables
check_tool nsenter

if [ $MISSING_TOOLS -eq 1 ]; then
    echo
    echo "Please install missing tools:"
    echo "  Ubuntu/Debian: sudo apt-get install iproute2 iptables util-linux"
    echo "  RHEL/CentOS: sudo yum install iproute iptables util-linux"
    exit 1
fi

# Check overlay filesystem
echo
echo "Checking overlay filesystem..."
if grep -q overlay /proc/filesystems; then
    echo "✓ OverlayFS is available"
else
    echo "Loading overlay module..."
    modprobe overlay
    if grep -q overlay /proc/filesystems; then
        echo "✓ OverlayFS loaded successfully"
    else
        echo "✗ Failed to load OverlayFS"
        exit 1
    fi
fi

# Enable IP forwarding
echo
echo "Enabling IP forwarding..."
sysctl -w net.ipv4.ip_forward=1 > /dev/null
echo "✓ IP forwarding enabled"

# Build CRuntime
echo
echo "Building CRuntime..."
if [ ! -f "Makefile" ]; then
    echo "Error: Run this script from the CRuntime source directory"
    exit 1
fi

make clean
make

if [ ! -f "bin/cruntime" ]; then
    echo "✗ Build failed"
    exit 1
fi

echo "✓ Build successful"

# Install
echo
echo "Installing CRuntime..."
make install
echo "✓ Installed to /usr/local/bin/cruntime"

# Create test rootfs
echo
echo "Setting up test environment..."

TEST_DIR="/tmp/cruntime-test"
mkdir -p $TEST_DIR

# Create minimal Alpine rootfs for testing
ROOTFS_DIR="$TEST_DIR/alpine-rootfs"
if [ ! -d "$ROOTFS_DIR" ]; then
    echo "Downloading Alpine Linux minirootfs..."
    mkdir -p $ROOTFS_DIR
    cd $TEST_DIR
    
    # Download Alpine minirootfs (adjust version as needed)
    ALPINE_VERSION="3.18"
    ALPINE_ARCH="x86_64"
    ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/releases/${ALPINE_ARCH}/alpine-minirootfs-${ALPINE_VERSION}.0-${ALPINE_ARCH}.tar.gz"
    
    wget -q $ALPINE_URL -O alpine.tar.gz || {
        echo "Warning: Could not download Alpine. You'll need to provide your own rootfs."
        echo "Place a rootfs at: /var/lib/cruntime/images/test/rootfs"
        exit 0
    }
    
    tar -xzf alpine.tar.gz -C $ROOTFS_DIR
    rm alpine.tar.gz
    echo "✓ Alpine rootfs extracted"
fi

# Setup image directory
IMAGE_DIR="/var/lib/cruntime/images/alpine"
mkdir -p $IMAGE_DIR
rm -rf $IMAGE_DIR/rootfs
cp -a $ROOTFS_DIR $IMAGE_DIR/rootfs

echo "✓ Test rootfs installed"

# Run test container
echo
echo "=================================="
echo " Running Test Container"
echo "=================================="
echo

echo "Starting container with 'ls /' command..."
cruntime run --name test-container alpine ls /

echo
echo "=================================="
echo " Setup Complete!"
echo "=================================="
echo
echo "You can now run containers:"
echo
echo "  # Run interactive shell"
echo "  sudo cruntime run alpine /bin/sh"
echo
echo "  # Run with resource limits"
echo "  sudo cruntime run --memory 512M --cpus 1.0 alpine /bin/sh"
echo
echo "  # Run with port forwarding"
echo "  sudo cruntime run -p 8080:80 alpine /bin/sh"
echo
echo "  # Run detached"
echo "  sudo cruntime run --detach alpine sleep 3600"
echo
