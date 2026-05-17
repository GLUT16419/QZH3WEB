#ifndef CGI_UTILS_H
#define CGI_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void cgi_init(void);
void cgi_set_content_type(const char *type);
void cgi_set_cache_control(int seconds);
void cgi_send_json(const char *json);
void cgi_send_error(int code, const char *message);
char *cgi_get_query(void);
char *cgi_get_post_data(void);
char *cgi_param(const char *name);
char *url_decode(const char *src);
void cgi_free memory(char *ptr);

#endif
