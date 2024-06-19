#include "Http.h"

static esp_http_client_handle_t s_http_client;

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

    ESP_ERROR_CHECK(esp_http_client_set_method(s_http_client, HTTP_METHOD_POST));
    ESP_ERROR_CHECK(esp_http_client_set_header(s_http_client, "Content-Type", "application/octet-stream"));
}

esp_err_t http_send(const int16_t* const samples, size_t count) {
    ESP_ERROR_CHECK(esp_http_client_set_post_field(s_http_client, (const char*) samples, sizeof(uint16_t) * count));
    return esp_http_client_perform(s_http_client);
}
