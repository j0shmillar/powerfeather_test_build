#ifndef WIFI_H
#define WIFI_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_http_client.h>
#include <esp_event.h>
#include <esp_err.h>

#define WIFI_SSID "SSID"
#define WIFI_PASS "password"
#define WIFI_MAX_RETRY 5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

extern EventGroupHandle_t s_wifi_event_group;

void wifi_init();
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void http_init();
esp_err_t wifi_send(const int16_t* const samples, size_t count);

#endif