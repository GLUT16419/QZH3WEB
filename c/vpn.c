#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define VPN_CONF_FILE "/etc/wireguard/wg0.conf"
#define VPN_CLIENT_DIR "/etc/wireguard/clients"
#define VPN_CLIENT_FILE "/etc/wireguard/clients/clients.list"

char *get_param(const char *name) {
    char *method = getenv("REQUEST_METHOD");
    static char result[512];
    static char post_data[4096];
    static int data_loaded = 0;
    result[0] = '\0';

    if (!method) {
        return NULL;
    }

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

void generate_key_pair(char *private_key, char *public_key) {
    FILE *fp = popen("wg genkey", "r");
    if (fp) {
        fgets(private_key, 45, fp);
        private_key[strcspn(private_key, "\n")] = '\0';
        pclose(fp);
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo '%s' | wg pubkey", private_key);
    fp = popen(cmd, "r");
    if (fp) {
        fgets(public_key, 45, fp);
        public_key[strcspn(public_key, "\n")] = '\0';
        pclose(fp);
    }
}

void get_vpn_status() {
    FILE *fp = fopen("/sys/class/net/wg0/operstate", "r");
    if (!fp) {
        printf("{\"status\": \"stopped\"}");
        return;
    }

    char state[16];
    fgets(state, sizeof(state), fp);
    fclose(fp);

    state[strcspn(state, "\n")] = '\0';

    if (strcmp(state, "up") == 0 || strcmp(state, "unknown") == 0) {
        char public_key[45] = "";
        char port[8] = "";
        char endpoint[64] = "";
        char allowed_ips[64] = "";

        FILE *fp2 = popen("sudo wg show wg0 public-key 2>/dev/null", "r");
        if (fp2) {
            fgets(public_key, sizeof(public_key), fp2);
            public_key[strcspn(public_key, "\n")] = '\0';
            pclose(fp2);
        }

        FILE *fp3 = popen("sudo wg show wg0 listen-port 2>/dev/null", "r");
        if (fp3) {
            fgets(port, sizeof(port), fp3);
            port[strcspn(port, "\n")] = '\0';
            pclose(fp3);
        }

        FILE *fp_conf = fopen("/etc/wireguard/wg0.conf", "r");
        if (fp_conf) {
            char line[256];
            while (fgets(line, sizeof(line), fp_conf)) {
                if (strstr(line, "Endpoint")) {
                    char *p = strchr(line, '=');
                    if (p) {
                        p++;
                        while (*p == ' ') p++;
                        char *end = strchr(p, ':');
                        if (end) {
                            strncpy(endpoint, p, end - p);
                            endpoint[end - p] = '\0';
                        }
                    }
                }
                if (strstr(line, "AllowedIPs")) {
                    char *p = strchr(line, '=');
                    if (p) {
                        p++;
                        while (*p == ' ') p++;
                        char *end = strchr(p, '\n');
                        if (end) *end = '\0';
                        strncpy(allowed_ips, p, sizeof(allowed_ips) - 1);
                    }
                }
            }
            fclose(fp_conf);
        }

        if (strlen(public_key) > 0) {
            printf("{\"status\": \"running\", \"interface\": \"wg0\", \"public_key\": \"%s\", \"port\": \"%s\", \"endpoint\": \"%s\", \"allowed_ips\": \"%s\"}",
                   public_key, port, endpoint, allowed_ips);
        } else {
            printf("{\"status\": \"stopped\"}");
        }
    } else {
        printf("{\"status\": \"stopped\"}");
    }
}

typedef struct {
    char name[64];
    char public_key[45];
    char private_key[45];
    char assigned_ip[16];
    time_t created_time;
    int enabled;
} VPNClient;

static VPNClient clients[32];
static int client_count = 0;

void load_clients() {
    client_count = 0;
    FILE *fp = fopen(VPN_CLIENT_FILE, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp) && client_count < 32) {
        char name[64], key[45], ip[16];
        time_t created;
        int enabled;

        if (sscanf(line, "%63[^:]:%44[^:]:%15[^:]:%ld:%d", name, key, ip, &created, &enabled) == 5) {
            strcpy(clients[client_count].name, name);
            strcpy(clients[client_count].public_key, key);
            strcpy(clients[client_count].assigned_ip, ip);
            clients[client_count].created_time = created;
            clients[client_count].enabled = enabled;
            client_count++;
        }
    }
    fclose(fp);
}

void save_clients() {
    FILE *fp = fopen(VPN_CLIENT_FILE, "w");
    if (!fp) return;

    for (int i = 0; i < client_count; i++) {
        fprintf(fp, "%s:%s:%s:%ld:%d\n",
                clients[i].name,
                clients[i].public_key,
                clients[i].assigned_ip,
                (long)clients[i].created_time,
                clients[i].enabled);
    }
    fclose(fp);
}

int find_free_ip() {
    int used_ips[256] = {0};

    FILE *fp = fopen(VPN_CLIENT_FILE, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char ip[16];
            if (sscanf(line, "%*[^:]:%*[^:]:%15[^:]", ip) == 1) {
                int num = atoi(strrchr(ip, '.') + 1);
                if (num >= 2 && num < 256) used_ips[num] = 1;
            }
        }
        fclose(fp);
    }

    for (int i = 2; i < 256; i++) {
        if (!used_ips[i]) return i;
    }
    return 2;
}

