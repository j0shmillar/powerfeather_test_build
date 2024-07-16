#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include <PowerFeather.h>
#include <esp_camera.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_pm.h>

#include "cam.h"
#include "mic.h"
#include "wifi.h"
#include "Q.h"

#include <sys/time.h>

time_t now()
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 }; 
    uint32_t sec;
            gettimeofday(&tv, NULL); 
            (sec) = tv.tv_sec;  
    int8_t hours = (sec % 3600) / 3600;
    return hours;
}

#define BATTERY_CAPACITY 0
bool inited = false;
esp_err_t error;

const size_t SAMPLE_RATE = 16000; 
const size_t RECORD_DURATION_SECONDS = 3;
const size_t TOTAL_SAMPLES = SAMPLE_RATE * RECORD_DURATION_SECONDS;

const gpio_num_t PIR = GPIO_NUM_11;
const gpio_num_t A0 = GPIO_NUM_10;

#define NUM_ACTIONS 6
#define T_INT 0.3
int duration = 3;
float ACTIONS[NUM_ACTIONS] = {3, 5, 60, 300, 600, 1800};
float epsilon = 0.3;
float learning_rate = 0.1;
float discount_factor = 0.9;

//TODO 
// - I drops post cam wake up to ~70uA then increase to ~90uA ... why?
// - add core tasks
// - PRIOR: reduce MIC I

typedef struct 
{
    uint8_t hour; 
} rtc_data_t;

RTC_DATA_ATTR rtc_data_t rtc_data;

void update_hour() 
{
    rtc_data.hour = (rtc_data.hour + 1) % 24;
}

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

//------------------------------------------------------------------------------------------------------------//
int16_t get_sleep_duration(int period) 
{
    int best_action = 0;
    float max_value = Q[period][0];

    for (int i = 1; i < NUM_ACTIONS; i++) {
        if (Q[period][i] > max_value) {
            max_value = Q[period][i];
            best_action = i;
        }
    }

    return ACTIONS[best_action];
}

int update(int period) 
{
    int best_action = 0;
    if (((float)rand() / (float)RAND_MAX) < epsilon) 
    {
        best_action = rand() % NUM_ACTIONS;
    } 
    else 
    {
        for (int i = 1; i < NUM_ACTIONS; i++) 
        {
            if (Q[period][i] > Q[period][best_action]) 
            {
                best_action = i;
            }
        }
    }
    return best_action;
}

// tidy!!!
void q_write(float array[24][7]) {
    FILE *file = fopen("Q.h", "w");
    if (file == NULL) 
    {
        return;
    }
    fprintf(file, "#ifndef Q_H\n#define Q_H\n\nfloat Q[24][7] = {\n");
    for (int i = 0; i < 24; i++) {
        fprintf(file, "    {");
        for (int j = 0; j < 7; j++) {
            fprintf(file, "%f", array[i][j]);
            if (j < 6) fprintf(file, ", ");
        }
        fprintf(file, "}");
        if (i < 23) fprintf(file, ",\n");
    }
    fprintf(file, "\n};\n\n#endif // Q_H\n");

    fclose(file);
}

void q_update(int period, int action) 
{
    int reward = 1;
    int next_period = period+1;
    int next_action = update(period+1);
    Q[period][action] += learning_rate * (reward + discount_factor * Q[next_period][next_action] - Q[period][action]);
    q_write(Q);
}
//------------------------------------------------------------------------------------------------------------//

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
                // printf("capture failed\n");
                return;
            }

            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted)
            {
                // printf("JPEG compression failed\n");
                esp_camera_fb_return(fb);
            }

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            wifi_init();
            esp_err_t err = wifi_send_image(_jpg_buf, _jpg_buf_len);
            if (err != ESP_OK) 
            {
                printf("failed to send image: %s", esp_err_to_name(err));
            } 
            esp_wifi_stop();
            esp_wifi_deinit();
            
            free(_jpg_buf);
            esp_camera_fb_return(fb);

            while(gpio_get_level(PIR)==1) //TODO - fix sensitivity 
            {
                // printf("waiting\n");
            }

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            esp_camera_deinit();
            sleep_config();

            // update_hour();
            // int8_t period = now();
            // q_update(period, duration);
            // duration = get_sleep_duration(period);
            esp_deep_sleep(10 * 1000000);
        }
        else
        {
            PowerFeather::Board.setEN(true);

            mic_init();

            int16_t* readings = (int16_t*)malloc(sizeof(int16_t) * TOTAL_SAMPLES);
            size_t total_samples_read = 0;
            if (readings == nullptr) 
            {
                // printf("memory allocation failed\n");
                return;
            }

            int64_t start_time = esp_timer_get_time();
            // printf("\nrecording...\n");
            while (esp_timer_get_time() - start_time < RECORD_DURATION_SECONDS * 1000000) 
            {
                total_samples_read += mic_read(readings + total_samples_read, TOTAL_SAMPLES - total_samples_read);
            }
            if (total_samples_read == 0) 
            {
                // printf("no data recorded\n");
                mic_deinit();
                mic_init();
                free(readings);
                return;
            } 
            free(readings);
            mic_deinit();

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            /* note: wifi transmission whilst recording causes noise on power line */
            wifi_init();
            // printf("sending: %u readings", total_samples_read);
            esp_err_t error = wifi_send_audio(readings, total_samples_read);
            if (error != ESP_OK) 
            {
                printf("sending failed: %s", esp_err_to_name(error));
            }
            esp_wifi_stop();
            esp_wifi_deinit();

            sleep_config();

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            // update_hour();
            // int8_t period = now();
            // // q_update(period, duration);
            // duration = get_sleep_duration(period);

            esp_deep_sleep(10 * 1000000);
        }
    }
}

extern "C" void app_main()
{
    rtc_gpio_init(PIR);
    rtc_gpio_set_direction(PIR, RTC_GPIO_MODE_INPUT_ONLY); 
    rtc_gpio_wakeup_enable(PIR, GPIO_INTR_HIGH_LEVEL);

    // printf("\n%lld\n", (long long) now());

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
        // printf("board init success\n");
        inited = true;
    }

    loop();
}
