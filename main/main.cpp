#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include <PowerFeather.h>
#include <esp_camera.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_dsp.h>
#include <esp_pm.h>

#include "cam.h"
#include "mic.h"
#include "wifi.h"
#include "Q.h"
#include "birdnet.h"
#include "goertzel.cpp"
#include "dsps_fft2r.h"

#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"


#include <sys/time.h>

//TODO 
// - I drops post cam wake up to ~70uA then increase to ~90uA ... why?
// - add core tasks
// - PRIOR: reduce MIC I

namespace
{
	const tflite::Model* model_l = nullptr;
	tflite::MicroInterpreter* interpreter = nullptr;
	TfLiteTensor* input = nullptr;
	// TfLiteTensor* output = nullptr;
    tflite::MicroMutableOpResolver<6> micro_op_resolver;
} 

#define K_TENSOR_ARENA_SIZE 1024*32
uint8_t tensor_arena[K_TENSOR_ARENA_SIZE];
bool tflite_initialized = false;

#define SECONDS_TO_TICKS(x) ((x) * configTICK_RATE_HZ)

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

//------------------------------------------------------------------------------------------------------------//s

void setup_tflite() {
    if (tflite_initialized) 
    {
        return;
    }
    tflite_initialized = true;
    model_l = tflite::GetModel(model);
    micro_op_resolver.AddConv2D();
    micro_op_resolver.AddMaxPool2D();
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddFullyConnected();
    micro_op_resolver.AddSoftmax();

    static tflite::MicroInterpreter static_interpreter(model_l, micro_op_resolver, tensor_arena, K_TENSOR_ARENA_SIZE);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) 
    {
        return;
    }

    input = interpreter->input(0);
    if (input == nullptr) 
    {
        return;
    }

    interpreter->AllocateTensors();
}

// void free_tflite() 
// {
//     interpreter = nullptr;
//     tflite_initialized = false;
// }

void run_inference(float* mel_spectrogram) 
{
    setup_tflite();
    input = interpreter->input(0);
    uint32_t num_elements = input->bytes / sizeof(float); 
    for (int8_t j = 0; j < num_elements; j++) 
    {
        input->data.int8[j] = mel_spectrogram[j];
    }

    if (interpreter->Invoke() != kTfLiteOk) 
    {
        return;
    }
    // free_tflite();
}


//------------------------------------------------------------------------------------------------------------//

#define WINDOW_SIZE 256
#define FFT_SIZE 256

void compute_stft(const int16_t *input, size_t length, float *output) {
    if (length < FFT_SIZE) 
    {
        return;
    }

    float fft_input[FFT_SIZE];
    float fft_output[FFT_SIZE] = {0};
    for (size_t i = 0; i < FFT_SIZE; ++i) 
    {
        fft_input[i] = (i < length) ? (float)input[i] : 0.0f;
    }

    dsps_fft2r_fc32(fft_input, FFT_SIZE);

    for (size_t i = 0; i < FFT_SIZE; ++i) 
    {
        output[i] = fft_input[i];
    }
}

#define MEL_BINS 40

void mel_filter_bank(const float* fft_magnitudes, float* mel_spectrogram, size_t num_frames) 
{
    float mel_min = 0;
    float mel_max = 2595 * log10(1 + 16000 / 700.0);
    float mel_bin_width = (mel_max - mel_min) / (MEL_BINS - 1);
    
    for (size_t i = 0; i < num_frames; ++i) 
    {
        float mel_sum[MEL_BINS] = {0};
        for (int j = 0; j < FFT_SIZE / 2; ++j) 
        {
            float mel_freq = 2595 * log10(1 + (float)j * SAMPLE_RATE / FFT_SIZE / 700.0);
            int mel_bin = (mel_freq - mel_min) / mel_bin_width;
            if (mel_bin >= 0 && mel_bin < MEL_BINS) 
            {
                mel_sum[mel_bin] += fft_magnitudes[i * (FFT_SIZE / 2) + j];
            }
        }
        for (int k = 0; k < MEL_BINS; ++k) 
        {
            mel_spectrogram[i * MEL_BINS + k] = mel_sum[k];
        }
    }
}


//------------------------------------------------------------------------------------------------------------//

