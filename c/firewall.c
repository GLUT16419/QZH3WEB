#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_RULES 100
#define CONFIG_FILE "/etc/firewall/rules.conf"
#define CHAIN_NAME "CUSTOM_FW"

typedef struct {
    int id;
    char name[64];
    char action[16];      // ACCEPT, DROP, REJECT
    char direction[16];   // INPUT, OUTPUT, FORWARD
    char protocol[16];    // TCP, UDP, ICMP, ALL
    char src_ip[32];
    char dst_ip[32];
    int src_port;
    int dst_port;
    int enabled;
} FirewallRule;

static FirewallRule rules[MAX_RULES];
static int rule_count = 0;

void print_json_success(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"message\": \"%s\"}\n", message);
}

void print_json_error(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": false, \"message\": \"%s\"}\n", message);
}

void print_rules_json(FirewallRule *rules, int count) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"rules\": [");

    for (int i = 0; i < count; i++) {
        if (i > 0) printf(",");
        printf("{\"id\": %d, \"name\": \"%s\", \"action\": \"%s\", \"direction\": \"%s\", \"protocol\": \"%s\", \"src_ip\": \"%s\", \"dst_ip\": \"%s\", \"src_port\": %d, \"dst_port\": %d, \"enabled\": %d}",
               rules[i].id,
               rules[i].name,
               rules[i].action,
               rules[i].direction,
               rules[i].protocol,
               rules[i].src_ip,
               rules[i].dst_ip,
               rules[i].src_port,
               rules[i].dst_port,
               rules[i].enabled);
    }

    printf("]}\n");
}

int load_rules(void) {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) {
        rule_count = 0;
        return 0;
    }

    rule_count = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp) && rule_count < MAX_RULES) {
        if (line[0] == '#' || line[0] == '\n') continue;

        FirewallRule rule;
        memset(&rule, 0, sizeof(rule));
        rule.enabled = 1;
        rule.src_port = -1;
        rule.dst_port = -1;
        strcpy(rule.src_ip, "0.0.0.0/0");
        strcpy(rule.dst_ip, "0.0.0.0/0");
        strcpy(rule.protocol, "ALL");

        sscanf(line, "%d|%63[^|]|%15[^|]|%15[^|]|%15[^|]|%31[^|]|%31[^|]|%d|%d|%d",
               &rule.id,
               rule.name,
               rule.action,
               rule.direction,
               rule.protocol,
               rule.src_ip,
               rule.dst_ip,
               &rule.src_port,
               &rule.dst_port,
               &rule.enabled);

        rules[rule_count++] = rule;
    }

    fclose(fp);
    return rule_count;
}

int save_rules(void) {
    FILE *fp = fopen(CONFIG_FILE, "w");
    if (!fp) return -1;

    fprintf(fp, "# Firewall Rules Configuration\n");
    fprintf(fp, "# Format: id|name|action|direction|protocol|src_ip|dst_ip|src_port|dst_port|enabled\n\n");

    for (int i = 0; i < rule_count; i++) {
        fprintf(fp, "%d|%s|%s|%s|%s|%s|%s|%d|%d|%d\n",
                rules[i].id,
                rules[i].name,
                rules[i].action,
                rules[i].direction,
                rules[i].protocol,
                rules[i].src_ip,
                rules[i].dst_ip,
                rules[i].src_port,
                rules[i].dst_port,
                rules[i].enabled);
    }

    fclose(fp);
    return 0;
}

int get_next_id(void) {
    int max_id = 0;
    for (int i = 0; i < rule_count; i++) {
        if (rules[i].id > max_id) max_id = rules[i].id;
    }
    return max_id + 1;
}

void apply_iptables_rule(FirewallRule *rule) {
    char cmd[512];

    if (!rule->enabled) {
        snprintf(cmd, sizeof(cmd), "iptables -D %s -j %s 2>/dev/null",
                 rule->direction, rule->action);
        system(cmd);
        return;
    }

    snprintf(cmd, sizeof(cmd), "iptables -A %s", rule->direction);

    if (strcmp(rule->protocol, "TCP") == 0) {
        strcat(cmd, " -p tcp");
    } else if (strcmp(rule->protocol, "UDP") == 0) {
        strcat(cmd, " -p udp");
    } else if (strcmp(rule->protocol, "ICMP") == 0) {
        strcat(cmd, " -p icmp");
    }

    if (strcmp(rule->src_ip, "0.0.0.0/0") != 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), " -s %s", rule->src_ip);
        strcat(cmd, tmp);
    }

    if (strcmp(rule->dst_ip, "0.0.0.0/0") != 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), " -d %s", rule->dst_ip);
        strcat(cmd, tmp);
    }

    if (rule->src_port > 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), " --sport %d", rule->src_port);
        strcat(cmd, tmp);
    }

    if (rule->dst_port > 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), " --dport %d", rule->dst_port);
        strcat(cmd, tmp);
    }

    char tmp[128];
    snprintf(tmp, sizeof(tmp), " -j %s", rule->action);
    strcat(cmd, tmp);

    system(cmd);
}

