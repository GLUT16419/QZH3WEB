#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define QUOTA_CONFIG_FILE "/etc/traffic_quota.conf"
#define TRAFFIC_STATS_FILE "/var/log/traffic_stats.log"
#define IPTABLES_PATH "/usr/sbin/iptables"
#define TC_PATH "/usr/sbin/tc"

char *get_param(const char *name) {
    char *method = getenv("REQUEST_METHOD");
    static char result[512];
    static char post_data[4096];
    static int data_loaded = 0;
    result[0] = '\0';

    if (!method) return NULL;

    if (strcmp(method, "POST") == 0) {
        if (!data_loaded) {
            char *len_str = getenv("CONTENT_LENGTH");
            if (len_str) {
                int len = atoi(len_str);
                if (len > 0 && len < 4096) {
                    fread(post_data, 1, len, stdin);
                    post_data[len] = '\0';
                }
            }
            data_loaded = 1;
        }

        char *p = strstr(post_data, name);
        if (p) {
            p += strlen(name) + 1;
            int i = 0;
            while (*p && *p != '&' && *p != '\n' && *p != '\r' && i < 511) {
                result[i++] = *p++;
            }
            result[i] = '\0';
            return result;
        }
        return NULL;
    } else if (strcmp(method, "GET") == 0) {
        char *query = getenv("QUERY_STRING");
        if (query) {
            char *p = strstr(query, name);
            if (p) {
                p += strlen(name) + 1;
                int i = 0;
                while (*p && *p != '&' && i < 511) {
                    result[i++] = *p++;
                }
                result[i] = '\0';
                return result;
            }
        }
        return NULL;
    }
    return NULL;
}

typedef struct {
    char ip[16];
    char name[32];
    unsigned long long daily_quota;
    unsigned long long monthly_quota;
    unsigned long long daily_used;
    unsigned long long monthly_used;
    int enabled;
    int action;
    time_t last_reset_daily;
    time_t last_reset_monthly;
} QuotaEntry;

void ensure_config_file() {
    FILE *fp = fopen(QUOTA_CONFIG_FILE, "r");
    if (!fp) {
        fp = fopen(QUOTA_CONFIG_FILE, "w");
        if (fp) fclose(fp);
    } else {
        fclose(fp);
    }
}

void load_quotas(QuotaEntry *quotas, int *count, int max_count) {
    *count = 0;
    ensure_config_file();
    
    FILE *fp = fopen(QUOTA_CONFIG_FILE, "r");
    if (!fp) return;

    char line[512];
    while (fgets(line, sizeof(line), fp) && *count < max_count) {
        QuotaEntry *q = &quotas[*count];
        if (sscanf(line, "%15[^:]:%31[^:]:%llu:%llu:%llu:%llu:%d:%d:%ld:%ld",
                   q->ip, q->name, &q->daily_quota, &q->monthly_quota,
                   &q->daily_used, &q->monthly_used, &q->enabled, &q->action,
                   (long *)&q->last_reset_daily, (long *)&q->last_reset_monthly) == 10) {
            (*count)++;
        }
    }
    fclose(fp);
}

void save_quotas(QuotaEntry *quotas, int count) {
    FILE *fp = fopen(QUOTA_CONFIG_FILE, "w");
    if (!fp) return;

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s:%s:%llu:%llu:%llu:%llu:%d:%d:%ld:%ld\n",
                quotas[i].ip, quotas[i].name,
                quotas[i].daily_quota, quotas[i].monthly_quota,
                quotas[i].daily_used, quotas[i].monthly_used,
                quotas[i].enabled, quotas[i].action,
                (long)quotas[i].last_reset_daily,
                (long)quotas[i].last_reset_monthly);
    }
    fclose(fp);
}

unsigned long long get_current_traffic(const char *ip) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "iptables -L -v -n | grep %s | awk '{sum+=$2} END {print sum}'", ip);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    unsigned long long bytes = 0;
    char line[64];
    if (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "K")) {
            bytes = atof(line) * 1024;
        } else if (strstr(line, "M")) {
            bytes = atof(line) * 1024 * 1024;
        } else if (strstr(line, "G")) {
            bytes = atof(line) * 1024 * 1024 * 1024;
        } else {
            bytes = atoll(line);
        }
    }
    pclose(fp);
    return bytes;
}

