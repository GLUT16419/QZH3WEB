#!/bin/bash
echo "Content-Type: text/html"
echo ""

clients=$(iw dev wlan0 station dump 2>/dev/null | grep -E "^Station" | awk '{print $2}')

echo "<table class='client-table'>"
echo "<tr><th>MAC地址</th><th>信号强度</th><th>发送流量</th><th>操作</th></tr>"

if [ -z "$clients" ]; then
    echo "<tr><td colspan='4'>暂无连接设备</td></tr>"
else
    for mac in $clients; do
        signal=$(iw dev wlan0 station get $mac 2>/dev/null | grep "signal:" | awk '{print $2}')
        tx_bytes=$(iw dev wlan0 station get $mac 2>/dev/null | grep "tx bytes:" | awk '{print $3}')
        
        echo "<tr>"
        echo "<td>$mac</td>"
        echo "<td>${signal:-N/A} dBm</td>"
        echo "<td>${tx_bytes:-N/A} bytes</td>"
        echo "<td><button class='kick-btn' onclick='kickClient(\"$mac\")'>踢出</button></td>"
        echo "</tr>"
    done
fi

echo "</table>"

cat << 'STYLE'
<style>
.client-table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 15px;
}
.client-table th, .client-table td {
    padding: 12px;
    text-align: left;
    border-bottom: 1px solid #ddd;
}
.client-table th {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
}
.client-table tr:hover {
    background: #f5f5f5;
}
.kick-btn {
    background: #f44336;
    color: white;
    border: none;
    padding: 6px 12px;
    border-radius: 5px;
    cursor: pointer;
    font-size: 0.85em;
    transition: background 0.3s;
}
.kick-btn:hover {
    background: #d32f2f;
}
</style>
STYLE