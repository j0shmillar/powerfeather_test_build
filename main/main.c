#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "Wifi.h"
#include "I2S.h"
#include "Http.h"
#include <esp_camera.h>
#include <esp_log.h>
#include <esp_sleep.h>

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 2 //XCLK
#define CAM_PIN_SIOD 35 //SDA
#define CAM_PIN_SIOC 36 //SCL

#define CAM_PIN_D7 45
#define CAM_PIN_D6 12
#define CAM_PIN_D5 17
#define CAM_PIN_D4 18
#define CAM_PIN_D3 15
#define CAM_PIN_D2 16
#define CAM_PIN_D1 37
#define CAM_PIN_D0 6
#define CAM_PIN_VSYNC 3 //VSYNC
#define CAM_PIN_HREF 8 //HS
#define CAM_PIN_PCLK 1 //PC

static const char *TAG = "powerfeather";
// const gpio_num_t PIR_PIN = 15;
// int warm_up;

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void data_loop() {
    // int16_t* samples = (int16_t*) malloc(sizeof(uint16_t) * I2S_SAMPLE_COUNT);
    // esp_err_t error;
    while (true) {
        gpio_set_level(46, !gpio_get_level(46));

        ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();

        if (pic) {
            ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
            esp_camera_fb_return(pic);
        } else {
            ESP_LOGE(TAG, "Failed to take picture");
        }

        vTaskDelay(5000);

        /*mic read*/
        // // comment all the rest for mpm, incl. wifi_init + http_init
        // size_t sample_read = mic_read(samples);
        // if (sample_read == 0) {
        //     ESP_LOGE(TAG, "No data available");
        //     mic_deinit();
        //     mic_init();
        //     continue;
        // }
        
        /*send mic data*/
        // // comment all the rest for mpm, uncomment "esp_deep_sleep(3000000);"
        // ESP_LOGI(TAG, "Send: %u", sample_read);
        // error = http_send(samples, sample_read);
        // if (error != ESP_OK) {
        //     ESP_LOGE(TAG, "Sending has failed: %s", esp_err_to_name(error));
        // }
        // esp_deep_sleep(3000000);
        // // or
        // esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000)
        // esp_deep_sleep_start();

        /*pir*/
        // // comment all the rest for mpm, incl. wifi_init + http_init
        // int sensor_output = gpio_get_level(PIR_PIN);
        // if (sensor_output == 1) {
        //     if (warm_up == 1) {
        //         printf("Warming up\n\n");
        //         warm_up = 0;
        //         vTaskDelay(pdMS_TO_TICKS(30));
        //     }
        //     vTaskDelay(pdMS_TO_TICKS(10));
        // } else {
        //     printf("Object detected\n\n");
        //     warm_up = 1;
        //     vTaskDelay(pdMS_TO_TICKS(10));
        // }

        vTaskDelay(5000 / portTICK_RATE_MS); // comment for cont.
    }
}

void app_main(void)
{
    gpio_reset_pin(4); // GPIO4 controls the 3V3 output
    gpio_set_direction(4, GPIO_MODE_OUTPUT);
    gpio_set_level(4, 1);

    gpio_reset_pin(0);
    gpio_set_direction(0, GPIO_MODE_INPUT);

    gpio_reset_pin(46);
    gpio_set_direction(46, GPIO_MODE_INPUT_OUTPUT);

    // gpio_reset_pin(PIR_PIN);
    // gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
     
    if(ESP_OK != init_camera()) {
        return;
    }

    // mic_init();
    // wifi_init();
    // http_init();

    data_loop();
}
