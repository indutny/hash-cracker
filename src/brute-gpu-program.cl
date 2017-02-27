#if BRUTE_VEC_WIDTH == 1
# define BRUTE_UVEC uint
# define BRUTE_VEC int
#elif BRUTE_VEC_WIDTH == 2
# define BRUTE_UVEC uint2
# define BRUTE_VEC int2
#elif BRUTE_VEC_WIDTH == 4
# define BRUTE_UVEC uint4
# define BRUTE_VEC int4
#elif BRUTE_VEC_WIDTH == 8
# define BRUTE_UVEC uint8
# define BRUTE_VEC int8
#elif BRUTE_VEC_WIDTH == 16
# define BRUTE_UVEC uint16
# define BRUTE_VEC int16
#endif

struct brute_result_s {
  uint seed;
  int score;
};

static inline BRUTE_UVEC v8_jenkins(const BRUTE_UVEC input,
                                    const BRUTE_UVEC seed) {
  BRUTE_UVEC hash;
  BRUTE_UVEC p;

  hash = seed;
  p = input;

  for (uint i = 0; i < 4; i++) {
    hash += p & 0xff;
    hash += hash << 10;
    hash ^= hash >> 6;
    p >>= 8;
  }

  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;

  return hash & 0x3fffffff;
}


__kernel void brute_wide_map(const uint seed_off,
                             __constant const uint* dataset,
                             __global struct brute_result_s* results) {
  uint seed_start;
  BRUTE_UVEC seed;
  int gid;
  BRUTE_UVEC hashes[BRUTE_KEY_COUNT];
  BRUTE_VEC score;
  struct brute_result_s best;

  gid = get_global_id(0);

  seed_start = seed_off + gid * BRUTE_VEC_WIDTH;
  seed = seed_start;
  for (uint i = 0; i < BRUTE_VEC_WIDTH; i++)
    seed[i] += i;

  /* Compute hashes first */
  for (uint i = 0; i < BRUTE_KEY_COUNT; i++)
    hashes[i] = v8_jenkins(dataset[i], seed);

  /* Compute score */
  score = 0;
  for (uint i = BRUTE_KEY_COUNT; i < BRUTE_DATASET_SIZE; i += 2) {
    BRUTE_UVEC left_hash;
    BRUTE_UVEC right_hash;
    BRUTE_VEC lpos;
    BRUTE_VEC rpos;

    left_hash = v8_jenkins(dataset[i], seed);
    right_hash = v8_jenkins(dataset[i + 1], seed);

    lpos = 0;
    rpos = 0;
    for (uint j = 0; j < BRUTE_KEY_COUNT; j++) {
      BRUTE_UVEC key_hash;

      key_hash = hashes[j];
      lpos += select(BRUTE_VEC(1), BRUTE_VEC(0), left_hash < key_hash);
      rpos += select(BRUTE_VEC(1), BRUTE_VEC(0), right_hash < key_hash);
    }

    /* Counter-intuitively left should be further in the list than right */
    score += clamp(lpos - rpos, BRUTE_VEC(0), BRUTE_VEC(1));
  }

  best.score = 0;
  best.seed = 0;
  for (uint i = 0; i < BRUTE_VEC_WIDTH; i++) {
    best.seed = select(best.seed, seed[i], score[i] > best.score);
    best.score = max(best.score, score[i]);
  }
  results[gid].score = best.score;
  results[gid].seed = best.seed;
}
