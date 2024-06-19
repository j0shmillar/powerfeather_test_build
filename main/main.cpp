#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include <PowerFeather.h>

#include <esp_camera.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>

#include "cam.h"
#include "mic.h"
#include "wifi.h"
#include "http.h"

#define BATTERY_CAPACITY 0

// FIX - rename files
// FIX - include wifi etc. 

using namespace PowerFeather; // for PowerFeather::Board //FIX - this aint working
static const char *TAG = "powerfeather";

bool inited = false;
int warm_up = 0;
esp_err_t error;

// #define CAMERA_MODEL_AI_THINKER // FIX

// FIX - tidy
const size_t SAMPLE_RATE = 16000; 
const size_t RECORD_DURATION_SECONDS = 10;
const size_t TOTAL_SAMPLES = SAMPLE_RATE * RECORD_DURATION_SECONDS;

void loop() {
    while (true) 
    {
        /*--------------------------------------------------------------------------------*/
        ESP_LOGI(TAG, "PIR");
        int sensor_output = gpio_get_level(PowerFeather::Mainboard::Pin::D13);
        // ESP_LOGI(TAG, "sensor output: %d", sensor_output);
        if (sensor_output == 0) {
            if (warm_up == 1) {
                ESP_LOGI(TAG, "warming up");
                warm_up = 0;
            }
        } else {
            ESP_LOGI(TAG, "motion detected");
            warm_up = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        /*--------------------------------------------------------------------------------*/
    }
}

extern "C" void app_main()
{
    gpio_reset_pin(PowerFeather::Mainboard::Pin::BTN);
    gpio_set_direction(PowerFeather::Mainboard::Pin::BTN, GPIO_MODE_INPUT);

    gpio_reset_pin(PowerFeather::Mainboard::Pin::LED);
    gpio_set_direction(PowerFeather::Mainboard::Pin::LED, GPIO_MODE_INPUT_OUTPUT);

    // FIX - assign Mainboard::Pin::D13 to PIR
    gpio_reset_pin(PowerFeather::Mainboard::Pin::D13);
    gpio_set_direction(PowerFeather::Mainboard::Pin::D13, GPIO_MODE_INPUT);
    gpio_set_level(PowerFeather::Mainboard::Pin::D13, 0);

    if (PowerFeather::Board.init(BATTERY_CAPACITY) == PowerFeather::Result::Ok)
    {
        PowerFeather::Board.setBatteryChargingMaxCurrent(50);
        PowerFeather::Board.setSupplyMaintainVoltage(4600);
        PowerFeather::Board.enableBatteryCharging(false);
        PowerFeather::Board.enableBatteryFuelGauge(false);
        PowerFeather::Board.enableBatteryTempSense(false);
        printf("board init success\n");
        inited = true;
    }

    PowerFeather::Board.setEN(false); 
    PowerFeather::Board.enable3V3(false);
    PowerFeather::Board.enableVSQT(false);

    /*--------------------------------------------------------------------------------*/
    esp_sleep_enable_timer_wakeup(30 * 1000000);
    esp_deep_sleep_start();
    /*--------------------------------------------------------------------------------*/
    // /* deep sleep wake-up and image capture (every 30s) */
    // esp_sleep_enable_ext0_wakeup(Mainboard::Pin::D13, 1);
    // rtc_gpio_isolate(PowerFeather::Mainboard::Pin::D13);
    // if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) 
    // {
    //     PowerFeather::Board.enable3V3(true); 
    //     if(ESP_OK != init_cam()) 
    //     {
    //         return;
    //     }
    //     capture();
    // }
    // PowerFeather::Board.enable3V3(false); 
    // esp_deep_sleep(30 * 1000000);
    /*--------------------------------------------------------------------------------*/
    // PowerFeather::Board.setEN(true);
    // mic_init();
    // // FIX - make into 1 
    // // wifi_init();
    // // http_init();
    // // FIX - tidy all this
    // int16_t* readings = (int16_t*)malloc(sizeof(int16_t) * TOTAL_SAMPLES);
    // if (readings == nullptr) {
    //     ESP_LOGE(TAG, "Memory allocation failed");
    //     return;
    // }
    // size_t total_samples_read = 0;
    // int64_t start_time = esp_timer_get_time();
    // while (esp_timer_get_time() - start_time < RECORD_DURATION_SECONDS * 1000000) {
    //     total_samples_read += mic_read(readings + total_samples_read, TOTAL_SAMPLES - total_samples_read);
    // }
    // if (total_samples_read == 0) {
    //     ESP_LOGE(TAG, "No data available");
    //     mic_deinit();
    //     mic_init();
    //     free(readings);
    //     return;
    // }
    // // ESP_LOGI(TAG, "sending: %u readings", total_samples_read);
    // // esp_err_t error = http_send(readings, total_samples_read);
    // // if (error != ESP_OK) {
    // //     ESP_LOGE(TAG, "sending failed: %s", esp_err_to_name(error));
    // // }
    // free(readings);
    // PowerFeather::Board.setEN(false);
    // esp_deep_sleep(30 * 1000000);
    /*--------------------------------------------------------------------------------*/

    loop();
}