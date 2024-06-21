#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>

#include <PowerFeather.h>
#include <esp_camera.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "cam.h"
#include "mic.h"
#include "wifi.h"

static const char *TAG = "powerfeather";
#define BATTERY_CAPACITY 0
bool inited = false;
esp_err_t error;

int warm_up = 0;

const size_t SAMPLE_RATE = 16000; 
const size_t RECORD_DURATION_SECONDS = 10;
const size_t TOTAL_SAMPLES = SAMPLE_RATE * RECORD_DURATION_SECONDS;

const gpio_num_t PIR = GPIO_NUM_11;

void loop() {
    vTaskDelay(pdMS_TO_TICKS(3000));
    while (true) 
    {
        /*--------------------------------------------------------------------------------*/
        int output = gpio_get_level(PIR);
        ESP_LOGI(TAG, "%d", output);
        if (output == 0) {
            if (warm_up == 1) {
                ESP_LOGI(TAG, "warming up");
                warm_up = 0;
                vTaskDelay(pdMS_TO_TICKS(30));
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGI(TAG, "motion detected");
            warm_up = 1;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        /*--------------------------------------------------------------------------------*/
    }
}

void sleep_config()
{
    // mic - power down
    PowerFeather::Board.setEN(false); 

    // cam - power down
    PowerFeather::Board.enable3V3(false);
    gpio_set_level(PowerFeather::Mainboard::Pin::A0, 1);
    rtc_gpio_hold_en(PowerFeather::Mainboard::Pin::A0);

    // other v sources
    PowerFeather::Board.enableVSQT(false);
    gpio_reset_pin(GPIO_NUM_15);
    gpio_reset_pin(GPIO_NUM_16);
    gpio_reset_pin(GPIO_NUM_37);
    gpio_reset_pin(GPIO_NUM_6);
    gpio_reset_pin(GPIO_NUM_17);
    gpio_reset_pin(GPIO_NUM_18);
    gpio_reset_pin(GPIO_NUM_45);
    gpio_reset_pin(GPIO_NUM_12);
    gpio_reset_pin(GPIO_NUM_44);
    gpio_reset_pin(GPIO_NUM_42);
    gpio_reset_pin(GPIO_NUM_41);
    gpio_reset_pin(GPIO_NUM_40);
    gpio_reset_pin(GPIO_NUM_39);
    gpio_reset_pin(GPIO_NUM_43);
    gpio_reset_pin(GPIO_NUM_36);
    gpio_reset_pin(GPIO_NUM_35);
    gpio_reset_pin(GPIO_NUM_21); 
    gpio_reset_pin(GPIO_NUM_5); 
    gpio_reset_pin(GPIO_NUM_46); 
    gpio_reset_pin(GPIO_NUM_0);
}

extern "C" void app_main()
{
    gpio_reset_pin(PowerFeather::Mainboard::Pin::BTN);
    gpio_set_direction(PowerFeather::Mainboard::Pin::BTN, GPIO_MODE_INPUT);

    gpio_reset_pin(PowerFeather::Mainboard::Pin::LED);
    gpio_set_direction(PowerFeather::Mainboard::Pin::LED, GPIO_MODE_INPUT_OUTPUT);

    // PIR
    gpio_reset_pin(PIR);
    gpio_set_direction(PIR, GPIO_MODE_INPUT); 
    esp_sleep_enable_ext0_wakeup(PIR, 1);
    
    if (PowerFeather::Board.init(BATTERY_CAPACITY) == PowerFeather::Result::Ok)
    {
        PowerFeather::Board.enableBatteryCharging(false);
        PowerFeather::Board.enableBatteryFuelGauge(false);
        PowerFeather::Board.enableBatteryTempSense(false);
        printf("board init success\n");
        inited = true;
    }

    /*--------------------------------------------------------------------------------*/
    // esp_sleep_enable_timer_wakeup(30 * 1000000);
    // sleep_config();
    // esp_deep_sleep_start();
    /*--------------------------------------------------------------------------------*/
    /* deep sleep wake-up and image capture (every 30s) */
    // rtc_gpio_isolate(PowerFeather::Mainboard::Pin::D13);
    // PowerFeather::Board.enable3V3(true); 
    // if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) 
    // {
    //     PowerFeather::Board.enable3V3(true); 
    //     if(ESP_OK != init_cam()) 
    //     {
    //         return;
    //     }
    //     capture();
    // }
    // sleep_config();
    // esp_sleep_enable_timer_wakeup(30 * 1000000);
    // esp_deep_sleep_start();
    /*--------------------------------------------------------------------------------*/
    /* deep sleep wake-up and record for 30s (every 30s) */
    /* Note: wifi transmission + recording causes noise on power line */
    // PowerFeather::Board.setEN(true);
    // mic_init();
    // // wifi_init();
    // int16_t* readings = (int16_t*)malloc(sizeof(int16_t) * TOTAL_SAMPLES);
    // size_t total_samples_read = 0;
    // if (readings == nullptr) 
    // {
    //     ESP_LOGE(TAG, "memory allocation failed");
    //     return;
    // }
    // int64_t start_time = esp_timer_get_time();
    // while (esp_timer_get_time() - start_time < RECORD_DURATION_SECONDS * 1000000) 
    // {
    //     total_samples_read += mic_read(readings + total_samples_read, TOTAL_SAMPLES - total_samples_read);
    // }
    // if (total_samples_read == 0) 
    // {
    //     ESP_LOGE(TAG, "no data read");
    //     mic_deinit();
    //     mic_init();
    //     free(readings);
    //     return;
    // }
    // // ESP_LOGI(TAG, "sending: %u readings", total_samples_read);
    // // esp_err_t error = wifi_send(readings, total_samples_read);
    // // if (error != ESP_OK) 
    // // {
    // //     ESP_LOGE(TAG, "sending failed: %s", esp_err_to_name(error));
    // // }
    // free(readings);
    // PowerFeather::Board.setEN(false);
    // esp_deep_sleep(30 * 1000000);
    /*--------------------------------------------------------------------------------*/

    loop();
}