void add_client(const char *name, const char *public_key, const char *private_key) {
    load_clients();

    if (client_count >= 32) {
        printf("{\"success\": false, \"message\": \"客户端数量已达上限\"}");
        return;
    }

    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, name) == 0) {
            printf("{\"success\": false, \"message\": \"客户端名称已存在\"}");
            return;
        }
    }

    VPNClient *c = &clients[client_count];
    strncpy(c->name, name, sizeof(c->name) - 1);
    strncpy(c->public_key, public_key, sizeof(c->public_key) - 1);
    strncpy(c->private_key, private_key, sizeof(c->private_key) - 1);

    int ip_num = find_free_ip();
    snprintf(c->assigned_ip, sizeof(c->assigned_ip), "10.0.0.%d", ip_num);
    c->created_time = time(NULL);
    c->enabled = 1;

    client_count++;
    save_clients();

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wg set wg0 peer %s allowed-ips %s/32", public_key, c->assigned_ip);
    system(cmd);

    printf("{\"success\": true, \"message\": \"客户端 %s 已添加\", \"client\": {\"name\": \"%s\", \"public_key\": \"%s\", \"assigned_ip\": \"%s\"}}",
           name, name, public_key, c->assigned_ip);
}

void remove_client(const char *public_key) {
    load_clients();

    int found = -1;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].public_key, public_key) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        printf("{\"success\": false, \"message\": \"客户端不存在\"}");
        return;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wg set wg0 peer %s remove 2>/dev/null", public_key);
    system(cmd);

    for (int i = found; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }
    client_count--;

    save_clients();
    printf("{\"success\": true, \"message\": \"客户端已移除\"}");
}

void toggle_client(const char *public_key) {
    load_clients();

    int found = -1;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].public_key, public_key) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        printf("{\"success\": false, \"message\": \"客户端不存在\"}");
        return;
    }

    clients[found].enabled = !clients[found].enabled;

    char cmd[512];
    if (clients[found].enabled) {
        snprintf(cmd, sizeof(cmd), "wg set wg0 peer %s allowed-ips %s/32", public_key, clients[found].assigned_ip);
    } else {
        snprintf(cmd, sizeof(cmd), "wg set wg0 peer %s allowed-ips ::/0 2>/dev/null || wg set wg0 peer %s remove", public_key, public_key);
    }
    system(cmd);

    save_clients();
    printf("{\"success\": true, \"message\": \"客户端状态已更新\"}");
}

