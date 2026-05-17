#!/bin/bash
echo "Content-Type: text/plain"
echo ""

read -r DATA

HOSTNAME=$(echo "$DATA" | grep -o '"hostname":"[^"]*"' | cut -d'"' -f4)

if [ -n "$HOSTNAME" ]; then
    echo "$HOSTNAME" > /etc/hostname
    sed -i "s/^\(127\.0\.0\.1\s*\).*/\1 $HOSTNAME/" /etc/hosts
    hostname "$HOSTNAME"
    echo "主机名已更新为: $HOSTNAME"
else
    echo "错误: 主机名不能为空"
fi