void apply_all_rules(void) {
    system("iptables -F CUSTOM_FW 2>/dev/null");
    system("iptables -N CUSTOM_FW 2>/dev/null || iptables -F CUSTOM_FW");

    for (int i = 0; i < rule_count; i++) {
        if (rules[i].enabled) {
            apply_iptables_rule(&rules[i]);
        }
    }
}

void clear_all_fw_rules(void) {
    system("iptables -F");
    system("iptables -X");
    system("iptables -Z");
}

void setup_default_policy(void) {
    system("iptables -P INPUT DROP");
    system("iptables -P FORWARD DROP");
    system("iptables -P OUTPUT ACCEPT");

    system("iptables -A INPUT -i lo -j ACCEPT");
    system("iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT");
    system("iptables -A INPUT -p icmp -j ACCEPT");
    system("iptables -A INPUT -p tcp --dport 22 -j ACCEPT");
    system("iptables -A INPUT -p tcp --dport 80 -j ACCEPT");
    system("iptables -A INPUT -p tcp --dport 443 -j ACCEPT");
}

int add_rule(const char *name, const char *action, const char *direction,
             const char *protocol, const char *src_ip, const char *dst_ip,
             int src_port, int dst_port) {
    if (rule_count >= MAX_RULES) {
        return -1;
    }

    FirewallRule rule;
    memset(&rule, 0, sizeof(rule));

    rule.id = get_next_id();
    strncpy(rule.name, name, sizeof(rule.name) - 1);
    strncpy(rule.action, action, sizeof(rule.action) - 1);
    strncpy(rule.direction, direction, sizeof(rule.direction) - 1);
    strncpy(rule.protocol, protocol, sizeof(rule.protocol) - 1);
    strncpy(rule.src_ip, src_ip, sizeof(rule.src_ip) - 1);
    strncpy(rule.dst_ip, dst_ip, sizeof(rule.dst_ip) - 1);
    rule.src_port = src_port;
    rule.dst_port = dst_port;
    rule.enabled = 1;

    rules[rule_count++] = rule;

    if (save_rules() < 0) {
        rule_count--;
        return -1;
    }

    apply_iptables_rule(&rule);
    return 0;
}

int delete_rule(int id) {
    for (int i = 0; i < rule_count; i++) {
        if (rules[i].id == id) {
            rules[i].enabled = 0;
            apply_iptables_rule(&rules[i]);

            for (int j = i; j < rule_count - 1; j++) {
                rules[j] = rules[j + 1];
            }
            rule_count--;

            save_rules();
            return 0;
        }
    }
    return -1;
}

int toggle_rule(int id) {
    for (int i = 0; i < rule_count; i++) {
        if (rules[i].id == id) {
            rules[i].enabled = !rules[i].enabled;
            apply_iptables_rule(&rules[i]);
            save_rules();
            return 0;
        }
    }
    return -1;
}

int apply_firewall(void) {
    clear_all_fw_rules();
    setup_default_policy();
    apply_all_rules();
    save_rules();
    return 0;
}

