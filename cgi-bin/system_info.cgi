#!/bin/bash
echo "Content-Type: application/json"
echo ""

# 获取CPU负载
cpu_load=$(cat /proc/loadavg | awk '{print $1 " " $2 " " $3}')

# 获取内存信息
mem_total=$(grep MemTotal /proc/meminfo | awk '{print $2}')
mem_available=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
mem_used=$((mem_total - mem_available))
mem_percent=$((mem_used * 100 / mem_total))
memory="${mem_percent}% (${mem_used}KB / ${mem_total}KB)"

# 获取运行时间
uptime_sec=$(cat /proc/uptime | awk '{print int($1)}')
days=$((uptime_sec / 86400))
hours=$(((uptime_sec % 86400) / 3600))
mins=$(((uptime_sec % 3600) / 60))
uptime="${days}天 ${hours}小时 ${mins}分钟"

# 获取温度（如果有）
if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
    temp_raw=$(cat /sys/class/thermal/thermal_zone0/temp)
    temp=$((temp_raw / 1000))
    temperature="${temp}°C"
else
    temperature="N/A"
fi

# 输出JSON
cat << JSON
{
    "cpu_load": "${cpu_load}",
    "memory": "${memory}",
    "uptime": "${uptime}",
    "temp": "${temperature}"
}
JSON
