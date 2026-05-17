#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_ALERTS 100
#define MAX_LINE 512
#define LOG_FILE "/var/log/ids.log"

typedef struct {
    int id;
    time_t timestamp;
    char type[32];      // 攻击类型
    char severity[16];   // 严重级别: low, medium, high, critical
    char source_ip[16];
    char dest_ip[16];
    int dest_port;
    char protocol[16];
    char description[256];
    int blocked;
} Alert;

typedef struct {
    char ip[16];
    int port_scan_count;
    time_t last_scan_time;
} PortScanTracker;

static Alert alerts[MAX_ALERTS];
static int alert_count = 0;
static PortScanTracker port_scanners[32];
static int scanner_count = 0;

void print_json_success(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"message\": \"%s\"}\n", message);
}

void print_json_error(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": false, \"message\": \"%s\"}\n", message);
}

void print_alerts() {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"alerts\": [");
    
    for (int i = 0; i < alert_count; i++) {
        if (i > 0) printf(",");
        printf("{\"id\": %d, \"time\": %lu, \"type\": \"%s\", \"severity\": \"%s\", \"src_ip\": \"%s\", \"dst_ip\": \"%s\", \"dst_port\": %d, \"protocol\": \"%s\", \"desc\": \"%s\", \"blocked\": %d}",
               alerts[i].id,
               (unsigned long)alerts[i].timestamp,
               alerts[i].type,
               alerts[i].severity,
               alerts[i].source_ip,
               alerts[i].dest_ip,
               alerts[i].dest_port,
               alerts[i].protocol,
               alerts[i].description,
               alerts[i].blocked);
    }
    
    printf("]}\n");
}

void add_alert(const char *type, const char *severity, const char *src_ip, 
               const char *dst_ip, int dst_port, const char *protocol, 
               const char *desc, int blocked) {
    if (alert_count >= MAX_ALERTS) {
        for (int i = 1; i < MAX_ALERTS; i++) {
            alerts[i-1] = alerts[i];
        }
        alert_count--;
    }
    
    alerts[alert_count].id = alert_count + 1;
    alerts[alert_count].timestamp = time(NULL);
    strncpy(alerts[alert_count].type, type, sizeof(alerts[alert_count].type)-1);
    strncpy(alerts[alert_count].severity, severity, sizeof(alerts[alert_count].severity)-1);
    strncpy(alerts[alert_count].source_ip, src_ip, sizeof(alerts[alert_count].source_ip)-1);
    strncpy(alerts[alert_count].dest_ip, dst_ip, sizeof(alerts[alert_count].dest_ip)-1);
    alerts[alert_count].dest_port = dst_port;
    strncpy(alerts[alert_count].protocol, protocol, sizeof(alerts[alert_count].protocol)-1);
    strncpy(alerts[alert_count].description, desc, sizeof(alerts[alert_count].description)-1);
    alerts[alert_count].blocked = blocked;
    alert_count++;
    
    FILE *fp = fopen(LOG_FILE, "a");
    if (fp) {
        fprintf(fp, "[%lu] %s [%s] %s -> %s:%d (%s) - %s [Blocked: %d]\n",
                (unsigned long)alerts[alert_count-1].timestamp,
                alerts[alert_count-1].type,
                alerts[alert_count-1].severity,
                alerts[alert_count-1].source_ip,
                alerts[alert_count-1].dest_ip,
                alerts[alert_count-1].dest_port,
                alerts[alert_count-1].protocol,
                alerts[alert_count-1].description,
                alerts[alert_count-1].blocked);
        fclose(fp);
    }
}

void detect_port_scan(const char *src_ip, int dst_port) {
    time_t now = time(NULL);
    
    for (int i = 0; i < scanner_count; i++) {
        if (strcmp(port_scanners[i].ip, src_ip) == 0) {
            port_scanners[i].port_scan_count++;
            port_scanners[i].last_scan_time = now;
            
            if (port_scanners[i].port_scan_count >= 10) {
                char desc[256];
                snprintf(desc, sizeof(desc), "检测到端口扫描行为，已扫描%d个端口", port_scanners[i].port_scan_count);
                add_alert("PORT_SCAN", "high", src_ip, "192.168.4.1", dst_port, "TCP", desc, 0);
                
                char cmd[MAX_LINE];
                snprintf(cmd, sizeof(cmd), "iptables -A INPUT -s %s -j DROP", src_ip);
                system(cmd);
                
                alerts[alert_count-1].blocked = 1;
            }
            return;
        }
    }
    
    if (scanner_count < 32) {
        strncpy(port_scanners[scanner_count].ip, src_ip, sizeof(port_scanners[scanner_count].ip)-1);
        port_scanners[scanner_count].port_scan_count = 1;
        port_scanners[scanner_count].last_scan_time = now;
        scanner_count++;
    }
}

void detect_syn_flood() {
    FILE *fp = fopen("/proc/net/sockstat", "r");
    if (!fp) return;
    
    char line[MAX_LINE];
    int tcp_syn_recv = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "TCP: %*d %*d %*d %d", &tcp_syn_recv) == 1) {
            break;
        }
    }
    fclose(fp);
    
    if (tcp_syn_recv > 50) {
        add_alert("SYN_FLOOD", "critical", "Unknown", "192.168.4.1", 0, "TCP", 
                  "检测到SYN洪水攻击，SYN_RECV队列超过50", 0);
    }
}

