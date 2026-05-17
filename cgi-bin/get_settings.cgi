#!/bin/bash
echo "Content-Type: application/json"
echo ""

WAN_MODE="dhcp"
STATIC_IP=""
STATIC_MASK=""
STATIC_GATEWAY=""
WIFI_SSID=""
WIFI_PASSWORD=""
WIFI_ENCRYPTION="wpa2"
HOSTNAME="router"

if [ -f /etc/network/interfaces ]; then
    if grep -q "static" /etc/network/interfaces; then
        WAN_MODE="static"
        STATIC_IP=$(grep -A10 "iface eth0" /etc/network/interfaces | grep "address" | awk '{print $2}')
        STATIC_MASK=$(grep -A10 "iface eth0" /etc/network/interfaces | grep "netmask" | awk '{print $2}')
        STATIC_GATEWAY=$(grep -A10 "iface eth0" /etc/network/interfaces | grep "gateway" | awk '{print $2}')
    fi
fi

if [ -f /etc/hostapd/hostapd.conf ]; then
    WIFI_SSID=$(grep "^ssid=" /etc/hostapd/hostapd.conf | cut -d'=' -f2)
    WIFI_ENCRYPTION=$(grep "^wpa_key_mgmt=" /etc/hostapd/hostapd.conf | cut -d'=' -f2)
    if [ "$WIFI_ENCRYPTION" = "WPA-PSK WPA2-PSK" ]; then
        WIFI_ENCRYPTION="wpa2"
    elif [ "$WIFI_ENCRYPTION" = "WPA3-SAE" ]; then
        WIFI_ENCRYPTION="wpa3"
    fi
fi

if [ -f /etc/hostname ]; then
    HOSTNAME=$(cat /etc/hostname)
fi

cat << JSON
{
    "wan_mode": "$WAN_MODE",
    "static_ip": "$STATIC_IP",
    "static_mask": "$STATIC_MASK",
    "static_gateway": "$STATIC_GATEWAY",
    "wifi_ssid": "$WIFI_SSID",
    "wifi_password": "",
    "wifi_encryption": "$WIFI_ENCRYPTION",
    "hostname": "$HOSTNAME"
}
JSON