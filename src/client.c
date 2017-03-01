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

#define ASSERT(X) if (!(X)) abort()

static int prepare_req(const char* probe, char* out, size_t out_len) {
  int written;

  written = snprintf(out, out_len,
                     "POST / HTTP/1.1\r\n"
                     "Content-Length:%d\r\n\r\n"
                     "%s\r\n",
                     (int) strlen(probe),
                     probe);
  ASSERT(written <= (int) out_len);

  return written;
}


static int send_req(int fd, const char* req, int len) {
  int err;

  do {
    err = write(fd, req, len);
    if (err > 0) {
      req += err;
      len -= err;
    }
  } while (len != 0 && err == -1 && errno == EINTR);

  return err >= 0 ? 0 : -1;
}


static int recv_res(int fd, int single_byte) {
  int err;
  char tmp[16 * 1024];
  int count;
  static char* CRLF = "\r\n\r\n";

  count = 0;
  do {
    err = read(fd, tmp, single_byte ? 1 : sizeof(tmp));
    if (err > 0) {
      if (single_byte)
        return 0;

      for (int i = 0; count < 4 && i < err; i++)
        if (tmp[i] != CRLF[count++])
          count = 0;
    }
  } while (err == -1 && errno == EINTR);

  return count == 4 ? 0 : -1;
}


static void run(int fd) {
  int err;
  char* buf;
  size_t buf_size;
  char* req_buf;
  size_t req_buf_size;

  buf_size = 64 * 1024 * 1024;
  req_buf_size = 64 * 1024 * 1024 + 512;
  buf = malloc(buf_size);
  req_buf = malloc(req_buf_size);
  if (buf == NULL || req_buf == NULL) {
    free(buf);
    free(req_buf);
    return;
  }

  while (fgets(buf, buf_size, stdin) != NULL) {
    size_t req_len;
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

    /* Remove `\n` */
    buf[strlen(buf) - 1] = '\0';

    req_len = prepare_req(buf, req_buf, req_buf_size);

    /* Send all, but the last byte */
    err = send_req(fd, req_buf, req_len - 1);
    ASSERT(err == 0);

#ifdef __APPLE__
    if (mach_timebase_info(&info) != KERN_SUCCESS)
      abort();

    start_ns = mach_absolute_time() * info.numer / info.denom;
#elif defined(__linux__)
    err = clock_gettime(CLOCK_MONOTONIC, &start_ts);
    ASSERT(err == 0);
#else
    err = gettimeofday(&start_tv, NULL);
    ASSERT(err == 0);
#endif  /* __APPLE__ */

    /* Send the last byte */
    err = send_req(fd, req_buf + req_len - 1, 1);
    ASSERT(err == 0);

    /* Recv first byte */
    err = recv_res(fd, 1);
    ASSERT(err == 0);

#ifdef __APPLE__
    end_ns = mach_absolute_time() * info.numer / info.denom;

    delta = end_ns - start_ns;
#elif defined(__linux__)
    err = clock_gettime(CLOCK_MONOTONIC, &end_ts);
    ASSERT(err == 0);

    delta = (int64_t) (end_ts.tv_sec - start_ts.tv_sec) * 1e9 +
            (end_ts.tv_nsec - start_ts.tv_nsec);
#else
    err = gettimeofday(&end_tv, NULL);
    ASSERT(err == 0);

    delta = (int64_t) (end_tv.tv_sec - start_tv.tv_sec) * 1e6 +
            (end_tv.tv_usec - start_tv.tv_usec);
#endif  /* __APPLE__ */

    /* Recv rest byte */
    err = recv_res(fd, 0);
    ASSERT(err == 0);

    fprintf(stdout, "%f\n", delta);
    fflush(stdout);
  }

  free(req_buf);
  free(buf);
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


int main(int argc, char** argv) {
  int port;
  const char* host;
  struct sockaddr_in addr;
  int err;
  int fd;

  if (argc < 3) {
    fprintf(stderr, "%s port host\n", argv[0]);
    return -1;
  }

  signal(SIGPIPE, SIG_IGN);

  port = atoi(argv[1]);
  host = argv[2];

  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    fprintf(stderr, "failed to parse addr \"%s\"\n", host);
    return -1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT(fd != -1);
  do
    err = connect(fd, (const struct sockaddr*) &addr, sizeof(addr));
  while (err == -1 && errno == EINTR);
  ASSERT(err == 0);

  run(fd);

  close(fd);

  return 0;
}