char* get_param(const char *query, const char *key) {
    static char value[256];
    static char buffer[1024];
    static int initialized = 0;

    if (!initialized) {
        char *method = getenv("REQUEST_METHOD");
        if (method && strcmp(method, "POST") == 0) {
            char *len_str = getenv("CONTENT_LENGTH");
            if (len_str) {
                int len = atoi(len_str);
                if (len > 0 && len < sizeof(buffer)) {
                    fread(buffer, 1, len, stdin);
                    buffer[len] = '\0';
                    initialized = 1;
                }
            }
        } else {
            char *qs = getenv("QUERY_STRING");
            if (qs) {
                strncpy(buffer, qs, sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                initialized = 1;
            }
        }
    }

    if (query && strlen(query) > 0) {
        strncpy(buffer, query, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
    }

    if (!buffer[0]) return NULL;

    char *start = strstr(buffer, key);
    if (!start) return NULL;

    start += strlen(key);
    if (*start == '=') start++;

    char *end = strchr(start, '&');
    if (end) {
        int len = end - start;
        if (len >= sizeof(value)) len = sizeof(value) - 1;
        strncpy(value, start, len);
        value[len] = '\0';
    } else {
        strncpy(value, start, sizeof(value) - 1);
        value[sizeof(value) - 1] = '\0';
    }

    for (char *p = value; *p; p++) {
        if (*p == '\n' || *p == '\r') *p = '\0';
    }

    return value;
}

int main(void) {
    char *query_str = NULL;
    char *method = getenv("REQUEST_METHOD");

    if (method && strcmp(method, "POST") == 0) {
        char *len_str = getenv("CONTENT_LENGTH");
        if (len_str) {
            int len = atoi(len_str);
            if (len > 0) {
                static char post_data[1024];
                fread(post_data, 1, len < sizeof(post_data) ? len : sizeof(post_data) - 1, stdin);
                post_data[len] = '\0';
                query_str = post_data;
            }
        }
    } else {
        query_str = getenv("QUERY_STRING");
    }

    load_rules();

    if (!query_str || !*query_str) {
        print_rules_json(rules, rule_count);
        return 0;
    }

    if (strncmp(query_str, "action=list", 12) == 0) {
        print_rules_json(rules, rule_count);
    }
    else if (strncmp(query_str, "action=add", 10) == 0) {
        char *name = get_param(query_str, "name");
        char *action = get_param(query_str, "action");
        char *direction = get_param(query_str, "direction");
        char *protocol = get_param(query_str, "protocol");
        char *src_ip = get_param(query_str, "src_ip");
        char *dst_ip = get_param(query_str, "dst_ip");
        char *src_port_str = get_param(query_str, "src_port");
        char *dst_port_str = get_param(query_str, "dst_port");

        if (!name || !action || !direction) {
            print_json_error("缺少必要参数");
            return 0;
        }

        int src_port = src_port_str ? atoi(src_port_str) : -1;
        int dst_port = dst_port_str ? atoi(dst_port_str) : -1;

        if (!src_ip) src_ip = "0.0.0.0/0";
        if (!dst_ip) dst_ip = "0.0.0.0/0";
        if (!protocol) protocol = "ALL";

        if (add_rule(name, action, direction, protocol, src_ip, dst_ip, src_port, dst_port) == 0) {
            print_json_success("规则添加成功");
        } else {
            print_json_error("规则添加失败");
        }
    }
    else if (strncmp(query_str, "action=delete", 13) == 0) {
        char *id_str = get_param(query_str, "id");
        if (!id_str) {
            print_json_error("缺少规则ID");
            return 0;
        }

        if (delete_rule(atoi(id_str)) == 0) {
            print_json_success("规则删除成功");
        } else {
            print_json_error("规则删除失败");
        }
    }
    else if (strncmp(query_str, "action=toggle", 13) == 0) {
        char *id_str = get_param(query_str, "id");
        if (!id_str) {
            print_json_error("缺少规则ID");
            return 0;
        }

        if (toggle_rule(atoi(id_str)) == 0) {
            print_json_success("规则状态切换成功");
        } else {
            print_json_error("规则状态切换失败");
        }
    }
    else if (strncmp(query_str, "action=apply", 12) == 0) {
        if (apply_firewall() == 0) {
            print_json_success("防火墙规则已应用");
        } else {
            print_json_error("防火墙规则应用失败");
        }
    }
    else if (strncmp(query_str, "action=reset", 12) == 0) {
        clear_all_fw_rules();
        setup_default_policy();
        save_rules();
        print_json_success("防火墙已重置为默认策略");
    }
    else if (strncmp(query_str, "action=add_template", 18) == 0) {
        char *template = get_param(query_str, "template");

        if (!template) {
            print_json_error("请选择模板");
            return 0;
        }

        int added = 0;

        if (strcmp(template, "allow_http") == 0) {
            add_rule("允许HTTP", "ACCEPT", "INPUT", "TCP", "0.0.0.0/0", "0.0.0.0/0", -1, 80);
            add_rule("允许HTTPS", "ACCEPT", "INPUT", "TCP", "0.0.0.0/0", "0.0.0.0/0", -1, 443);
            added = 2;
        }
        else if (strcmp(template, "allow_ssh") == 0) {
            add_rule("允许SSH", "ACCEPT", "INPUT", "TCP", "0.0.0.0/0", "0.0.0.0/0", -1, 22);
            added = 1;
        }
        else if (strcmp(template, "allow_dns") == 0) {
            add_rule("允许DNS", "ACCEPT", "INPUT", "UDP", "0.0.0.0/0", "0.0.0.0/0", -1, 53);
            add_rule("允许DNS", "ACCEPT", "OUTPUT", "UDP", "0.0.0.0/0", "0.0.0.0/0", -1, 53);
            added = 2;
        }
        else if (strcmp(template, "block_ping") == 0) {
            add_rule("禁止Ping", "DROP", "INPUT", "ICMP", "0.0.0.0/0", "0.0.0.0/0", -1, -1);
            added = 1;
        }
        else if (strcmp(template, "allow_rdp") == 0) {
            add_rule("允许RDP", "ACCEPT", "INPUT", "TCP", "0.0.0.0/0", "0.0.0.0/0", -1, 3389);
            added = 1;
        }
        else {
            print_json_error("未知模板");
            return 0;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "已添加%d条规则", added);
        print_json_success(msg);
    }
    else {
        print_json_error("未知操作");
    }

    return 0;
}