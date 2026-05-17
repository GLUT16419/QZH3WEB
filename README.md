# QZH3WEB - Embedded Linux Router Web Management System

基于嵌入式Linux的智能路由器Web管理系统，提供完整的网络设备管理和安全防护功能。

## 功能特性

### 🔧 系统管理
- **系统状态监控** - CPU负载、内存使用、运行时间、温度监控
- **系统信息展示** - 主机名、操作系统、内核版本
- **定时任务管理** - 自动重启、配置备份、日志清理

### 📊 网络监控
- **实时流量分析** - 基于Canvas的流量图表可视化
- **设备流量排行** - WiFi客户端流量统计
- **速率监控** - WAN/LAN接口速率实时显示

### 🌐 网络配置
- **端口转发** - 自定义端口映射规则
- **DMZ主机** - 将内网设备暴露给外网
- **QoS服务质量** - 带宽优先级管理

### 🔐 安全防护
- **防火墙规则** - 自定义iptables规则管理
- **IDS入侵检测** - 端口扫描、SYN Flood、暴力破解检测
- **IP黑名单** - 恶意IP阻止与解除

### 🛡️ VPN管理
- **WireGuard服务器** - VPN服务器配置与管理
- **客户端管理** - 客户端密钥生成、配置文件下载
- **二维码导出** - 一键扫码配置VPN客户端

### 📈 流量控制
- **流量配额** - 日/月流量限制设置
- **设备限速** - 按IP地址进行带宽限制
- **超限处理** - 自动断网或限速

### 📝 日志系统
- **系统日志** - 系统运行日志查看
- **操作日志** - 用户操作记录审计
- **日志清理** - 自动/手动日志清理

## 技术栈

- **后端**: C语言 CGI 程序
- **前端**: HTML5 + CSS3 + JavaScript (ES6)
- **服务器**: lighttpd
- **平台**: 嵌入式Linux (ARM)

## 项目结构

```
QZH3WEB/
├── c/                    # C语言CGI源代码
│   ├── auth.c            # 用户认证模块
│   ├── firewall.c        # 防火墙管理模块
│   ├── ids.c             # 入侵检测模块
│   ├── vpn.c             # VPN管理模块
│   ├── quota.c           # 流量配额模块
│   ├── system_info.c     # 系统信息模块
│   ├── traffic.c         # 流量监控模块
│   ├── tasks.c           # 定时任务模块
│   ├── dmz.c             # DMZ模块
│   ├── monitoring.c      # 监控模块
│   ├── network_stats.c   # 网络状态模块
│   ├── logger.c          # 日志模块
│   ├── cgi_utils.c/h     # CGI工具函数
│   └── Makefile          # 编译配置
├── cgi-bin/              # Shell CGI脚本
├── html/                 # 前端界面
│   ├── index.html        # 主页面
│   ├── login.html        # 登录页面
│   ├── main.js           # 前端逻辑
│   └── style.css         # 样式文件
├── USER_MANUAL.md        # 用户手册
└── README.md             # 项目说明
```

## 编译与部署

### 编译CGI程序
```bash
cd c/
make
```

### 部署到路由器
```bash
# 将编译后的程序复制到路由器
scp c/* root@router:/var/www/cgi-bin/
scp html/* root@router:/var/www/html/
```

## 访问方式

- **WiFi访问**: `http://192.168.4.1/login.html`
- **以太网访问**: `http://192.168.10.100/login.html`

## 默认凭证

- **用户名**: `admin`
- **密码**: `admin`

## 支持的平台

- 基于Allwinner H3芯片的嵌入式Linux设备
- 支持OpenWrt/LEDE等Linux发行版

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！
