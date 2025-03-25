#!/bin/bash

echo "Installing required dependencies..."

sudo apt update
sudo apt install libgpiod-dev

echo "Setting up the daemon..."

USER="alek"
GROUP="users"
LOGPATH="/var/lib/"

echo "Editing Permissions..."
chown "$USER":"$GROUP" "$LOGPATH"
chmod 700 "$LOGPATH"

APP_PATH="/home/$USER/UPS-driver"
APP_PATH_EXECUTABLE="$APP_PATH/main"

SERVICE_FILE="/etc/systemd/system/ups-driver.service"

make -C "$APP_PATH"

chmod +x "$APP_PATH_EXECUTABLE"

sudo bash -c "echo '[Unit]' > $SERVICE_FILE"
sudo bash -c "echo 'Description=UPS-driver' >> $SERVICE_FILE"
sudo bash -c "echo 'After=network.target' >> $SERVICE_FILE"
sudo bash -c "echo 'StartLimitIntervalSec=0' >> $SERVICE_FILE"

sudo bash -c "echo '' >> $SERVICE_FILE"
sudo bash -c "echo '[Service]' >> $SERVICE_FILE"
sudo bash -c "echo 'Type=simple' >> $SERVICE_FILE"
sudo bash -c "echo 'Restart=always' >> $SERVICE_FILE"
sudo bash -c "echo 'RestartSec=1' >> $SERVICE_FILE"
sudo bash -c "echo 'User=$USER' >> $SERVICE_FILE"
sudo bash -c "echo 'ExecStart=$APP_PATH_EXECUTABLE' >> $SERVICE_FILE"

sudo bash -c "echo '' >> $SERVICE_FILE"
sudo bash -c "echo '[Install]' >> $SERVICE_FILE"
sudo bash -c "echo 'WantedBy=multi-user.target' >> $SERVICE_FILE"

sudo systemctl daemon-reload
sudo systemctl enable ups-driver.service
sudo systemctl start ups-driver.service
sudo systemctl status ups-driver.service