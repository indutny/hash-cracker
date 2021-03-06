#ifndef SRC_BRUTE_GPU_H_
#define SRC_BRUTE_GPU_H_

#ifdef __APPLE__
#include "OpenCL/opencl.h"
#else
#include "CL/cl.h"
#endif

/* #define BRUTE_PROFILING */

#ifndef BRUTE_SEED_LIMIT
# define BRUTE_SEED_LIMIT 0x40000000ULL
#endif  /* !BRUTE_SEED_LIMIT */

#ifndef BRUTE_MAX_DEVICE_COUNT
# define BRUTE_MAX_DEVICE_COUNT 8
#endif  /* !BRUTE_MAX_DEVICE_COUNT */

#ifndef BRUTE_SECTION_SIZE
# define BRUTE_SECTION_SIZE 0x40000
#endif  /* !BRUTE_SECTION_SIZE */

#ifndef BRUTE_VEC_WIDTH
# define BRUTE_VEC_WIDTH 4
#endif  /* !BRUTE_VEC_WIDTH */

typedef struct brute_state_s brute_state_t;
typedef struct brute_state_options_s brute_state_options_t;
typedef struct brute_section_s brute_section_t;
typedef struct brute_result_s brute_result_t;
typedef struct brute_result_list_s brute_result_list_t;

struct brute_state_s {
  unsigned int key_count;
  unsigned int probe_count;
  unsigned int dataset_size;
  unsigned int best_count;

  cl_device_id device;
  cl_context context;
  cl_program program;
  cl_command_queue queue;
  cl_mem dataset;
};

struct brute_state_options_s {
  int device;
  unsigned int best_count;

  const unsigned int* dataset;
  unsigned int key_count;
  unsigned int probe_count;
};

struct brute_section_s {
  cl_kernel kernel;
  cl_event event;

  /* Pipeline of buffers (allocate once) */
  unsigned int result_count;

  cl_mem results;

  brute_result_t* host_results;
};

struct brute_result_s {
  unsigned int seed;
  int score;
};

struct brute_result_list_s {
  brute_result_t* list;
  unsigned int count;
};

static int brute_state_init(brute_state_t* st, brute_state_options_t* options);
static int brute_state_destroy(brute_state_t* st);

static int brute_log_event_time(cl_event event, const char* desc);
static int brute_section_init(brute_state_t* st, brute_section_t* sect);
static int brute_section_destroy(brute_section_t* sect);
static int brute_section_enqueue(brute_state_t* st, brute_section_t* sect,
                                 unsigned int seed_off);
static int brute_section_merge_results(brute_state_t* st, brute_section_t* sect,
                                       brute_result_list_t* global);

static int brute_result_cmp(const void* a, const void* b);
static void brute_result_list_merge(brute_result_list_t* a,
                                    brute_result_list_t* b);

static int brute_run(brute_state_t* st);

#endif  /* SRC_BRUTE_GPU_H_ */
