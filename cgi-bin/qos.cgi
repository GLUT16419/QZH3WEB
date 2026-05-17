#!/bin/bash
echo "Content-Type: application/json"
echo ""

# QoS配置文件
QoS_FILE="/etc/qos.conf"
touch "$QoS_FILE"

# 读取POST数据
read -r POST_DATA

# 解析参数
UPSTREAM=$(echo "$POST_DATA" | grep -o '"upstream":"[^"]*"' | cut -d'"' -f4)
DOWNSTREAM=$(echo "$POST_DATA" | grep -o '"downstream":"[^"]*"' | cut -d'"' -f4)
PRIORITY=$(echo "$POST_DATA" | grep -o '"priority":"[^"]*"' | cut -d'"' -f4)

# 验证输入
if [ -z "$UPSTREAM" ] || [ -z "$DOWNSTREAM" ]; then
    echo '{"success":false,"message":"带宽参数不能为空"}'
    exit 0
fi

# 清除旧的QoS规则
tc qdisc del dev eth0 root 2>/dev/null || true
tc qdisc del dev wlan0 root 2>/dev/null || true

# 配置QoS（简单版本）
# 将带宽转换为kbps
UPSTREAM_KBPS=$((UPSTREAM * 1024))
DOWNSTREAM_KBPS=$((DOWNSTREAM * 1024))

# 配置下载QoS (eth0)
tc qdisc add dev eth0 root handle 1: htb default 30

# 创建主分类
tc class add dev eth0 parent 1: classid 1:1 htb rate "${DOWNSTREAM_KBPS}kbit" burst 15k

# 高优先级 (视频/游戏)
tc class add dev eth0 parent 1:1 classid 1:10 htb rate "$((DOWNSTREAM_KBPS * 30 / 100))kbit" burst 15k
# 中优先级 (网页/邮件)
tc class add dev eth0 parent 1:1 classid 1:20 htb rate "$((DOWNSTREAM_KBPS * 50 / 100))kbit" burst 15k
# 低优先级 (下载)
tc class add dev eth0 parent 1:1 classid 1:30 htb rate "$((DOWNSTREAM_KBPS * 20 / 100))kbit" burst 15k

# 配置上传QoS (wlan0)
tc qdisc add dev wlan0 root handle 1: htb default 30
tc class add dev wlan0 parent 1: classid 1:1 htb rate "${UPSTREAM_KBPS}kbit" burst 15k
tc class add dev wlan0 parent 1:1 classid 1:10 htb rate "$((UPSTREAM_KBPS * 30 / 100))kbit" burst 15k
tc class add dev wlan0 parent 1:1 classid 1:20 htb rate "$((UPSTREAM_KBPS * 50 / 100))kbit" burst 15k
tc class add dev wlan0 parent 1:1 classid 1:30 htb rate "$((UPSTREAM_KBPS * 20 / 100))kbit" burst 15k

# 保存配置
echo "upstream=$UPSTREAM" > "$QoS_FILE"
echo "downstream=$DOWNSTREAM" >> "$QoS_FILE"
echo "priority=$PRIORITY" >> "$QoS_FILE"

echo "{\"success\":true,\"message\":\"QoS设置已应用，上行${UPSTREAM}Mbps，下行${DOWNSTREAM}Mbps\"}"