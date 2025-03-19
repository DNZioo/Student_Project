#ifndef HTTP_H
#define HTTP_H

#include <esp_err.h>
#include "esp_http_client.h"

// Define maximum buffer size
#define MAX_BUFFER_SIZE 256

// Callback function declaration
esp_err_t http_event_handler(esp_http_client_event_t *evt);

// HTTP GET and PUT function declarations
esp_err_t http_client_get_req(char *data, const char *url);
esp_err_t http_client_post_req(char *data, const char *url);

#endif
