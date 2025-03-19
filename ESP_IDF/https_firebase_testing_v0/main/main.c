#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "dht.h"
#include "bh1750.h"

// --- Constants and Definitions ---
#define WIFI_SSID "DNZio"
#define WIFI_PASS "11112222"
#define I2C_SDA GPIO_NUM_21
#define I2C_SCK GPIO_NUM_22
#define SENSOR_TYPE DHT_TYPE_AM2301
#define CONFIG_DATA_GPIO GPIO_NUM_4
#define WIFI_CONNECTED_BIT BIT0
#define LED_GPIO GPIO_NUM_2
// Firebase URLs
#define FIREBASE_DHT_URL "https://https-start-617d7-default-rtdb.firebaseio.com/sensor_data.json"
#define FIREBASE_LIGHT_URL "https://https-start-617d7-default-rtdb.firebaseio.com/Light_data.json"
#define FIREBASE_TEST_URL "https://https-start-617d7-default-rtdb.firebaseio.com/test1.json"

// External Certificates
extern const uint8_t certificate_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t certificate_pem_end[] asm("_binary_certificate_pem_end");

// Event Groups and Tags
static EventGroupHandle_t wifi_event_group;
static const char *TAG_WIFI = "WiFi";
static const char *TAG_HTTP = "HTTP_CLIENT";
static const char *TAG_DHT = "DHT_SENSOR";
static const char *TAG_BH1750 = "BH1750_SENSOR";

// --- Function Prototypes ---
void wifi_init(void);
void dht_task(void *params);
void bh1750_task(void *params);
void perform_https_post(void);

// --- Wi-Fi Event Handler ---
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG_WIFI, "Connecting to Wi-Fi...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGE(TAG_WIFI, "Disconnected. Reconnecting...");
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

// --- Initialize Wi-Fi ---
void wifi_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_event_group = xEventGroupCreate();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG_WIFI, "Wi-Fi connected.");
}

// --- HTTPS Event Handler ---
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG_HTTP, "HTTP_EVENT_ON_DATA: %.*s", evt->data_len, (char *)evt->data);
            break;
        default:
            break;
    }
    return ESP_OK;
}

// --- DHT Sensor Task ---
void dht_task(void *params) {
    float temp, humidity;

    while (1) {
        if (dht_read_float_data(SENSOR_TYPE, CONFIG_DATA_GPIO, &humidity, &temp) == ESP_OK) {
            ESP_LOGI(TAG_DHT, "Humidity: %.1f%%, Temp: %.1fC", humidity, temp);

            // Create JSON payload
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "temperature", temp);
            cJSON_AddNumberToObject(json, "humidity", humidity);
            const char *data = cJSON_Print(json);

            // HTTP client configuration
            esp_http_client_config_t config = {
                .url = FIREBASE_DHT_URL,
                .cert_pem = (const char *)certificate_pem_start,
                .event_handler = http_event_handler
            };

            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_http_client_set_method(client, HTTP_METHOD_PUT);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, data, strlen(data));

            if (esp_http_client_perform(client) == ESP_OK) {
                ESP_LOGI(TAG_HTTP, "DHT data uploaded successfully.");
            } else {
                ESP_LOGE(TAG_HTTP, "Failed to upload DHT data.");
            }

            cJSON_Delete(json);
            esp_http_client_cleanup(client);
        } else {
            ESP_LOGE(TAG_DHT, "Failed to read DHT sensor.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- BH1750 Task ---
void bh1750_task(void *params) {
    i2c_dev_t dev = { 0 };
    if (bh1750_init_desc(&dev, BH1750_ADDR_LO, 0, I2C_SDA, I2C_SCK) != ESP_OK || 
        bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH) != ESP_OK) {
        ESP_LOGE(TAG_BH1750, "Failed to initialize BH1750.");
        vTaskDelete(NULL);
    }

    while (1) {
        uint16_t lux;
        if (bh1750_read(&dev, &lux) == ESP_OK) {
            ESP_LOGI(TAG_BH1750, "Light Intensity: %d lux", lux);

            // Create JSON payload
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "light_intensity", lux);
            const char *data = cJSON_Print(json);

            esp_http_client_config_t config = {
                .url = FIREBASE_LIGHT_URL,
                .cert_pem = (const char *)certificate_pem_start,
                .event_handler = http_event_handler
            };

            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_http_client_set_method(client, HTTP_METHOD_PUT);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, data, strlen(data));

            if (esp_http_client_perform(client) == ESP_OK) {
                ESP_LOGI(TAG_HTTP, "BH1750 data uploaded successfully.");
            } else {
                ESP_LOGE(TAG_HTTP, "Failed to upload BH1750 data.");
            }

            cJSON_Delete(json);
            esp_http_client_cleanup(client);
        } else {
            ESP_LOGE(TAG_BH1750, "Failed to read BH1750.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
//get request
void firebase_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG_HTTP, "Fetching data from Firebase...");
        
        // HTTP client configuration
        esp_http_client_config_t config = {
            .url = "https://https-start-617d7-default-rtdb.firebaseio.com/led_control.json", // Firebase URL
            .cert_pem = (const char *)certificate_pem_start, // Use your certificate
            .event_handler = http_event_handler // Event handler
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_method(client, HTTP_METHOD_GET);

        // Perform the GET request
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG_HTTP, "HTTP GET Status = %d", status_code);

            if (status_code == 200) {
                char response_buffer[512];
                int data_read = esp_http_client_read(client, response_buffer, sizeof(response_buffer) - 1);
                if (data_read > 0) {
                    response_buffer[data_read] = '\0';
                    ESP_LOGI(TAG_HTTP, "Firebase Response: %s", response_buffer);
                } else {
                    ESP_LOGE(TAG_HTTP, "Failed to read HTTP response.");
                }
            } else {
                ESP_LOGE(TAG_HTTP, "Unexpected HTTP response code.");
            }
        } else {
            ESP_LOGE(TAG_HTTP, "HTTP GET request failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        // Delay before the next fetch (e.g., 5 seconds)
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG_WIFI, "Starting application...");
    wifi_init();
    if (i2cdev_init() != ESP_OK) {
        ESP_LOGE(TAG_WIFI, "Failed to initialize I2C.");
        return;
    }
    xTaskCreatePinnedToCore(dht_task, "DHT Task", 4096, NULL, 5, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(bh1750_task, "BH1750 Task", 4096, NULL, 5, NULL, APP_CPU_NUM);
    xTaskCreate(firebase_task, "GET DATA", configMINIMAL_STACK_SIZE + 2048, NULL, 1,NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
    //perform_https_post();
}
