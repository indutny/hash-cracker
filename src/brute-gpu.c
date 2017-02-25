#include "src/brute-gpu.h"

#include <assert.h>
#include <stdio.h>
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

static const char* kBruteGPUPrograms[] = { brute_gpu_program };
static size_t kBruteGPUProgramLengths[] = { sizeof(brute_gpu_program) - 1 };


static int brute_report_build_log(brute_state_t* st, const char* type) {
  cl_int err;
  cl_uint i;

  fprintf(stderr, "%s log:\n", type);
  for (i = 0; i < st->device_count; i++) {
    char log[16384];
    size_t log_size;

    err = clGetProgramBuildInfo(st->program, st->devices[i],
        CL_PROGRAM_BUILD_LOG, sizeof(log), log, &log_size);
    OPENCL_CHECK(err, "clGetProgramBuildInfo");

    fprintf(stderr, "%.*s\n", (int) log_size, log);
  }
  return 0;
}


int brute_state_init(brute_state_t* st,
                     brute_state_options_t* options) {
  cl_int err;
  cl_uint platform_count;
  cl_uint i;

  st->key_count = options->key_count;
  st->probe_count = options->probe_count;
  st->dataset_size = st->key_count + st->probe_count;
  st->device = options->device;

  err = clGetPlatformIDs(1, &st->platform, &platform_count);
  OPENCL_CHECK(err, "clGetPlatformIDs");

  if (platform_count < 1) {
    fprintf(stderr, "No OpenCL platforms to run on\n");
    return -1;
  }

  err = clGetDeviceIDs(st->platform, CL_DEVICE_TYPE_GPU,
                       ARRAY_SIZE(st->devices), st->devices, &st->device_count);
  OPENCL_CHECK(err, "clGetDeviceIDs");

  if (st->device_count < 1) {
    fprintf(stderr, "No OpenCL devices to run on\n");
    return -1;
  }

  fprintf(stderr, "Found %d devices:\n", (int) st->device_count);
  for (i = 0; i < st->device_count; i++) {
    char name[1024];
    size_t name_len;
    cl_uint compute_units;
    cl_uint freq;

    err = clGetDeviceInfo(st->devices[i], CL_DEVICE_NAME, sizeof(name), name,
                          &name_len);
    OPENCL_CHECK(err, "clGetDeviceInfo");

    err = clGetDeviceInfo(st->devices[i], CL_DEVICE_MAX_COMPUTE_UNITS,
                          sizeof(compute_units), &compute_units,
                          NULL);
    OPENCL_CHECK(err, "clGetDeviceInfo(MAX_COMPUTE_UNITS)");

    err = clGetDeviceInfo(st->devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY,
                          sizeof(freq), &freq,
                          NULL);
    OPENCL_CHECK(err, "clGetDeviceInfo(MAX_CLOCK_FREQUENCY)");

    fprintf(stderr, "  %s [%d] %.*s, units=%d, freq=%d\n",
            st->device == i ? "*" : " ",
            (int) i, (int) name_len, name, (int) compute_units, (int) freq);
  }

  if (st->device < 0 || st->device >= st->device_count) {
    fprintf(stderr, "Invalid device %d\n", st->device);
    return -1;
  }

  st->context =
      clCreateContext(0, st->device_count, st->devices, NULL, NULL, &err);
  OPENCL_CHECK(err, "clCreateContext");

  st->program =
      clCreateProgramWithSource(st->context, ARRAY_SIZE(kBruteGPUPrograms),
                                kBruteGPUPrograms, kBruteGPUProgramLengths,
                                &err);
  OPENCL_CHECK(err, "clCreateProgramWithSource");

  {
    char options[1024];

    snprintf(options, sizeof(options),
             "-DBRUTE_KEY_COUNT=%d "
             "-DBRUTE_DATASET_SIZE=%d "
             "-cl-strict-aliasing ",
             st->key_count, st->dataset_size);

    err = clBuildProgram(st->program, st->device_count, st->devices,
                         options, NULL, NULL);
    brute_report_build_log(st, "Build");
    OPENCL_CHECK(err, "clBuildProgram");
  }

  st->dataset = clCreateBuffer(
      st->context,
      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_NO_ACCESS,
      (options->key_count + options->probe_count) * sizeof(*options->dataset),
      (void*) options->dataset, &err);
  OPENCL_CHECK(err, "clCreateBuffer(dataset)");

  for (i = 0; i < st->device_count; i++) {
    st->queues[i] = clCreateCommandQueue(st->context, st->devices[i],
#ifdef BRUTE_PROFILING
        CL_QUEUE_PROFILING_ENABLE,
#else
        0,
#endif  /* BRUTE_PROFILING */
        &err);
    OPENCL_CHECK(err, "clCreateCommandQueue");
  }

  return 0;
}