void apply_quota_limit(const char *ip, int action) {
    char cmd[512];
    
    if (action == 1) {
        snprintf(cmd, sizeof(cmd), 
                 "%s -I FORWARD -s %s -j DROP 2>/dev/null", IPTABLES_PATH, ip);
        system(cmd);
        snprintf(cmd, sizeof(cmd), 
                 "%s -I FORWARD -d %s -j DROP 2>/dev/null", IPTABLES_PATH, ip);
        system(cmd);
    } else if (action == 2) {
        snprintf(cmd, sizeof(cmd),
                 "%s class add dev br-lan parent 1: classid 1:100 htb rate 256kbit ceil 512kbit 2>/dev/null",
                 TC_PATH);
        system(cmd);
        snprintf(cmd, sizeof(cmd),
                 "%s filter add dev br-lan protocol ip parent 1:0 prio 1 u32 match ip src %s flowid 1:100 2>/dev/null",
                 TC_PATH, ip);
        system(cmd);
    }
}

void remove_quota_limit(const char *ip) {
    char cmd[512];
    
    snprintf(cmd, sizeof(cmd),
             "%s -D FORWARD -s %s -j DROP 2>/dev/null", IPTABLES_PATH, ip);
    system(cmd);
    snprintf(cmd, sizeof(cmd),
             "%s -D FORWARD -d %s -j DROP 2>/dev/null", IPTABLES_PATH, ip);
    system(cmd);
}

void check_and_apply_quotas() {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    int current_day = tm_now->tm_mday;
    int current_month = tm_now->tm_mon;

    for (int i = 0; i < count; i++) {
        QuotaEntry *q = &quotas[i];
        
        if (!q->enabled) continue;

        struct tm *tm_last = localtime(&q->last_reset_daily);
        if (tm_last->tm_mday != current_day) {
            q->daily_used = 0;
            q->last_reset_daily = now;
        }

        tm_last = localtime(&q->last_reset_monthly);
        if (tm_last->tm_mon != current_month) {
            q->monthly_used = 0;
            q->last_reset_monthly = now;
        }

        q->daily_used = get_current_traffic(q->ip);
        q->monthly_used += q->daily_used;

        if ((q->daily_quota > 0 && q->daily_used >= q->daily_quota) ||
            (q->monthly_quota > 0 && q->monthly_used >= q->monthly_quota)) {
            apply_quota_limit(q->ip, q->action);
        } else {
            remove_quota_limit(q->ip);
        }
    }

    save_quotas(quotas, count);
}

void get_all_quotas() {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    printf("{\"success\": true, \"quotas\": [");
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(",");
        printf("{\"ip\": \"%s\", \"name\": \"%s\", \"daily_quota\": %llu, \"monthly_quota\": %llu, "
               "\"daily_used\": %llu, \"monthly_used\": %llu, \"enabled\": %d, \"action\": %d, "
               "\"daily_percent\": %.1f, \"monthly_percent\": %.1f}",
               quotas[i].ip, quotas[i].name, quotas[i].daily_quota, quotas[i].monthly_quota,
               quotas[i].daily_used, quotas[i].monthly_used, quotas[i].enabled, quotas[i].action,
               quotas[i].daily_quota > 0 ? (double)quotas[i].daily_used * 100 / quotas[i].daily_quota : 0,
               quotas[i].monthly_quota > 0 ? (double)quotas[i].monthly_used * 100 / quotas[i].monthly_quota : 0);
    }
    printf("], \"count\": %d}", count);
}

void add_quota(const char *ip, const char *name, 
               unsigned long long daily_quota, unsigned long long monthly_quota,
               int action) {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    if (count >= 64) {
        printf("{\"success\": false, \"message\": \"配额规则数量已达上限\"}");
        return;
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(quotas[i].ip, ip) == 0) {
            printf("{\"success\": false, \"message\": \"该IP已存在配额规则\"}");
            return;
        }
    }

    QuotaEntry *q = &quotas[count];
    strncpy(q->ip, ip, sizeof(q->ip) - 1);
    strncpy(q->name, name, sizeof(q->name) - 1);
    q->daily_quota = daily_quota;
    q->monthly_quota = monthly_quota;
    q->daily_used = 0;
    q->monthly_used = 0;
    q->enabled = 1;
    q->action = action;
    q->last_reset_daily = time(NULL);
    q->last_reset_monthly = time(NULL);

    count++;
    save_quotas(quotas, count);

    printf("{\"success\": true, \"message\": \"配额规则已添加\"}");
}

void update_quota(const char *ip, 
                  unsigned long long daily_quota, unsigned long long monthly_quota,
                  int action) {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    for (int i = 0; i < count; i++) {
        if (strcmp(quotas[i].ip, ip) == 0) {
            quotas[i].daily_quota = daily_quota;
            quotas[i].monthly_quota = monthly_quota;
            quotas[i].action = action;
            save_quotas(quotas, count);
            printf("{\"success\": true, \"message\": \"配额规则已更新\"}");
            return;
        }
    }

    printf("{\"success\": false, \"message\": \"未找到该IP的配额规则\"}");
}

