#!/bin/bash
echo "Content-Type: application/json"
echo ""

# 获取WAN (eth0) 流量
if [ -f /proc/net/dev ]; then
    wan_data=$(grep "eth0:" /proc/net/dev | sed 's/eth0://')
    wan_rx=$(echo $wan_data | awk '{print $1}')
    wan_tx=$(echo $wan_data | awk '{print $9}')
    
    # 格式化字节数
    format_bytes() {
        local bytes=$1
        if [ $bytes -lt 1024 ]; then
            echo "${bytes} B"
        elif [ $bytes -lt 1048576 ]; then
            echo "$((bytes / 1024)) KB"
        elif [ $bytes -lt 1073741824 ]; then
            echo "$((bytes / 1048576)) MB"
        else
            echo "$((bytes / 1073741824)) GB"
        fi
    }
    
    wan_rx_fmt=$(format_bytes $wan_rx)
    wan_tx_fmt=$(format_bytes $wan_tx)
    
    # 检查接口状态
    if ifconfig eth0 | grep -q "UP"; then
        wan_status="已连接"
    else
        wan_status="未连接"
    fi
else
    wan_rx_fmt="N/A"
    wan_tx_fmt="N/A"
    wan_status="未知"
fi

# 获取WiFi (wlan0) 流量
if [ -f /proc/net/dev ]; then
    wifi_data=$(grep "wlan0:" /proc/net/dev | sed 's/wlan0://')
    wifi_rx=$(echo $wifi_data | awk '{print $1}')
    wifi_tx=$(echo $wifi_data | awk '{print $9}')
    
    wifi_rx_fmt=$(format_bytes $wifi_rx)
    wifi_tx_fmt=$(format_bytes $wifi_tx)
    
    # 检查接口状态
    if ifconfig wlan0 | grep -q "UP"; then
        wifi_status="已连接"
    else
        wifi_status="未连接"
    fi
else
    wifi_rx_fmt="N/A"
    wifi_tx_fmt="N/A"
    wifi_status="未知"
fi

# 输出JSON
cat << JSON
{
    "wan": {
        "status": "${wan_status}",
        "rx": "${wan_rx_fmt}",
        "tx": "${wan_tx_fmt}"
    },
    "wifi": {
        "status": "${wifi_status}",
        "rx": "${wifi_rx_fmt}",
        "tx": "${wifi_tx_fmt}"
    }
}
JSON
