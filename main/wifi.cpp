#include "wifi.h"

#include <freertos/task.h>
#include <nvs_flash.h>

#include <esp_http_client.h>
#include <esp_wifi.h>
#include <esp_log.h>

static esp_http_client_handle_t s_http_client = NULL;  

static const char* TAG = "wifi";
EventGroupHandle_t s_wifi_event_group;
static int s_wifi_retry_num = 0;

void http_init() {
    esp_http_client_config_t config = {
        .host = "172.20.10.7",
        .port = 5003,
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .path = "/samples",
        .disable_auto_redirect = true,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
    };
    s_http_client = esp_http_client_init(&config);

    if (s_http_client == NULL) {
        ESP_LOGE(TAG, "failed to initialize HTTP client");
        return;
    }

    ESP_ERROR_CHECK(esp_http_client_set_method(s_http_client, HTTP_METHOD_POST));
    ESP_ERROR_CHECK(esp_http_client_set_header(s_http_client, "Content-Type", "application/octet-stream"));
}

void wifi_init() {
    esp_err_t rv = nvs_flash_init();
    if (rv == ESP_ERR_NVS_NO_FREE_PAGES || rv == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        rv = nvs_flash_init();
    }
    ESP_ERROR_CHECK(rv);

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to SSID: <%s>", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "unable to connect to SSID: <%s>", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "unexpected error");
    }

    http_init();
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGI(TAG, "connecting...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connecting failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_send_audio(const int16_t* const samples, size_t count) {
    ESP_ERROR_CHECK(esp_http_client_set_post_field(s_http_client, (const char*) samples, sizeof(uint16_t) * count));
    return esp_http_client_perform(s_http_client);
}

esp_err_t wifi_send_image(const uint8_t* image_data, size_t length) {
    if (s_http_client == NULL) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_FAIL;
    }

    if (image_data == NULL) {
        ESP_LOGE(TAG, "Image data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_http_client_set_post_field(s_http_client, (const char*) image_data, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to set post field: %s", esp_err_to_name(err));
        return err;
    }

    return esp_http_client_perform(s_http_client);
}