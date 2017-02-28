#include "src/brute-gpu.h"

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef BRUTE_PROFILING
# include <sys/time.h>
#endif  /* BRUTE_PROFILING */

#include "src/common.h"
#include "src/brute-gpu-program.h"

#define OPENCL_CHECK(err, msg)                                                \
    do {                                                                      \
      cl_int t_err = (err);                                                   \
      if (t_err != CL_SUCCESS) {                                              \
        fprintf(stderr, "OpenCL failure: " msg " err=%d\n", t_err);           \
        return -1;                                                            \
      }                                                                       \
    } while (0)

#define OPENCL_CHECK_GOTO(err, msg, LABEL)                                    \
    do {                                                                      \
      cl_int t_err = (err);                                                   \
      if (t_err != CL_SUCCESS) {                                              \
        fprintf(stderr, "OpenCL failure: " msg " err=%d\n", t_err);           \
        goto LABEL;                                                           \
      }                                                                       \
    } while (0)

static const char* kBruteGPUPrograms[] = { brute_gpu_program };
static size_t kBruteGPUProgramLengths[] = { sizeof(brute_gpu_program) - 1 };


static int brute_report_build_log(brute_state_t* st, const char* type) {
  cl_int err;
  char* log;
  size_t log_size;

  fprintf(stderr, "%s log:\n", type);

  err = clGetProgramBuildInfo(st->program, st->device,
      CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
  OPENCL_CHECK(err, "clGetProgramBuildInfo (query)");

  log = malloc(log_size);
  if (log == NULL)
    return -1;

  err = clGetProgramBuildInfo(st->program, st->device,
      CL_PROGRAM_BUILD_LOG, log_size, log, &log_size);
  OPENCL_CHECK_GOTO(err, "clGetProgramBuildInfo", fail_get_info);

  fprintf(stderr, "%.*s\n", (int) log_size, log);
  free(log);

  return 0;

fail_get_info:
  free(log);
  return -1;
}


int brute_state_init(brute_state_t* st,
                     brute_state_options_t* options) {
  cl_int err;
  cl_uint platform_count;
  cl_uint i;
  cl_platform_id platform;
  cl_device_id devices[BRUTE_MAX_DEVICE_COUNT];
  cl_uint device_count;

  st->key_count = options->key_count;
  st->probe_count = options->probe_count;
  st->dataset_size = st->key_count + st->probe_count;
  st->best_count = options->best_count;

  err = clGetPlatformIDs(1, &platform, &platform_count);
  OPENCL_CHECK(err, "clGetPlatformIDs");

  if (platform_count < 1) {
    fprintf(stderr, "No OpenCL platforms to run on\n");
    return -1;
  }

  err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU,
                       ARRAY_SIZE(devices), devices, &device_count);
  OPENCL_CHECK(err, "clGetDeviceIDs");

  if (device_count < 1) {
    fprintf(stderr, "No OpenCL devices to run on\n");
    return -1;
  }

  fprintf(stderr, "Found %d devices:\n", (int) device_count);
  for (i = 0; i < device_count; i++) {
    char name[1024];
    size_t name_len;
    cl_uint compute_units;
    cl_uint freq;

    err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(name), name,
                          &name_len);
    OPENCL_CHECK(err, "clGetDeviceInfo");

    err = clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS,
                          sizeof(compute_units), &compute_units,
                          NULL);
    OPENCL_CHECK(err, "clGetDeviceInfo(MAX_COMPUTE_UNITS)");

    err = clGetDeviceInfo(devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY,
                          sizeof(freq), &freq,
                          NULL);
    OPENCL_CHECK(err, "clGetDeviceInfo(MAX_CLOCK_FREQUENCY)");

    fprintf(stderr, "  %s [%d] %.*s, units=%d, freq=%d\n",
            options->device == (int) i ? "*" : " ",
            (int) i, (int) name_len, name, (int) compute_units, (int) freq);
  }

  /* Just printing devices */
  if (options->device < 0)
    return -1;

  if (options->device >= (int) device_count) {
    fprintf(stderr, "Invalid device %d\n", options->device);
    return -1;
  }

  st->device = devices[options->device];

  st->context =
      clCreateContext(0, 1, &st->device, NULL, NULL, &err);
  OPENCL_CHECK(err, "clCreateContext");

  st->program =
      clCreateProgramWithSource(st->context, ARRAY_SIZE(kBruteGPUPrograms),
                                kBruteGPUPrograms, kBruteGPUProgramLengths,
                                &err);
  OPENCL_CHECK_GOTO(err, "clCreateProgramWithSource", fail_create_program);

  {
    char options[1024];

    snprintf(options, sizeof(options),
             "-DBRUTE_KEY_COUNT=%d "
             "-DBRUTE_DATASET_SIZE=%d "
             "-DBRUTE_VEC_WIDTH=%d "
             "-cl-strict-aliasing ",
             st->key_count, st->dataset_size, BRUTE_VEC_WIDTH);

    err = clBuildProgram(st->program, 1, &st->device,
                         options, NULL, NULL);
    brute_report_build_log(st, "Build");
    OPENCL_CHECK_GOTO(err, "clBuildProgram", fail_build_program);
  }

  st->dataset = clCreateBuffer(
      st->context,
      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_NO_ACCESS,
      (options->key_count + options->probe_count) * sizeof(*options->dataset),
      (void*) options->dataset, &err);
  OPENCL_CHECK_GOTO(err, "clCreateBuffer(dataset)", fail_build_program);

  st->queue = clCreateCommandQueue(st->context, st->device,
#ifdef BRUTE_PROFILING
      CL_QUEUE_PROFILING_ENABLE,
#else
      0,
#endif  /* BRUTE_PROFILING */
      &err);
  OPENCL_CHECK_GOTO(err, "clCreateCommandQueue", fail_create_queue);

  return 0;

