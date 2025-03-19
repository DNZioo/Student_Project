#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
//#include "esp_netif.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "dht.h"
#include "bh1750.h"
#include "protocol_examples_common.h"

// --- Constants and Definitions ---
#define I2C_SDA GPIO_NUM_21
#define I2C_SCK GPIO_NUM_22
#define SENSOR_TYPE DHT_TYPE_AM2301
#define CONFIG_DATA_GPIO GPIO_NUM_4
//button pins
#define BUTTON1_GPIO GPIO_NUM_2
#define BUTTON2_GPIO GPIO_NUM_19
#define BUTTON3_GPIO GPIO_NUM_23
// Firebase URLs
#define FIREBASE_DHT_URL "https://https-start-617d7-default-rtdb.firebaseio.com/sensor_data.json"
#define FIREBASE_LIGHT_URL "https://https-start-617d7-default-rtdb.firebaseio.com/Light_data.json"
#define FIREBASE_BUTTON_URL "https://https-start-617d7-default-rtdb.firebaseio.com/button_state.json"
#define MAX_BUFFER_SIZE 256
// External Certificates
extern const uint8_t certificate_pem_start[] asm("_binary_certificate_pem_start");
extern const uint8_t certificate_pem_end[] asm("_binary_certificate_pem_end");

// Event Groups and Tags
static const char *TAG_WIFI = "WiFi";
static const char *TAG = "HTTP_CLIENT";
static const char *TAG_DHT = "DHT_SENSOR";
static const char *TAG_BH1750 = "BH1750_SENSOR";
static const char *TAG_BUTTON = "BUTTON";

// --- Function Prototypes ---
void dht_task(void *params);
void bh1750_task(void *params);
void button_task(void *params);
void firebase_task(void *pvParameters);

/* Callback or event handler */
esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;

    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;

    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d, data=%.*s", evt->data_len, evt->data_len, (char*)evt->data);

        if (evt->data_len > MAX_BUFFER_SIZE)
            return ESP_FAIL;

        if (evt->user_data)
        {
            memcpy(evt->user_data, evt->data, evt->data_len);
        }
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;

    default:
        break;
    }
    return ESP_OK;
}
/* Functions GET method */
esp_err_t http_client_get_req(char* data, const char* url)
{
    esp_err_t ret_code = ESP_FAIL;
    // HTTP client configuration
    esp_http_client_config_t config = {
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_GET,
        .port = 80,
        .url = url,
        .user_data = data,
        .cert_pem = (const char *)certificate_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);

        if (status == 200)
        {
            ESP_LOGI(TAG, "HTTP GET status: %d", status);
            ret_code = ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "HTTP GET status: %d", status);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to send GET request");
    }
    esp_http_client_cleanup(client);

    return ret_code;
}

/* HTTP POST function */
esp_err_t http_client_post_req(const char* data_post, const char* post_url) 
{
    esp_err_t ret_code = ESP_FAIL;
    // HTTP client configuration
    esp_http_client_config_t config = {
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_PUT,
        .port = 80,
        .url = post_url,
        .cert_pem = (const char *)certificate_pem_start, // Use your certificate
        
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data_post, strlen(data_post));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) 
    {
        int status = esp_http_client_get_status_code(client);
        if (status == 200)
        {
            ESP_LOGI(TAG, "HTTP POST status: %d", status);
            ret_code = ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "HTTP POST status: %d", status);
        }
    }
    else 
    {
        ESP_LOGE(TAG, "Failed to send POST request");
    }
    esp_http_client_cleanup(client);
    return ret_code;
}

void button_task(void* arg) {
    char data[MAX_BUFFER_SIZE] = {0};
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON1_GPIO) | (1ULL << BUTTON2_GPIO) | (1ULL << BUTTON3_GPIO),
        .mode = GPIO_MODE_OUTPUT,  
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(BUTTON1_GPIO, 0);
    gpio_set_level(BUTTON2_GPIO, 0);
    gpio_set_level(BUTTON3_GPIO, 0);
