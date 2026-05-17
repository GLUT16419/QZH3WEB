// 流量统计和限速功能
function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + ' KB';
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
}

function loadTrafficStats() {
    fetch('/cgi-bin/traffic.cgi?action=stats')
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('traffic-body');
            var select = document.getElementById('limit-ip');

            tbody.innerHTML = '';
            select.innerHTML = '<option value="">请选择设备</option>';

            if (data.devices && data.devices.length > 0) {
                data.devices.forEach(device => {
                    var row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${device.ip}</td>
                        <td>${device.mac}</td>
                        <td>${device.name}</td>
                        <td>${formatBytes(device.rx_bytes)}</td>
                        <td>${formatBytes(device.tx_bytes)}</td>
                        <td>${device.speed_limit > 0 ? device.speed_limit + ' Mbps' : '不限速'}</td>
                        <td><button onclick="setDeviceLimit('${device.ip}')" class="btn btn-primary">设置限速</button></td>
                    `;
                    tbody.appendChild(row);

                    var option = document.createElement('option');
                    option.value = device.ip;
                    option.textContent = device.ip + ' (' + device.name + ')';
                    select.appendChild(option);
                });
            } else {
                tbody.innerHTML = '<tr><td colspan="7" class="text-center">暂无设备数据</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('traffic-body').innerHTML = '<tr><td colspan="7" class="text-center">加载失败</td></tr>';
        });
}

function setDeviceLimit(ip) {
    document.getElementById('limit-ip').value = ip;
    document.getElementById('limit-speed').focus();
}

function applySpeedLimit() {
    var ip = document.getElementById('limit-ip').value;
    var limit = document.getElementById('limit-speed').value;

    if (!ip) {
        showTrafficMessage('请选择设备', false);
        return;
    }

    fetch('/cgi-bin/traffic.cgi?action=limit&ip=' + encodeURIComponent(ip) + '&limit=' + limit)
        .then(response => response.json())
        .then(data => {
            showTrafficMessage(data.message, data.success);
            loadTrafficStats();
        })
        .catch(err => {
            showTrafficMessage('设置失败: ' + err.message, false);
        });
}

function showTrafficMessage(message, success) {
    var msgDiv = document.getElementById('traffic-message');
    msgDiv.textContent = message;
    msgDiv.className = success ? 'success' : 'error';
    msgDiv.style.display = 'block';

    setTimeout(() => {
        msgDiv.style.display = 'none';
    }, 3000);
}

// 实时流量图表功能
var wanChartCtx, wlanChartCtx;
var wanHistory = [];
var wlanHistory = [];
var chartInterval;

function initCharts() {
    var wanCanvas = document.getElementById('wanChart');
    var wlanCanvas = document.getElementById('wlanChart');
    
    if (wanCanvas) {
        wanChartCtx = wanCanvas.getContext('2d');
        resizeCanvas(wanCanvas);
    }
    if (wlanCanvas) {
        wlanChartCtx = wlanCanvas.getContext('2d');
        resizeCanvas(wlanCanvas);
    }
}

function resizeCanvas(canvas) {
    var container = canvas.parentElement;
    canvas.width = Math.min(container.clientWidth - 20, 600);
}

function drawChart(ctx, history, title) {
    if (!ctx || !history || history.length < 2) return;

    var width = ctx.canvas.width;
    var height = ctx.canvas.height;
    var padding = 40;
    
    ctx.clearRect(0, 0, width, height);
    
    var maxValue = 0;
    history.forEach(h => {
        maxValue = Math.max(maxValue, h.rx, h.tx);
    });
    
    if (maxValue === 0) maxValue = 1;
    
    var gradient = ctx.createLinearGradient(0, 0, 0, height - padding);
    gradient.addColorStop(0, 'rgba(52, 152, 219, 0.3)');
    gradient.addColorStop(1, 'rgba(52, 152, 219, 0)');
    
    ctx.fillStyle = '#2c3e50';
    ctx.fillRect(padding, padding, width - padding * 2, height - padding * 2);
    
    ctx.strokeStyle = '#34495e';
    ctx.lineWidth = 1;
    for (var i = 0; i <= 4; i++) {
        var y = padding + (height - padding * 2) * i / 4;
        ctx.beginPath();
        ctx.moveTo(padding, y);
        ctx.lineTo(width - padding, y);
        ctx.stroke();
        
        var label = formatBytes(Math.round(maxValue * (4 - i) / 4)) + '/s';
        ctx.fillStyle = '#95a5a6';
        ctx.font = '10px Arial';
        ctx.textAlign = 'right';
        ctx.fillText(label, padding - 5, y + 3);
    }
    
    var stepX = (width - padding * 2) / (history.length - 1);
    
    ctx.beginPath();
    ctx.strokeStyle = '#3498db';
    ctx.lineWidth = 2;
    ctx.moveTo(padding, height - padding);
    
    history.forEach((h, i) => {
        var x = padding + i * stepX;
        var y = height - padding - (h.rx / maxValue) * (height - padding * 2);
        ctx.lineTo(x, y);
    });
    ctx.stroke();
    
    ctx.beginPath();
    ctx.strokeStyle = '#e74c3c';
    ctx.lineWidth = 2;
    ctx.moveTo(padding, height - padding);
    
    history.forEach((h, i) => {
        var x = padding + i * stepX;
        var y = height - padding - (h.tx / maxValue) * (height - padding * 2);
        ctx.lineTo(x, y);
    });
    ctx.stroke();
    
    ctx.fillStyle = '#95a5a6';
    ctx.font = '10px Arial';
    ctx.textAlign = 'center';
    for (var i = 0; i < history.length; i += Math.max(1, Math.floor(history.length / 6))) {
        var x = padding + i * stepX;
        var time = new Date(history[i].time * 1000);
        var label = time.getHours().toString().padStart(2, '0') + ':' + 
                    time.getMinutes().toString().padStart(2, '0');
        ctx.fillText(label, x, height - 5);
    }
    
    ctx.fillStyle = '#3498db';
    ctx.fillRect(width - 80, padding, 10, 10);
    ctx.fillStyle = '#95a5a6';
    ctx.font = '10px Arial';
    ctx.textAlign = 'left';
    ctx.fillText('接收', width - 65, padding + 9);
    
    ctx.fillStyle = '#e74c3c';
    ctx.fillRect(width - 80, padding + 15, 10, 10);
    ctx.fillStyle = '#95a5a6';
    ctx.fillText('发送', width - 65, padding + 24);
}

function updateTrafficHistory() {
    fetch('/cgi-bin/traffic.cgi?action=wan_history')
        .then(response => response.json())
        .then(data => {
            if (data.history) {
                wanHistory = data.history;
                drawChart(wanChartCtx, wanHistory, 'WAN');
            }
        });
    
    fetch('/cgi-bin/traffic.cgi?action=wlan_history')
        .then(response => response.json())
        .then(data => {
            if (data.history) {
                wlanHistory = data.history;
                drawChart(wlanChartCtx, wlanHistory, 'WiFi');
            }
        });
    
    fetch('/cgi-bin/traffic.cgi?action=overall')
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                document.getElementById('wan-rx').textContent = formatBytes(data.wan_rx) + '/s';
                document.getElementById('wan-tx').textContent = formatBytes(data.wan_tx) + '/s';
                document.getElementById('wlan-rx').textContent = formatBytes(data.wlan_rx) + '/s';
                document.getElementById('wlan-tx').textContent = formatBytes(data.wlan_tx) + '/s';
            }
        });
}

function startTrafficMonitoring() {
    initCharts();
    updateTrafficHistory();
    chartInterval = setInterval(updateTrafficHistory, 2000);
}

function stopTrafficMonitoring() {
    if (chartInterval) {
        clearInterval(chartInterval);
        chartInterval = null;
    }
}

// 更新loadTrafficStats函数
function loadTrafficStats() {
    fetch('/cgi-bin/traffic.cgi?action=stats')
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('traffic-body');
            var select = document.getElementById('limit-ip');

            tbody.innerHTML = '';
            select.innerHTML = '<option value="">请选择设备</option>';

            if (data.devices && data.devices.length > 0) {
                var sortedDevices = data.devices.sort((a, b) => 
                    (b.rx_bytes + b.tx_bytes) - (a.rx_bytes + a.tx_bytes)
                );
                
                sortedDevices.forEach((device, index) => {
                    var totalBytes = device.rx_bytes + device.tx_bytes;
                    var currentRate = device.rx_rate || 0;
                    
                    var row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${index + 1}</td>
                        <td>${device.ip}</td>
                        <td>${device.mac}</td>
                        <td>${device.name}</td>
                        <td>${formatBytes(device.rx_bytes)}</td>
                        <td>${formatBytes(device.tx_bytes)}</td>
                        <td>${formatBytes(totalBytes)}</td>
                        <td>${formatBytes(currentRate)}/s</td>
                        <td><button onclick="setDeviceLimit('${device.ip}')" class="btn btn-primary">设置限速</button></td>
                    `;
                    tbody.appendChild(row);

                    var option = document.createElement('option');
                    option.value = device.ip;
                    option.textContent = device.ip + ' (' + device.name + ')';
                    select.appendChild(option);
                });
            } else {
                tbody.innerHTML = '<tr><td colspan="9" class="text-center">暂无设备数据</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('traffic-body').innerHTML = '<tr><td colspan="9" class="text-center">加载失败</td></tr>';
        });
}

