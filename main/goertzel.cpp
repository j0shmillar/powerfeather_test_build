#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define FREQUENCY 1000
#define BANDWIDTH 200
#define GOERTZEL_N 30
#define WINDOW_SIZE 1600  // 0.1 seconds at 16000kHz

void precompute_constants(int L, double *c, double *hamming_window, size_t SAMPLE_RATE) 
{
    double alpha = 0.54;
    double beta = 1.0 - alpha;
    *c = 2.0 * cos(2.0 * M_PI * FREQUENCY / SAMPLE_RATE);
    for (int j = 0; j < L; j++) 
    {
        hamming_window[j] = alpha - beta * cos(2.0 * M_PI * j / (L - 1));
    }
}

void goertzel_filter(int16_t *samples, int L, double c, const double *hamming_window, double *magnitude) 
{
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;
    for (int j = 0; j < L; j++) 
    {
        double windowed_sample = samples[j] * hamming_window[j];
        q0 = c * q1 - q2 + windowed_sample;
        q2 = q1;
        q1 = q0;
    }
    *magnitude = sqrt(q1 * q1 + q2 * q2 - q1 * q2 * c);
}

int compare_doubles(const void *a, const void *b) 
{
    double arg1 = *(const double *)a;
    double arg2 = *(const double *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

double compute_median(double *array, int size) 
{
    qsort(array, size, sizeof(double), compare_doubles);
    if (size % 2 == 0) 
    {
        return (array[size / 2 - 1] + array[size / 2]) / 2.0;
    } else 
    {
        return array[size / 2];
    }
}

double process_audio_samples(int16_t *audio_samples, int total_samples, size_t L, size_t SAMPLE_RATE) 
{
    if (L <= 0 || total_samples <= 0) 
    {
        // printf("invalid parameters: L or total_samples are non-positive.\n");
        return -1;
    }

    double c;
    double *hamming_window = (double *)malloc(L * sizeof(double));
    if (hamming_window == NULL) 
    {
        // printf("memory allocation failed for hamming window.\n");
        return -1;
    }
    
    double magnitudes[GOERTZEL_N];
    int window_count = total_samples / L;
    if (window_count > GOERTZEL_N) 
    {
        window_count = GOERTZEL_N;
        // printf("window count exceeds GOERTZEL_N - limiting to GOERTZEL_N.\n");
    }

    precompute_constants(L, &c, hamming_window, SAMPLE_RATE);
    memset(magnitudes, 0, GOERTZEL_N * sizeof(double));
    for (int i = 0; i < window_count; i++) 
    {
        goertzel_filter(&audio_samples[i * L], L, c, hamming_window, &magnitudes[i]);
    }

    double median_magnitude = compute_median(magnitudes, window_count);

    free(hamming_window);
    return median_magnitude;
}
