/*
 * ICD-PRNG TestU01 Interface
 * 
 * Uses icd_prng.h — single source of truth.
 * 
 * Build (requires TestU01 installed):
 *   gcc -O3 -march=native -o icd_testu01 icd_testu01.c -ltestu01 -lprobdist -lmylib -lm
 * 
 * Usage:
 *   ./icd_testu01 small                    # SmallCrush (~10 sec)
 *   ./icd_testu01 crush                    # Crush (~30 min)
 *   ./icd_testu01 big                      # BigCrush (~4 hours)
 *   ./icd_testu01 big --seed "my_seed"     # BigCrush with custom seed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../icd_prng.h"

/* TestU01 headers */
#include "unif01.h"
#include "bbattery.h"

/* Global context for TestU01 callback */
static ICD_PRNG g_ctx;

/* TestU01 callback: returns double in [0, 1) */
static double icd_testu01_double(void) {
    return icd_next_double(&g_ctx);
}

/* TestU01 callback: returns 32-bit value */
static unsigned int icd_testu01_bits(void) {
    return (unsigned int)(icd_next_u64(&g_ctx) >> 32);
}

static void print_usage(const char *prog) {
    fprintf(stderr, "ICD-PRNG TestU01 Interface\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <test> [--seed <string>]\n\n", prog);
    fprintf(stderr, "Tests:\n");
    fprintf(stderr, "  small   SmallCrush (15 tests, ~10 sec)\n");
    fprintf(stderr, "  crush   Crush (144 tests, ~30 min)\n");
    fprintf(stderr, "  big     BigCrush (160 tests, ~4 hours)\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s small\n", prog);
    fprintf(stderr, "  %s big --seed \"test_001\"\n", prog);
}

int main(int argc, char **argv) {
    const char *test_name = NULL;
    const char *seed_str = "ICD-PRNG-TESTU01-DEFAULT";
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed_str = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (!test_name) {
            test_name = argv[i];
        }
    }
    
    if (!test_name) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Initialize PRNG */
    icd_init(&g_ctx, (const uint8_t *)seed_str, strlen(seed_str));
    
    /* Create TestU01 generator */
    unif01_Gen *gen = unif01_CreateExternGenBits("ICD-PRNG", icd_testu01_bits);
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                   ICD-PRNG TestU01                            ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Seed: %-54s ║\n", seed_str);
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    /* Run selected test */
    if (strcmp(test_name, "small") == 0) {
        printf("Running SmallCrush (15 tests)...\n\n");
        bbattery_SmallCrush(gen);
    } else if (strcmp(test_name, "crush") == 0) {
        printf("Running Crush (144 tests)...\n\n");
        bbattery_Crush(gen);
    } else if (strcmp(test_name, "big") == 0) {
        printf("Running BigCrush (160 tests)...\n\n");
        bbattery_BigCrush(gen);
    } else {
        fprintf(stderr, "Unknown test: %s\n", test_name);
        fprintf(stderr, "Use: small, crush, or big\n");
        unif01_DeleteExternGenBits(gen);
        return 1;
    }
    
    /* Cleanup */
    unif01_DeleteExternGenBits(gen);
    
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                      TEST COMPLETE                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
