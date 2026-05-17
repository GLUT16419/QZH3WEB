#include "cgi_utils.h"

void cgi_init(void) {
    printf("Content-Type: application/json\r\n");
    printf("\r\n");
    fflush(stdout);
}

void cgi_set_content_type(const char *type) {
    printf("Content-Type: %s\r\n", type);
}

void cgi_set_cache_control(int seconds) {
    printf("Cache-Control: max-age=%d, public\r\n", seconds);
}

void cgi_send_json(const char *json) {
    printf("%s", json);
}

void cgi_send_error(int code, const char *message) {
    printf("Status: %d %s\r\n", code, message);
    printf("Content-Type: application/json\r\n");
    printf("\r\n");
    printf("{\"error\": \"%s\"}", message);
    exit(0);
}

char *cgi_get_query(void) {
    return getenv("QUERY_STRING") ? strdup(getenv("QUERY_STRING")) : NULL;
}

char *cgi_get_post_data(void) {
    char *content_length_str = getenv("CONTENT_LENGTH");
    if (!content_length_str) return NULL;

    int length = atoi(content_length_str);
    if (length <= 0) return NULL;

    char *data = malloc(length + 1);
    if (!data) return NULL;

    fgets(data, length + 1, stdin);
    return data;
}

char *cgi_param(const char *name) {
    char *query = cgi_get_query();
    if (!query) {
        char *post = cgi_get_post_data();
        query = post;
    }
    if (!query) return NULL;

    char *result = NULL;
    char *temp = strdup(query);
    char *token = strtok(temp, "&");

    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(token, name) == 0) {
                result = url_decode(eq + 1);
                break;
            }
        }
        token = strtok(NULL, "&");
    }

    free(temp);
    return result;
}

char *url_decode(const char *src) {
    if (!src) return NULL;

    char *dest = malloc(strlen(src) + 1);
    char *d = dest;
    char hex[3];

    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            hex[0] = src[1];
            hex[1] = src[2];
            hex[2] = '\0';
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

void cgi_free_memory(char *ptr) {
    if (ptr) free(ptr);
}
