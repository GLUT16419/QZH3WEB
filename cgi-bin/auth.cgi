#!/bin/bash
echo "Content-Type: application/json"
echo ""

# Session存储目录
SESSION_DIR="/tmp/sessions"
mkdir -p "$SESSION_DIR"

# 默认用户凭证（实际部署时应存储在安全位置）
DEFAULT_USER="admin"
DEFAULT_PASS="admin"

# 读取POST数据
read -r DATA

# 解析参数
ACTION=$(echo "$DATA" | grep -o 'action=[^&]*' | cut -d'=' -f2)
USERNAME=$(echo "$DATA" | grep -o 'username=[^&]*' | cut -d'=' -f2 | tr '+' ' ')
PASSWORD=$(echo "$DATA" | grep -o 'password=[^&]*' | cut -d'=' -f2 | tr '+' ' ')

# URL解码函数
url_decode() {
    echo "$1" | sed 's/%20/ /g; s/%21/!/g; s/%22/"/g; s/%23/#/g; s/%24/$/g; s/%25/%/g; s/%26/&/g; s/%27/'\''/g; s/%28/(/g; s/%29/)/g; s/%2A/*/g; s/%2B/+/g; s/%2C/,/g; s/%2D/-/g; s/%2E/./g; s/%2F/\//g'
}

USERNAME=$(url_decode "$USERNAME")
PASSWORD=$(url_decode "$PASSWORD")

if [ "$ACTION" = "login" ]; then
    if [ "$USERNAME" = "$DEFAULT_USER" ] && [ "$PASSWORD" = "$DEFAULT_PASS" ]; then
        # 生成session ID
        SESSION_ID=$(cat /proc/sys/kernel/random/uuid)
        # 设置session有效期为1小时
        EXPIRY=$(date -d "+1 hour" +%s)
        # 存储session
        echo "$EXPIRY" > "$SESSION_DIR/$SESSION_ID"
        # 设置cookie
        echo "Set-Cookie: session_id=$SESSION_ID; HttpOnly; Path=/"
        echo ""
        echo '{"success": true, "message": "登录成功"}'
    else
        echo '{"success": false, "message": "用户名或密码错误"}'
    fi

elif [ "$ACTION" = "check" ]; then
    # 获取cookie中的session_id
    COOKIE_HEADER="$HTTP_COOKIE"
    SESSION_ID=$(echo "$COOKIE_HEADER" | grep -o 'session_id=[^;]*' | cut -d'=' -f2)
    
    if [ -z "$SESSION_ID" ]; then
        echo '{"authenticated": false}'
        exit 0
    fi
    
    SESSION_FILE="$SESSION_DIR/$SESSION_ID"
    if [ -f "$SESSION_FILE" ]; then
        EXPIRY=$(cat "$SESSION_FILE")
        NOW=$(date +%s)
        if [ "$NOW" -lt "$EXPIRY" ]; then
            # 更新session有效期
            EXPIRY=$(date -d "+1 hour" +%s)
            echo "$EXPIRY" > "$SESSION_FILE"
            echo '{"authenticated": true}'
        else
            # session过期，删除
            rm "$SESSION_FILE"
            echo '{"authenticated": false}'
        fi
    else
        echo '{"authenticated": false}'
    fi

elif [ "$ACTION" = "logout" ]; then
    COOKIE_HEADER="$HTTP_COOKIE"
    SESSION_ID=$(echo "$COOKIE_HEADER" | grep -o 'session_id=[^;]*' | cut -d'=' -f2)
    
    if [ -n "$SESSION_ID" ]; then
        rm -f "$SESSION_DIR/$SESSION_ID"
    fi
    echo "Set-Cookie: session_id=; expires=Thu, 01 Jan 1970 00:00:00 UTC; Path=/"
    echo ""
    echo '{"success": true, "message": "注销成功"}'

else
    echo '{"success": false, "message": "无效的操作"}'
fi