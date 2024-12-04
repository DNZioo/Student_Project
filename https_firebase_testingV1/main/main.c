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
#include "cJSON.h" // JSON handling library
#include "dht.h"
#include "bh1750.h"

//BH1750 define
#define I2C_SDA GPIO_NUM_21
#define I2C_SCK GPIO_NUM_22

#define SENSOR_TYPE DHT_TYPE_AM2301
#define CONFIG_DATA_GPIO GPIO_NUM_4

// Certificates for HTTPS connection
extern const uint8_t certificate_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t certificate_pem_end[] asm("_binary_certificate_pem_end");

// Wi-Fi credentials (replace with your SSID and password)
#define WIFI_SSID "MrBlack"
#define WIFI_PASS "09072023"

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

// HTTP event handler
esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG_HTTP, "HTTP_EVENT_ON_DATA: %.*s", evt->data_len, (char *)evt->data);
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG_HTTP, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGW(TAG_HTTP, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// // Perform a GET request
// void perform_https_get() {
//     ESP_LOGI(TAG_HTTP, "Starting HTTPS GET request...");

//     esp_http_client_config_t client_config = {
//         .url = "https://https-start-617d7-default-rtdb.firebaseio.com/test.json",
//         .cert_pem = (const char *)certificate_pem_start,
//         .event_handler = http_event_handler
//     };

//     esp_http_client_handle_t client = esp_http_client_init(&client_config);

//     esp_err_t err = esp_http_client_perform(client);
//     if (err == ESP_OK) {
//         ESP_LOGI(TAG_HTTP, "HTTPS GET Status = %d, Content Length = %lld",
//                  esp_http_client_get_status_code(client),
//                  esp_http_client_get_content_length(client));
//     } else {
//         ESP_LOGE(TAG_HTTP, "HTTP GET request failed: %s", esp_err_to_name(err));
//     }

//     esp_http_client_cleanup(client);
// }

//Perform an HTTPs put dht data to firebase
void dht_firebase(void *params) {
    float Temp, Humidity;
    while (1) {
        if (dht_read_float_data(SENSOR_TYPE, CONFIG_DATA_GPIO, &Humidity, &Temp) == ESP_OK) {
            ESP_LOGI("DHT", "Humidity: %.1f%%, Temp: %.1fC", Humidity, Temp);

            // Create JSON payload
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", Temp);
            cJSON_AddNumberToObject(root, "humidity", Humidity);
            const char *put_data = cJSON_Print(root);

            // HTTP client configuration for PUT
            esp_http_client_config_t client_config = {
                .url = "https://https-start-617d7-default-rtdb.firebaseio.com/sensor_data.json",
                .cert_pem = (const char *)certificate_pem_start,
                .event_handler = http_event_handler
            };

            esp_http_client_handle_t client = esp_http_client_init(&client_config);

            // Set PUT method and data
            esp_http_client_set_method(client, HTTP_METHOD_PUT);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, put_data, strlen(put_data));

            // Perform PUT request
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                ESP_LOGI("HTTP_CLIENT", "PUT Status = %d, Content Length = %lld",
                         esp_http_client_get_status_code(client),
                         esp_http_client_get_content_length(client));
            } else {
                ESP_LOGE("HTTP_CLIENT", "PUT request failed: %s", esp_err_to_name(err));
            }

            // Clean up
            cJSON_Delete(root);
            esp_http_client_cleanup(client);
        } else {
            ESP_LOGE("DHT", "Could not read data from sensor");
        }

        // Wait for 1 second before the next reading
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//perform an HTTPS Put request
void bh1750_https_task(void *params) {
    i2c_dev_t dev;
    memset(&dev, 0, sizeof(i2c_dev_t));
    if (bh1750_init_desc(&dev, BH1750_ADDR_LO, 0, I2C_SDA, I2C_SCK) != ESP_OK) {
        ESP_LOGE("BH1750", "Failed to initialize BH1750 descriptor");
        vTaskDelete(NULL);
    }
    if (bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH) != ESP_OK) {
        ESP_LOGE("BH1750", "Failed to configure BH1750");
        vTaskDelete(NULL);
    }
    while (1) {
        uint16_t lux;
        if (bh1750_read(&dev, &lux) == ESP_OK) {
            ESP_LOGI("BH1750", "Light intensity (lux): %d", lux);

            // Create JSON payload
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "Intensity", lux);
            const char *put_data = cJSON_Print(root);

            // HTTP client configuration for PUT
            esp_http_client_config_t client_config = {
                .url = "https://https-start-617d7-default-rtdb.firebaseio.com/Light_data.json",
                .cert_pem = (const char *)certificate_pem_start,
                .event_handler = http_event_handler
            };

            esp_http_client_handle_t client = esp_http_client_init(&client_config);

            // Set PUT method and data
            esp_http_client_set_method(client, HTTP_METHOD_PUT);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, put_data, strlen(put_data));

            // Perform PUT request
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                ESP_LOGI("HTTP_CLIENT", "PUT Status = %d, Content Length = %lld",
                         esp_http_client_get_status_code(client),
                         esp_http_client_get_content_length(client));
            } else {
                ESP_LOGE("HTTP_CLIENT", "PUT request failed: %s", esp_err_to_name(err));
            }

            // Clean up
            cJSON_Delete(root);
            esp_http_client_cleanup(client);
        } else {
            ESP_LOGE("BH1750", "Failed to read data from BH1750");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// Perform an HTTPS POST request
void perform_https_post() {
    ESP_LOGI(TAG_HTTP, "Starting HTTPS POST request...");

    // JSON payload creation
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "message", "Hello, Firebase!");
    cJSON_AddNumberToObject(root, "value", 123);
    const char *post_data = cJSON_Print(root);

    esp_http_client_config_t client_config = {
        .url = "https://https-start-617d7-default-rtdb.firebaseio.com/test1.json",
        .cert_pem = (const char *)certificate_pem_start,
        .event_handler = http_event_handler
    };

    esp_http_client_handle_t client = esp_http_client_init(&client_config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_HTTP, "HTTPS POST Status = %d, Content Length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG_HTTP, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // Clean up
    cJSON_Delete(root);
    esp_http_client_cleanup(client);
}
// Main function
void app_main() {
    ESP_LOGI(TAG_WIFI, "Starting application...");
    wifi_init();         // Initialize Wi-Fi connection
    //perform_https_get(); // Perform HTTPS GET request
    perform_https_post();
    if (i2cdev_init() != ESP_OK) 
    { 
    ESP_LOGE("I2C", "Failed to initialize I2C driver"); 
    return; 
    } 
    xTaskCreate(dht_firebase, "dht_firebase_task", 4096, NULL, 5, NULL);
    xTaskCreate(bh1750_https_task, "bh1750_https_task", configMINIMAL_STACK_SIZE + 2048, NULL, 2, NULL);
}
