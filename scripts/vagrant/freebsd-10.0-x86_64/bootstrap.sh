#!/bin/sh

#
# Update the system.
#

# Update package list
pkg update

# Update base system
pkg upgrade -y

# Install packags needed to build.
pkg install -y \
	bash \
	clang35 \
	cmake \
	git \
	ninja \
	python \
	subversion \
	swig13 \
	tmux

#
# Add second disk.
# On VMware fusion, it shows up as /dev/da1.
#
DISK_DEVICE_NAME=da1
PARTITION_NAME=/dev/${DISK_DEVICE_NAME}p1
MOUNT_DIR=/mnt/lldb

# Create the partition.
gpart create -s GPT $DISK_DEVICE_NAME

# Set the partition type.
gpart add -t freebsd-ufs $DISK_DEVICE_NAME

# Create the filesystem.
newfs -U $PARTITION_NAME

# Create the mount point.
mkdir -p $MOUNT_DIR

# Add mount point to fstab so it is attached automatically on future boots.
echo "$PARTITION_NAME    $MOUNT_DIR    ufs    rw    2    2" | sudo tee -a /etc/fstab

# Mount it for this session.
mount $PARTITION_NAME

# Set permissions for vagrant user on new file system.
chown vagrant $MOUNT_DIR
chgrp vagrant $MOUNT_DIR

# Create link to /mnt/lldb from vagrant homedir
sudo -u vagrant ln -s $MOUNT_DIR ~vagrant/lldb