void get_clients() {
    load_clients();

    printf("{\"success\": true, \"clients\": [");
    for (int i = 0; i < client_count; i++) {
        if (i > 0) printf(",");
        printf("{\"name\": \"%s\", \"public_key\": \"%s\", \"assigned_ip\": \"%s\", \"created\": %ld, \"enabled\": %d}",
               clients[i].name,
               clients[i].public_key,
               clients[i].assigned_ip,
               (long)clients[i].created_time,
               clients[i].enabled);
    }
    printf("], \"count\": %d}", client_count);
}

void get_client_config(const char *public_key) {
    load_clients();

    int found = -1;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].public_key, public_key) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        printf("{\"success\": false, \"message\": \"客户端不存在\"}");
        return;
    }

    VPNClient *c = &clients[found];

    char server_pub[45] = "";
    char server_port[8] = "51820";
    char endpoint[128] = "";

    FILE *fp = popen("sudo wg show wg0 public-key 2>/dev/null", "r");
    if (fp) {
        fgets(server_pub, sizeof(server_pub), fp);
        server_pub[strcspn(server_pub, "\n")] = '\0';
        pclose(fp);
    }

    fp = fopen("/etc/wireguard/wg0.conf", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "ListenPort")) {
                char *p = strchr(line, '=');
                if (p) {
                    strncpy(server_port, p + 1, sizeof(server_port) - 1);
                    server_port[strcspn(server_port, "\n")] = '\0';
                }
            }
            if (strstr(line, "Endpoint")) {
                char *p = strchr(line, '=');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char *end = strchr(p, '\n');
                    if (end) *end = '\0';
                    strncpy(endpoint, p, sizeof(endpoint) - 1);
                }
            }
        }
        fclose(fp);
    }

    char dns1[32] = "8.8.8.8";
    char dns2[32] = "1.1.1.1";
    fp = fopen("/etc/wireguard/wg0.conf", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "DNS")) {
                char *p = strchr(line, '=');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char *end = strchr(p, '\n');
                    if (end) *end = '\0';
                    char *comma = strchr(p, ',');
                    if (comma) {
                        strncpy(dns1, p, comma - p);
                        dns1[comma - p] = '\0';
                        strncpy(dns2, comma + 1, sizeof(dns2) - 1);
                        dns2[strcspn(dns2, " ")] = '\0';
                    } else {
                        strncpy(dns1, p, sizeof(dns1) - 1);
                        dns1[strcspn(dns1, " ")] = '\0';
                    }
                }
            }
        }
        fclose(fp);
    }

    char config[2048];
    snprintf(config, sizeof(config),
             "[Interface]\n"
             "PrivateKey = %s\n"
             "Address = %s/24\n"
             "DNS = %s, %s\n"
             "\n"
             "[Peer]\n"
             "PublicKey = %s\n"
             "Endpoint = %s\n"
             "AllowedIPs = 0.0.0.0/0, ::/0\n"
             "PersistentKeepalive = 25\n",
             c->private_key,
             c->assigned_ip,
             dns1, dns2,
             server_pub,
             endpoint);

    printf("{\"success\": true, \"config\": \"%s\", \"name\": \"%s\", \"assigned_ip\": \"%s\"}",
           config, c->name, c->assigned_ip);
}

