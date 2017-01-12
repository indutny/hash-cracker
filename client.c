#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#define THREAD_COUNT 1
#define ITERATION_COUNT 1

static int port;
static const char* req;
static struct sockaddr_in addr;
static pthread_t threads[THREAD_COUNT];
static int64_t timings[THREAD_COUNT][ITERATION_COUNT];

static int send_req(int fd, const char* req, int req_len) {
  int err;

  do {
    err = write(fd, req, req_len);
    if (err > 0) {
      req += err;
      req_len -= err;
    }
  } while (req_len != 0 && err == -1 && errno == EINTR);

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


static void run(const int thread,
                const struct sockaddr* addr, socklen_t addr_len,
                const char* req, int req_len) {
  int fd;
  int err;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd != -1);

  do
    err = connect(fd, addr, addr_len);
  while (err == -1 && errno == EINTR);
  assert(err == 0);

  for (int i = 0; i < ITERATION_COUNT; i++) {
    struct timeval start_tv;
    struct timeval end_tv;
    int64_t delta;

    err = send_req(fd, req, req_len);
    assert(err == 0);

    err = gettimeofday(&start_tv, NULL);
    assert(err == 0);

    err = recv_res(fd);
    assert(err == 0);

    err = gettimeofday(&end_tv, NULL);
    assert(err == 0);

    delta = (int64_t) (end_tv.tv_sec - start_tv.tv_sec) * 1e6 +
            (end_tv.tv_usec - start_tv.tv_usec);

    timings[thread][i] = delta;
  }

  close(fd);
}


void* run_thread(void* arg) {
  int thread = (intptr_t) arg;
  run(thread, (const struct sockaddr*) &addr, sizeof(addr), req, strlen(req));
  return NULL;
}


int main(int argc, char** argv) {
  const char* host;

  if (argc < 4) {
    fprintf(stderr, "%s port host request\n", argv[0]);
    return -1;
  }

  port = atoi(argv[1]);
  host = argv[2];
  req = argv[3];

  memset(&addr, 0, sizeof(addr));
  if (inet_pton(AF_INET, argv[2], &addr.sin_addr) != 1) {
    fprintf(stderr, "failed to parse addr \"%s\"\n", host);
    return -1;
  }

  addr.sin_port = htons(port);

  memset(timings, 0, sizeof(timings));

  for (int i = 0; i < THREAD_COUNT; i++) {
    pthread_create(&threads[i], NULL, run_thread, (void*) (intptr_t) i);
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    void* res;
    pthread_join(threads[i], &res);
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    for (int j = 0; j < ITERATION_COUNT; j++) {
      fprintf(stdout, "%lld\n", timings[i][j]);
    }
  }

  return 0;
}
