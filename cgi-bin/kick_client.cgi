#!/bin/bash
echo "Content-Type: text/plain"
echo ""

MAC_ADDR=$QUERY_STRING
MAC_ADDR=$(echo "$MAC_ADDR" | sed 's/mac=//')

if [ -z "$MAC_ADDR" ]; then
    echo "错误: 未指定MAC地址"
    exit 1
fi

if iw dev wlan0 station del "$MAC_ADDR" 2>/dev/null; then
    echo "成功踢出设备: $MAC_ADDR"
else
    echo "踢出失败: 设备不存在或无法断开连接"
fi