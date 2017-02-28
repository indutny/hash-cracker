#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "src/common.h"

static char ALPHABET[] = "abcdefghijklmnopqrstuvwxyz0123456789";

static uint32_t v8_jenkins(const unsigned char* input, const uint32_t seed) {
  uint32_t hash;
  const unsigned char* p;

  hash = seed;
  p = input;

  for (;;) {
    unsigned char c;

    c = *(p++);
    if (c == '\0')
      break;

    hash += c;
    hash += hash << 10;
    hash ^= hash >> 6;
  }

  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;

  return hash & 0x3fffffff;
}


static const unsigned char* gen_str(uint64_t value) {
  static char buffer[1024];
  int i;

  i = 0;
  while (value != 0) {
    buffer[i++] = ALPHABET[value % (sizeof(ALPHABET) - 1)];
    value /= sizeof(ALPHABET) - 1;
  }
  buffer[i] = '\0';
  return (unsigned char*) buffer;
}


int main(int argc, char** argv) {
  uint32_t seed;
  uint32_t mask;
  int count;
  uint64_t off;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <seed> <mask> <count>\n", argv[0]);
    return -1;
  }

  seed = (uint32_t) strtol(argv[1], NULL, 0);
  mask = (uint32_t) strtol(argv[2], NULL, 0);
  count = (int) strtol(argv[3], NULL, 0);

  fprintf(stderr, "seed=%08x mask=%08x\n", seed, mask);

  for (off = 1; count > 0; off++) {
    const unsigned char* value;

    value = gen_str(off);
    if ((v8_jenkins(value, seed) & mask) != 0)
      continue;

    fprintf(stdout, "%s\n", (const char*) value);
    count--;
  }

  return 0;
}