void detect_ddos() {
    FILE *fp = fopen("/proc/net/stat/snmp", "r");
    if (!fp) return;
    
    char line[MAX_LINE];
    int ip_in_deliver = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "IpInDeliver")) {
            char *ptr = strchr(line, ':');
            if (ptr) {
                ptr++;
                while (*ptr == ' ') ptr++;
                ip_in_deliver = atoi(ptr);
            }
            break;
        }
    }
    fclose(fp);
    
    if (ip_in_deliver > 10000) {
        add_alert("DDoS", "critical", "Multiple", "192.168.4.1", 0, "UDP", 
                  "检测到DDoS攻击，数据包速率异常", 0);
    }
}

void detect_brute_force() {
    FILE *fp = fopen("/var/log/auth.log", "r");
    if (!fp) return;
    
    char line[MAX_LINE];
    int failed_attempts = 0;
    time_t recent_time = time(NULL) - 300;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Failed password")) {
            time_t log_time;
            struct tm tm;
            char month[4], ip[16];
            
            if (sscanf(line, "%3s %d %d:%d:%d", month, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 5) {
                if (strcmp(month, "Jan") == 0) tm.tm_mon = 0;
                else if (strcmp(month, "Feb") == 0) tm.tm_mon = 1;
                else if (strcmp(month, "Mar") == 0) tm.tm_mon = 2;
                else if (strcmp(month, "Apr") == 0) tm.tm_mon = 3;
                else if (strcmp(month, "May") == 0) tm.tm_mon = 4;
                else if (strcmp(month, "Jun") == 0) tm.tm_mon = 5;
                else if (strcmp(month, "Jul") == 0) tm.tm_mon = 6;
                else if (strcmp(month, "Aug") == 0) tm.tm_mon = 7;
                else if (strcmp(month, "Sep") == 0) tm.tm_mon = 8;
                else if (strcmp(month, "Oct") == 0) tm.tm_mon = 9;
                else if (strcmp(month, "Nov") == 0) tm.tm_mon = 10;
                else if (strcmp(month, "Dec") == 0) tm.tm_mon = 11;
                
                tm.tm_year = 126;
                log_time = mktime(&tm);
                
                if (log_time > recent_time) {
                    failed_attempts++;
                    if (strstr(line, "from")) {
                        char *from_ptr = strstr(line, "from");
                        sscanf(from_ptr, "from %15s", ip);
                        
                        if (failed_attempts >= 5) {
                            char desc[256];
                            snprintf(desc, sizeof(desc), "SSH暴力破解尝试，失败%d次", failed_attempts);
                            add_alert("BRUTE_FORCE", "high", ip, "192.168.4.1", 22, "TCP", desc, 0);
                            
                            char cmd[MAX_LINE];
                            snprintf(cmd, sizeof(cmd), "iptables -A INPUT -s %s -j DROP", ip);
                            system(cmd);
                            
                            alerts[alert_count-1].blocked = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    fclose(fp);
}

void run_detection() {
    detect_syn_flood();
    detect_ddos();
    detect_brute_force();
}

void get_stats() {
    int high_count = 0, medium_count = 0, low_count = 0, critical_count = 0;
    
    for (int i = 0; i < alert_count; i++) {
        if (strcmp(alerts[i].severity, "critical") == 0) critical_count++;
        else if (strcmp(alerts[i].severity, "high") == 0) high_count++;
        else if (strcmp(alerts[i].severity, "medium") == 0) medium_count++;
        else if (strcmp(alerts[i].severity, "low") == 0) low_count++;
    }
    
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"total_alerts\": %d, \"critical\": %d, \"high\": %d, \"medium\": %d, \"low\": %d, \"blocked_ips\": %d}\n",
           alert_count, critical_count, high_count, medium_count, low_count, scanner_count);
}

void block_ip(const char *ip) {
    char cmd[MAX_LINE];
    snprintf(cmd, sizeof(cmd), "iptables -A INPUT -s %s -j DROP", ip);
    system(cmd);
    
    add_alert("MANUAL_BLOCK", "medium", ip, "192.168.4.1", 0, "IPTABLES", "管理员手动阻止IP", 1);
    print_json_success("IP已被阻止");
}

void unblock_ip(const char *ip) {
    char cmd[MAX_LINE];
    snprintf(cmd, sizeof(cmd), "iptables -D INPUT -s %s -j DROP 2>/dev/null", ip);
    system(cmd);
    
    print_json_success("IP已解除阻止");
}

int main(void) {
    char *action = getenv("QUERY_STRING");
    
    if (!action) {
        print_json_error("缺少操作参数");
        return 0;
    }
    
    if (strncmp(action, "action=list", 12) == 0) {
        print_alerts();
    }
    else if (strncmp(action, "action=detect", 14) == 0) {
        run_detection();
        print_json_success("检测完成");
    }
    else if (strncmp(action, "action=stats", 13) == 0) {
        get_stats();
    }
    else if (strncmp(action, "action=block", 13) == 0) {
        char *ip = strstr(action, "ip=");
        if (!ip) {
            print_json_error("缺少IP参数");
            return 0;
        }
        ip += 3;
        block_ip(ip);
    }
    else if (strncmp(action, "action=unblock", 16) == 0) {
        char *ip = strstr(action, "ip=");
        if (!ip) {
            print_json_error("缺少IP参数");
            return 0;
        }
        ip += 3;
        unblock_ip(ip);
    }
    else if (strncmp(action, "action=port_scan", 18) == 0) {
        char *src_ip = strstr(action, "src=");
        char *dst_port_str = strstr(action, "port=");
        
        if (!src_ip || !dst_port_str) {
            print_json_error("参数不完整");
            return 0;
        }
        
        src_ip += 4;
        dst_port_str += 5;
        
        char *end = strchr(src_ip, '&');
        if (end) *end = 0;
        
        int dst_port = atoi(dst_port_str);
        detect_port_scan(src_ip, dst_port);
        print_json_success("端口扫描检测完成");
    }
    else {
        print_json_error("未知操作");
    }
    
    return 0;
}