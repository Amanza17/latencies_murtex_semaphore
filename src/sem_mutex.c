#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#define DURATION_MUTEX_SEC 60
#define DURATION_SEM_SEC   60
#define SLEEP_MS 10


// We define a maximum size for the sample arrays
// 50 million samples (uint64_t) are approx. 381 MiB.
#define MAX_SAMPLES (7000)

static inline uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int hold_cpu_dma_latency(void) {
    int fd = open("/dev/cpu_dma_latency", O_RDWR);
    if (fd < 0) {
        if (errno != ENOENT) {
             perror("open /dev/cpu_dma_latency");
        }
        return -1;
    }
    static int32_t target = 0;
    if (write(fd, &target, sizeof(target)) < 0) {
        perror("write /dev/cpu_dma_latency");
        close(fd);
        return -1;
    }
    return fd;
}


static int dump_csv(const char* path, const uint64_t* a, size_t n) {
    FILE* f = fopen(path, "w");
    if (!f) { perror(path); return -1; }
    for (size_t i = 0; i < n; ++i) {
        // write csv
        if (fprintf(f, "%" PRIu64 "\n", a[i]) < 0) { 
            perror("fprintf");
            fclose(f); 
            return -1; 
        }
    }
    fclose(f);
    return 0;
}

//print stats
static void compute_and_print_stats(const char* tag, const uint64_t* a, size_t n) {
    if (n == 0) {
        printf("[%s] amount=0 (No samples collected)\n", tag);
        return;
    }

    uint64_t minv = UINT64_MAX, maxv = 0;
    long double sum = 0.0L;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i]; 
        if (x < minv) minv = x;
        if (x > maxv) maxv = x;
        sum += (long double)x;
    }
    long double mean = sum / (long double)n;

    ///stdev
    long double var_sum = 0.0L;
    for (size_t i = 0; i < n; ++i) {
        long double diff = (long double)a[i] - mean;
        var_sum += diff * diff;
    }
    long double variance = var_sum / (long double)n;
    long double stddev = sqrtl(variance);

    printf("[%s] amount=%zu  min=%" PRIu64 " ns  max=%" PRIu64 " ns  avg=%.2Lf ns stddev=%.2Lf ns\n\n",
           tag, n, minv, maxv, mean,stddev);
}

//mutex measurements
static void measure_mutex(unsigned seconds, uint64_t *samples, size_t *p_size, size_t max_size) {
    pthread_mutex_t mtx;
    
    if (pthread_mutex_init(&mtx, NULL) != 0) {
        perror("pthread_mutex_init");
        *p_size = 0;
        return;
    }

    *p_size = 0; 
    uint64_t t_end = nsec_now() + (uint64_t)seconds * 1000000000ull;
    const useconds_t sleep_us = SLEEP_MS * 1000U;

    
    while (nsec_now() < t_end) {
        if (*p_size >= max_size) {
            fprintf(stderr, "WARN: Reached sample limit of %zu for MUTEX.\n", max_size);
            break;
        }

        uint64_t t0 = nsec_now();
        pthread_mutex_lock(&mtx); 
        uint64_t t1 = nsec_now();
        pthread_mutex_unlock(&mtx);
        usleep(sleep_us);

        samples[*p_size] = t1 - t0;
        (*p_size)++; 
        
        usleep(1);
    }

    pthread_mutex_destroy(&mtx);
}

//semaphore
static void measure_semaphore(unsigned seconds, uint64_t *samples, size_t *p_size, size_t max_size) {
    sem_t sem;
    if (sem_init(&sem, 0, 1) != 0) {
        perror("sem_init");
        *p_size = 0;
        return;
    }

    *p_size = 0; 
    uint64_t t_end = nsec_now() + (uint64_t)seconds * 1000000000ull;
    const useconds_t sleep_us = SLEEP_MS * 1000U;

    
    while (nsec_now() < t_end) {
        if (*p_size >= max_size) {
            fprintf(stderr, "WARN: Reached sample limit of %zu for SEMAPHORE.\n", max_size);
            break;
        }
        
        uint64_t t0 = nsec_now();
        sem_wait(&sem); 
        uint64_t t1 = nsec_now();
        sem_post(&sem);
        usleep(sleep_us);

        samples[*p_size] = t1 - t0;
        (*p_size)++; 
        
        usleep(1);
    }

    sem_destroy(&sem);
}

int main(void) {

    int dma_fd = hold_cpu_dma_latency(); 

    // mallocs
    uint64_t *mutex_arr = (uint64_t*)malloc(MAX_SAMPLES * sizeof(uint64_t));
    uint64_t *sem_arr = (uint64_t*)malloc(MAX_SAMPLES * sizeof(uint64_t));

    if (!mutex_arr || !sem_arr) {
        perror("malloc for sample arrays");
        free(mutex_arr);
        free(sem_arr);
        if (dma_fd >= 0) close(dma_fd);
        return 1;
    }
    
    size_t mutex_sz = 0;
    size_t sem_sz = 0;

    printf("Measuring %u s MUTEX (max %zu samples)...\n", DURATION_MUTEX_SEC, (size_t)MAX_SAMPLES);
    measure_mutex(DURATION_MUTEX_SEC, mutex_arr, &mutex_sz, MAX_SAMPLES);

    printf("Measuring %u s SEMAPHORE (max %zu samples)...\n", DURATION_SEM_SEC, (size_t)MAX_SAMPLES);
    measure_semaphore(DURATION_SEM_SEC, sem_arr, &sem_sz, MAX_SAMPLES);

    // print info screens
    compute_and_print_stats("mutex", mutex_arr, mutex_sz);
    compute_and_print_stats("sem",   sem_arr,   sem_sz);

    // save CSV 
    if (dump_csv("mutex_ns.csv", mutex_arr, mutex_sz) == 0)
        printf("CSV mutex_ns.csv saved (%zu samples).\n", mutex_sz);
    else
        fprintf(stderr, "Error saving mutex_ns.csv\n");

    if (dump_csv("sem_ns.csv", sem_arr, sem_sz) == 0)
        printf("CSV sem_ns.csv saved (%zu samples).\n", sem_sz);
    else
        fprintf(stderr, "Error saving sem_ns.csv\n");


    // clean
    free(mutex_arr);
    free(sem_arr);

    if (dma_fd >= 0) close(dma_fd);
    return 0;
}