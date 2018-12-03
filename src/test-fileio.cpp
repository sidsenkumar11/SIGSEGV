#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define min(a, b) ((a) < (b)? (a) : (b))
#define max(a, b) ((a) > (b)? (a) : (b))

const int64_t MAX_CALL = 1024 * 1024;
const int64_t MAX_SIZE = MAX_CALL * 2 + 2048;
const char    zeros[1024] = {0};

int64_t filesize = 0;
int64_t fileoffset = 0;
char    filedata[MAX_SIZE] = {0};


int64_t randomize(int64_t max, int64_t align) {
  assert(max <= RAND_MAX);
  return (rand() % max / align) * align;
}

void randbytes(char* buffer, int64_t size) {
  for(int i = 0; i < size; ++i) {
    buffer[i] = char(rand() % 26) + 'a';
  }

  buffer[size] = '\0';
}

int openfile(const char* file, int mode) {
  int fd = open(file, mode);
  if(fd <= 0) {
    fprintf(stderr, "FILE OPEN FAIL\n");
    exit(1);
  }

  return fd;
}

void read(const char* file, char* data, int64_t length, int64_t offset) {
  printf("READ:  %8" PRId64 " bytes at %8" PRId64 "\n", length, offset);
  int64_t len = min(filesize - offset, length);

  int fd = openfile(file, O_RDONLY);
  int64_t result = pread(fd, data, len, fileoffset + offset);

  if(result != len) {
    fprintf(stderr, "READ FAIL (%" PRId64 " != %" PRId64 ")\n", result, len);
    close(fd);
    exit(1);
  }

  if(offset < 0) {
    if(memcmp(data, zeros, -offset) != 0) {
      fprintf(stderr, "READ ZERO FAIL\n");
      close(fd);
      exit(1);
    }

    data -= offset;
    len  += offset;
    offset = 0;
  }

  if(memcmp(data, filedata + offset, len) != 0) {
    fprintf(stderr, "READ DATA FAIL\n");
    close(fd);
    exit(1);
  }

  close(fd);
}

void write(const char* file, const char* data, int64_t length, int64_t offset) {
  printf("WRITE: %8" PRId64 " bytes at %8" PRId64 "\n", length, offset);

  // Update our in-memory copy of the file:
  memcpy(filedata + offset, data, length);
  filesize = max(filesize, offset + length);

  // Update the real file:
  int fd = openfile(file, O_WRONLY);
  int64_t result = pwrite(fd, data, length, fileoffset + offset);

  if(result != length) {
    fprintf(stderr, "WRITE FAIL (%" PRId64 " != %" PRId64 ")\n", result, length);
    close(fd);
    exit(1);
  }

  result = fsync(fd);
  if(result != 0) {
    fprintf(stderr, "WRITE SYNC FAIL\n");
    close(fd);
    exit(1);
  }

  close(fd);
}

void trunc(const char* file, int64_t offset) {
  printf("TRUNC: %8" PRId64 " bytes\n", offset);

  // Update our in-memory copy of the file:
  memset(filedata + offset, 0, MAX_SIZE - offset);
  filesize = offset;

  // Update the real file:
  int fd = openfile(file, O_WRONLY);
  int result = truncate(file, offset);

  if(result != 0) {
    fprintf(stderr, "TRUNCATE FAIL\n");
    close(fd);
    exit(1);
  }
}

// void testr(const char* file, )

void test_write(const char* file, int64_t length, int64_t offset) {
  assert(offset + length <= MAX_SIZE);

  char buffer[MAX_CALL*2 + 2048];
  randbytes(buffer, length);
  write(file, buffer, length, offset);

  length += 2048;
  offset -= 1024;

  if(offset + fileoffset < 0) {
    offset = -fileoffset;
  }

  read(file, buffer + 7, length, offset);
}

void test_trunc(const char* file, int64_t offset) {
  assert(offset <= MAX_SIZE);
  trunc(file, offset);

  offset -= 1024;
  if(offset + fileoffset < 0) {
    offset = -fileoffset;
  }

  char buffer[2048];
  read(file, buffer + 7, 2048, offset);
}

int main(int argc, char** argv) {
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s [testfile]\n", argv[0]);
    exit(1);
  }

  int seed = time(NULL);
  printf("Running with file %s and seed %d...\n", argv[1], seed);
  srand(seed);

  int fd = open(argv[1], O_WRONLY | O_CREAT);
  chmod(argv[1], 0644);
  ftruncate(fd, 0);
  close(fd);

  for(int i = 0; i < 50; ++i) {
    test_write(argv[1], randomize(MAX_CALL,    1), randomize(MAX_CALL,    1));
    test_write(argv[1], randomize(1024,        1), randomize(1024,        1));
    test_write(argv[1], randomize(MAX_CALL, 1024), randomize(MAX_CALL, 1024));
    test_write(argv[1], randomize(4096,     1024), randomize(4096,     1024));

    test_trunc(argv[1], randomize(MAX_CALL,    1));
    test_trunc(argv[1], randomize(MAX_CALL, 1024));
  }

  return 0;
}