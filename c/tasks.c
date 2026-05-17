#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 256
#define CONFIG_FILE "/etc/cron.d/router_tasks"
#define BACKUP_DIR "/var/backups/router"

void print_json_success(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"message\": \"%s\"}\n", message);
}

void print_json_error(const char *message) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": false, \"message\": \"%s\"}\n", message);
}

void print_json_list(const char *tasks) {
    printf("Content-Type: application/json\n\n");
    printf("{\"success\": true, \"tasks\": [%s]}\n", tasks);
}

void add_cron_task(const char *minute, const char *hour, const char *day, const char *month, const char *weekday, const char *command) {
    FILE *fp = fopen(CONFIG_FILE, "a");
    if (!fp) {
        print_json_error("无法打开定时任务配置文件");
        return;
    }
    fprintf(fp, "%s %s %s %s %s root %s\n", minute, hour, day, month, weekday, command);
    fclose(fp);
    
    system("crontab /etc/cron.d/router_tasks");
    print_json_success("定时任务已添加");
}

void remove_cron_task(int line_num) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "sed -i '%dd' %s", line_num, CONFIG_FILE);
    system(cmd);
    system("crontab /etc/cron.d/router_tasks");
    print_json_success("定时任务已删除");
}

void list_cron_tasks() {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) {
        print_json_list("");
        return;
    }
    
    char line[MAX_LINE];
    char tasks[4096] = "";
    int line_num = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0 && line[0] != '#') {
            line_num++;
            if (line_num > 1) strcat(tasks, ",");
            char task[256];
            snprintf(task, sizeof(task), "{\"line\": %d, \"task\": \"%s\"}", line_num, line);
            strcat(tasks, task);
        }
    }
    fclose(fp);
    
    print_json_list(tasks);
}

void backup_configs() {
    system("mkdir -p " BACKUP_DIR);
    
    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tar -czf %s/config_backup_%s.tar.gz /etc/hostapd /etc/dnsmasq.conf /etc/lighttpd", BACKUP_DIR, timestamp);
    system(cmd);
    
    print_json_success("配置文件已备份");
}

void clean_logs() {
    system("cat /dev/null > /var/log/syslog");
    system("cat /dev/null > /var/log/auth.log");
    system("cat /dev/null > /var/log/hostapd.log");
    
    print_json_success("日志已清理");
}

void schedule_reboot(const char *hours, const char *minutes) {
    char command[128];
    snprintf(command, sizeof(command), "/sbin/reboot");
    add_cron_task(minutes, hours, "*", "*", "*", command);
}

int main(void) {
    char *action = getenv("QUERY_STRING");
    
    if (!action) {
        print_json_error("缺少操作参数");
        return 0;
    }
    
    if (strncmp(action, "action=add_task", 14) == 0) {
        char *minute = strstr(action, "minute=");
        char *hour = strstr(action, "hour=");
        char *day = strstr(action, "day=");
        char *month = strstr(action, "month=");
        char *weekday = strstr(action, "weekday=");
        char *command = strstr(action, "command=");
        
        if (!minute || !hour || !command) {
            print_json_error("参数不完整");
            return 0;
        }
        
        minute += 7; hour += 5; day += 4; month += 6; weekday += 8; command += 8;
        
        char *end;
        end = strchr(minute, '&'); if (end) *end = 0;
        end = strchr(hour, '&'); if (end) *end = 0;
        end = strchr(day, '&'); if (end) *end = 0;
        end = strchr(month, '&'); if (end) *end = 0;
        end = strchr(weekday, '&'); if (end) *end = 0;
        end = strchr(command, '&'); if (end) *end = 0;
        
        add_cron_task(minute, hour, day, month, weekday, command);
    }
    else if (strncmp(action, "action=remove_task", 17) == 0) {
        char *line_num_str = strstr(action, "line=");
        if (!line_num_str) {
            print_json_error("缺少行号参数");
            return 0;
        }
        line_num_str += 5;
        int line_num = atoi(line_num_str);
        remove_cron_task(line_num);
    }
    else if (strncmp(action, "action=list_tasks", 15) == 0) {
        list_cron_tasks();
    }
    else if (strncmp(action, "action=backup", 13) == 0) {
        backup_configs();
    }
    else if (strncmp(action, "action=clean_logs", 17) == 0) {
        clean_logs();
    }
    else if (strncmp(action, "action=schedule_reboot", 21) == 0) {
        char *hours = strstr(action, "hours=");
        char *minutes = strstr(action, "minutes=");
        
        if (!hours || !minutes) {
            print_json_error("参数不完整");
            return 0;
        }
        
        hours += 6; minutes += 8;
        
        char *end;
        end = strchr(hours, '&'); if (end) *end = 0;
        end = strchr(minutes, '&'); if (end) *end = 0;
        
        schedule_reboot(hours, minutes);
    }
    else {
        print_json_error("未知操作");
    }
    
    return 0;
}