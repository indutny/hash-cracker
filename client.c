#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef __APPLE__
# include <mach/mach.h>
# include <mach/mach_time.h>
#endif  /* __APPLE__ */

static struct {
  int repeat;
  char** probes;
  int probe_count;
  char* post;
  struct sockaddr_in addr;
  double* avg;
  double* stddev;

  char buffer[16 * 1024];
  int req_len;
} state;


static void prepare_req(const char* probe) {
  state.req_len = snprintf(state.buffer, sizeof(state.buffer),
                           "HEAD / HTTP/1.1\r\n%s:.\r\n%s\r\n",
                           probe, state.post);
}


static int send_req(int fd) {
  int err;
  int len;
  const char* req;

  req = state.buffer;
  len = state.req_len;

  do {
    err = write(fd, req, len);
    if (err > 0) {
      req += err;
      len -= err;
    }
  } while (len != 0 && err == -1 && errno == EINTR);

  return err >= 0 ? 0 : -1;
}


static int recv_res(int fd) {
  int err;
  char tmp[16 * 1024];
  int count;
  static char* CRLF = "\r\n\r\n";

  count = 0;
  do {
    err = read(fd, tmp, sizeof(tmp));
    if (err > 0)
      for (int i = 0; count < 4 && i < err; i++)
        if (tmp[i] != CRLF[count++])
          count = 0;
  } while (err == -1 && errno == EINTR);

  return count == 4 ? 0 : -1;
}


static void run() {
  int fd;
  int err;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd != -1);

  do {
    err = connect(fd, (const struct sockaddr*) &state.addr,
                  sizeof(state.addr));
  } while (err == -1 && errno == EINTR);
  assert(err == 0);

  for (int i = 0; i < state.repeat; i++) {
    for (int j = 0; j < state.probe_count; j++) {
      double delta;
#ifdef __APPLE__
      static mach_timebase_info_data_t info;
      int64_t start_ns;
      int64_t end_ns;
#elif defined(__linux__)
      struct timespec start_ts;
      struct timespec end_ts;
#else
      struct timeval start_tv;
      struct timeval end_tv;
#endif  /* __APPLE__ */

      prepare_req(state.probes[j]);

#ifdef __APPLE__
      if (mach_timebase_info(&info) != KERN_SUCCESS)
        abort();

      start_ns = mach_absolute_time() * info.numer / info.denom;
#elif defined(__linux__)
      err = clock_gettime(CLOCK_MONOTONIC, &start_ts);
      assert(err == 0);
#else
      err = gettimeofday(&start_tv, NULL);
      assert(err == 0);
#endif  /* __APPLE__ */

      err = send_req(fd);
      assert(err == 0);

      err = recv_res(fd);
      assert(err == 0);

#ifdef __APPLE__
      end_ns = mach_absolute_time() * info.numer / info.denom;

      delta = end_ns - start_ns;
#elif defined(__linux__)
      err = clock_gettime(CLOCK_MONOTONIC, &end_ts);
      assert(err == 0);

      delta = (int64_t) (end_ts.tv_sec - start_ts.tv_sec) * 1e9 +
              (end_ts.tv_nsec - start_ts.tv_nsec);
#else
      err = gettimeofday(&end_tv, NULL);
      assert(err == 0);

      delta = (int64_t) (end_tv.tv_sec - start_tv.tv_sec) * 1e6 +
              (end_tv.tv_usec - start_tv.tv_usec);
#endif  /* __APPLE__ */

      state.avg[j] += delta;
      state.stddev[j] += delta * delta;
    }
  }

  for (int i = 0; i < state.probe_count; i++) {
    state.avg[i] /= state.repeat;
    state.stddev[i] /= state.repeat;
    state.stddev[i] -= state.avg[i] * state.avg[i];
    state.stddev[i] = sqrt(state.stddev[i]);

    fprintf(stdout, "%f\n%f\n", state.avg[i], state.stddev[i]);
  }

  close(fd);
}


static int count_colon_sep(const char* str) {
  const char* p;
  int count;

  count = 0;
  p = str;
  for (;;) {
    count++;

    p = strchr(p, ':');
    if (p == NULL)
      break;
    p++;
  }

  return count;
}


static void split_probes(const char* str) {
  const char* p;
  int count;
  int i;
  char** res;

  count = count_colon_sep(str);

  res = malloc(sizeof(*res) * count);
  assert(res != NULL);

  p = str;
  i = 0;
  for (;; i++) {
    char* n;

    n = strchr(p, ':');
    if (n == NULL) {
      res[i] = strdup(p);
      assert(res[i] != NULL);
      break;
    } else {
      res[i] = strndup(p, n - p);
      assert(res[i] != NULL);
      p = n + 1;
    }
  }

  state.probes = res;
  state.probe_count = count;
}


static void join_keys(const char* str) {
  int count;
  char* res;
  const char* p;
  char* q;

  count = count_colon_sep(str);

  // `.\r\n` after each colon, ":.\r\n\0" at the end
  res = malloc(strlen(str) + 3 * count + 2);

  p = str;
  q = res;
  for (;;) {
    char* n;

    n = strchr(p, ':');
    if (n == NULL) {
      q += sprintf(q, "%.*s:.\r\n", (int) strlen(p), p);
      break;
    } else {
      q += sprintf(q, "%.*s:.\r\n", (int) (n - p), p);
      p = n + 1;
    }
  }

  state.post = res;
}


int main(int argc, char** argv) {
  int port;
  const char* host;
  const char* probes;
  const char* keys;

  if (argc < 6) {
    fprintf(stderr, "%s port host repeats probe1:... key1:...\n", argv[0]);
    return -1;
  }

  signal(SIGPIPE, SIG_IGN);

  port = atoi(argv[1]);
  host = argv[2];
  state.repeat = atoi(argv[3]);
  probes = argv[4];
  keys = argv[5];

  split_probes(probes);
  join_keys(keys);

  state.avg = calloc(1, state.probe_count * sizeof(*state.avg));
  state.stddev = calloc(1, state.probe_count * sizeof(*state.stddev));
  assert(state.avg != NULL && state.stddev != NULL);

  if (inet_pton(AF_INET, host, &state.addr.sin_addr) != 1) {
    fprintf(stderr, "failed to parse addr \"%s\"\n", host);
    return -1;
  }

  state.addr.sin_port = htons(port);

  run();

  for (int i = 0; i < state.probe_count; i++)
    free(state.probes[i]);
  free(state.probes);
  free(state.post);

  return 0;
}
