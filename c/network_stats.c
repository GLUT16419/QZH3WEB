#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

typedef struct {
    char status[32];
    char rx[64];
    char tx[64];
    unsigned long rx_bytes;
    unsigned long tx_bytes;
} net_info;

void format_bytes(unsigned long bytes, char *result, int size) {
    if (bytes >= 1073741824) {
        snprintf(result, size, "%.1f GB", bytes / 1073741824.0);
    } else if (bytes >= 1048576) {
        snprintf(result, size, "%.1f MB", bytes / 1048576.0);
    } else if (bytes >= 1024) {
        snprintf(result, size, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(result, size, "%lu B", bytes);
    }
}

int get_interface_stats(const char *iface, net_info *info) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return -1;

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, iface)) {
            unsigned long rx_bytes, tx_bytes;
            if (sscanf(line + strcspn(line, ":"), ":%lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %lu",
                       &rx_bytes, &tx_bytes) == 2) {
                info->rx_bytes = rx_bytes;
                info->tx_bytes = tx_bytes;
                format_bytes(rx_bytes, info->rx, sizeof(info->rx));
                format_bytes(tx_bytes, info->tx, sizeof(info->tx));
                strcpy(info->status, "已连接");
                found = 1;
            }
            break;
        }
    }
    fclose(fp);

    if (!found) {
        strcpy(info->status, "未连接");
        strcpy(info->rx, "0");
        strcpy(info->tx, "0");
        info->rx_bytes = 0;
        info->tx_bytes = 0;
    }

    return found ? 0 : -1;
}

int check_interface_exists(const char *iface) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return 0;

    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    int result = ioctl(s, SIOCGIFFLAGS, &ifr) == 0 && (ifr.ifr_flags & IFF_UP);
    close(s);
    return result;
}

int main() {
    net_info wan, wifi;

    if (check_interface_exists("eth0")) {
        get_interface_stats("eth0", &wan);
    } else {
        strcpy(wan.status, "未连接");
        strcpy(wan.rx, "0");
        strcpy(wan.tx, "0");
    }

    if (check_interface_exists("wlan0")) {
        get_interface_stats("wlan0", &wifi);
    } else {
        strcpy(wifi.status, "未连接");
        strcpy(wifi.rx, "0");
        strcpy(wifi.tx, "0");
    }

    printf("Content-Type: application/json\r\n");
    printf("Cache-Control: no-cache\r\n");
    printf("\r\n");
    printf("{\"wan\": {\"status\": \"%s\", \"rx\": \"%s\", \"tx\": \"%s\"}, "
           "\"wifi\": {\"status\": \"%s\", \"rx\": \"%s\", \"tx\": \"%s\"}}",
           wan.status, wan.rx, wan.tx, wifi.status, wifi.rx, wifi.tx);

    return 0;
}