void toggle_quota(const char *ip) {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    for (int i = 0; i < count; i++) {
        if (strcmp(quotas[i].ip, ip) == 0) {
            quotas[i].enabled = !quotas[i].enabled;
            
            if (quotas[i].enabled) {
                remove_quota_limit(ip);
            }
            
            save_quotas(quotas, count);
            printf("{\"success\": true, \"message\": \"配额规则状态已更新\"}");
            return;
        }
    }

    printf("{\"success\": false, \"message\": \"未找到该IP的配额规则\"}");
}

void delete_quota(const char *ip) {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    int found = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(quotas[i].ip, ip) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        printf("{\"success\": false, \"message\": \"未找到该IP的配额规则\"}");
        return;
    }

    remove_quota_limit(ip);

    for (int i = found; i < count - 1; i++) {
        quotas[i] = quotas[i + 1];
    }
    count--;

    save_quotas(quotas, count);
    printf("{\"success\": true, \"message\": \"配额规则已删除\"}");
}

void reset_quota_stats(const char *ip) {
    QuotaEntry quotas[64];
    int count;
    load_quotas(quotas, &count, 64);

    for (int i = 0; i < count; i++) {
        if (strcmp(quotas[i].ip, ip) == 0) {
            quotas[i].daily_used = 0;
            quotas[i].monthly_used = 0;
            quotas[i].last_reset_daily = time(NULL);
            quotas[i].last_reset_monthly = time(NULL);
            remove_quota_limit(ip);
            save_quotas(quotas, count);
            printf("{\"success\": true, \"message\": \"流量统计已重置\"}");
            return;
        }
    }

    printf("{\"success\": false, \"message\": \"未找到该IP的配额规则\"}");
}

void get_connected_devices() {
    FILE *fp = popen("cat /proc/net/arp | grep -v 'IP' | awk '{print $1\",\"$4}'", "r");
    if (!fp) {
        printf("{\"success\": false, \"devices\": []}");
        return;
    }

    printf("{\"success\": true, \"devices\": [");
    char line[128];
    int first = 1;
    while (fgets(line, sizeof(line), fp)) {
        char ip[16], mac[18];
        if (sscanf(line, "%15[^,],%17s", ip, mac) == 2) {
            if (strlen(mac) > 5 && strcmp(mac, "00:00:00:00:00:00") != 0) {
                if (!first) printf(",");
                printf("{\"ip\": \"%s\", \"mac\": \"%s\"}", ip, mac);
                first = 0;
            }
        }
    }
    pclose(fp);
    printf("]}");
}

int main() {
    char *action = get_param("action");

    printf("Content-Type: application/json\r\n");
    printf("\r\n");

    if (!action) {
        get_all_quotas();
        return 0;
    }

    if (strcmp(action, "list") == 0) {
        get_all_quotas();
    } else if (strcmp(action, "add") == 0) {
        char *ip = get_param("ip");
        char *name = get_param("name");
        char *daily = get_param("daily_quota");
        char *monthly = get_param("monthly_quota");
        char *act = get_param("limit_action");
        
        if (ip && name && daily && monthly && act) {
            add_quota(ip, name, atoll(daily), atoll(monthly), atoi(act));
        } else {
            printf("{\"success\": false, \"message\": \"缺少参数\"}");
        }
    } else if (strcmp(action, "update") == 0) {
        char *ip = get_param("ip");
        char *daily = get_param("daily_quota");
        char *monthly = get_param("monthly_quota");
        char *act = get_param("limit_action");
        
        if (ip && daily && monthly && act) {
            update_quota(ip, atoll(daily), atoll(monthly), atoi(act));
        } else {
            printf("{\"success\": false, \"message\": \"缺少参数\"}");
        }
    } else if (strcmp(action, "toggle") == 0) {
        char *ip = get_param("ip");
        if (ip) {
            toggle_quota(ip);
        } else {
            printf("{\"success\": false, \"message\": \"缺少IP参数\"}");
        }
    } else if (strcmp(action, "delete") == 0) {
        char *ip = get_param("ip");
        if (ip) {
            delete_quota(ip);
        } else {
            printf("{\"success\": false, \"message\": \"缺少IP参数\"}");
        }
    } else if (strcmp(action, "reset") == 0) {
        char *ip = get_param("ip");
        if (ip) {
            reset_quota_stats(ip);
        } else {
            printf("{\"success\": false, \"message\": \"缺少IP参数\"}");
        }
    } else if (strcmp(action, "check") == 0) {
        check_and_apply_quotas();
        printf("{\"success\": true, \"message\": \"配额检查完成\"}");
    } else if (strcmp(action, "devices") == 0) {
        get_connected_devices();
    } else {
        printf("{\"success\": false, \"message\": \"未知操作\"}");
    }

    return 0;
}