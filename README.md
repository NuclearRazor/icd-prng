# ICD-PRNG

High-quality pseudorandom number generator for Monte Carlo simulations.

**ICD** = Injection-Cascade-Dissipation — named after the turbulence model that inspired its 4-chain mixing architecture.

## Features

- **1024-bit state** — huge period, safe for parallel streams
- **~1.8 GB/s** — competitive with xoshiro256++
- **Passes all tests** — BigCrush 160/160, PractRand 2.4TB+ (50 seeds)
- **Header-only** — single file, zero dependencies
- **Deterministic** — same seed = same sequence

## Quick Start

```c
#include "icd_prng.h"

ICD_PRNG rng;
icd_init(&rng, "my_seed", 7);

double x = icd_next_double(&rng);      // [0, 1)
uint64_t n = icd_next_u64(&rng);       // full 64-bit
int dice = icd_range_int(&rng, 1, 6);  // [1, 6]
```

## API

| Function | Description |
|----------|-------------|
| `icd_init(ctx, seed, len)` | Initialize with any seed |
| `icd_next_u64(ctx)` | Random uint64 |
| `icd_next_double(ctx)` | Random double [0, 1) |
| `icd_next_float(ctx)` | Random float [0, 1) |
| `icd_range_int(ctx, min, max)` | Random int in [min, max] |
| `icd_range_u64(ctx, min, max)` | Random uint64 in [min, max] |
| `icd_bytes(ctx, buf, len)` | Fill buffer with random bytes |
| `icd_generate(ctx, buf, len)` | Bulk generation (fastest) |

## Test Results

```
TestU01 SmallCrush:   15/15 passed
TestU01 Crush:       144/144 passed
TestU01 BigCrush:    160/160 passed
PractRand:           2.4 TB tested (50 seeds), 0 failures
```

## Performance

Measured on Apple M1 Pro, compiled with `gcc -O3 -march=native`:

| Generator | Speed | Notes |
|-----------|-------|-------|
| xoshiro256++ | ~2.0 GB/s | 256-bit state |
| **ICD-PRNG** | **1.82 GB/s** | 1024-bit state |
| PCG64 | ~1.2 GB/s | 128-bit state |
| MT19937 | ~0.4 GB/s | 19937-bit state |

ICD-PRNG achieves near-xoshiro speed with 4× larger state for better parallel safety.

## Build

```bash
# Example with benchmark
gcc -O3 -march=native -o example example.c -lm
./example

# Generator (for PractRand testing)
gcc -O3 -march=native -o icd_generator icd_generator.c
./icd_generator | ./RNG_test stdin64 -tlmax 1GB

# TestU01 (requires libtestu01)
gcc -O3 -march=native -o icd_testu01 tests/icd_testu01.c -ltestu01 -lprobdist -lmylib -lm
./icd_testu01 big
```

## Warning

**NOT for cryptography.** Output = internal state. For crypto, use ChaCha20 or AES-CTR.

## License

MIT
