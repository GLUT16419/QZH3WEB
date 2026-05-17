#!/bin/bash
echo "Content-Type: text/plain"
echo ""

read -r DATA

SSID=$(echo "$DATA" | grep -o '"ssid":"[^"]*"' | cut -d'"' -f4)
PASSWORD=$(echo "$DATA" | grep -o '"password":"[^"]*"' | cut -d'"' -f4)
ENCRYPTION=$(echo "$DATA" | grep -o '"encryption":"[^"]*"' | cut -d'"' -f4)

if [ -z "$SSID" ] || [ -z "$PASSWORD" ]; then
    echo "错误: SSID和密码不能为空"
    exit 1
fi

if [ ${#PASSWORD} -lt 8 ]; then
    echo "错误: 密码长度至少需要8位"
    exit 1
fi

WPA_KEY_MGMT="WPA-PSK WPA2-PSK"
if [ "$ENCRYPTION" = "wpa3" ]; then
    WPA_KEY_MGMT="WPA3-SAE"
fi

cat > /etc/hostapd/hostapd.conf << EOF
interface=wlan0
driver=nl80211
ssid=$SSID
hw_mode=g
channel=6
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=$PASSWORD
wpa_key_mgmt=$WPA_KEY_MGMT
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

echo "WiFi设置已保存，重启hostapd服务后生效"