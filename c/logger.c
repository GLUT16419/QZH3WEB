#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_FILE "/var/log/router_operations.log"
#define MAX_LOG_ENTRIES 5000

// 获取参数
char* get_param(const char* name) {
    char* method = getenv("REQUEST_METHOD");
    static char result[1024];
    static char post_data[8192];
    static int data_loaded = 0;
    result[0] = '\0';

    if (!method) return NULL;

    if (strcmp(method, "POST") == 0) {
        if (!data_loaded) {
            char* len_str = getenv("CONTENT_LENGTH");
            if (len_str) {
                int len = atoi(len_str);
                if (len > 0 && len < 8192) {
                    fread(post_data, 1, len, stdin);
                    post_data[len] = '\0';
                }
            }
            data_loaded = 1;
        }

        char* p = strstr(post_data, name);
        if (p) {
            p += strlen(name) + 1;
            int i = 0;
            while (*p && *p != '&' && *p != '\n' && *p != '\r' && i < 1023) {
                result[i++] = *p++;
            }
            result[i] = '\0';
            return result;
        }
        return NULL;
    } else if (strcmp(method, "GET") == 0) {
        char* query = getenv("QUERY_STRING");
        if (query) {
            char* p = strstr(query, name);
            if (p) {
                p += strlen(name) + 1;
                int i = 0;
                while (*p && *p != '&' && i < 1023) {
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

// URL解码
void url_decode(char* dest, const char* src, size_t max_len) {
    size_t i = 0, j = 0;
    while (src[i] && j < max_len - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dest[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dest[j++] = ' ';
            i++;
        } else {
            dest[j++] = src[i++];
        }
    }
    dest[j] = '\0';
}

// 写入日志
void write_log(const char* module, const char* action, const char* details, const char* username) {
    // 确保日志目录存在
    FILE* fp = fopen(LOG_FILE, "a");
    if (!fp) {
        mkdir("/var/log", 0755);
        fp = fopen(LOG_FILE, "a");
    }
    
    if (!fp) return;

    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
    
    // 获取客户端IP
    const char* client_ip = getenv("REMOTE_ADDR") ?: "unknown";
    
    fprintf(fp, "[%s] [%s] [%s] [%s] %s\n", 
            time_str, username, module, action, details);
    fclose(fp);
}

// 读取日志
void read_logs(int limit, const char* filter) {
    FILE* fp = fopen(LOG_FILE, "r");
    if (!fp) {
        printf("{\"success\": true, \"logs\": []}");
        return;
    }

    // 反向读取，获取最新的日志
    char** lines = (char**)malloc(MAX_LOG_ENTRIES * sizeof(char*));
    int count = 0;
    char line[1024];
    
    while (fgets(line, sizeof(line), fp) && count < MAX_LOG_ENTRIES) {
        line[strcspn(line, "\n")] = '\0';
        line[strcspn(line, "\r")] = '\0';
        
        // 过滤
        if (filter && strlen(filter) > 0 && strstr(line, filter) == NULL) {
            continue;
        }
        
        lines[count] = strdup(line);
        count++;
    }
    fclose(fp);

    printf("{\"success\": true, \"logs\": [");
    int start = count > limit ? count - limit : 0;
    int printed = 0;
    
    for (int i = count - 1; i >= start; i--) {
        if (printed > 0) printf(",");
        
        // 解析日志格式: [时间] [用户] [模块] [动作] 详情
        char* log_line = lines[i];
        char time_part[64] = "", user_part[64] = "", module_part[64] = "", action_part[64] = "", details[512] = "";
        
        char* p = log_line;
        if (*p == '[') p++;
        char* end = strchr(p, ']');
        if (end) {
            strncpy(time_part, p, end - p);
            p = end + 1;
        }
        
        while (*p == ' ') p++;
        if (*p == '[') p++;
        end = strchr(p, ']');
        if (end) {
            strncpy(user_part, p, end - p);
            p = end + 1;
        }
        
        while (*p == ' ') p++;
        if (*p == '[') p++;
        end = strchr(p, ']');
        if (end) {
            strncpy(module_part, p, end - p);
            p = end + 1;
        }
        
        while (*p == ' ') p++;
        if (*p == '[') p++;
        end = strchr(p, ']');
        if (end) {
            strncpy(action_part, p, end - p);
            p = end + 1;
        }
        
        while (*p == ' ') p++;
        strncpy(details, p, sizeof(details) - 1);
        
        printf("{\"time\": \"%s\", \"user\": \"%s\", \"module\": \"%s\", \"action\": \"%s\", \"details\": \"%s\"}",
               time_part, user_part, module_part, action_part, details);
        printed++;
    }
    
    printf("], \"total\": %d}", count);

    // 释放内存
    for (int i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

// 清空日志
void clear_logs() {
    FILE* fp = fopen(LOG_FILE, "w");
    if (fp) {
        fclose(fp);
        printf("{\"success\": true, \"message\": \"操作日志已清空\"}");
    } else {
        printf("{\"success\": false, \"message\": \"清空日志失败\"}");
    }
}

int main() {
    printf("Content-Type: application/json\r\n\r\n");

    char* action = get_param("action");

    if (!action) {
        read_logs(100, NULL);
        return 0;
    }

    if (strcmp(action, "add") == 0) {
        char* module = get_param("module");
        char* act = get_param("act");
        char* details = get_param("details");
        char* username = get_param("user");
        
        char decoded_module[64], decoded_act[64], decoded_details[512], decoded_user[64];
        url_decode(decoded_module, module ?: "unknown", sizeof(decoded_module));
        url_decode(decoded_act, act ?: "unknown", sizeof(decoded_act));
        url_decode(decoded_details, details ?: "", sizeof(decoded_details));
        url_decode(decoded_user, username ?: "anonymous", sizeof(decoded_user));
        
        write_log(decoded_module, decoded_act, decoded_details, decoded_user);
        printf("{\"success\": true, \"message\": \"日志已记录\"}");
    } else if (strcmp(action, "list") == 0) {
        char* limit_str = get_param("limit");
        char* filter = get_param("filter");
        int limit = limit_str ? atoi(limit_str) : 100;
        read_logs(limit, filter);
    } else if (strcmp(action, "clear") == 0) {
        clear_logs();
    } else {
        printf("{\"success\": false, \"message\": \"未知操作\"}");
    }

    return 0;
}
