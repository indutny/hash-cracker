#include "src/brute-gpu.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

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

    fprintf(stderr, "  [%d] %.*s, units=%d, freq=%d\n",
            (int) i, (int) name_len, name, (int) compute_units, (int) freq);
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


int main() {
  brute_state_t st;
  brute_state_options_t options;

  static unsigned int key_count = 17;
  static unsigned int probe_count = 96;
  static unsigned int dataset[] = {
  0x2121212b,0x21212124,0x21212126,0x21212162,0x2121217c,0x21212127,0x21212125,0x21212121,
  0x2121212a,0x2121212d,0x21212160,0x2121212e,0x21212161,0x21212123,0x2121215e,0x2121215f,
  0x2121217e,
  0x21217223,0x2123256e,0x21246577,0x2123652b,0x21246d2e,0x21256275,0x21262373,0x21265e69,
  0x2127747a,0x212a2b6b,0x212b646d,0x212a6463,0x212b6f67,0x212d657e,0x212e6d74,0x212e242e,
  0x215f2a25,0x215e2671,0x215f746d,0x215f6921,0x217c672a,0x217c606f,0x217e6723,0x217e682e,
  0x21617068,0x21622760,0x21627264,0x21626721,0x2164636d,0x2163716f,0x2165777c,0x21656777,
  0x21666174,0x21666f5e,0x21672e62,0x21677524,0x21692424,0x21697c77,0x21697864,0x216a7164,
  0x216c2562,0x216b236f,0x216c7323,0x216c786f,0x216d6d2d,0x216e5f77,0x216e7671,0x216f5f73,
  0x21706f7a,0x21707524,0x21716b6e,0x21717624,0x21736565,0x2173672b,0x21746171,0x21737624,
  0x21752778,0x21756e5f,0x21766365,0x2176676b,0x21786870,0x21776b2d,0x21795f63,0x21796961,
  0x217a6c65,0x217a7060,0x23216070,0x23216979,0x2323717e,0x23242b74,0x23247673,0x23256e2e,
  0x23266a68,0x2327247e,0x23272d5f,0x2327746f,0x232a6571,0x232a6b6c,0x232d712e,0x232d6524,
  0x232e746b,0x232e7764,0x235e757c,0x235e5e7e,0x235f6b23,0x23605f2b,0x23606b6b,0x237c682a,
  0x237e7921,0x237e5f72,0x23616c70,0x23622324,0x23632e69,0x23627062,0x2364676b,0x23637663
  };

  options.device = 1;
  options.dataset = dataset;
  options.key_count = key_count;
  options.probe_count = probe_count;

  if (0 != brute_state_init(&st, &options))
    return -1;

  if (0 != brute_run(&st))
    return -1;

  if (0 != brute_state_destroy(&st))
    return -1;

  return 0;
}
