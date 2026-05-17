#!/bin/bash
echo "Content-Type: text/plain"
echo ""

read -r DATA

WAN_MODE=$(echo "$DATA" | grep -o '"wan_mode":"[^"]*"' | cut -d'"' -f4)
STATIC_IP=$(echo "$DATA" | grep -o '"static_ip":"[^"]*"' | cut -d'"' -f4)
STATIC_MASK=$(echo "$DATA" | grep -o '"static_mask":"[^"]*"' | cut -d'"' -f4)
STATIC_GATEWAY=$(echo "$DATA" | grep -o '"static_gateway":"[^"]*"' | cut -d'"' -f4)

if [ "$WAN_MODE" = "static" ]; then
    cat > /etc/network/interfaces << EOF
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet static
    address $STATIC_IP
    netmask $STATIC_MASK
    gateway $STATIC_GATEWAY
EOF
else
    cat > /etc/network/interfaces << EOF
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet dhcp
EOF
fi

echo "网络设置已保存，重启网络接口后生效"