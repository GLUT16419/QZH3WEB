#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DMZ_CONFIG_FILE "/etc/dmz.conf"

char *get_param(const char *name) {
    char *method = getenv("REQUEST_METHOD");
    static char result[256];
    static char post_data[1024];
    static int data_loaded = 0;
    result[0] = '\0';

    if (strcmp(method, "POST") == 0) {
        if (!data_loaded) {
            char *len_str = getenv("CONTENT_LENGTH");
            if (len_str) {
                int len = atoi(len_str);
                if (len > 0 && len < 1024) {
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
            while (*p && *p != '&' && *p != '\n' && *p != '\r' && i < 255) {
                result[i++] = *p++;
            }
            result[i] = '\0';
        } else {
            return NULL;
        }
    } else if (strcmp(method, "GET") == 0) {
        char *query = getenv("QUERY_STRING");
        if (query) {
            char *p = strstr(query, name);
            if (p) {
                p += strlen(name) + 1;
                int i = 0;
                while (*p && *p != '&' && i < 255) {
                    result[i++] = *p++;
                }
                result[i] = '\0';
            } else {
                return NULL;
            }
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
    return result;
}

char *url_decode(const char *src) {
    if (!src) return NULL;
    static char dest[256];
    char *d = dest;

    while (*src && *src != '&') {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *d++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *d++ = ' ';
            src++;
        } else {
            *d++ = *src++;
        }
    }
    *d = '\0';
    return dest;
}

void apply_dmz_rules(const char *ip) {
    system("iptables -t nat -F PREROUTING 2>/dev/null");
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "iptables -t nat -A PREROUTING -i eth0 -j DNAT --to-destination %s", ip);
    system(cmd);
    system("iptables -A FORWARD -i eth0 -j ACCEPT");
}

void disable_dmz(void) {
    system("iptables -t nat -F PREROUTING 2>/dev/null");
}

int save_config(const char *enabled, const char *ip) {
    FILE *fp = fopen(DMZ_CONFIG_FILE, "w");
    if (!fp) return 0;
    fprintf(fp, "enabled=%s\nip=%s\n", enabled, ip);
    fclose(fp);
    return 1;
}

int load_config(char *enabled, char *ip, int size) {
    FILE *fp = fopen(DMZ_CONFIG_FILE, "r");
    if (!fp) {
        strcpy(enabled, "0");
        strcpy(ip, "");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "enabled=") == line) {
            sscanf(line, "enabled=%s", enabled);
        } else if (strstr(line, "ip=") == line) {
            sscanf(line, "ip=%s", ip);
        }
    }
    fclose(fp);
    return 1;
}

int main() {
    char *action = get_param("action");
    char enabled[2] = "0";
    char ip[64] = "";

    if (!action) {
        load_config(enabled, ip, sizeof(ip));
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"enabled\": \"%s\", \"ip\": \"%s\"}", enabled, ip);
        return 0;
    }

    if (strcmp(action, "enable") == 0) {
        char *ip_param = url_decode(get_param("ip"));
        if (ip_param && strlen(ip_param) > 0) {
            apply_dmz_rules(ip_param);
            save_config("1", ip_param);
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": true, \"message\": \"DMZ enabled for %s\"}", ip_param);
        } else {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": false, \"message\": \"Invalid IP address\"}");
        }
    } else if (strcmp(action, "disable") == 0) {
        disable_dmz();
        save_config("0", "");
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"success\": true, \"message\": \"DMZ disabled\"}");
    } else {
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"error\": \"Unknown action\"}");
    }

    return 0;
}
