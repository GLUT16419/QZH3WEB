#!/bin/bash
echo "Content-Type: text/plain"
echo ""

# 日志文件路径
SYSLOG_FILE="/var/log/syslog"
WIFILOG_FILE="/var/log/hostapd.log"
AUTHLOG_FILE="/var/log/auth.log"

# 获取查询参数
LOG_TYPE=$(echo "$QUERY_STRING" | grep -o 'type=[^&]*' | cut -d'=' -f2)
ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d'=' -f2)

if [ "$ACTION" = "clear" ]; then
    # 清空日志
    echo -n > "$SYSLOG_FILE" 2>/dev/null || true
    echo -n > "$WIFILOG_FILE" 2>/dev/null || true
    echo -n > "$AUTHLOG_FILE" 2>/dev/null || true
    echo '{"success":true,"message":"日志已清空"}'
    exit 0
fi

case "$LOG_TYPE" in
    "wifi")
        if [ -f "$WIFILOG_FILE" ]; then
            tail -100 "$WIFILOG_FILE"
        else
            echo "WiFi日志文件不存在"
        fi
        ;;
    "auth")
        if [ -f "$AUTHLOG_FILE" ]; then
            tail -100 "$AUTHLOG_FILE"
        else
            echo "认证日志文件不存在"
        fi
        ;;
    "system" | *)
        if [ -f "$SYSLOG_FILE" ]; then
            tail -100 "$SYSLOG_FILE"
        else
            echo "系统日志文件不存在"
        fi
        ;;
esac