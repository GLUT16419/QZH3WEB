#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_DEVICES 32
#define MAX_LINE 256
#define HISTORY_SIZE 60

typedef struct {
    char ip[16];
    char mac[18];
    char name[32];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    unsigned long long rx_bytes_prev;
    unsigned long long tx_bytes_prev;
    int speed_limit;
    double rx_rate;  // bytes per second
    double tx_rate;  // bytes per second
} DeviceStats;

typedef struct {
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    time_t timestamp;
} TrafficHistory;

static TrafficHistory wan_history[HISTORY_SIZE];
static TrafficHistory wlan_history[HISTORY_SIZE];
static int history_index = 0;

void print_json_success(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"message\": \"%s\"}\n", message);
}

void print_json_error(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": false, \"message\": \"%s\"}\n", message);
}

void print_device_stats(DeviceStats *devices, int count) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"devices\": [");
    
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(",");
        printf("{\"ip\": \"%s\", \"mac\": \"%s\", \"name\": \"%s\", \"rx_bytes\": %llu, \"tx_bytes\": %llu, \"speed_limit\": %d, \"rx_rate\": %.2f, \"tx_rate\": %.2f}",
               devices[i].ip, devices[i].mac, devices[i].name,
               devices[i].rx_bytes, devices[i].tx_bytes, devices[i].speed_limit,
               devices[i].rx_rate, devices[i].tx_rate);
    }
    
    printf("]}\n");
}

void update_traffic_history(const char *iface, TrafficHistory *history) {
    char path[MAX_LINE];
    unsigned long long rx_bytes = 0, tx_bytes = 0;
    
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", iface);
    FILE *rfp = fopen(path, "r");
    if (rfp) {
        fscanf(rfp, "%llu", &rx_bytes);
        fclose(rfp);
    }
    
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", iface);
    FILE *tfp = fopen(path, "r");
    if (tfp) {
        fscanf(tfp, "%llu", &tx_bytes);
        fclose(tfp);
    }
    
    history[history_index].rx_bytes = rx_bytes;
    history[history_index].tx_bytes = tx_bytes;
    history[history_index].timestamp = time(NULL);
    
    history_index = (history_index + 1) % HISTORY_SIZE;
}

void print_traffic_history(TrafficHistory *history) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"history\": [");
    
    for (int i = 0; i < HISTORY_SIZE; i++) {
        int idx = (history_index + i) % HISTORY_SIZE;
        if (history[idx].timestamp == 0) continue;
        
        if (i > 0 && history[(history_index + i - 1) % HISTORY_SIZE].timestamp != 0) 
            printf(",");
        
        printf("{\"time\": %lu, \"rx\": %llu, \"tx\": %llu}",
               (unsigned long)history[idx].timestamp,
               history[idx].rx_bytes,
               history[idx].tx_bytes);
    }
    
    printf("]}\n");
}

void get_device_stats(DeviceStats *devices, int *count) {
    *count = 0;
    
    FILE *fp = fopen("/proc/net/arp", "r");
    if (!fp) return;
    
    char line[MAX_LINE];
    fgets(line, sizeof(line), fp);
    
    while (fgets(line, sizeof(line), fp) && *count < MAX_DEVICES) {
        char ip[16], mac[18], dev[16];
        if (sscanf(line, "%15s %*s %*s %17s %*s %15s", ip, mac, dev) == 3) {
            strcpy(devices[*count].ip, ip);
            strcpy(devices[*count].mac, mac);
            strcpy(devices[*count].name, "Unknown");
            devices[*count].speed_limit = 0;
            
            char path[MAX_LINE];
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", dev);
            FILE *rfp = fopen(path, "r");
            if (rfp) {
                fscanf(rfp, "%llu", &devices[*count].rx_bytes);
                fclose(rfp);
            }
            
            snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", dev);
            FILE *tfp = fopen(path, "r");
            if (tfp) {
                fscanf(tfp, "%llu", &devices[*count].tx_bytes);
                fclose(tfp);
            }
            
            (*count)++;
        }
    }
    fclose(fp);
}

void set_speed_limit(const char *ip, int limit) {
    char cmd[MAX_LINE];
    
    if (limit > 0) {
        snprintf(cmd, sizeof(cmd), "tc qdisc add dev eth0 root handle 1: htb default 12");
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "tc class add dev eth0 parent 1: classid 1:1 htb rate %dmbit");
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dst %s flowid 1:1", ip);
        system(cmd);
        
        snprintf(cmd, sizeof(cmd), "tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip src %s flowid 1:1", ip);
        system(cmd);
        
        FILE *fp = fopen("/etc/limits.conf", "a");
        if (fp) {
            fprintf(fp, "%s %d\n", ip, limit);
            fclose(fp);
        }
    } else {
        snprintf(cmd, sizeof(cmd), "tc qdisc del dev eth0 root");
        system(cmd);
    }
    
    print_json_success("限速设置已应用");
}

void get_overall_stats() {
    printf("Content-Type: application/json\n\n");
    
    unsigned long long wan_rx = 0, wan_tx = 0, wlan_rx = 0, wlan_tx = 0;
    char path[MAX_LINE];
    
    snprintf(path, sizeof(path), "/sys/class/net/eth0/statistics/rx_bytes");
    FILE *fp = fopen(path, "r");
    if (fp) { fscanf(fp, "%llu", &wan_rx); fclose(fp); }
    
    snprintf(path, sizeof(path), "/sys/class/net/eth0/statistics/tx_bytes");
    fp = fopen(path, "r");
    if (fp) { fscanf(fp, "%llu", &wan_tx); fclose(fp); }
    
    snprintf(path, sizeof(path), "/sys/class/net/wlan0/statistics/rx_bytes");
    fp = fopen(path, "r");
    if (fp) { fscanf(fp, "%llu", &wlan_rx); fclose(fp); }
    
    snprintf(path, sizeof(path), "/sys/class/net/wlan0/statistics/tx_bytes");
    fp = fopen(path, "r");
    if (fp) { fscanf(fp, "%llu", &wlan_tx); fclose(fp); }
    
    printf("{\"success\": true, \"wan_rx\": %llu, \"wan_tx\": %llu, \"wlan_rx\": %llu, \"wlan_tx\": %llu}\n",
           wan_rx, wan_tx, wlan_rx, wlan_tx);
}

int main(void) {
    char *action = getenv("QUERY_STRING");
    
    if (!action) {
        print_json_error("缺少操作参数");
        return 0;
    }
    
    if (strncmp(action, "action=stats", 13) == 0) {
        DeviceStats devices[MAX_DEVICES];
        int count;
        get_device_stats(devices, &count);
        print_device_stats(devices, count);
    }
    else if (strncmp(action, "action=limit", 13) == 0) {
        char *ip = strstr(action, "ip=");
        char *limit_str = strstr(action, "limit=");
        
        if (!ip || !limit_str) {
            print_json_error("参数不完整");
            return 0;
        }
        
        ip += 3;
        limit_str += 6;
        
        char *end;
        end = strchr(ip, '&');
        if (end) *end = 0;
        
        int limit = atoi(limit_str);
        set_speed_limit(ip, limit);
    }
    else if (strncmp(action, "action=overall", 15) == 0) {
        get_overall_stats();
    }
    else if (strncmp(action, "action=wan_history", 18) == 0) {
        update_traffic_history("eth0", wan_history);
        print_traffic_history(wan_history);
    }
    else if (strncmp(action, "action=wlan_history", 20) == 0) {
        update_traffic_history("wlan0", wlan_history);
        print_traffic_history(wlan_history);
    }
    else {
        print_json_error("未知操作");
    }
    
    return 0;
}