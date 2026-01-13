/*
 * ICD-PRNG Binary Output Generator
 * 
 * Uses icd_prng.h — single source of truth.
 * 
 * Build:
 *   gcc -O3 -march=native -o icd_generator icd_generator.c
 * 
 * Usage:
 *   ./icd_generator | ./RNG_test stdin64 -tlmax 128GB
 *   ./icd_generator --seed "my_seed_123" | ./RNG_test stdin64 -tlmax 1GB
 *   ./icd_generator --seed "test" --bytes 1000000000 > data.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "icd_prng.h"

static void print_usage(const char *prog) 
{
    fprintf(stderr, "ICD-PRNG Binary Output Generator\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [options]\n\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --seed <string>   Set seed (default: ICD-PRNG-DEFAULT-SEED-2026)\n");
    fprintf(stderr, "  --bytes <n>       Generate exactly n bytes (default: infinite)\n");
    fprintf(stderr, "  --help            Show this help\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s | ./RNG_test stdin64 -tlmax 1GB\n", prog);
    fprintf(stderr, "  %s --seed \"test_001\" | ./RNG_test stdin64 -tlmax 1GB\n", prog);
    fprintf(stderr, "  %s --seed \"my_seed\" --bytes 1000000000 > data.bin\n", prog);
}

int main(int argc, char **argv) 
{
    const char *seed_str = "ICD-PRNG-DEFAULT-SEED-2026";
    size_t total_bytes = 0;  /* 0 = infinite */
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) 
    {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) 
        {
            seed_str = argv[++i];
        } 
        else if (strcmp(argv[i], "--bytes") == 0 && i + 1 < argc) 
        {
            total_bytes = strtoull(argv[++i], NULL, 10);
        } 
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    /* Initialize using icd_prng.h */
    ICD_PRNG ctx;
    icd_init(&ctx, (const uint8_t *)seed_str, strlen(seed_str));
    
    /* Generate */
    uint8_t block[128];
    size_t written = 0;
    
    if (total_bytes == 0) 
    {
        /* Infinite mode */
        while (1) 
        {
            icd_generate(&ctx, block, 128);
            if (fwrite(block, 1, 128, stdout) != 128) break;
        }
    } 
    else 
    {
        /* Fixed size */
        while (written < total_bytes) 
        {
            const size_t to_write = (total_bytes - written < 128) ? (total_bytes - written) : 128;
            icd_generate(&ctx, block, to_write);

            if (fwrite(block, 1, to_write, stdout) != to_write) 
                break;

            written += to_write;
        }
    }
    
    return 0;
}
