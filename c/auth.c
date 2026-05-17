#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define SESSION_DIR "/tmp/sessions"
#define USER_FILE "/etc/web_users.conf"
#define MAX_USERS 10
#define SESSION_TIMEOUT 3600

typedef struct {
    char username[64];
    char password_hash[128];
    int role;
} user_t;

typedef enum { ROLE_ADMIN = 1, ROLE_USER = 0 } role_t;

user_t users[MAX_USERS];
int user_count = 0;

char *get_cookie(const char *name) {
    char *cookies = getenv("HTTP_COOKIE");
    if (!cookies) return NULL;

    static char result[256];
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s=", name);

    char *start = strstr(cookies, pattern);
    if (!start) return NULL;

    start += strlen(pattern);
    char *end = strchr(start, ';');
    if (end) {
        int len = end - start;
        if (len > 255) len = 255;
        strncpy(result, start, len);
        result[len] = '\0';
    } else {
        strncpy(result, start, 255);
        result[255] = '\0';
    }
    return result;
}

char *generate_session_id(void) {
    static char session_id[64];
    snprintf(session_id, sizeof(session_id), "%ld%ld%ld",
             (long)time(NULL), (long)getpid(), (long)rand());
    return session_id;
}

int check_session(const char *session_id) {
    if (!session_id || strlen(session_id) == 0) return 0;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, session_id);

    struct stat st;
    if (stat(path, &st) != 0) return 0;

    time_t now = time(NULL);
    if (now - st.st_mtime > SESSION_TIMEOUT) {
        unlink(path);
        return 0;
    }

    return 1;
}

int verify_user(const char *username, const char *password, user_t *user) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) {
        if (strcmp(username, "admin") == 0 && strcmp(password, "admin") == 0) {
            strcpy(user->username, "admin");
            user->role = ROLE_ADMIN;
            return 1;
        }
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char u[64], p[128];
        int r;
        if (sscanf(line, "%63[^:]:%127[^:]:%d", u, p, &r) == 3) {
            if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
                strcpy(user->username, u);
                user->role = r;
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);
    return 0;
}

void create_session(const char *session_id) {
    mkdir(SESSION_DIR, 0755);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, session_id);
    FILE *fp = fopen(path, "w");
    if (fp) fclose(fp);
}

void delete_session(const char *session_id) {
    if (!session_id) return;
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SESSION_DIR, session_id);
    unlink(path);
}

int add_user(const char *username, const char *password, int role) {
    FILE *fp = fopen(USER_FILE, "a");
    if (!fp) return 0;
    fprintf(fp, "%s:%s:%d\n", username, password, role);
    fclose(fp);
    return 1;
}

int change_password(const char *username, const char *old_pass, const char *new_pass) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) return 0;

    char temp_file[] = "/tmp/web_users_XXXXXX";
    int temp_fd = mkstemp(temp_file);
    if (temp_fd < 0) {
        fclose(fp);
        return 0;
    }
    FILE *temp_fp = fdopen(temp_fd, "w");

    char line[256];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        char u[64], p[128];
        int r;
        if (sscanf(line, "%63[^:]:%127[^:]:%d", u, p, &r) == 3) {
            if (strcmp(u, username) == 0) {
                fprintf(temp_fp, "%s:%s:%d\n", u, new_pass, r);
                found = 1;
            } else {
                fprintf(temp_fp, "%s", line);
            }
        }
    }
    fclose(fp);
    fclose(temp_fp);

    if (found) {
        rename(temp_file, USER_FILE);
        return 1;
    }
    unlink(temp_file);
    return 0;
}

char *url_decode(const char *src) {
    if (!src) return NULL;
    static char dest[1024];
    char *d = dest;

    while (*src) {
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

char *get_param(const char *name) {
    char *method = getenv("REQUEST_METHOD");
    static char result[1024];
    static char post_data[4096];
    static int data_loaded = 0;
    result[0] = '\0';

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
            while (*p && *p != '&' && *p != '\n' && *p != '\r' && i < 1023) {
                result[i++] = *p++;
            }
            result[i] = '\0';
        }
    }
    return url_decode(result);
}

int main() {
    srand(time(NULL));

    char *action = get_param("action");
    char *session_id = get_cookie("session_id");

    if (!action) {
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"error\": \"No action specified\"}");
        return 0;
    }

    if (strcmp(action, "login") == 0) {
        char *username = get_param("username");
        char *password = get_param("password");

        if (!username || !password) {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": false, \"message\": \"Missing credentials\"}");
            return 0;
        }

        user_t user;
        if (verify_user(username, password, &user)) {
            char *new_session_id = generate_session_id();
            create_session(new_session_id);

            printf("Content-Type: application/json\r\n");
            printf("Set-Cookie: session_id=%s; HttpOnly; Path=/\r\n", new_session_id);
            printf("\r\n");
            printf("{\"success\": true, \"message\": \"Login successful\", \"username\": \"%s\", \"role\": %d}",
                   user.username, user.role);
        } else {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": false, \"message\": \"Invalid credentials\"}");
        }

    } else if (strcmp(action, "check") == 0) {
        if (session_id && check_session(session_id)) {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"authenticated\": true}");
        } else {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"authenticated\": false}");
        }

    } else if (strcmp(action, "logout") == 0) {
        if (session_id) {
            delete_session(session_id);
        }
        printf("Content-Type: application/json\r\n");
        printf("Set-Cookie: session_id=; expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/\r\n");
        printf("\r\n");
        printf("{\"success\": true, \"message\": \"Logged out\"}");

    } else if (strcmp(action, "adduser") == 0) {
        if (!session_id || !check_session(session_id)) {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": false, \"message\": \"Unauthorized\"}");
            return 0;
        }

        char *username = get_param("username");
        char *password = get_param("password");
        char *role_str = get_param("role");
        int role = role_str ? atoi(role_str) : ROLE_USER;

        if (username && password) {
            if (add_user(username, password, role)) {
                printf("Content-Type: application/json\r\n");
                printf("\r\n");
                printf("{\"success\": true, \"message\": \"User added\"}");
            } else {
                printf("Content-Type: application/json\r\n");
                printf("\r\n");
                printf("{\"success\": false, \"message\": \"Failed to add user\"}");
            }
        } else {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": false, \"message\": \"Missing parameters\"}");
        }

    } else if (strcmp(action, "changepwd") == 0) {
        char *username = get_param("username");
        char *old_pass = get_param("old_password");
        char *new_pass = get_param("new_password");

        if (username && old_pass && new_pass) {
            if (change_password(username, old_pass, new_pass)) {
                printf("Content-Type: application/json\r\n");
                printf("\r\n");
                printf("{\"success\": true, \"message\": \"Password changed\"}");
            } else {
                printf("Content-Type: application/json\r\n");
                printf("\r\n");
                printf("{\"success\": false, \"message\": \"Failed to change password\"}");
            }
        } else {
            printf("Content-Type: application/json\r\n");
            printf("\r\n");
            printf("{\"success\": false, \"message\": \"Missing parameters\"}");
        }

    } else {
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        printf("{\"error\": \"Unknown action\"}");
    }

    return 0;
}
