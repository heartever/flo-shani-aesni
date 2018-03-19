/**
 * Copyright (c) 2017 Armando Faz <armfazh@ic.unicamp.br>.
 * Institute of Computing.
 * University of Campinas, Brazil.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 2 or greater.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdint.h>
#include <shani.h>
#include <openssl/sha.h>
#include<openssl/objects.h>
#include <prng/random.h>
#include "clocks.h"

#define MAX_SIZE_BITS 7

struct seqTimings {
  uint64_t size;
  uint64_t openssl;
  uint64_t shani;
};

struct parallelTimings {
  uint64_t size;
  uint64_t _1x;
  uint64_t _2x;
  uint64_t _4x;
  uint64_t _8x;
};

void print_tableParallel(struct parallelTimings *table, int items) {
  int i;
  printf(" Figure 5: Multiple-message Hashing \n");
  printf("╔═════════╦═════════╦═════════╦═════════╦═════════╗\n");
  printf("║  bytes  ║   1x    ║   2x    ║   4x    ║   8x    ║\n");
  printf("╠═════════╩═════════╩═════════╩═════════╩═════════╣\n");
  for (i = 0; i < items; i++) {
    printf("║%9ld║%9.2f║%9.2f║%9.2f║%9.2f║\n",
           table[i].size,
           1.0*table[i]._1x/(double)table[i]._1x,
           2.0*table[i]._1x/(double)table[i]._2x,
           4.0*table[i]._1x/(double)table[i]._4x,
           8.0*table[i]._1x/(double)table[i]._8x);
  }
  printf("╚═════════╩═════════╩═════════╩═════════╩═════════╝\n");
}

#define BENCH_SIZE_1W(FUNC, IMPL)       \
  do {                                 \
    long BENCH_TIMES = 0, CYCLES = 0;  \
    for(it=0;it<MAX_SIZE_BITS;it++) {  \
      int message_size = 1<<it;        \
      BENCH_TIMES = 512-it*20;         \
      CLOCKS(FUNC(message,message_size,digest));\
      table[it].size = message_size;   \
      table[it].IMPL = CYCLES;         \
    }                                  \
  }while(0)


#define BENCH_SIZE_NW(FUNC, N)                   \
do{                                              \
    long BENCH_TIMES = 0, CYCLES = 0;            \
    unsigned long it=0;                          \
    unsigned long MAX_SIZE = 1 << MAX_SIZE_BITS; \
    uint8_t *message[N];                       \
    uint8_t *digest[N];                        \
    for(it=0;it<N;it++) {                        \
        message[it] = (uint8_t*)_mm_malloc(MAX_SIZE,ALIGN_BYTES);  \
        digest[it] = (uint8_t*)_mm_malloc(32,ALIGN_BYTES);  \
        random_bytes(message[it],MAX_SIZE);  \
    }                                        \
    for(it=0;it<MAX_SIZE_BITS;it++) {        \
        int message_size = 1<<it;            \
        BENCH_TIMES = 512-it*20;             \
        CLOCKS(FUNC(message,message_size,digest));  \
        table[it].size = message_size;   \
        table[it]._ ## N ## x = CYCLES;  \
    }                                    \
    for(it=0;it<N;it++) {                \
        _mm_free(message[it]);           \
        _mm_free(digest[it]);            \
    }                                    \
}while(0)

void bench_Nw() {
  struct parallelTimings table[MAX_SIZE_BITS];
  unsigned long it = 0;
  unsigned char digest[32];
  unsigned long MAX_SIZE = 1 << MAX_SIZE_BITS;
  unsigned char *message = (unsigned char *) _mm_malloc(MAX_SIZE, ALIGN_BYTES);

  printf("Multibuffer SEQ/AVX/AVX2/SHANI \n");

  printf("Running 1x:\n");
  BENCH_SIZE_1W(sha256_update_shani, _1x);
  printf("Running 2x:\n");
  BENCH_SIZE_NW(sha256_x2_update_shani_2x, 2);
  printf("Running 4x:\n");
  BENCH_SIZE_NW(sha256_x4_update_shani_4x, 4);
  printf("Running 8x:\n");
  BENCH_SIZE_NW(sha256_x8_update_shani_8x, 8);
  print_tableParallel(table,MAX_SIZE_BITS);
}



void print_tableSeq(struct seqTimings *table, int items) {
  int i;
  printf("    SHA256: OpenSSL vs SHANI             \n");
  printf("╔═════════╦═════════╦═════════╦═════════╗\n");
  printf("║  bytes  ║ OpenSSL ║  SHANI  ║ Speedup ║\n");
  printf("╠═════════╩═════════╩═════════╩═════════╣\n");
  for (i = 0; i < items; i++) {
    printf("║%9ld║%9ld║%9ld║%9.2f║\n",
           table[i].size, table[i].openssl,
           table[i].shani, table[i].openssl / (double) table[i].shani);
  }
  printf("╚═════════╩═════════╩═════════╩═════════╝\n");
}

void bench_1w() {
  struct seqTimings table[MAX_SIZE_BITS];
  unsigned long it = 0;
  unsigned long MAX_SIZE = 1 << MAX_SIZE_BITS;
  unsigned char *message = (unsigned char *) _mm_malloc(MAX_SIZE, ALIGN_BYTES);
  unsigned char digest[32];

  uint64_t *c = OPENSSL_ia32cap_loc();
  printf("Running OpenSSL:\n");
  BENCH_SIZE_1W(SHA256, openssl);

  // Disable SHA-NI
  c[1] &= 0x10000000;
  printf("Running OpenSSL:\n");
  BENCH_SIZE_1W(SHA256, openssl);

  printf("Running shani:\n");
  BENCH_SIZE_1W(sha256_update_shani, shani);
  print_tableSeq(table, MAX_SIZE_BITS);
  _mm_free(message);
}

int main(void) {

  printf("== Start of Benchmark ===\n");
  printf("OpenSSL version: %s\n", SSLeay_version(SSLEAY_VERSION));

  bench_1w();
  bench_Nw();
  printf("== End of Benchmark =====\n");
  return 0;
}
