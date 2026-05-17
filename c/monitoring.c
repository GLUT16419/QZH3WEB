#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ALERT_CONFIG_FILE "/etc/monitor.conf"
#define ALERT_LOG_FILE "/var/log/alerts.log"

typedef struct {
    int cpu_threshold;
    int mem_threshold;
    int temp_threshold;
    int enabled;
} alert_config_t;

alert_config_t config = {80, 90, 70, 1};

int read_cpu_usage(void) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) return 0;
    float load;
    int core_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (fscanf(fp, "%f", &load) == 1) {
        fclose(fp);
        return (int)(load * 100 / core_count);
    }
    fclose(fp);
    return 0;
}

int read_mem_usage(void) {
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
    return (total > 0) ? (int)(used * 100 / total) : 0;
}

int read_temperature(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return 0;
    int temp;
    if (fscanf(fp, "%d", &temp) == 1) {
        fclose(fp);
        return temp / 1000;
    }
    fclose(fp);
    return 0;
}

void log_alert(const char *type, int value, int threshold) {
    FILE *fp = fopen(ALERT_LOG_FILE, "a");
    if (!fp) return;

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';

    fprintf(fp, "[%s] ALERT: %s %d%% exceeds threshold %d%%\n",
            time_str, type, value, threshold);
    fclose(fp);
}

void check_alerts(void) {
    int cpu = read_cpu_usage();
    int mem = read_mem_usage();
    int temp = read_temperature();

    if (config.enabled) {
        if (cpu > config.cpu_threshold) {
            log_alert("CPU", cpu, config.cpu_threshold);
        }
        if (mem > config.mem_threshold) {
            log_alert("Memory", mem, config.mem_threshold);
        }
        if (temp > config.temp_threshold) {
            log_alert("Temperature", temp, config.temp_threshold);
        }
    }
}

char *get_param(const char *name) {
    char *method = getenv("REQUEST_METHOD");
    static char result[256];
    result[0] = '\0';

    if (strcmp(method, "POST") == 0) {
        char *len_str = getenv("CONTENT_LENGTH");
        if (len_str) {
            int len = atoi(len_str);
            char *data = malloc(len + 1);
            if (data) {
                fgets(data, len + 1, stdin);
                char *p = strstr(data, name);
                if (p) {
                    p += strlen(name) + 1;
                    int i = 0;
                    while (*p && *p != '&' && i < 255) {
                        result[i++] = *p++;
                    }
                    result[i] = '\0';
                }
                free(data);
            }
        }
    }
    return result;
}

int save_config(void) {
    FILE *fp = fopen(ALERT_CONFIG_FILE, "w");
    if (!fp) return 0;
    fprintf(fp, "cpu_threshold=%d\nmem_threshold=%d\ntemp_threshold=%d\nenabled=%d\n",
            config.cpu_threshold, config.mem_threshold, config.temp_threshold, config.enabled);
    fclose(fp);
    return 1;
}

int load_config(void) {
    FILE *fp = fopen(ALERT_CONFIG_FILE, "r");
    if (!fp) return 0;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "cpu_threshold=%d", &config.cpu_threshold);
        sscanf(line, "mem_threshold=%d", &config.mem_threshold);
        sscanf(line, "temp_threshold=%d", &config.temp_threshold);
        sscanf(line, "enabled=%d", &config.enabled);
    }
    fclose(fp);
    return 1;
}

int main() {
    char *action = get_param("action");

    if (!action || strlen(action) == 0) {
        int cpu = read_cpu_usage();
        int mem = read_mem_usage();
        int temp = read_temperature();
        load_config();

        printf("Content-Type: application/json\r\n");
        printf("Cache-Control: no-cache\r\n");
        printf("\r\n");
        printf("{\"cpu\": %d, \"mem\": %d, \"temp\": %d, \"config\": {\"cpu_th\": %d, \"mem_th\": %d, \"temp_th\": %d, \"enabled\": %d}}",
               cpu, mem, temp, config.cpu_threshold, config.mem_threshold, config.temp_threshold, config.enabled);
        return 0;
    }

    if (strcmp(action, "check") == 0) {
        check_alerts();
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"success\": true, \"message\": \"Alerts checked\"}");

    } else if (strcmp(action, "config") == 0) {
        char *cpu_str = get_param("cpu_threshold");
        char *mem_str = get_param("mem_threshold");
        char *temp_str = get_param("temp_threshold");
        char *enabled_str = get_param("enabled");

        if (cpu_str) config.cpu_threshold = atoi(cpu_str);
        if (mem_str) config.mem_threshold = atoi(mem_str);
        if (temp_str) config.temp_threshold = atoi(temp_str);
        if (enabled_str) config.enabled = atoi(enabled_str);

        save_config();
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"success\": true, \"message\": \"Configuration saved\"}");

    } else if (strcmp(action, "logs") == 0) {
        FILE *fp = fopen(ALERT_LOG_FILE, "r");
        printf("Content-Type: application/json\r\n");
        printf("\r\n");

        if (fp) {
            char line[512];
            printf("{\"logs\": [");
            int first = 1;
            while (fgets(line, sizeof(line), fp)) {
                if (!first) printf(", ");
                line[strcspn(line, "\n")] = '\0';
                printf("\"%s\"", line);
                first = 0;
            }
            printf("]}");
            fclose(fp);
        } else {
            printf("{\"logs\": []}");
        }

    } else {
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"error\": \"Unknown action\"}");
    }

    return 0;
}
