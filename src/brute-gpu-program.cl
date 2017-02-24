struct brute_result_s {
  uint seed;
  uint score;
};

static inline uint4 v8_jenkins(const uint4 input, const uint4 seed) {
  uint4 hash;
  uint4 p;

  hash = seed;
  p = input;

  /* i = 0 */
  hash += p & 0xff;
  hash += hash << 10;
  hash ^= hash >> 6;
  p >>= 8;

  /* i = 1 */
  hash += p & 0xff;
  hash += hash << 10;
  hash ^= hash >> 6;
  p >>= 8;

  /* i = 2 */
  hash += p & 0xff;
  hash += hash << 10;
  hash ^= hash >> 6;
  p >>= 8;

  /* i = 3 */
  hash += p & 0xff;
  hash += hash << 10;
  hash ^= hash >> 6;

  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;

  return hash & 0x3fffffff;
}


__kernel void brute_wide_map(const uint seed_off,
                             __global const uint* dataset,
                             __global struct brute_result_s* results) {
  uint seed_start;
  uint4 seed;
  int gid;
  uint4 hashes[BRUTE_KEY_COUNT];
  uint4 score;
  struct brute_result_s best;

  gid = get_global_id(0);

  seed_start = seed_off + gid * 4;
  seed = seed_start;
  seed += (uint4) (1, 2, 3, 4);

  /* Compute hashes first */
  for (uint i = 0; i < BRUTE_KEY_COUNT; i++)
    hashes[i] = v8_jenkins(dataset[i], seed);

  /* Compute score */
  score = 0;
  for (uint i = BRUTE_KEY_COUNT; i < BRUTE_DATASET_SIZE; i += 2) {
    uint4 left_hash;
    uint4 right_hash;

    left_hash = v8_jenkins(dataset[i], seed);
    right_hash = v8_jenkins(dataset[i + 1], seed);

    for (uint j = 0; j < BRUTE_KEY_COUNT; j++) {
      uint4 key_hash;

      key_hash = hashes[j];
      score += select(uint4(0), 1, left_hash < key_hash);
      score += select(uint4(0), 1, key_hash <= right_hash);
    }
  }

  best.score = 0;
  best.seed = 0;
  for (uint i = 0; i < 4; i++) {
    best.seed = select(best.seed, seed[i], score[i] > best.score);
    best.score = max(best.score, score[i]);
  }
  results[gid].score = best.score;
  results[gid].seed = best.seed;
}