int brute_state_destroy(brute_state_t* st) {
  cl_int err;
  cl_uint i;

  for (i = 0; i < st->device_count; i++) {
    err = clReleaseCommandQueue(st->queues[i]);
    OPENCL_CHECK(err, "clCreateCommandQueue");
  }

  err = clReleaseMemObject(st->dataset);
  OPENCL_CHECK(err, "clReleaseBuffer(dataset)");

  err = clReleaseProgram(st->program);
  OPENCL_CHECK(err, "clReleaseCheck");

  err = clReleaseContext(st->context);
  OPENCL_CHECK(err, "clReleaseCheck");

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

  sect->result_count = BRUTE_SECTION_SIZE / 4;

  sect->results = clCreateBuffer(
      st->context,
      CL_MEM_WRITE_ONLY,
      sect->result_count * sizeof(brute_result_t),
      NULL, &err);
  OPENCL_CHECK(err, "clCreateBuffer(results)");

  /* Storage for host results */
  sect->host_results = malloc(sect->result_count * sizeof(*sect->host_results));
  if (sect->host_results == NULL)
    return -1;

  return 0;
}


int brute_section_destroy(brute_section_t* sect) {
  cl_int err;

  free(sect->host_results);
  sect->host_results = NULL;

  err = clReleaseMemObject(sect->results);
  OPENCL_CHECK(err, "clReleaseMemObject(sect)");

  return 0;
}


int brute_section_enqueue(brute_state_t* st,
                          brute_section_t* sect,
                          unsigned int seed_off) {
  size_t global_size;
  cl_int err;
  cl_kernel kernel;

  kernel = clCreateKernel(st->program, "brute_wide_map", &err);
  OPENCL_CHECK(err, "clCreateKernel(wide_map)");

  err |= clSetKernelArg(kernel, 0, sizeof(seed_off), &seed_off);
  err |= clSetKernelArg(kernel, 1, sizeof(st->dataset), &st->dataset);
  err |= clSetKernelArg(kernel, 2, sizeof(sect->results), &sect->results);
  OPENCL_CHECK(err, "clSetKernelArg(wide_map)");

  global_size = sect->result_count;
  err = clEnqueueNDRangeKernel(st->queues[st->device], kernel,
                               1,
                               NULL,
                               &global_size, NULL,
                               0, NULL, &sect->event);
  clReleaseKernel(kernel);
  OPENCL_CHECK(err, "clEnqueueNDRangeKernel(wide_map)");

#ifdef BRUTE_PROFILING
  clWaitForEvents(1, &sect->event);

  brute_log_event_time(sect->event, "wide_map");
#endif  /* BRUTE_PROFILING */

  return 0;
}


int brute_section_get_result(brute_state_t* st, brute_section_t* sect,
                             brute_result_t* result) {
  cl_int err;
  unsigned int i;

  err = clEnqueueReadBuffer(st->queues[st->device],
                            sect->results,
                            CL_BLOCKING,
                            0,
                            sect->result_count * sizeof(*sect->host_results),
                            sect->host_results,
                            1, &sect->event, NULL);
  OPENCL_CHECK(err, "clEnqueueReadBuffer");

  result->score = 0;
  result->seed = 0;
  for (i = 0; i < sect->result_count; i++) {
    if (sect->host_results[i].score < result->score)
      continue;
    *result = sect->host_results[i];
  }

  return 0;
}


int brute_run(brute_state_t* st) {
  unsigned int section_count;
  brute_section_t sect;
  brute_result_t best;
  unsigned int i;
  unsigned int percent_part;

  section_count = BRUTE_SEED_LIMIT / BRUTE_SECTION_SIZE;
  percent_part = section_count / 100;

  if (0 != brute_section_init(st, &sect))
    return -1;

  best.score = 0;
  best.seed = 0;
  for (i = 0; i < section_count; i++) {
    brute_result_t result;
    int err;
    unsigned int seed_off;
#ifdef BRUTE_PROFILING
    struct timeval tv_start;
    struct timeval tv_end;

    gettimeofday(&tv_start, NULL);
#endif  /* BRUTE_PROFILING */

    seed_off = i * BRUTE_SECTION_SIZE;
    if (0 != brute_section_enqueue(st, &sect, seed_off))
      return -1;

    err = brute_section_get_result(st, &sect, &result);

#ifdef BRUTE_PROFILING
    gettimeofday(&tv_end, NULL);
    fprintf(stderr, "total=%fms\n",
            (tv_end.tv_sec - tv_start.tv_sec) * 1e3 +
                (tv_end.tv_usec - tv_start.tv_usec) / 1e6);
#endif  /* BRUTE_PROFILING */

    if (i % percent_part == 0)
      fprintf(stderr, "[%02d%%]\n", (i * 100) / section_count);
    if (err != 0)
      continue;

    if (result.score < best.score)
      continue;

    best = result;
  }

  fprintf(stderr, "seed=%08x score=%d\n", best.seed, (int) best.score);
  brute_section_destroy(&sect);

  return 0;
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

  return result;
}


int main(int argc, char** argv) {
  brute_state_t st;
  brute_state_options_t options;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s device_id dataset\n", argv[0]);
    fprintf(stderr, "Not enough arguments\n");
    fprintf(stderr, "Run '%s -1 :' to get the list of devices\n", argv[0]);
    return -1;
  }

  options.device = atoi(argv[1]);
  options.dataset = brute_parse_dataset(argv[2], &options.key_count,
                                        &options.probe_count);

  if (0 != brute_state_init(&st, &options))
    return -1;

  if (0 != brute_run(&st))
    return -1;

  if (0 != brute_state_destroy(&st))
    return -1;

  return 0;
}
