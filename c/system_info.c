#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>

char *get_hostname() {
    static char result[256] = "N/A";
    gethostname(result, sizeof(result) - 1);
    return result;
}

char *get_os_info() {
    static char result[256] = "N/A";
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *start = strchr(line, '"');
                char *end = NULL;
                if (start) {
                    start++;
                    end = strchr(start, '"');
                    if (end) {
                        *end = '\0';
                        snprintf(result, sizeof(result), "%s", start);
                    }
                }
                break;
            }
        }
        fclose(fp);
    }
    return result;
}

char *get_kernel() {
    static char result[128] = "N/A";
    FILE *fp = fopen("/proc/version", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char *pos = strstr(line, "Linux version ");
            if (pos) {
                pos += 14;
                char *end = strchr(pos, ' ');
                if (end) {
                    *end = '\0';
                    snprintf(result, sizeof(result), "%s", pos);
                }
            }
        }
        fclose(fp);
    }
    return result;
}

void print_json_response(const char *cpu_load, const char *memory, const char *uptime, const char *temp, 
                         const char *hostname, const char *os_info, const char *kernel) {
    printf("Content-Type: application/json\r\n");
    printf("\r\n");
    printf("{\"cpu_load\": \"%s\", \"memory\": \"%s\", \"uptime\": \"%s\", \"temp\": \"%s\", "
           "\"hostname\": \"%s\", \"os_info\": \"%s\", \"kernel\": \"%s\"}",
           cpu_load, memory, uptime, temp, hostname, os_info, kernel);
}

char *get_cpu_load() {
    static char result[128];
    FILE *fp = fopen("/proc/loadavg", "r");
    if (fp) {
        float load1, load5, load15;
        fscanf(fp, "%f %f %f", &load1, &load5, &load15);
        snprintf(result, sizeof(result), "%.2f %.2f %.2f", load1, load5, load15);
        fclose(fp);
    } else {
        strcpy(result, "N/A");
    }
    return result;
}

char *get_memory() {
    static char result[256];
    FILE *fp = fopen("/proc/meminfo", "r");
    long total = 0, free = 0, buffers = 0, cached = 0;

    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
            if (sscanf(line, "MemFree: %ld kB", &free) == 1) continue;
            if (sscanf(line, "Buffers: %ld kB", &buffers) == 1) continue;
            if (sscanf(line, "Cached: %ld kB", &cached) == 1) continue;
        }
        fclose(fp);
    }

    long used = total - free - buffers - cached;
    int percent = (total > 0) ? (used * 100 / total) : 0;
    snprintf(result, sizeof(result), "%d%% (%ldMB / %ldMB)", percent, used / 1024, total / 1024);
    return result;
}

char *get_uptime() {
    static char result[64];
    struct sysinfo info;

    if (sysinfo(&info) == 0) {
        long days = info.uptime / 86400;
        long hours = (info.uptime % 86400) / 3600;
        long mins = (info.uptime % 3600) / 60;
        snprintf(result, sizeof(result), "%ld天 %ld小时 %ld分钟", days, hours, mins);
    } else {
        strcpy(result, "N/A");
    }
    return result;
}

char *get_temperature() {
    static char result[32] = "N/A";
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp) {
        int temp;
        if (fscanf(fp, "%d", &temp) == 1) {
            snprintf(result, sizeof(result), "%d°C", temp / 1000);
        }
        fclose(fp);
    }
    return result;
}

int main() {
    print_json_response(get_cpu_load(), get_memory(), get_uptime(), get_temperature(),
                       get_hostname(), get_os_info(), get_kernel());
    return 0;
}