fail_create_queue:
  clReleaseMemObject(st->dataset);

fail_build_program:
  clReleaseProgram(st->program);

fail_create_program:
  clReleaseContext(st->context);
  return -1;
}


int brute_state_destroy(brute_state_t* st) {
  clReleaseCommandQueue(st->queue);
  clReleaseMemObject(st->dataset);
  clReleaseProgram(st->program);
  clReleaseContext(st->context);

  return 0;
}


int brute_log_event_time(cl_event event, const char* desc) {
  cl_int err;
  cl_ulong start;
  cl_ulong end;

  err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START,
                                sizeof(start), &start, NULL);
  OPENCL_CHECK(err, "clGetEventProfilingInfo(start)");
  err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END,
                                sizeof(end), &end, NULL);
  OPENCL_CHECK(err, "clGetEventProfilingInfo(end)");

  fprintf(stderr, "%s delta=%fms start=%llu end=%llu\n", desc,
          (end - start) / 1e6, start, end);

  return 0;
}


int brute_section_init(brute_state_t* st, brute_section_t* sect) {
  cl_int err;
  cl_kernel kernel;

  sect->result_count = BRUTE_SECTION_SIZE / BRUTE_VEC_WIDTH;

  sect->results = clCreateBuffer(
      st->context,
      CL_MEM_WRITE_ONLY,
      sect->result_count * sizeof(brute_result_t),
      NULL, &err);
  OPENCL_CHECK(err, "clCreateBuffer(results)");

  /* Storage for host results */
  sect->host_results = malloc(sect->result_count * sizeof(*sect->host_results));
  if (sect->host_results == NULL)
    goto fail_alloc_results;

  kernel = clCreateKernel(st->program, "brute_wide_map", &err);
  OPENCL_CHECK_GOTO(err, "clCreateKernel(wide_map)", fail_create_kernel);

  err = clSetKernelArg(kernel, 1, sizeof(st->dataset), &st->dataset);
  err |= clSetKernelArg(kernel, 2, sizeof(sect->results), &sect->results);
  OPENCL_CHECK_GOTO(err, "clSetKernelArg(wide_map)", fail_set_arg);

  sect->kernel = kernel;

  return 0;

fail_set_arg:
  clReleaseKernel(kernel);

fail_create_kernel:
  free(sect->host_results);

fail_alloc_results:
  clReleaseMemObject(sect->results);
  return -1;
}