// 防火墙规则管理功能
function loadFirewallRules() {
    fetch('/cgi-bin/firewall.cgi?action=list')
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('firewall-body');
            tbody.innerHTML = '';

            if (data.rules && data.rules.length > 0) {
                data.rules.forEach(rule => {
                    var ports = '';
                    if (rule.src_port > 0 && rule.dst_port > 0) {
                        ports = rule.src_port + ' -> ' + rule.dst_port;
                    } else if (rule.dst_port > 0) {
                        ports = ':' + rule.dst_port;
                    } else if (rule.src_port > 0) {
                        ports = rule.src_port + ':*';
                    } else {
                        ports = '-';
                    }

                    var statusClass = rule.enabled ? 'status-enabled' : 'status-disabled';
                    var statusText = rule.enabled ? '启用' : '禁用';

                    var row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${rule.id}</td>
                        <td>${rule.name}</td>
                        <td><span class="action-${rule.action.toLowerCase()}">${rule.action}</span></td>
                        <td>${rule.direction}</td>
                        <td>${rule.protocol}</td>
                        <td>${rule.src_ip}</td>
                        <td>${rule.dst_ip}</td>
                        <td>${ports}</td>
                        <td><span class="${statusClass}">${statusText}</span></td>
                        <td>
                            <button onclick="toggleFirewallRule(${rule.id})" class="btn btn-warning btn-sm">切换</button>
                            <button onclick="deleteFirewallRule(${rule.id})" class="btn btn-danger btn-sm">删除</button>
                        </td>
                    `;
                    tbody.appendChild(row);
                });
            } else {
                tbody.innerHTML = '<tr><td colspan="10" class="text-center">暂无防火墙规则</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('firewall-body').innerHTML = '<tr><td colspan="10" class="text-center">加载失败</td></tr>';
        });
}

function addFirewallRule() {
    var name = document.getElementById('fw-name').value;
    var action = document.getElementById('fw-action').value;
    var direction = document.getElementById('fw-direction').value;
    var protocol = document.getElementById('fw-protocol').value;
    var srcIp = document.getElementById('fw-src-ip').value || '0.0.0.0/0';
    var dstIp = document.getElementById('fw-dst-ip').value || '0.0.0.0/0';
    var srcPort = document.getElementById('fw-src-port').value || '-1';
    var dstPort = document.getElementById('fw-dst-port').value || '-1';

    if (!name) {
        showFirewallMessage('请输入规则名称', false);
        return;
    }

    var params = 'action=add' +
        '&name=' + encodeURIComponent(name) +
        '&action=' + encodeURIComponent(action) +
        '&direction=' + encodeURIComponent(direction) +
        '&protocol=' + encodeURIComponent(protocol) +
        '&src_ip=' + encodeURIComponent(srcIp) +
        '&dst_ip=' + encodeURIComponent(dstIp) +
        '&src_port=' + srcPort +
        '&dst_port=' + dstPort;

    fetch('/cgi-bin/firewall.cgi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params
    })
    .then(response => response.json())
    .then(data => {
        showFirewallMessage(data.message, data.success);
        if (data.success) {
            clearFirewallForm();
            loadFirewallRules();
        }
    })
    .catch(err => {
        showFirewallMessage('添加失败: ' + err.message, false);
    });
}

function addTemplate(template) {
    if (!confirm('确定要添加此预设模板吗？')) return;

    fetch('/cgi-bin/firewall.cgi?action=add_template&template=' + template)
        .then(response => response.json())
        .then(data => {
            showFirewallMessage(data.message, data.success);
            if (data.success) {
                loadFirewallRules();
            }
        })
        .catch(err => {
            showFirewallMessage('添加模板失败: ' + err.message, false);
        });
}

function toggleFirewallRule(id) {
    fetch('/cgi-bin/firewall.cgi?action=toggle&id=' + id)
        .then(response => response.json())
        .then(data => {
            showFirewallMessage(data.message, data.success);
            if (data.success) {
                loadFirewallRules();
            }
        })
        .catch(err => {
            showFirewallMessage('切换状态失败: ' + err.message, false);
        });
}

function deleteFirewallRule(id) {
    if (!confirm('确定要删除这条防火墙规则吗？')) return;

    fetch('/cgi-bin/firewall.cgi?action=delete&id=' + id)
        .then(response => response.json())
        .then(data => {
            showFirewallMessage(data.message, data.success);
            if (data.success) {
                loadFirewallRules();
            }
        })
        .catch(err => {
            showFirewallMessage('删除失败: ' + err.message, false);
        });
}

function applyFirewall() {
    fetch('/cgi-bin/firewall.cgi?action=apply')
        .then(response => response.json())
        .then(data => {
            showFirewallMessage(data.message, data.success);
        })
        .catch(err => {
            showFirewallMessage('应用规则失败: ' + err.message, false);
        });
}

function resetFirewall() {
    if (!confirm('确定要重置防火墙为默认策略吗？这将清除所有自定义规则。')) return;

    fetch('/cgi-bin/firewall.cgi?action=reset')
        .then(response => response.json())
        .then(data => {
            showFirewallMessage(data.message, data.success);
            if (data.success) {
                loadFirewallRules();
            }
        })
        .catch(err => {
            showFirewallMessage('重置失败: ' + err.message, false);
        });
}

function clearFirewallForm() {
    document.getElementById('fw-name').value = '';
    document.getElementById('fw-src-ip').value = '';
    document.getElementById('fw-dst-ip').value = '';
    document.getElementById('fw-src-port').value = '';
    document.getElementById('fw-dst-port').value = '';
}

function showFirewallMessage(message, success) {
    var msgDiv = document.getElementById('firewall-message');
    msgDiv.textContent = message;
    msgDiv.className = success ? 'success' : 'error';
    msgDiv.style.display = 'block';

    setTimeout(() => {
        msgDiv.style.display = 'none';
    }, 3000);
}

// 更新showSection函数以加载防火墙规则
var originalShowSection3 = showSection;
showSection = function(sectionId) {
    originalShowSection3(sectionId);

    if (sectionId === 'system') {
        loadSystemInfo();
    }
    
    if (sectionId === 'firewall') {
        loadFirewallRules();
    }
    
    if (sectionId === 'traffic') {
        startTrafficMonitoring();
    } else {
        stopTrafficMonitoring();
    }
    
    if (sectionId === 'ids') {
        loadIdsStats();
        loadIdsAlerts();
    }
};

// 加载系统状态信息
function loadSystemInfo() {
    fetch('/cgi-bin/system_info.cgi')
        .then(response => response.json())
        .then(data => {
            if (data.cpu_load) {
                document.getElementById('cpu-load').textContent = data.cpu_load;
            }
            if (data.memory) {
                document.getElementById('memory-usage').textContent = data.memory;
            }
            if (data.uptime) {
                document.getElementById('uptime').textContent = data.uptime;
            }
            if (data.temp) {
                document.getElementById('cpu-temp').textContent = data.temp;
            }
            if (data.hostname) {
                document.getElementById('hostname').textContent = data.hostname;
            }
            if (data.os_info) {
                document.getElementById('os-info').textContent = data.os_info;
            }
            if (data.kernel) {
                document.getElementById('kernel').textContent = data.kernel;
            }
        })
        .catch(err => {
            console.error('加载系统信息失败:', err);
        });
}

// IDS入侵检测功能
function loadIdsStats() {
    fetch('/cgi-bin/ids.cgi?action=stats')
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                document.getElementById('ids-critical').textContent = data.critical || 0;
                document.getElementById('ids-high').textContent = data.high || 0;
                document.getElementById('ids-medium').textContent = data.medium || 0;
                document.getElementById('ids-low').textContent = data.low || 0;
            }
        })
        .catch(err => {
            console.error('加载IDS统计失败:', err);
        });
}

function loadIdsAlerts() {
    fetch('/cgi-bin/ids.cgi?action=list')
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('ids-alerts-body');
            tbody.innerHTML = '';

            if (data.alerts && data.alerts.length > 0) {
                data.alerts.forEach(alert => {
                    var time = new Date(alert.time * 1000);
                    var timeStr = time.toLocaleString();
                    
                    var severityClass = 'severity-' + alert.severity;
                    var blockedText = alert.blocked ? '已阻止' : '监控中';
                    var blockedClass = alert.blocked ? 'status-blocked' : 'status-monitoring';
                    
                    var row = document.createElement('tr');
                    row.innerHTML = `
                        <td>${timeStr}</td>
                        <td>${alert.type}</td>
                        <td><span class="${severityClass}">${alert.severity.toUpperCase()}</span></td>
                        <td>${alert.src_ip}</td>
                        <td>${alert.dst_ip}:${alert.dst_port || '-'}</td>
                        <td>${alert.protocol}</td>
                        <td>${alert.desc}</td>
                        <td><span class="${blockedClass}">${blockedText}</span></td>
                        <td>
                            ${alert.blocked ? 
                                '<button onclick="unblockIpFromAlert(\'' + alert.src_ip + '\')" class="btn btn-success btn-sm">解除</button>' : 
                                '<button onclick="blockIpFromAlert(\'' + alert.src_ip + '\')" class="btn btn-danger btn-sm">阻止</button>'
                            }
                        </td>
                    `;
                    tbody.appendChild(row);
                });
            } else {
                tbody.innerHTML = '<tr><td colspan="9" class="text-center">暂无告警记录</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('ids-alerts-body').innerHTML = '<tr><td colspan="9" class="text-center">加载失败</td></tr>';
        });
}

function runIdsDetection() {
    fetch('/cgi-bin/ids.cgi?action=detect')
        .then(response => response.json())
        .then(data => {
            showIdsMessage(data.message, data.success);
            loadIdsStats();
            loadIdsAlerts();
        })
        .catch(err => {
            showIdsMessage('检测失败: ' + err.message, false);
        });
}

function blockIp() {
    var ip = document.getElementById('block-ip').value;
    if (!ip || !validateIp(ip)) {
        showIdsMessage('请输入有效的IP地址', false);
        return;
    }
    
    fetch('/cgi-bin/ids.cgi?action=block&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showIdsMessage(data.message, data.success);
            loadIdsStats();
            loadIdsAlerts();
            document.getElementById('block-ip').value = '';
        })
        .catch(err => {
            showIdsMessage('操作失败: ' + err.message, false);
        });
}

function unblockIp() {
    var ip = document.getElementById('unblock-ip').value;
    if (!ip || !validateIp(ip)) {
        showIdsMessage('请输入有效的IP地址', false);
        return;
    }
    
    fetch('/cgi-bin/ids.cgi?action=unblock&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showIdsMessage(data.message, data.success);
            loadIdsStats();
            loadIdsAlerts();
            document.getElementById('unblock-ip').value = '';
        })
        .catch(err => {
            showIdsMessage('操作失败: ' + err.message, false);
        });
}

function blockIpFromAlert(ip) {
    fetch('/cgi-bin/ids.cgi?action=block&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showIdsMessage(data.message, data.success);
            loadIdsStats();
            loadIdsAlerts();
        })
        .catch(err => {
            showIdsMessage('操作失败: ' + err.message, false);
        });
}

function unblockIpFromAlert(ip) {
    fetch('/cgi-bin/ids.cgi?action=unblock&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showIdsMessage(data.message, data.success);
            loadIdsStats();
            loadIdsAlerts();
        })
        .catch(err => {
            showIdsMessage('操作失败: ' + err.message, false);
        });
}

function validateIp(ip) {
    var parts = ip.split('.');
    if (parts.length !== 4) return false;
    return parts.every(part => {
        var num = parseInt(part);
        return !isNaN(num) && num >= 0 && num <= 255;
    });
}

function showIdsMessage(message, success) {
    var msgDiv = document.getElementById('ids-message');
    msgDiv.textContent = message;
    msgDiv.className = success ? 'success' : 'error';
    msgDiv.style.display = 'block';

    setTimeout(() => {
        msgDiv.style.display = 'none';
    }, 3000);
}

// VPN客户端管理功能
var currentClientConfig = null;

function loadVPNStatus() {
    fetch('/cgi-bin/vpn.cgi?action=status')
        .then(response => response.json())
        .then(data => {
            var statusEl = document.getElementById('vpn-status');
            if (data.status === 'running') {
                statusEl.textContent = '已运行';
                statusEl.className = 'status-running';
                document.getElementById('vpn-public-key').textContent = data.public_key || '--';
                document.getElementById('vpn-port').textContent = data.port || '51820';
                document.getElementById('vpn-endpoint').textContent = (data.endpoint || '--') + ':' + (data.port || '51820');
            } else {
                statusEl.textContent = '已停止';
                statusEl.className = 'status-stopped';
            }
        })
        .catch(err => {
            document.getElementById('vpn-status').textContent = '错误';
            document.getElementById('vpn-status').className = 'status-stopped';
        });
}

function loadVPNServerConfig() {
    fetch('/cgi-bin/vpn.cgi?action=server_config')
        .then(response => response.json())
        .then(data => {
            if (data.success && data.server) {
                document.getElementById('vpn-public-key').textContent = data.server.public_key || '--';
                document.getElementById('vpn-port').textContent = data.server.port || '51820';
                document.getElementById('vpn-endpoint').textContent = data.server.endpoint || '--';
            }
        })
        .catch(err => {
            console.error('加载服务器配置失败:', err);
        });
}

function startVPN() {
    fetch('/cgi-bin/vpn.cgi?action=start')
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            loadVPNStatus();
        })
        .catch(err => {
            alert('启动VPN失败: ' + err.message);
        });
}

function stopVPN() {
    fetch('/cgi-bin/vpn.cgi?action=stop')
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            loadVPNStatus();
        })
        .catch(err => {
            alert('停止VPN失败: ' + err.message);
        });
}

function generateVPNKey() {
    fetch('/cgi-bin/vpn.cgi?action=generate_key')
        .then(response => response.json())
        .then(data => {
            document.getElementById('client-private-key').value = data.private_key || '';
            document.getElementById('client-public-key').value = data.public_key || '';
            document.getElementById('vpn-key-section').style.display = 'block';
        })
        .catch(err => {
            alert('生成密钥失败: ' + err.message);
        });
}

function addVPNClient() {
    var name = document.getElementById('client-name').value;
    var privateKey = document.getElementById('client-private-key').value;
    var publicKey = document.getElementById('client-public-key').value;

    if (!name) {
        alert('请输入客户端名称');
        return;
    }

    if (!privateKey || !publicKey) {
        alert('请先生成密钥');
        return;
    }

    fetch('/cgi-bin/vpn.cgi?action=add_client&name=' + encodeURIComponent(name) + 
          '&public_key=' + encodeURIComponent(publicKey) + 
          '&private_key=' + encodeURIComponent(privateKey))
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            if (data.success) {
                document.getElementById('client-name').value = '';
                document.getElementById('client-private-key').value = '';
                document.getElementById('client-public-key').value = '';
                document.getElementById('vpn-key-section').style.display = 'none';
                loadVPNClients();
            }
        })
        .catch(err => {
            alert('添加客户端失败: ' + err.message);
        });
}

function loadVPNClients() {
    fetch('/cgi-bin/vpn.cgi?action=clients')
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('vpn-clients-body');
            tbody.innerHTML = '';

            if (data.clients && data.clients.length > 0) {
                document.getElementById('client-count').textContent = '(' + data.clients.length + '个客户端)';
                
                data.clients.forEach(client => {
                    var statusClass = client.enabled ? 'status-running' : 'status-stopped';
                    var statusText = client.enabled ? '启用' : '禁用';
                    var created = new Date(client.created * 1000).toLocaleString();
                    var shortKey = client.public_key.substring(0, 12) + '...';
                    
                    var row = document.createElement('tr');
                    row.innerHTML = '<td><span class="' + statusClass + '">' + statusText + '</span></td>' +
                        '<td>' + client.name + '</td>' +
                        '<td>' + client.assigned_ip + '</td>' +
                        '<td title="' + client.public_key + '">' + shortKey + '</td>' +
                        '<td>' + created + '</td>' +
                        '<td>' +
                        '<button onclick="toggleVPNClient(\'' + client.public_key + '\')" class="btn btn-warning btn-sm">切换</button> ' +
                        '<button onclick="getVPNClientConfig(\'' + client.public_key + '\')" class="btn btn-primary btn-sm">配置</button> ' +
                        '<button onclick="removeVPNClient(\'' + client.public_key + '\')" class="btn btn-danger btn-sm">删除</button>' +
                        '</td>';
                    tbody.appendChild(row);
                });
            } else {
                document.getElementById('client-count').textContent = '(0个客户端)';
                tbody.innerHTML = '<tr><td colspan="6" class="text-center">暂无客户端，请添加</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('vpn-clients-body').innerHTML = '<tr><td colspan="6" class="text-center">加载失败</td></tr>';
        });
}

function toggleVPNClient(publicKey) {
    fetch('/cgi-bin/vpn.cgi?action=toggle_client&public_key=' + encodeURIComponent(publicKey))
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            loadVPNClients();
        })
        .catch(err => {
            alert('操作失败: ' + err.message);
        });
}

function getVPNClientConfig(publicKey) {
    fetch('/cgi-bin/vpn.cgi?action=get_config&public_key=' + encodeURIComponent(publicKey))
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                currentClientConfig = data.config;
                document.getElementById('client-config-display').textContent = data.config;
                document.getElementById('client-config-section').style.display = 'block';
                document.getElementById('qrcode-container').style.display = 'none';
            } else {
                alert(data.message || '获取配置失败');
            }
        })
        .catch(err => {
            alert('获取配置失败: ' + err.message);
        });
}

function downloadClientConfig() {
    if (!currentClientConfig) {
        alert('请先选择客户端获取配置');
        return;
    }

    var blob = new Blob([currentClientConfig], { type: 'text/plain' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = 'wireguard-client.conf';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

function copyClientConfig() {
    if (!currentClientConfig) {
        alert('请先选择客户端获取配置');
        return;
    }

    navigator.clipboard.writeText(currentClientConfig).then(() => {
        alert('配置已复制到剪贴板');
    }).catch(err => {
        alert('复制失败: ' + err.message);
    });
}

function showQRCode() {
    if (!currentClientConfig) {
        alert('请先选择客户端获取配置');
        return;
    }

    var canvas = document.getElementById('qrcode-canvas');
    var container = document.getElementById('qrcode-container');
    
    container.style.display = 'block';
    
    if (canvas.qrcode) {
        canvas.qrcode.clear();
        canvas.qrcode.makeCode(currentClientConfig);
    } else {
        var script = document.createElement('script');
        script.src = 'https://cdn.jsdelivr.net/npm/qrcodejs@1.0.0/qrcode.min.js';
        script.onload = function() {
            canvas.qrcode = new QRCode(canvas, {
                text: currentClientConfig,
                width: 200,
                height: 200
            });
        };
        document.head.appendChild(script);
    }
}

function removeVPNClient(publicKey) {
    if (!confirm('确定要删除这个VPN客户端吗？')) return;

    fetch('/cgi-bin/vpn.cgi?action=remove_client&public_key=' + encodeURIComponent(publicKey))
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            loadVPNClients();
        })
        .catch(err => {
            alert('删除客户端失败: ' + err.message);
        });
}

function copyToClipboard(elementId) {
    var element = document.getElementById(elementId);
    if (!element) return;
    
    navigator.clipboard.writeText(element.value).then(() => {
        alert('已复制到剪贴板');
    }).catch(err => {
        alert('复制失败: ' + err.message);
    });
}

// 流量配额管理功能
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    var k = 1024;
    var sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    var i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function gbToBytes(gb) {
    return gb * 1024 * 1024 * 1024;
}

function loadQuotaRules() {
    fetch('/cgi-bin/quota.cgi?action=list')
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('quota-rules-body');
            tbody.innerHTML = '';

            if (data.success && data.quotas && data.quotas.length > 0) {
                var totalActive = 0;
                var totalExceeded = 0;
                var totalBlocked = 0;

                data.quotas.forEach(quota => {
                    if (quota.enabled) totalActive++;
                    if (quota.daily_percent >= 100 || quota.monthly_percent >= 100) totalExceeded++;

                    var statusClass = quota.enabled ? 'status-running' : 'status-stopped';
                    var statusText = quota.enabled ? '启用' : '禁用';
                    
                    var dailyPercent = quota.daily_percent || 0;
                    var monthlyPercent = quota.monthly_percent || 0;
                    
                    var dailyClass = dailyPercent >= 100 ? 'progress-danger' : (dailyPercent >= 80 ? 'progress-warning' : 'progress-success');
                    var monthlyClass = monthlyPercent >= 100 ? 'progress-danger' : (monthlyPercent >= 80 ? 'progress-warning' : 'progress-success');
                    
                    var actionText = quota.action === 1 ? '断网' : '限速';
                    
                    var row = document.createElement('tr');
                    row.innerHTML = '<td><span class="' + statusClass + '">' + statusText + '</span></td>' +
                        '<td>' + quota.name + '</td>' +
                        '<td>' + quota.ip + '</td>' +
                        '<td>' + (quota.daily_quota > 0 ? formatBytes(quota.daily_quota) : '不限制') + '</td>' +
                        '<td><div class="progress-container"><div class="progress-bar ' + dailyClass + '" style="width: ' + Math.min(dailyPercent, 100) + '%"></div><span class="progress-text">' + formatBytes(quota.daily_used) + ' (' + dailyPercent.toFixed(1) + '%)</span></div></td>' +
                        '<td>' + (quota.monthly_quota > 0 ? formatBytes(quota.monthly_quota) : '不限制') + '</td>' +
                        '<td><div class="progress-container"><div class="progress-bar ' + monthlyClass + '" style="width: ' + Math.min(monthlyPercent, 100) + '%"></div><span class="progress-text">' + formatBytes(quota.monthly_used) + ' (' + monthlyPercent.toFixed(1) + '%)</span></div></td>' +
                        '<td>' + actionText + '</td>' +
                        '<td>' +
                        '<button onclick="toggleQuotaRule(\'' + quota.ip + '\')" class="btn btn-warning btn-sm">切换</button> ' +
                        '<button onclick="resetQuotaStats(\'' + quota.ip + '\')" class="btn btn-info btn-sm">重置</button> ' +
                        '<button onclick="deleteQuotaRule(\'' + quota.ip + '\')" class="btn btn-danger btn-sm">删除</button>' +
                        '</td>';
                    tbody.appendChild(row);
                });

                document.getElementById('quota-total').textContent = data.quotas.length;
                document.getElementById('quota-active').textContent = totalActive;
                document.getElementById('quota-exceeded').textContent = totalExceeded;
                document.getElementById('quota-blocked').textContent = totalBlocked;
            } else {
                document.getElementById('quota-total').textContent = '0';
                document.getElementById('quota-active').textContent = '0';
                document.getElementById('quota-exceeded').textContent = '0';
                document.getElementById('quota-blocked').textContent = '0';
                tbody.innerHTML = '<tr><td colspan="9" class="text-center">暂无配额规则，请添加</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('quota-rules-body').innerHTML = '<tr><td colspan="9" class="text-center">加载失败: ' + err.message + '</td></tr>';
        });
}

function loadConnectedDevicesForQuota() {
    fetch('/cgi-bin/quota.cgi?action=devices')
        .then(response => response.json())
        .then(data => {
            var select = document.getElementById('quota-device-select');
            select.innerHTML = '<option value="">手动输入IP</option>';
            
            if (data.success && data.devices) {
                data.devices.forEach(device => {
                    var option = document.createElement('option');
                    option.value = device.ip;
                    option.textContent = device.ip + ' (' + device.mac + ')';
                    select.appendChild(option);
                });
            }
        })
        .catch(err => {
            console.error('加载设备列表失败:', err);
        });
}

function addQuotaRule() {
    var deviceSelect = document.getElementById('quota-device-select').value;
    var ipInput = document.getElementById('quota-ip').value;
    var name = document.getElementById('quota-name').value;
    var daily = document.getElementById('quota-daily').value;
    var monthly = document.getElementById('quota-monthly').value;
    var action = document.getElementById('quota-action').value;

    var ip = deviceSelect || ipInput;

    if (!ip) {
        showQuotaMessage('请输入或选择IP地址', false);
        return;
    }

    if (!name) {
        showQuotaMessage('请输入设备名称', false);
        return;
    }

    if (daily == 0 && monthly == 0) {
        showQuotaMessage('请至少设置一个配额限制', false);
        return;
    }

    var dailyBytes = gbToBytes(parseFloat(daily));
    var monthlyBytes = gbToBytes(parseFloat(monthly));

    fetch('/cgi-bin/quota.cgi?action=add&ip=' + encodeURIComponent(ip) + 
          '&name=' + encodeURIComponent(name) + 
          '&daily_quota=' + dailyBytes + 
          '&monthly_quota=' + monthlyBytes + 
          '&limit_action=' + action)
        .then(response => response.json())
        .then(data => {
            showQuotaMessage(data.message, data.success);
            if (data.success) {
                document.getElementById('quota-ip').value = '';
                document.getElementById('quota-name').value = '';
                document.getElementById('quota-daily').value = '0';
                document.getElementById('quota-monthly').value = '0';
                loadQuotaRules();
            }
        })
        .catch(err => {
            showQuotaMessage('添加失败: ' + err.message, false);
        });
}

function toggleQuotaRule(ip) {
    fetch('/cgi-bin/quota.cgi?action=toggle&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showQuotaMessage(data.message, data.success);
            loadQuotaRules();
        })
        .catch(err => {
            showQuotaMessage('操作失败: ' + err.message, false);
        });
}

function deleteQuotaRule(ip) {
    if (!confirm('确定要删除该配额规则吗？')) return;

    fetch('/cgi-bin/quota.cgi?action=delete&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showQuotaMessage(data.message, data.success);
            loadQuotaRules();
        })
        .catch(err => {
            showQuotaMessage('删除失败: ' + err.message, false);
        });
}

function resetQuotaStats(ip) {
    if (!confirm('确定要重置该设备的流量统计吗？')) return;

    fetch('/cgi-bin/quota.cgi?action=reset&ip=' + encodeURIComponent(ip))
        .then(response => response.json())
        .then(data => {
            showQuotaMessage(data.message, data.success);
            loadQuotaRules();
        })
        .catch(err => {
            showQuotaMessage('重置失败: ' + err.message, false);
        });
}

function checkAllQuotas() {
    fetch('/cgi-bin/quota.cgi?action=check')
        .then(response => response.json())
        .then(data => {
            showQuotaMessage(data.message, data.success);
            loadQuotaRules();
        })
        .catch(err => {
            showQuotaMessage('检查失败: ' + err.message, false);
        });
}

function showQuotaMessage(message, success) {
    var msgDiv = document.getElementById('quota-message');
    msgDiv.textContent = message;
    msgDiv.className = success ? 'message-box success' : 'message-box error';
    msgDiv.style.display = 'block';

    setTimeout(() => {
        msgDiv.style.display = 'none';
    }, 3000);
}

// ==================== 操作日志功能 ====================

var currentUsername = 'admin';

// 记录操作日志
function logOperation(module, action, details) {
    if (!details) details = '';
    fetch('/cgi-bin/logger.cgi?action=add&module=' + encodeURIComponent(module) + 
          '&act=' + encodeURIComponent(action) + 
          '&details=' + encodeURIComponent(details) + 
          '&user=' + encodeURIComponent(currentUsername));
}

// 加载操作日志
function loadOperationLogs() {
    var filter = document.getElementById('log-filter-module').value;
    var limit = document.getElementById('log-limit').value;
    var url = '/cgi-bin/logger.cgi?action=list&limit=' + limit;
    if (filter) {
        url += '&filter=' + encodeURIComponent(filter);
    }
    
    fetch(url)
        .then(response => response.json())
        .then(data => {
            var tbody = document.getElementById('operation-logs-body');
            if (data.success && data.logs && data.logs.length > 0) {
                tbody.innerHTML = '';
                data.logs.forEach(log => {
                    var row = document.createElement('tr');
                    row.innerHTML = '<td>' + log.time + '</td>' +
                        '<td>' + log.user + '</td>' +
                        '<td>' + log.module + '</td>' +
                        '<td>' + log.action + '</td>' +
                        '<td>' + log.details + '</td>';
                    tbody.appendChild(row);
                });
            } else {
                tbody.innerHTML = '<tr><td colspan="5" class="text-center">暂无操作日志</td></tr>';
            }
        })
        .catch(err => {
            document.getElementById('operation-logs-body').innerHTML = 
                '<tr><td colspan="5" class="text-center">加载失败: ' + err.message + '</td></tr>';
        });
}

// 清空操作日志
function clearOperationLogs() {
    if (!confirm('确定要清空所有操作日志吗？此操作不可恢复！')) return;
    
    fetch('/cgi-bin/logger.cgi?action=clear')
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            if (data.success) {
                loadOperationLogs();
            }
        })
        .catch(err => {
            alert('清空失败: ' + err.message);
        });
}

// ==================== 友好通知提示 ====================

// 创建通知容器
function createNotificationContainer() {
    var container = document.createElement('div');
    container.className = 'notification-container';
    container.id = 'notificationContainer';
    document.body.appendChild(container);
    return container;
}

// 显示通知
function showNotification(title, message, type, duration) {
    var container = document.getElementById('notificationContainer') || createNotificationContainer();
    
    var notification = document.createElement('div');
    notification.className = 'notification ' + type;
    
    var icons = {
        success: '✅',
        error: '❌',
        warning: '⚠️',
        info: 'ℹ️'
    };
    
    notification.innerHTML = `
        <span class="notification-icon">${icons[type] || '📢'}</span>
        <div class="notification-content">
            <div class="notification-title">${title}</div>
            <div class="notification-message">${message}</div>
        </div>
        <button class="notification-close" onclick="this.parentElement.remove()">&times;</button>
    `;
    
    container.appendChild(notification);
    
    if (duration !== 0) {
        setTimeout(() => {
            notification.style.opacity = '0';
            notification.style.transform = 'translateX(100%)';
            notification.style.transition = 'all 0.3s';
            setTimeout(() => notification.remove(), 300);
        }, duration || 4000);
    }
    
    return notification;
}

// 便捷函数
function showSuccess(title, message) {
    return showNotification(title, message, 'success', 3000);
}

function showError(title, message) {
    return showNotification(title, message, 'error', 5000);
}

function showWarning(title, message) {
    return showNotification(title, message, 'warning', 4000);
}

function showInfo(title, message) {
    return showNotification(title, message, 'info', 3000);
}

// 友好的确认对话框
function showConfirm(title, message, onConfirm, onCancel) {
    var modal = document.createElement('div');
    modal.className = 'confirm-modal';
    modal.innerHTML = `
        <div class="confirm-modal-content">
            <h3>${title}</h3>
            <p>${message}</p>
            <div class="confirm-modal-buttons">
                <button class="btn-secondary" id="confirmCancel">取消</button>
                <button class="btn-primary" id="confirmOk">确定</button>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    
    document.getElementById('confirmOk').onclick = function() {
        modal.remove();
        if (onConfirm) onConfirm();
    };
    
    document.getElementById('confirmCancel').onclick = function() {
        modal.remove();
        if (onCancel) onCancel();
    };
}

// 端口转发弹窗操作
function closeModal() {
    document.getElementById('port-forward-modal').style.display = 'none';
}

function showPortForwardModal() {
    document.getElementById('port-forward-modal').style.display = 'flex';
}