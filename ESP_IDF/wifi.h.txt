#ifndef WIFI_H
#define WIFI_H

#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"

// Event group handle for Wi-Fi events
extern EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Function prototypes
void wifi_init(void);

#endif // WIFI_H