while (1)
    {
        // Send HTTP GET request to Firebase to retrieve button states
        if (http_client_get_req(data, FIREBASE_BUTTON_URL) == ESP_OK)
        {
            ESP_LOGI(TAG_BUTTON, "Received button states: %s", data);

            // Parse the JSON response
            cJSON* json = cJSON_Parse(data);
            if (json == NULL)
            {
                ESP_LOGE(TAG_BUTTON, "Failed to parse JSON response.");
            }
            else
            {
                // Extract button states from the JSON
                cJSON* button1 = cJSON_GetObjectItem(json, "button1");
                cJSON* button2 = cJSON_GetObjectItem(json, "button2");
                cJSON* button3 = cJSON_GetObjectItem(json, "button3");

                if (button1 && button2 && button3)
                {
                    int button1_state = button1->valueint;
                    int button2_state = button2->valueint;
                    int button3_state = button3->valueint;

                    ESP_LOGI(TAG_BUTTON, "Button1: %d, Button2: %d, Button3: %d", button1_state, button2_state, button3_state);

                    // Control LEDs based on Firebase button states
                    gpio_set_level(BUTTON1_GPIO, button1_state);
                    gpio_set_level(BUTTON2_GPIO, button2_state);
                    gpio_set_level(BUTTON3_GPIO, button3_state);
                }
                else
                {
                    ESP_LOGE(TAG_BUTTON, "Failed to extract button states from JSON.");
                }

                cJSON_Delete(json); // Free the JSON object
            }
        }
        else
        {
            ESP_LOGE(TAG_BUTTON, "Failed to retrieve button states from Firebase.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
    vTaskDelete(NULL);
}
// --- DHT Sensor Task ---
void dht_task(void* arg)
{
    float temp, humidity;

    while (1)
    {
        if (dht_read_float_data(SENSOR_TYPE, CONFIG_DATA_GPIO, &humidity, &temp) == ESP_OK)
        {
            ESP_LOGI(TAG_DHT, "Humidity: %.1f%%, Temp: %.1fC", humidity, temp);

            // Create JSON payload
            cJSON* json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "temperature", temp);
            cJSON_AddNumberToObject(json, "humidity", humidity);
            char* data = cJSON_Print(json);

            // Send data to Firebase
            if (http_client_post_req(data, FIREBASE_DHT_URL) == ESP_OK)
            {
                ESP_LOGI(TAG, "DHT data uploaded successfully.");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to upload DHT data.");
            }

            cJSON_Delete(json);
            free(data);
        }
        else
        {
            ESP_LOGE(TAG_DHT, "Failed to read DHT sensor.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
    vTaskDelete(NULL);
}

void bh1750_task(void* arg)
{
    i2c_dev_t dev = { 0 };
    if (bh1750_init_desc(&dev, BH1750_ADDR_LO, 0, I2C_SDA, I2C_SCK) != ESP_OK || 
        bh1750_setup(&dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH) != ESP_OK)
    {
        ESP_LOGE(TAG_BH1750, "Failed to initialize BH1750.");
        vTaskDelete(NULL);
    }

    while (1)
    {
        uint16_t lux;
        if (bh1750_read(&dev, &lux) == ESP_OK)
        {
            ESP_LOGI(TAG_BH1750, "Light Intensity: %d lux", lux);

            // Create JSON payload
            cJSON* json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "light_intensity", lux);
            char* data = cJSON_Print(json);

            // Send data to Firebase
            if (http_client_post_req(data, FIREBASE_LIGHT_URL) == ESP_OK)
            {
                ESP_LOGI(TAG, "BH1750 data uploaded successfully.");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to upload BH1750 data.");
            }

            cJSON_Delete(json);
            free(data);
        }
        else
        {
            ESP_LOGE(TAG_BH1750, "Failed to read BH1750.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
    vTaskDelete(NULL);
}

void app_main(void) {

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t err = ESP_FAIL;
    while (err != ESP_OK)
    {
        err = example_connect();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Unable to connect to WiFi.");
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
    if (i2cdev_init() != ESP_OK) {
        ESP_LOGE(TAG_WIFI, "Failed to initialize I2C.");
        return;
    }
    // Start sensor tasks
    xTaskCreate(dht_task, "DHT Task", 4096, NULL, 2, NULL);
    xTaskCreate(bh1750_task, "BH1750 Task", 4096, NULL, 2, NULL);
    xTaskCreate(button_task, "Button Task", 4096, NULL, 2, NULL);
    vTaskDelete(NULL);
}