int brute_section_destroy(brute_section_t* sect) {
  free(sect->host_results);
  sect->host_results = NULL;

  clReleaseMemObject(sect->results);
  return 0;
}


int brute_section_enqueue(brute_state_t* st,
                          brute_section_t* sect,
                          unsigned int seed_off) {
  size_t global_size;
  cl_int err;

  err = clSetKernelArg(sect->kernel, 0, sizeof(seed_off), &seed_off);
  OPENCL_CHECK(err, "clSetKernelArg(wide_map)");

  global_size = sect->result_count;
  err = clEnqueueNDRangeKernel(st->queue, sect->kernel,
                               1,
                               NULL,
                               &global_size, NULL,
                               0, NULL, &sect->event);
  OPENCL_CHECK(err, "clEnqueueNDRangeKernel(wide_map)");

#ifdef BRUTE_PROFILING
  clWaitForEvents(1, &sect->event);

  brute_log_event_time(sect->event, "wide_map");
#endif  /* BRUTE_PROFILING */

  return 0;
}


int brute_result_cmp(const void* a, const void* b) {
  return ((const brute_result_t*) b)->score -
         ((const brute_result_t*) a)->score;
}


void brute_result_list_merge(brute_result_list_t* a,
                             brute_result_list_t* b) {
  unsigned int i;
  unsigned int j;

  for (i = 0; i < b->count; i++) {
    /* Find insertion spot */
    for (j = a->count; j > 0; j--)
      if (a->list[j - 1].score > b->list[i].score)
        break;

    if (j == a->count)
      continue;

    /* Shift everything to the right */
    memmove(&a->list[j + 1], &a->list[j], a->count - j - 1);

    /* Copy */
    a->list[j] = b->list[i];
  }
}


int brute_section_merge_results(brute_state_t* st, brute_section_t* sect,
                                brute_result_list_t* global) {
  cl_int err;
  brute_result_list_t local;

  local.list = sect->host_results;
  local.count = sect->result_count;
  clWaitForEvents(1, &sect->event);

  err = clEnqueueReadBuffer(st->queue,
                            sect->results,
                            CL_BLOCKING,
                            0,
                            local.count * sizeof(*local.list),
                            local.list,
                            1, &sect->event, NULL);
  OPENCL_CHECK(err, "clEnqueueReadBuffer");

  brute_result_list_merge(global, &local);

  return 0;
}


int brute_run(brute_state_t* st) {
  unsigned int section_count;
  brute_section_t sect;
  brute_result_list_t result_list;
  unsigned int i;
  unsigned int percent_part;

  section_count = BRUTE_SEED_LIMIT / BRUTE_SECTION_SIZE;
  percent_part = section_count / 100;

  if (0 != brute_section_init(st, &sect))
    return -1;

  result_list.count = st->best_count;
  result_list.list = calloc(result_list.count, sizeof(*result_list.list));
  if (result_list.list == NULL)
    goto fail_alloc_list;

  for (i = 0; i < section_count; i++) {
    unsigned int seed_off;
#ifdef BRUTE_PROFILING
    struct timeval tv_start;
    struct timeval tv_end;

    gettimeofday(&tv_start, NULL);
#endif  /* BRUTE_PROFILING */

    seed_off = i * BRUTE_SECTION_SIZE;
    if (0 != brute_section_enqueue(st, &sect, seed_off))
      goto fail_enqueue;

    brute_section_merge_results(st, &sect, &result_list);

#ifdef BRUTE_PROFILING
    gettimeofday(&tv_end, NULL);
    fprintf(stderr, "total=%fms\n",
            (tv_end.tv_sec - tv_start.tv_sec) * 1e3 +
                (tv_end.tv_usec - tv_start.tv_usec) / 1e6);
#endif  /* BRUTE_PROFILING */

    if (i % percent_part == 0) {
      fprintf(stderr, "\r[%02d%%]", (i * 100) / section_count);
    }
  }

  fprintf(stderr, "\n");
  for (i = 0; i < result_list.count; i++) {
    brute_result_t* r;

    r = &result_list.list[i];
    fprintf(stderr, "[%i] seed=%08x score=%d\n", i, r->seed, r->score);
  }

  free(result_list.list);
  brute_section_destroy(&sect);

  return 0;

fail_enqueue:
  free(result_list.list);

fail_alloc_list:
  brute_section_destroy(&sect);
  return -1;
}


