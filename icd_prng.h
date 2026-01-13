/*
 * ICD-PRNG v1.0 — Header-Only Library
 * 
 * High-quality PRNG for Monte Carlo simulations.
 * 1024-bit state, passes BigCrush and PractRand 2.4TB+.
 * 
 * PLATFORM: Optimized for 64-bit architectures (x86-64, ARM64).
 *           Works on 32-bit but significantly slower.
 * 
 * Usage:
 *   #include "icd_prng.h"
 *   
 *   ICD_PRNG rng;
 *   icd_init(&rng, "my_seed", 7);
 *   double x = icd_next_double(&rng);
 * 
 * WARNING: This is NOT a cryptographic RNG!
 * For crypto, use ChaCha20 or AES-CTR.
 * 
 * Author: Ivan Blagopoluchnyi, 2026
 * License: MIT
 */

#ifndef ICD_PRNG_H
#define ICD_PRNG_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                              CONSTANTS                                      */
/* ═══════════════════════════════════════════════════════════════════════════ */

/*
 * MIX constant from MurmurHash3 finalizer / SplitMix64.
 * Properties: odd, ~50% bit density, proven good avalanche.
 */
#define ICD_MIX_CONST       0xff51afd7ed558ccdULL

/*
 * MIX shift amount — optimized for 64-bit avalanche effect.
 * Standard value used in MurmurHash3, SplitMix64, etc.
 */
#define ICD_MIX_SHIFT       33

/*
 * SHA-512 initialization vector constants.
 * Derived from fractional parts of square roots of first 8 primes.
 * Source: FIPS 180-4, well-studied, no hidden structure.
 */
#define ICD_SHA512_IV_0     0x6a09e667f3bcc908ULL  /* frac(sqrt(2))  */
#define ICD_SHA512_IV_1     0xbb67ae8584caa73bULL  /* frac(sqrt(3))  */
#define ICD_SHA512_IV_2     0x3c6ef372fe94f82bULL  /* frac(sqrt(5))  */
#define ICD_SHA512_IV_3     0xa54ff53a5f1d36f1ULL  /* frac(sqrt(7))  */
#define ICD_SHA512_IV_4     0x510e527fade682d1ULL  /* frac(sqrt(11)) */
#define ICD_SHA512_IV_5     0x9b05688c2b3e6c1fULL  /* frac(sqrt(13)) */
#define ICD_SHA512_IV_6     0x1f83d9abfb41bd6bULL  /* frac(sqrt(17)) */
#define ICD_SHA512_IV_7     0x5be0cd19137e2179ULL  /* frac(sqrt(19)) */

/*
 * Golden ratio constant (variant used in hash functions).
 * Based on 2^64/φ where φ = (1+√5)/2, with low bits adjusted.
 * Used for state diversification. Well-distributed.
 */
#define ICD_GOLDEN_RATIO    0x9e3779b97f4a7c15ULL

/*
 * Pi-based diversification constant (variant used in hash functions).
 * Based on 2^64/π, with low bits adjusted for odd value.
 * Provides mixing independence from golden ratio.
 */
#define ICD_PI_CONST        0x517cc1b727220a95ULL

/*
 * Number of warmup blocks to discard after seeding.
 * Ensures full state diffusion before output.
 * 100 blocks = 6400 MIX operations, far exceeds minimum needed.
 */
#define ICD_WARMUP_BLOCKS   100

/*
 * Block size constants.
 */
#define ICD_BLOCK_U64       16      /* Block size in uint64_t units */
#define ICD_BLOCK_BYTES     128     /* Block size in bytes (16 * 8) */

/*
 * IEEE-754 conversion factors for uniform [0, 1) generation.
 * double: 53-bit mantissa, so we use 2^(-53)
 * float:  24-bit mantissa, so we use 2^(-24)
 */
#define ICD_TO_DOUBLE       0x1.0p-53   /* 2^(-53) */
#define ICD_TO_FLOAT        0x1.0p-24f  /* 2^(-24) */

/*
 * Bit shifts to extract mantissa bits from uint64.
 * For double: discard 64-53=11 bits
 * For float:  discard 64-24=40 bits
 */
#define ICD_DOUBLE_SHIFT    11
#define ICD_FLOAT_SHIFT     40

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                              TYPES                                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t a[4];                      /* State chain A */
    uint64_t b[4];                      /* State chain B */
    uint64_t c[4];                      /* State chain C */
    uint64_t d[4];                      /* State chain D */
    uint64_t counter;                   /* Block counter (injected each round) */
    uint64_t buffer[ICD_BLOCK_U64];     /* Output buffer for single-value API */
    int buf_pos;                        /* Current position in buffer */
} ICD_PRNG;

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                           INTERNAL                                          */
/* ═══════════════════════════════════════════════════════════════════════════ */