void get_server_config() {
    char server_pub[45] = "";
    char server_port[8] = "51820";
    char endpoint[64] = "";
    char allowed_ips[64] = "";
    char dns1[32] = "", dns2[32] = "";

    FILE *fp = popen("sudo wg show wg0 public-key 2>/dev/null", "r");
    if (fp) {
        fgets(server_pub, sizeof(server_pub), fp);
        server_pub[strcspn(server_pub, "\n")] = '\0';
        pclose(fp);
    }

    fp = fopen("/etc/wireguard/wg0.conf", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "ListenPort")) {
                char *p = strchr(line, '=');
                if (p) strncpy(server_port, p + 1, sizeof(server_port) - 1);
                server_port[strcspn(server_port, "\n")] = '\0';
            }
            if (strstr(line, "Endpoint")) {
                char *p = strchr(line, '=');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char *end = strchr(p, ':');
                    if (end) {
                        strncpy(endpoint, p, end - p);
                        endpoint[end - p] = '\0';
                    }
                }
            }
            if (strstr(line, "AllowedIPs")) {
                char *p = strchr(line, '=');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char *end = strchr(p, '\n');
                    if (end) *end = '\0';
                    strncpy(allowed_ips, p, sizeof(allowed_ips) - 1);
                }
            }
            if (strstr(line, "DNS")) {
                char *p = strchr(line, '=');
                if (p) {
                    p++;
                    while (*p == ' ') p++;
                    char *end = strchr(p, '\n');
                    if (end) *end = '\0';
                    char *comma = strchr(p, ',');
                    if (comma) {
                        strncpy(dns1, p, comma - p);
                        dns1[comma - p] = '\0';
                        strncpy(dns2, comma + 1, sizeof(dns2) - 1);
                        dns2[strcspn(dns2, " ")] = '\0';
                    } else {
                        strncpy(dns1, p, sizeof(dns1) - 1);
                        dns1[strcspn(dns1, " ")] = '\0';
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("{\"success\": true, \"server\": {\"public_key\": \"%s\", \"port\": \"%s\", \"endpoint\": \"%s\", \"allowed_ips\": \"%s\", \"dns\": \"%s, %s\"}}",
           server_pub, server_port, endpoint, allowed_ips, dns1, dns2);
}

void start_vpn() {
    system("wg-quick up wg0");
    printf("{\"success\": true, \"message\": \"VPN服务已启动\"}");
}

void stop_vpn() {
    system("wg-quick down wg0");
    printf("{\"success\": true, \"message\": \"VPN服务已停止\"}");
}

int main() {
    srand(time(NULL));

    char *action = get_param("action");

    printf("Content-Type: application/json\r\n");
    printf("\r\n");

    if (!action) {
        get_vpn_status();
        return 0;
    }

    if (strcmp(action, "status") == 0) {
        get_vpn_status();
    } else if (strcmp(action, "start") == 0) {
        start_vpn();
    } else if (strcmp(action, "stop") == 0) {
        stop_vpn();
    } else if (strcmp(action, "add_client") == 0) {
        char *name = get_param("name");
        char *pub_key = get_param("public_key");
        char *priv_key = get_param("private_key");
        if (name && pub_key && priv_key) {
            add_client(name, pub_key, priv_key);
        } else {
            printf("{\"success\": false, \"message\": \"缺少参数\"}");
        }
    } else if (strcmp(action, "remove_client") == 0) {
        char *key = get_param("public_key");
        if (key) {
            remove_client(key);
        } else {
            printf("{\"success\": false, \"message\": \"缺少参数\"}");
        }
    } else if (strcmp(action, "toggle_client") == 0) {
        char *key = get_param("public_key");
        if (key) {
            toggle_client(key);
        } else {
            printf("{\"success\": false, \"message\": \"缺少参数\"}");
        }
    } else if (strcmp(action, "clients") == 0) {
        get_clients();
    } else if (strcmp(action, "generate_key") == 0) {
        char priv[45], pub[45];
        generate_key_pair(priv, pub);
        printf("{\"private_key\": \"%s\", \"public_key\": \"%s\"}", priv, pub);
    } else if (strcmp(action, "get_config") == 0) {
        char *key = get_param("public_key");
        if (key) {
            get_client_config(key);
        } else {
            printf("{\"success\": false, \"message\": \"缺少参数\"}");
        }
    } else if (strcmp(action, "server_config") == 0) {
        get_server_config();
    } else {
        printf("{\"error\": \"Unknown action\"}");
    }

    return 0;
}