#ifndef HTTP_H
#define HTTP_H

#include <esp_http_client.h>
#include <esp_err.h>

void http_init();
esp_err_t http_send(const int16_t* const samples, size_t count);

#endif // HTTP_H