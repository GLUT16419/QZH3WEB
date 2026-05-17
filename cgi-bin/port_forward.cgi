#!/bin/bash
echo "Content-Type: application/json"
echo ""

# 端口转发规则存储文件
RULES_FILE="/etc/port_forward_rules.conf"
touch "$RULES_FILE"

# 获取查询参数
ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d'=' -f2)
ID=$(echo "$QUERY_STRING" | grep -o 'id=[^&]*' | cut -d'=' -f2)

# 读取POST数据
read -r POST_DATA

if [ "$ACTION" = "list" ]; then
    # 列出所有规则
    rules=()
    while IFS='|' read -r id protocol external_port internal_ip internal_port; do
        if [ -n "$id" ]; then
            rules+=("{\"id\":$id,\"protocol\":\"$protocol\",\"external_port\":\"$external_port\",\"internal_ip\":\"$internal_ip\",\"internal_port\":\"$internal_port\"}")
        fi
    done < "$RULES_FILE"
    
    echo "{\"success\":true,\"rules\":[${rules[*]}]}"

elif [ "$ACTION" = "add" ]; then
    # 添加规则
    PROTOCOL=$(echo "$POST_DATA" | grep -o '"protocol":"[^"]*"' | cut -d'"' -f4)
    EXTERNAL_PORT=$(echo "$POST_DATA" | grep -o '"external_port":"[^"]*"' | cut -d'"' -f4)
    INTERNAL_IP=$(echo "$POST_DATA" | grep -o '"internal_ip":"[^"]*"' | cut -d'"' -f4)
    INTERNAL_PORT=$(echo "$POST_DATA" | grep -o '"internal_port":"[^"]*"' | cut -d'"' -f4)
    
    # 验证输入
    if [ -z "$PROTOCOL" ] || [ -z "$EXTERNAL_PORT" ] || [ -z "$INTERNAL_IP" ] || [ -z "$INTERNAL_PORT" ]; then
        echo '{"success":false,"message":"参数不完整"}'
        exit 0
    fi
    
    # 生成新ID
    NEW_ID=$(date +%s)
    
    # 保存规则
    echo "$NEW_ID|$PROTOCOL|$EXTERNAL_PORT|$INTERNAL_IP|$INTERNAL_PORT" >> "$RULES_FILE"
    
    # 应用iptables规则
    if [ "$PROTOCOL" = "tcp" ] || [ "$PROTOCOL" = "both" ]; then
        iptables -t nat -A PREROUTING -i eth0 -p tcp --dport "$EXTERNAL_PORT" -j DNAT --to-destination "$INTERNAL_IP:$INTERNAL_PORT"
        iptables -A FORWARD -i eth0 -o wlan0 -p tcp --dport "$INTERNAL_PORT" -d "$INTERNAL_IP" -j ACCEPT
    fi
    
    if [ "$PROTOCOL" = "udp" ] || [ "$PROTOCOL" = "both" ]; then
        iptables -t nat -A PREROUTING -i eth0 -p udp --dport "$EXTERNAL_PORT" -j DNAT --to-destination "$INTERNAL_IP:$INTERNAL_PORT"
        iptables -A FORWARD -i eth0 -o wlan0 -p udp --dport "$INTERNAL_PORT" -d "$INTERNAL_IP" -j ACCEPT
    fi
    
    # 保存iptables规则
    iptables-save > /etc/iptables/rules.v4 2>/dev/null || true
    
    echo '{"success":true,"message":"端口转发规则添加成功"}'

elif [ "$ACTION" = "delete" ]; then
    # 删除规则
    if [ -z "$ID" ]; then
        echo '{"success":false,"message":"未指定规则ID"}'
        exit 0
    fi
    
    # 获取要删除的规则信息
    RULE=$(grep "^$ID|" "$RULES_FILE")
    if [ -z "$RULE" ]; then
        echo '{"success":false,"message":"规则不存在"}'
        exit 0
    fi
    
    PROTOCOL=$(echo "$RULE" | cut -d'|' -f2)
    EXTERNAL_PORT=$(echo "$RULE" | cut -d'|' -f3)
    INTERNAL_IP=$(echo "$RULE" | cut -d'|' -f4)
    INTERNAL_PORT=$(echo "$RULE" | cut -d'|' -f5)
    
    # 删除iptables规则
    if [ "$PROTOCOL" = "tcp" ] || [ "$PROTOCOL" = "both" ]; then
        iptables -t nat -D PREROUTING -i eth0 -p tcp --dport "$EXTERNAL_PORT" -j DNAT --to-destination "$INTERNAL_IP:$INTERNAL_PORT" 2>/dev/null || true
        iptables -D FORWARD -i eth0 -o wlan0 -p tcp --dport "$INTERNAL_PORT" -d "$INTERNAL_IP" -j ACCEPT 2>/dev/null || true
    fi
    
    if [ "$PROTOCOL" = "udp" ] || [ "$PROTOCOL" = "both" ]; then
        iptables -t nat -D PREROUTING -i eth0 -p udp --dport "$EXTERNAL_PORT" -j DNAT --to-destination "$INTERNAL_IP:$INTERNAL_PORT" 2>/dev/null || true
        iptables -D FORWARD -i eth0 -o wlan0 -p udp --dport "$INTERNAL_PORT" -d "$INTERNAL_IP" -j ACCEPT 2>/dev/null || true
    fi
    
    # 删除配置文件中的规则
    grep -v "^$ID|" "$RULES_FILE" > "${RULES_FILE}.tmp"
    mv "${RULES_FILE}.tmp" "$RULES_FILE"
    
    # 保存iptables规则
    iptables-save > /etc/iptables/rules.v4 2>/dev/null || true
    
    echo '{"success":true,"message":"端口转发规则删除成功"}'

else
    echo '{"success":false,"message":"无效的操作"}'
fi