/*
 * MIX function: bijective 64-bit → 64-bit transformation.
 * Provides avalanche: each input bit affects all output bits.
 */
#define ICD_MIX(x) do { \
    (x) ^= (x) >> ICD_MIX_SHIFT; \
    (x) *= ICD_MIX_CONST; \
    (x) ^= (x) >> ICD_MIX_SHIFT; \
} while(0)

/*
 * Seed expansion hash.
 * Expands arbitrary-length seed into 64 bytes of pseudo-random state.
 * Uses SHA-512 IV as initial state for nothing-up-my-sleeve property.
 */
static inline void icd_simple_hash_(register const uint8_t * restrict in, register const size_t len, register uint8_t * restrict out) 
{
    uint64_t state[8] = 
    {
        ICD_SHA512_IV_0, ICD_SHA512_IV_1,
        ICD_SHA512_IV_2, ICD_SHA512_IV_3,
        ICD_SHA512_IV_4, ICD_SHA512_IV_5,
        ICD_SHA512_IV_6, ICD_SHA512_IV_7
    };
    
    /* Absorb input bytes */
    for (size_t i = 0; i < len; i++) 
    {
        state[i % 8] ^= (uint64_t)in[i] << ((i % 8) * 8);
        ICD_MIX(state[i % 8]);
        state[(i + 1) % 8] += state[i % 8];
    }
    
    /* Final mixing rounds for full diffusion */
    for (int round = 0; round < 16; round++) 
    {
        for (int i = 0; i < 8; i++) 
        {
            state[i] += state[(i + 1) % 8];
            ICD_MIX(state[i]);
        }
    }
    
    memcpy(out, state, 64);
}

/*
 * Core block generation function.
 * Produces 128 bytes (16 × uint64) of output per call.
 * 
 * Structure: 4 rounds, each with:
 *   - Sequential mixing within each chain (a,b,c,d)
 *   - Diagonal cross-mixing between chains
 *   - Counter injection (round 1 only)
 */