int16_t get_sleep_duration(int period) 
{
    int best_action = 0;
    float max_value = Q[period][0];

    for (int i = 1; i < NUM_ACTIONS; i++) 
    {
        if (Q[period][i] > max_value) 
        {
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

void q_write(float array[24][7]) {
    FILE *file = fopen("Q.h", "w");
    if (file == NULL) 
    {
        return;
    }
    fprintf(file, "#ifndef Q_H\n#define Q_H\n\nfloat Q[24][7] = {\n");
    for (int i = 0; i < 24; i++) 
    {
        fprintf(file, "    {");
        for (int j = 0; j < 7; j++) 
        {
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

            // wifi_init();
            // esp_err_t err = wifi_send_image(_jpg_buf, _jpg_buf_len);
            // if (err != ESP_OK) 
            // {
            //     printf("failed to send image: %s", esp_err_to_name(err));
            // } 
            // esp_wifi_stop();
            // esp_wifi_deinit();
            
            free(_jpg_buf);
            esp_camera_fb_return(fb);
            esp_camera_deinit();

            while(gpio_get_level(PIR)==1) //TODO - fix sensitivity 
            {
                // printf("waiting\n");
            }

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
            int16_t* readings1 = (int16_t*)malloc(sizeof(int16_t) * (SAMPLE_RATE * 0.1));
            size_t total_samples_read = 0;
            if (readings1 == nullptr) 
            {
                // printf("memory allocation failed\n");
                return;
            }
                
            ////////////////////////////////////////////////TFL////////////////////////////////////////////////

            // int64_t start_time = esp_timer_get_time();
            // while (esp_timer_get_time() - start_time < 0.1 * 1000000) 
            // {
            //     total_samples_read += mic_read(readings1 + total_samples_read, (SAMPLE_RATE*0.1) - total_samples_read);
            // }
            // if (total_samples_read == 0) 
            // {
            //     mic_deinit();
            //     free(readings1);
            //     return;
            // }
            // mic_deinit();

            // esp_err_t ret;
            // ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
            // size_t num_frames = ((SAMPLE_RATE*0.1) - WINDOW_SIZE) / WINDOW_SIZE;
            // float* stft_result = (float*)malloc(num_frames * (FFT_SIZE / 2) * sizeof(float));
            // if (stft_result == NULL) {
            //     // printf("Failed to allocate STFT result memory\n");
            //     free(readings1);
            //     return;
            // }
            // compute_stft(readings1, (SAMPLE_RATE*0.1), stft_result);
            // float* mel_spectrogram = (float*)malloc(num_frames * MEL_BINS * sizeof(float));
            // if (mel_spectrogram == NULL) {
            //     free(stft_result);
            //     free(readings1);
            //     return;
            // }
            // mel_filter_bank(stft_result, mel_spectrogram, num_frames);
            // free(stft_result);
            // free(readings1);

            // run_inference(mel_spectrogram);
            // free(mel_spectrogram);

            //////////////////////////////////////////////GOERTZEL/////////////////////////////////////////////

            // int64_t start_time = esp_timer_get_time();
            // while (esp_timer_get_time() - start_time < 0.1 * 1000000) 
            // {
            //     total_samples_read += mic_read(readings1 + total_samples_read, (SAMPLE_RATE*0.1) - total_samples_read);
            // }
            // if (total_samples_read == 0) 
            // {
            //     mic_deinit();
            //     free(readings1);
            //     return;
            // }
            // mic_deinit();

            // double magnitude = process_audio_samples(readings1, (SAMPLE_RATE*0.1), 1600, SAMPLE_RATE);

            // free(readings1);

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            total_samples_read = 0;
            int16_t* readings2 = (int16_t*)malloc(sizeof(int16_t) * TOTAL_SAMPLES);
            if (readings2 == nullptr) 
            {
                // printf("memory allocation failed\n");
                return;
            }

            start_time = esp_timer_get_time();
            // printf("\nrecording...\n");
            while (esp_timer_get_time() - start_time < RECORD_DURATION_SECONDS * 1000000) 
            {
                // printf("%d\n", total_samples_read);
                total_samples_read += mic_read(readings2 + total_samples_read, TOTAL_SAMPLES - total_samples_read);
            }
            if (total_samples_read == 0) 
            {
                // printf("no data recorded\n");
                mic_deinit();
                free(readings2);
                return;
            } 
            mic_deinit();

            /* note: wifi transmission whilst recording causes noise on power line */
            // wifi_init();
            // // printf("sending: %u readings", total_samples_read);
            // esp_err_t error = wifi_send_audio(readings2, total_samples_read);
            // if (error != ESP_OK) 
            // {
            //     printf("sending failed: %s", esp_err_to_name(error));
            // }
            // esp_wifi_stop();
            // esp_wifi_deinit();
    
            free(readings2);

            sleep_config();

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
