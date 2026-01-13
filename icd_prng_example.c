/*
 * ICD-PRNG Example & Benchmark
 * 
 * Build:
 *   gcc -O3 -march=native -o icd_prng_example icd_prng_example.c -lm
 * 
 * Run:
 *   ./icd_prng_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "icd_prng.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* High-resolution timer */
static double get_time(void) 
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) 
{
    printf("ICD-PRNG Example & Benchmark\n");
    printf("============================\n\n");
    
    /* Initialize with string seed */
    ICD_PRNG ctx;
    icd_init(&ctx, (const uint8_t *)"my_seed_123", 11);
    
    /* 1. Generate random doubles */
    printf("1. Random doubles [0, 1):\n   ");
    for (int i = 0; i < 5; i++) 
    {
        printf("%.6f ", icd_next_double(&ctx));
    }
    printf("\n\n");
    
    /* 2. Generate random integers in range */
    printf("2. Random integers [1, 100]:\n   ");
    for (int i = 0; i < 10; i++) 
    {
        printf("%lld ", (long long)icd_range_int(&ctx, 1, 100));
    }
    printf("\n\n");
    
    /* 3. Monte Carlo Pi estimation */
    printf("3. Monte Carlo Pi estimation:\n");
    
    int inside = 0;
    int N = 10000000;
    for (int i = 0; i < N; i++) 
    {
        const double x = icd_next_double(&ctx);
        const double y = icd_next_double(&ctx);
        if (x*x + y*y <= 1.0) 
            inside++;
    }

    const double pi_est = 4.0 * inside / N;

    printf("   N = %d samples\n", N);
    printf("   Pi = %.8f (error: %.2e)\n\n", pi_est, fabs(pi_est - M_PI));
    
    /* 4. Bulk generation */
    printf("4. Raw bytes (hex):\n   ");
    uint8_t buf[32];
    icd_bytes(&ctx, buf, 32);
    for (int i = 0; i < 32; i++) 
    {
        printf("%02x", buf[i]);
    }
    printf("\n\n");
    
    /* 5. Reproducibility test */
    printf("5. Reproducibility test:\n");
    ICD_PRNG ctx1, ctx2;
    icd_init(&ctx1, (const uint8_t *)"same_seed", 9);
    icd_init(&ctx2, (const uint8_t *)"same_seed", 9);
    
    int match = 1;
    for (int i = 0; i < 1000; i++) 
    {
        if (icd_next_u64(&ctx1) != icd_next_u64(&ctx2)) 
        {
            match = 0;
            break;
        }
    }
    printf("   Same seed = same sequence: %s\n\n", match ? "OK" : "FAIL");
    
    /* ═══════════════════════════════════════════════════════════════════ */
    /*                         BENCHMARK                                   */
    /* ═══════════════════════════════════════════════════════════════════ */
    
    printf("6. Performance Benchmark:\n");
    printf("   ─────────────────────────────────────────\n");
    
    ICD_PRNG bench_ctx;
    icd_init(&bench_ctx, (const uint8_t *)"benchmark", 9);
    
    /* Bulk generation benchmark */
    {
        const size_t SIZE = 256 * 1024 * 1024;  /* 256 MB */
        uint8_t *data = (uint8_t *)malloc(SIZE);
        if (data) 
        {
            double t0 = get_time();
            icd_generate(&bench_ctx, data, SIZE);
            double t1 = get_time();
            
            double elapsed = t1 - t0;
            double speed_mbs = (SIZE / (1024.0 * 1024.0)) / elapsed;
            double speed_gbs = speed_mbs / 1024.0;
            
            printf("   Bulk (256 MB):    %.2f MB/s (%.2f GB/s)\n", speed_mbs, speed_gbs);
            
            free(data);
        }
    }
    
    /* Single uint64 benchmark */
    {
        const long long COUNT = 100000000LL;  /* 100M values */
        uint64_t sum = 0;
        
        double t0 = get_time();
        for (long long i = 0; i < COUNT; i++) 
        {
            sum += icd_next_u64(&bench_ctx);
        }
        double t1 = get_time();
        
        double elapsed = t1 - t0;
        double values_per_sec = COUNT / elapsed;
        double bytes_per_sec = (COUNT * 8.0) / elapsed;
        
        printf("   Single uint64:    %.0f M values/s (%.0f MB/s)\n",
               values_per_sec / 1e6, bytes_per_sec / (1024*1024));
        
        /* Prevent optimization */
        if (sum == 0) 
            printf("(sum=%llu)\n", (unsigned long long)sum);
    }
    
    /* Double generation benchmark */
    {
        const long long COUNT = 100000000LL;
        double sum = 0;
        
        double t0 = get_time();
        for (long long i = 0; i < COUNT; i++) 
        {
            sum += icd_next_double(&bench_ctx);
        }
        double t1 = get_time();
        
        double elapsed = t1 - t0;
        double values_per_sec = COUNT / elapsed;
        
        printf("   Double [0,1):     %.0f M values/s\n", values_per_sec / 1e6);
        
        if (sum == 0) printf("(sum=%.1f)\n", sum);
    }
    
    printf("   ─────────────────────────────────────────\n\n");
    
    return 0;
}
