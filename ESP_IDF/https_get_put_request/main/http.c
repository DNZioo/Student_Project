#include "http.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>

// Certificate for HTTPS connection
extern const uint8_t certificate_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t certificate_pem_end[] asm("_binary_certificate_pem_end");

static const char *TAG_HTTP = "HTTP_CLIENT";

/* Callback or event handler */
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG_HTTP, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG_HTTP, "HTTP_EVENT_ON_CONNECTED");
        break;

    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG_HTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

        if (evt->data_len > MAX_BUFFER_SIZE)
            return ESP_FAIL;

        if (evt->user_data)
        {
            memcpy(evt->user_data, evt->data, evt->data_len);
            ((char *)evt->user_data)[evt->data_len] = '\0'; // Ensure null termination
        }
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_DISCONNECTED");
        break;

    default:
        break;
    }
    return ESP_OK;
}

/* HTTP GET request */
esp_err_t http_client_get_req(char *data, const char *url)
{
    esp_http_client_config_t config = {
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_GET,
        .url = url,
        .cert_pem = (const char *)certificate_pem_start,
        .user_data = data
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    esp_err_t ret_code = ESP_FAIL;

    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        if (status == 200)
        {
            ESP_LOGI(TAG_HTTP, "HTTP GET status: %d", status);
            ret_code = ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG_HTTP, "HTTP GET status: %d", status);
        }
    }
    else
    {
        ESP_LOGE(TAG_HTTP, "Failed to send GET request");
    }

    esp_http_client_cleanup(client);
    return ret_code;
}

/* HTTP PUT request */
esp_err_t http_client_post_req(char *data, const char *url)
{
    esp_http_client_config_t config = {
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_PUT,
        .url = url,
        .cert_pem = (const char *)certificate_pem_start,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data, strlen(data));

    esp_err_t err = esp_http_client_perform(client);
    esp_err_t ret_code = ESP_FAIL;

    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        if (status == 200)
        {
            ESP_LOGI(TAG_HTTP, "HTTP PUT status: %d", status);
            ret_code = ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG_HTTP, "HTTP PUT status: %d", status);
        }
    }
    else
    {
        ESP_LOGE(TAG_HTTP, "Failed to send PUT request");
    }

    esp_http_client_cleanup(client);
    return ret_code;
}