static unsigned int* brute_parse_dataset(const char* value,
                                         unsigned int* key_count,
                                         unsigned int* probe_count) {
  const char* p;
  unsigned int i;
  unsigned int total;
  unsigned int acc;
  unsigned int shift;
  unsigned int* result;

  p = value;
  for (total = 1; *p != '\0'; p++)
    if (*p == ':' || *p == '@')
      total++;

  result = malloc(total * sizeof(*result));
  if (result == NULL)
    return NULL;

  acc = 0;
  p = value;
  for (i = 0, shift = 0; *p != '\0'; p++) {
    if (*p == ':' || *p == '@') {
      result[i++] = acc;
      acc = 0;
      shift = 0;

      if (*p == '@')
        *key_count = i;
    } else {
      acc |= (unsigned int) *p << shift;
      shift += 8;
    }
  }
  result[i] = acc;

  *probe_count = total - *key_count;
  if (*probe_count % 2 != 0) {
    free(result);
    return NULL;
  }

  return result;
}


static void brute_print_help(int argc, char** argv) {
  fprintf(stderr, "Usage: %s -d <device index> dataset\n", argv[0]);
  fprintf(stderr, "\n");
  fprintf(stderr, "  -d, --device  - device index to run on\n");
  fprintf(stderr, "  -l, --list    - print all available devices\n");
  fprintf(stderr, "  -b, --best    - number of best seeds to print\n");
  fprintf(stderr, "  -h, --help    - print this message\n");
  fprintf(stderr, "\n");
}


static void brute_print_devices() {
  brute_state_t st;
  brute_state_options_t options;

  options.device = -1;
  if (0 != brute_state_init(&st, &options))
    return;

  brute_state_destroy(&st);
}


static int brute_parse_argv(int argc, char** argv,
                            brute_state_options_t* options) {
  int c;
  static const char long_flags[] = "d:b:lh";
  static struct option long_options[] = {
    { "device", 1, NULL, 'd' },
    { "best", 1, NULL, 'b' },
    { "list", 0, NULL, 'l' },
    { "help", 0, NULL, 'h' },
    { NULL, 0, NULL, 0 }
  };

  memset(options, 0, sizeof(*options));
  options->device = -1;
  options->best_count = 1;

  do {
    int index;

    index = 0;
    c = getopt_long(argc, argv, long_flags, long_options, &index);
    switch (c) {
      case 'd':
        options->device = atoi(optarg);
        break;
      case 'b':
        options->best_count = atoi(optarg);
        break;
      case 'l':
        brute_print_devices();
        exit(0);
        return -1;
      case 'h':
        brute_print_help(argc, argv);
        exit(0);
        return -1;
      default:
        break;
    }
  } while (c != -1);

  if (options->device < 0) {
    brute_print_help(argc, argv);
    return -1;
  }

  if (argc <= optind) {
    brute_print_help(argc, argv);
    fprintf(stderr, "`dataset` is required argument\n");
    return -1;
  }

  options->dataset = brute_parse_dataset(argv[optind],
                                         &options->key_count,
                                         &options->probe_count);
  if (options->key_count == 0 || options->probe_count == 0) {
    brute_print_help(argc, argv);
    fprintf(stderr,
            "`dataset` must have both keys and even number of probes\n");
    free((void*) options->dataset);
    return -1;
  }

  return 0;
}


int main(int argc, char** argv) {
  brute_state_t st;
  brute_state_options_t options;

  if (0 != brute_parse_argv(argc, argv, &options))
    return -1;

  if (0 != brute_state_init(&st, &options))
    return -1;

  if (0 != brute_run(&st))
    return -1;

  if (0 != brute_state_destroy(&st))
    return -1;

  return 0;
}
