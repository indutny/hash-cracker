#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

#include "src/common.h"

const char* keys[] = {"+!!!","$!!!","&!!!","b!!!","|!!!","'!!!","%!!!","!!!!","*!!!","-!!!","`!!!",".!!!","a!!!","#!!!","^!!!","_!!!","~!!!"};
const char* probes[] = {"#r!!","n%#!","we$!","+e#!",".m$!","ub%!","s#&!","i^&!","zt'!","k+*!","md+!","cd*!","go+!","~e-!","tm.!",".$.!","%*_!","q&^!","mt_!","!i_!","*g|!","o`|!","#g~!",".h~!","hpa!","`'b!","drb!","!gb!","mcd!","oqc!","|we!","wge!","taf!","^of!","b.g!","$ug!","$$i!","w|i!","dxi!","dqj!","b%l!","o#k!","#sl!","oxl!","-mm!","w_n!","qvn!","s_o!","zop!","$up!","nkq!","$vq!","ees!","+gs!","qat!","$vs!","x'u!","_nu!","ecv!","kgv!","phx!","-kw!","c_y!","aiy!","elz!","`pz!","p`!#","yi!#","~q##","t+$#","sv$#",".n%#","hj&#","~$'#","_-'#","ot'#","qe*#","lk*#",".q-#","$e-#","kt.#","dw.#","|u^#","~^^#","#k_#","+_`#","kk`#","*h|#","!y~#","r_~#","pla#","$#b#","i.c#","bpb#","kgd#","cvc#"};

#ifndef THREAD_COUNT
# define THREAD_COUNT 8
#endif  /* THREAD_COUNT */
#define SPLIT_SIZE (0x100000000LL / THREAD_COUNT)
#define KEY_SIZE 4


static pthread_t thread[THREAD_COUNT];
static uint32_t global_best_seed[THREAD_COUNT];
static int global_best_seed_score[THREAD_COUNT];


static uint32_t jenkins(const char* str, uint32_t seed) {
  uint32_t hash;

  hash = seed;
  for (int i = 0; i < KEY_SIZE; i++) {
    hash += (uint32_t) str[i];
    hash += hash << 10;
    hash ^= hash >> 6;
  }

  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;

  return hash & 0x3fffffff;
}


static int check(uint32_t seed) {
  int score;
  uint32_t key_hashes[ARRAY_SIZE(keys)];

  score = 0;

  for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
    key_hashes[i] = jenkins(keys[i], seed);
  }

  for (size_t i = 0; i < ARRAY_SIZE(probes); i += 2) {
    uint32_t l;
    uint32_t r;

    l = jenkins(probes[i], seed);
    r = jenkins(probes[i + 1], seed);

    for (size_t j = 0; j < ARRAY_SIZE(keys); j++) {
      if (l < key_hashes[j])
        score++;
      if (key_hashes[j] <= r)
        score++;
    }
  }

  return score;
}


void* compute(void* arg) {
  int thread_num;
  int best;
  uint32_t best_seed;
  uint32_t seed;
  float last_progress;

  thread_num = (intptr_t) arg;
  seed = thread_num * SPLIT_SIZE;

  global_best_seed[thread_num] = 0;
  global_best_seed_score[thread_num] = 0;

  best = 0;
  best_seed = 0;
  last_progress = 0;
  for (size_t off = 0; off < SPLIT_SIZE; off++, seed++) {
    int score;
    float progress;

    progress = (off * 100.0) / ((float) SPLIT_SIZE);

    if (progress - last_progress >= 1.0) {
      last_progress = progress;
      fprintf(stderr, "t=%02d p=%02d%% c=%08x | seed=%08x score=%d\n",
              thread_num, (int) last_progress, seed, best_seed, best);
    }

    score = check(seed);
    if (score <= best)
      continue;

    best = score;
    best_seed = seed;
  }

  global_best_seed[thread_num] = best_seed;
  global_best_seed_score[thread_num] = best;

  return NULL;
}


int main() {
  int err;
  uint32_t best_seed;
  int best;

  assert(ARRAY_SIZE(probes) % 2 == 0);

  for (int i = 0; i < THREAD_COUNT; i++) {
    err = pthread_create(&thread[i], NULL, compute, (void*) (intptr_t) i);
    assert(err == 0);
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    err = pthread_join(thread[i], NULL);
    assert(err == 0);
  }

  best = 0;
  best_seed = 0;
  for (int i = 0; i < THREAD_COUNT; i++) {
    if (global_best_seed_score[i] < best)
      continue;

    best = global_best_seed_score[i];
    best_seed = global_best_seed[i];
  }

  fprintf(stderr, "%08x - %d\n", best_seed, best);

  return 0;
}