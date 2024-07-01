#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>

#include <PowerFeather.h>
#include <esp_camera.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_pm.h>

#include "cam.h"
#include "mic.h"
#include "wifi.h"

static const char *TAG = "powerfeather";
#define BATTERY_CAPACITY 0
bool inited = false;
esp_err_t error;

const size_t SAMPLE_RATE = 16000; 
const size_t RECORD_DURATION_SECONDS = 3;
const size_t TOTAL_SAMPLES = SAMPLE_RATE * RECORD_DURATION_SECONDS;

const gpio_num_t PIR = GPIO_NUM_11;

//TODO 
// - consumption drops post cam wake up to ~70uA then increase to ~90uA ... why?
// - update CONFIG_ESP32S3_DEEP_SLEEP_WAKEUP_DELAY 
// - add core tasks
// - PRIOR: reduce MIC I

void sleep_config()
{
    rtc_gpio_init(PIR);
    rtc_gpio_set_direction_in_sleep(PIR, RTC_GPIO_MODE_INPUT_ONLY);
    esp_sleep_enable_ext0_wakeup(PIR, 1);

    // mic - power down
    PowerFeather::Board.setEN(false); 


    // cam - power down
    PowerFeather::Board.enable3V3(false);

    rtc_gpio_init(PowerFeather::Mainboard::Pin::A0);
    rtc_gpio_set_direction_in_sleep(PowerFeather::Mainboard::Pin::A0, RTC_GPIO_MODE_INPUT_ONLY); 
    rtc_gpio_set_level(PowerFeather::Mainboard::Pin::A0, 1);
    rtc_gpio_hold_en(PowerFeather::Mainboard::Pin::A0);

    // other v sources
    PowerFeather::Board.enableVSQT(false);
}

void loop() {
    while (true) 
    {
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
        {
            PowerFeather::Board.enable3V3(true); 
            rtc_gpio_set_level(PowerFeather::Mainboard::Pin::A0, 0);
            
            if(ESP_OK != init_cam()) 
            {
                return;
            }
    
            sensor_t * sensor = esp_camera_sensor_get();
            sensor->set_whitebal(sensor, 1);
            sensor->set_awb_gain(sensor, 1);
            sensor->set_wb_mode(sensor, 0);
            
            size_t _jpg_buf_len;
            uint8_t * _jpg_buf;
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) 
            {
                ESP_LOGE(TAG, "capture failed");
                return;
            }

            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted)
            {
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
            }

            // wifi_init();
            // esp_err_t err = wifi_send_image(_jpg_buf, _jpg_buf_len);
            // if (err != ESP_OK) 
            // {
            //     ESP_LOGE(TAG, "failed to send image: %s", esp_err_to_name(err));
            // } 
            // else 
            // {
            //     ESP_LOGI(TAG, "image sent");
            // }
            // esp_wifi_stop();
            // esp_wifi_deinit();
            
            free(_jpg_buf);
            esp_camera_fb_return(fb);

            while(gpio_get_level(PIR)==1) //TODO - fix sensitivity 
            {
                ESP_LOGI(TAG, "waiting");
            }

            esp_camera_deinit();
            sleep_config();
            esp_deep_sleep(10 * 3000000);
        }
        else
        {
            PowerFeather::Board.setEN(true);
            mic_init();

            int16_t* readings = (int16_t*)malloc(sizeof(int16_t) * TOTAL_SAMPLES);
            size_t total_samples_read = 0;
            if (readings == nullptr) 
            {
                ESP_LOGE(TAG, "memory allocation failed");
                return;
            }

            int64_t start_time = esp_timer_get_time();
            ESP_LOGI(TAG, "recording...");
            while (esp_timer_get_time() - start_time < RECORD_DURATION_SECONDS * 1000000) 
            {
                total_samples_read += mic_read(readings + total_samples_read, TOTAL_SAMPLES - total_samples_read);
            }
            if (total_samples_read == 0) 
            {
                ESP_LOGE(TAG, "no data recorded");
                mic_deinit();
                mic_init();
                free(readings);
                return;
            } 
            free(readings);
            mic_deinit();

            /* note: wifi transmission whilst recording causes noise on power line */
            // wifi_init();
            // ESP_LOGI(TAG, "sending: %u readings", total_samples_read);
            // esp_err_t error = wifi_send_audio(readings, total_samples_read);
            // if (error != ESP_OK) 
            // {
            //     ESP_LOGE(TAG, "sending failed: %s", esp_err_to_name(error));
            // }
            // esp_wifi_stop();
            // esp_wifi_deinit();

            sleep_config();
            esp_deep_sleep(10 * 3000000);
        }
    }
}

extern "C" void app_main()
{
    rtc_gpio_init(PIR);
    rtc_gpio_set_direction(PIR, RTC_GPIO_MODE_INPUT_ONLY); 
    rtc_gpio_wakeup_enable(PIR, GPIO_INTR_HIGH_LEVEL);

    // esp_pm_config_t pm_config = {
    //         .max_freq_mhz = 80,
    //         .min_freq_mhz = 80,
    //         .light_sleep_enable = true};
    // esp_pm_configure(&pm_config);

    if (PowerFeather::Board.init(BATTERY_CAPACITY) == PowerFeather::Result::Ok)
    {
        PowerFeather::Board.enableBatteryCharging(false);
        PowerFeather::Board.enableBatteryFuelGauge(false);
        PowerFeather::Board.enableBatteryTempSense(false);
        printf("board init success\n");
        inited = true;
    }

    loop();
}