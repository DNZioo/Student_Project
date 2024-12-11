#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "driver/gpio.h"

// Define constants
#define WIFI_SSID "DNZio"
#define WIFI_PASS "11112222"
#define FIREBASE_URL "https://fir-getstart-9f542-default-rtdb.firebaseio.com/hello.json"
#define LED GPIO_NUM_2

// Certificate for HTTPS connection
extern const uint8_t certificate_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t certificate_pem_end[] asm("_binary_certificate_pem_end");

// Event group for Wi-Fi events
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Log tags
static const char *TAG_WIFI = "WiFi";
static const char *TAG_HTTP = "HTTP_CLIENT";

// Wi-Fi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG_WIFI, "WiFi connecting...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGE(TAG_WIFI, "WiFi disconnected. Reconnecting...");
                esp_wifi_connect();
                xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize Wi-Fi connection
void wifi_init() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Initialize the TCP/IP stack and Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configure Wi-Fi settings
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };

    // Register Wi-Fi event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection
    ESP_LOGI(TAG_WIFI, "WiFi initialization complete.");
    wifi_event_group = xEventGroupCreate();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

// Variables for HTTP response handling
static char *response_buffer = NULL;
static int response_len = 0;

// HTTP event handler
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!response_buffer) {
                response_buffer = malloc(evt->data_len + 1);
                memcpy(response_buffer, evt->data, evt->data_len);
                response_len = evt->data_len;
            } else {
                response_buffer = realloc(response_buffer, response_len + evt->data_len + 1);
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
            }
            response_buffer[response_len] = '\0'; // Null-terminate
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG_HTTP, "Response: %s", response_buffer);
            cJSON *json = cJSON_Parse(response_buffer);
            if (json == NULL) {
                ESP_LOGE(TAG_HTTP, "JSON parsing error: %s", cJSON_GetErrorPtr());
            } else {
                cJSON *led_status = cJSON_GetObjectItem(json, "LED");
                if (cJSON_IsNumber(led_status)) {
                    int led_value = led_status->valueint;
                    gpio_set_level(LED, led_value);
                    ESP_LOGI(TAG_HTTP, "LED status set to: %d", led_value);
                } else {
                    ESP_LOGW(TAG_HTTP, "'LED' key not found or invalid.");
                }
                cJSON_Delete(json);
            }
            free(response_buffer);
            response_buffer = NULL;
            response_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Task for real-time Firebase updates
void firebase_task(void *param) {
    esp_http_client_config_t config = {
        .url = FIREBASE_URL,
        .cert_pem = (char *)certificate_pem_start,
        .event_handler = http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Configure GPIO for LED
    gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config);

    while (1) {
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG_HTTP, "HTTP GET Status = %d, content_length = %lld", 
                     esp_http_client_get_status_code(client), 
                     esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG_HTTP, "HTTP GET request failed: %s", esp_err_to_name(err));
            gpio_set_level(LED, 0); // Turn off LED if request fails
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Fetch data every 1 second
    }
}

void app_main(void) {
    ESP_LOGI(TAG_WIFI, "Starting application...");
    wifi_init();
    xTaskCreate(firebase_task, "firebase_task", 4096, NULL, 5, NULL);
}
