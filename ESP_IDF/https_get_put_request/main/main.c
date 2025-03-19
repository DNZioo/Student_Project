#include "http.h"
#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"


#define BUTTON_URL "https://https-start-617d7-default-rtdb.firebaseio.com/button_state.json"
#define FIREBASE_URL1 "https://https-start-617d7-default-rtdb.firebaseio.com/Light_data.json"
#define TEMPERATURE_URL "https://https-start-617d7-default-rtdb.firebaseio.com/sensor_data.json"

#define LED1 GPIO_NUM_2
#define SENSOR_TYPE DHT_TYPE_DHT11
#define GPIO_DATA GPIO_NUM_4
static const char *TAG_HTTP = "HTTP_CLIENT";
static const char *TAG_DHT = "DHT";

void dht_firebase_task(void *params) {
    float Temp, Humidity;
    while (1) {
        // Read data from DHT sensor
        if (dht_read_float_data(SENSOR_TYPE, GPIO_DATA, &Humidity, &Temp) == ESP_OK) {
            ESP_LOGI(TAG_DHT, "Humidity: %.1f%%, Temp: %.1fC", Humidity, Temp);

            // Create JSON payload
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", Temp);
            cJSON_AddNumberToObject(root, "humidity", Humidity);
            const char *put_data = cJSON_Print(root);

            // Send data to Firebase using PUT request
            if (http_client_post_req((char *)put_data, TEMPERATURE_URL) == ESP_OK) {
                ESP_LOGI(TAG_DHT, "Data successfully sent to Firebase");
            } else {
                ESP_LOGE(TAG_DHT, "Failed to send data to Firebase");
            }

            // Clean up
            cJSON_Delete(root);
            free((void *)put_data);
        } else {
            ESP_LOGE(TAG_DHT, "Could not read data from sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

void Post_task(void *arg)
{
    int num_firebase_fail = 0;
    while (1)
    {
        // JSON payload creation
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "message", "Hello, Firebase!");
        cJSON_AddNumberToObject(root, "value", 123);
        const char *post_data = cJSON_Print(root);

        if (http_client_post_req((char *)post_data, FIREBASE_URL1) == ESP_OK)
        {
            num_firebase_fail = 0;
            ESP_LOGI(TAG_HTTP, "Data successfully sent to Firebase");
        }
        else
        {
            num_firebase_fail++;
            ESP_LOGE(TAG_HTTP, "Failed to send data to Firebase, attempt %d", num_firebase_fail);
            if (num_firebase_fail >= 10)
            {
                //turn off the light here
            }
        }
        free(post_data);
        cJSON_Delete(root); // Free JSON object
        vTaskDelay(pdMS_TO_TICKS(2000)); // Delay 2 seconds
    }
    vTaskDelete(NULL);
}


/* Task functions */
void Get_task(void *arg)
{
    int num_firebase_fail = 0;

    // Configure GPIO for LED
    gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << LED1,
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io_config);

    while (1)
    {
        char data[MAX_BUFFER_SIZE] = {0};

        /* If the attribute is returned correctly */
        if (http_client_get_req(data, BUTTON_URL) == ESP_OK)
        {
            /* Reset num_attr_fail */
            num_firebase_fail = 0;
            /* Directly convert the response to an integer */
            int led_status = atoi(data); // Convert string to integer
            gpio_set_level(LED1, led_status ? 1 : 0);
            ESP_LOGI(TAG_HTTP, "LED1 state: %s", led_status ? "ON" : "OFF");
            // /* Parse JSON to get LED state */
            // cJSON *root = cJSON_Parse(data);
            // if (root)
            // {
            //     // cJSON *led_state = cJSON_GetObjectItem(root, "LED1"); // Look for "LED1"
            //     cJSON *led_state = cJSON_GetObjectItem(root, "Led1Status");
            //     if (cJSON_IsNumber(led_state))
            //     {
            //         int is_on = led_state->valueint; // Get the integer value
            //         gpio_set_level(LED1, is_on ? 1 : 0);
            //         ESP_LOGI(TAG_HTTP, "LED1 state: %s", is_on ? "ON" : "OFF");
            //     }
            //     else
            //     {
            //         ESP_LOGW(TAG_HTTP, "LED1 not found or not an integer");
            //     }
            //     cJSON_Delete(root);
            // }
            // else
            // {
            //     ESP_LOGE(TAG_HTTP, "Failed to parse JSON");
            // }
        }
        /* If the attribute fails 10 times, turn off the LED (for safety) */
        else
        {
            num_firebase_fail++;
            if (num_firebase_fail >= 10)
            {
                ESP_LOGW(TAG_HTTP, "Failed to retrieve data 10 times, turning off LED");
                gpio_set_level(LED1, 0);
            }
            // if (num_firebase_fail >= 10)
            // {
            //     ESP_LOGW(TAG_HTTP, "Failed to retrieve data 10 times, turning off LED");
            //     gpio_set_level(LED1, 0);
            // }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}


void app_main(void)
{
    ESP_LOGI("APP_MAIN", "Starting application...");
    wifi_init();

    xTaskCreate(Get_task, "firebase_task", 4096, NULL, 5, NULL);
    //xTaskCreate(Post_task, "firebase_put_task", 4096, NULL, 5, NULL);
    xTaskCreate(dht_firebase_task, "Sensor_put_task", 4096, NULL, 5, NULL);
}