static inline void icd_block_(register ICD_PRNG * restrict ctx, register uint64_t * restrict out) 
{
    register uint64_t *a = ctx->a;
    register uint64_t *b = ctx->b;
    register uint64_t *c = ctx->c;
    register uint64_t *d = ctx->d;
    register const uint64_t cnt = ++ctx->counter;
    
    /* Round 1: with counter injection */
    a[0] += a[3] + cnt; ICD_MIX(a[0]);
    a[1] += a[0];       ICD_MIX(a[1]);
    a[2] += a[1];       ICD_MIX(a[2]);
    a[3] += a[2];       ICD_MIX(a[3]);
    
    b[0] += b[3] + cnt; ICD_MIX(b[0]);
    b[1] += b[0];       ICD_MIX(b[1]);
    b[2] += b[1];       ICD_MIX(b[2]);
    b[3] += b[2];       ICD_MIX(b[3]);
    
    c[0] += c[3] + cnt; ICD_MIX(c[0]);
    c[1] += c[0];       ICD_MIX(c[1]);
    c[2] += c[1];       ICD_MIX(c[2]);
    c[3] += c[2];       ICD_MIX(c[3]);
    
    d[0] += d[3] + cnt; ICD_MIX(d[0]);
    d[1] += d[0];       ICD_MIX(d[1]);
    d[2] += d[1];       ICD_MIX(d[2]);
    d[3] += d[2];       ICD_MIX(d[3]);
    
    /* Diagonal cross-mix 1: position [0] */
    a[0] += b[2] + c[3] + d[1];
    b[0] += a[3] + c[1] + d[2];
    c[0] += a[1] + b[3] + d[0];
    d[0] += a[2] + b[1] + c[2];
    
    /* Round 2 */
    a[0] += a[3]; ICD_MIX(a[0]);
    a[1] += a[0]; ICD_MIX(a[1]);
    a[2] += a[1]; ICD_MIX(a[2]);
    a[3] += a[2]; ICD_MIX(a[3]);
    
    b[0] += b[3]; ICD_MIX(b[0]);
    b[1] += b[0]; ICD_MIX(b[1]);
    b[2] += b[1]; ICD_MIX(b[2]);
    b[3] += b[2]; ICD_MIX(b[3]);
    
    c[0] += c[3]; ICD_MIX(c[0]);
    c[1] += c[0]; ICD_MIX(c[1]);
    c[2] += c[1]; ICD_MIX(c[2]);
    c[3] += c[2]; ICD_MIX(c[3]);
    
    d[0] += d[3]; ICD_MIX(d[0]);
    d[1] += d[0]; ICD_MIX(d[1]);
    d[2] += d[1]; ICD_MIX(d[2]);
    d[3] += d[2]; ICD_MIX(d[3]);
    
    /* Diagonal cross-mix 2: position [1] */
    a[1] += b[3] + c[0] + d[2];
    b[1] += a[0] + c[2] + d[3];
    c[1] += a[2] + b[0] + d[1];
    d[1] += a[3] + b[2] + c[3];
    
    /* Round 3 */
    a[0] += a[3]; ICD_MIX(a[0]);
    a[1] += a[0]; ICD_MIX(a[1]);
    a[2] += a[1]; ICD_MIX(a[2]);
    a[3] += a[2]; ICD_MIX(a[3]);
    
    b[0] += b[3]; ICD_MIX(b[0]);
    b[1] += b[0]; ICD_MIX(b[1]);
    b[2] += b[1]; ICD_MIX(b[2]);
    b[3] += b[2]; ICD_MIX(b[3]);
    
    c[0] += c[3]; ICD_MIX(c[0]);
    c[1] += c[0]; ICD_MIX(c[1]);
    c[2] += c[1]; ICD_MIX(c[2]);
    c[3] += c[2]; ICD_MIX(c[3]);
    
    d[0] += d[3]; ICD_MIX(d[0]);
    d[1] += d[0]; ICD_MIX(d[1]);
    d[2] += d[1]; ICD_MIX(d[2]);
    d[3] += d[2]; ICD_MIX(d[3]);
    
    /* Diagonal cross-mix 3: position [2] */
    a[2] += b[0] + c[1] + d[3];
    b[2] += a[1] + c[3] + d[0];
    c[2] += a[3] + b[1] + d[2];
    d[2] += a[0] + b[3] + c[0];
    
    /* Round 4 */
    a[0] += a[3]; ICD_MIX(a[0]);
    a[1] += a[0]; ICD_MIX(a[1]);
    a[2] += a[1]; ICD_MIX(a[2]);
    a[3] += a[2]; ICD_MIX(a[3]);
    
    b[0] += b[3]; ICD_MIX(b[0]);
    b[1] += b[0]; ICD_MIX(b[1]);
    b[2] += b[1]; ICD_MIX(b[2]);
    b[3] += b[2]; ICD_MIX(b[3]);
    
    c[0] += c[3]; ICD_MIX(c[0]);
    c[1] += c[0]; ICD_MIX(c[1]);
    c[2] += c[1]; ICD_MIX(c[2]);
    c[3] += c[2]; ICD_MIX(c[3]);
    
    d[0] += d[3]; ICD_MIX(d[0]);
    d[1] += d[0]; ICD_MIX(d[1]);
    d[2] += d[1]; ICD_MIX(d[2]);
    d[3] += d[2]; ICD_MIX(d[3]);
    
    /* Diagonal cross-mix 4: position [3] */
    a[3] += b[1] + c[2] + d[0];
    b[3] += a[2] + c[0] + d[1];
    c[3] += a[0] + b[2] + d[3];
    d[3] += a[1] + b[0] + c[1];
    
    /* Output: direct state copy */
    out[0]  = a[0]; out[1]  = a[1]; out[2]  = a[2]; out[3]  = a[3];
    out[4]  = b[0]; out[5]  = b[1]; out[6]  = b[2]; out[7]  = b[3];
    out[8]  = c[0]; out[9]  = c[1]; out[10] = c[2]; out[11] = c[3];
    out[12] = d[0]; out[13] = d[1]; out[14] = d[2]; out[15] = d[3];
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                            PUBLIC API                                       */
/* ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Initialize PRNG with arbitrary seed.
 * Same seed always produces same sequence (deterministic).
 * 
 * @param ctx   Pointer to PRNG context
 * @param seed  Seed bytes (any length, any content)
 * @param len   Seed length in bytes
 */
static inline void icd_init(register ICD_PRNG * restrict ctx, register const uint8_t * restrict seed, register const size_t len) 
{
    uint8_t hash[64];
    icd_simple_hash_(seed, len, hash);
    
    /* Initialize chains a,b from hash */
    memcpy(ctx->a, hash, 32);
    memcpy(ctx->b, hash + 32, 32);
    
    /* Derive chains c,d using irrational constants for independence */
    for (int i = 0; i < 4; i++) 
    {
        ctx->c[i] = ctx->a[i] ^ (ICD_GOLDEN_RATIO * (uint64_t)(i + 1));
        ctx->d[i] = ctx->b[i] ^ (ICD_PI_CONST * (uint64_t)(i + 1));
    }
    ctx->counter = 0;
    ctx->buf_pos = ICD_BLOCK_U64;  /* Force refill on first use */
    
    /* Warmup: discard initial blocks for full state diffusion */
    uint64_t dummy[ICD_BLOCK_U64];
    for (int i = 0; i < ICD_WARMUP_BLOCKS; i++) 
    {
        icd_block_(ctx, dummy);
    }
}

/**
 * Generate bulk random bytes.
 * Most efficient for large outputs (>= 128 bytes).
 * 
 * @param ctx     Pointer to PRNG context
 * @param output  Output buffer
 * @param len     Number of bytes to generate
 */
static inline void icd_generate(register ICD_PRNG * restrict ctx, register uint8_t * restrict output, register const size_t len) 
{
    size_t i = 0;
    uint64_t *out64 = (uint64_t *)output;
    
    /* Full 128-byte blocks */
    for (; i + ICD_BLOCK_BYTES <= len; i += ICD_BLOCK_BYTES) 
    {
        icd_block_(ctx, out64);
        out64 += ICD_BLOCK_U64;
    }
    
    /* Remaining bytes (partial block) */
    if (i < len) 
    {
        uint64_t block[ICD_BLOCK_U64];
        icd_block_(ctx, block);
        memcpy(output + i, block, len - i);
    }
}

/**
 * Get single uint64.
 * Uses internal buffer for efficiency (amortized cost).
 * 
 * @param ctx  Pointer to PRNG context
 * @return     Random uint64_t
 */
static inline uint64_t icd_next_u64(register ICD_PRNG * restrict ctx) 
{
    if (ctx->buf_pos >= ICD_BLOCK_U64) 
    {
        icd_block_(ctx, ctx->buffer);
        ctx->buf_pos = 0;
    }
    return ctx->buffer[ctx->buf_pos++];
}

/**
 * Get double in [0, 1).
 * Uses 53 bits for full IEEE-754 double precision mantissa.
 * 
 * @param ctx  Pointer to PRNG context
 * @return     Uniform random double in [0, 1)
 */
static inline double icd_next_double(register ICD_PRNG * restrict ctx)
{
    return (double)(icd_next_u64(ctx) >> ICD_DOUBLE_SHIFT) * ICD_TO_DOUBLE;
}

/**
 * Get float in [0, 1).
 * Uses 24 bits for full IEEE-754 float precision mantissa.
 * 
 * @param ctx  Pointer to PRNG context
 * @return     Uniform random float in [0, 1)
 */
static inline float icd_next_float(register ICD_PRNG * restrict ctx) 
{
    return (float)(icd_next_u64(ctx) >> ICD_FLOAT_SHIFT) * ICD_TO_FLOAT;
}

/**
 * Get uint64 in [min, max] (inclusive, unbiased).
 * Uses rejection sampling to eliminate modulo bias.
 * Expected iterations: < 2 for any range.
 * 
 * @param ctx  Pointer to PRNG context
 * @param min  Minimum value (inclusive)
 * @param max  Maximum value (inclusive)
 * @return     Uniform random value in [min, max]
 */
static inline uint64_t icd_range_u64(register ICD_PRNG * restrict ctx, register const uint64_t min, register const uint64_t max) 
{
    if (min >= max) 
        return min;
    
    const uint64_t range = max - min + 1;
    const uint64_t limit = (-range) % range;  /* Rejection threshold */
    
    uint64_t r;
    do 
    {
        r = icd_next_u64(ctx);
    } while (r < limit);
    
    return min + (r % range);
}

/**
 * Get int64 in [min, max] (inclusive, unbiased).
 * 
 * @param ctx  Pointer to PRNG context
 * @param min  Minimum value (inclusive)
 * @param max  Maximum value (inclusive)
 * @return     Uniform random value in [min, max]
 */
static inline int64_t icd_range_int(register ICD_PRNG * restrict ctx, register const int64_t min, register const int64_t max) 
{
    if (min >= max) 
        return min;
    
    const uint64_t range = (uint64_t)(max - min) + 1;
    const uint64_t limit = (-range) % range;
    
    uint64_t r;
    do 
    {
        r = icd_next_u64(ctx);
    } while (r < limit);
    
    return min + (int64_t)(r % range);
}

/**
 * Get random bytes (alias for icd_generate).
 * 
 * @param ctx     Pointer to PRNG context
 * @param output  Output buffer
 * @param len     Number of bytes to generate
 */
static inline void icd_bytes(register ICD_PRNG * restrict ctx, register void * restrict output, register const size_t len) 
{
    icd_generate(ctx, (uint8_t *)output, len);
}

#ifdef __cplusplus
}
#endif

#endif /* ICD_PRNG_H */
