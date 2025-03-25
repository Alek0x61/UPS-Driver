#!/bin/bash

echo "Enabling I2C interface..."

CONFIG_FILE="/boot/firmware/config.txt"

if grep -q '^#dtparam=i2c_arm=on' "$CONFIG_FILE"; then
    # Uncomment the line
    sed -i 's/^#dtparam=i2c_arm=on/dtparam=i2c_arm=on/' "$CONFIG_FILE"
elif ! grep -q '^dtparam=i2c_arm=on' "$CONFIG_FILE"; then
    # If it's not found, append the line to the end of the file
    echo "dtparam=i2c_arm=on" >> "$CONFIG_FILE"
fi

if ! lsmod | grep -q 'i2c_bcm2708'; then
  sudo modprobe i2c_bcm2708
fi

if ! lsmod | grep -q 'i2c_dev'; then
  sudo modprobe i2c_dev
fi

if ! grep -q 'i2c_dev' /etc/modules; then
  echo "i2c_dev" | sudo tee -a /etc/modules
fi

echo "I2C interface enabled successfully."

read -p "Do you want to restart now? (y/n): " RESTART

if [[ "$RESTART" =~ ^[Yy]$ ]]; then
    echo "Restarting the system..."
    sudo reboot
fi