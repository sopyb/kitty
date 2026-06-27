#include "audio_transport.h"
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

extern int shm_open(const char *name, int oflag, mode_t mode);
extern int shm_unlink(const char *name);

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static int validate_temp_file_path(const char *path) {
  if (!path || !*path)
    return -1;

  char dirname_buf[4096], basename_buf[4096];
  strncpy(dirname_buf, path, sizeof(dirname_buf) - 1);
  dirname_buf[sizeof(dirname_buf) - 1] = '\0';

  const char *dir = dirname(dirname_buf);

  if (strcmp(dir, "/tmp") != 0 && strcmp(dir, "/dev/shm") != 0) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || strcmp(dir, tmpdir) != 0) {
      return -1;
    }
  }

  strncpy(basename_buf, path, sizeof(basename_buf) - 1);
  basename_buf[sizeof(basename_buf) - 1] = '\0';
  const char *name = basename(basename_buf);

  if (strncmp(name, "tty-audio-protocol-", 19) != 0) {
    return -1;
  }

  return 0;
}

static int validate_file_path(const char *path) {
  if (!path || !*path)
    return -1;
  if (strncmp(path, "/proc", 5) == 0 || strncmp(path, "/sys", 4) == 0 ||
      strncmp(path, "/dev", 4) == 0) {
    return -1;
  }
  return 0;
}

int audio_transport_read_temp_file(const char *path, uint32_t offset,
                                   uint32_t size, uint8_t **output,
                                   size_t *output_sz) {
  if (!path || !output || !output_sz)
    return -1;

  if (validate_temp_file_path(path) != 0) {
    return -1;
  }

  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return -1;
  }

  if (!S_ISREG(st.st_mode)) {
    close(fd);
    return -1;
  }

  if (size == 0) {
    size = st.st_size > offset ? st.st_size - offset : 0;
  }

  if (size == 0) {
    *output = NULL;
    *output_sz = 0;
    close(fd);
    unlink(path);
    return 0;
  }

  *output = malloc(size);
  if (!*output) {
    close(fd);
    return -1;
  }

  if (lseek(fd, offset, SEEK_SET) < 0) {
    free(*output);
    *output = NULL;
    close(fd);
    return -1;
  }

  ssize_t n = read(fd, *output, size);
  close(fd);

  if (n < 0) {
    free(*output);
    *output = NULL;
    return -1;
  }

  unlink(path);

  *output_sz = n;
  return 0;
}

int audio_transport_read_file(const char *path, uint32_t offset, uint32_t size,
                              uint8_t **output, size_t *output_sz) {
  if (!path || !output || !output_sz)
    return -1;

  if (validate_file_path(path) != 0) {
    return -1;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return -1;
  }

  if (!S_ISREG(st.st_mode)) {
    close(fd);
    return -1;
  }

  if (size == 0) {
    size = st.st_size > offset ? st.st_size - offset : 0;
  }

  if (size == 0) {
    *output = NULL;
    *output_sz = 0;
    close(fd);
    return 0;
  }

  *output = malloc(size);
  if (!*output) {
    close(fd);
    return -1;
  }

  if (lseek(fd, offset, SEEK_SET) < 0) {
    free(*output);
    *output = NULL;
    close(fd);
    return -1;
  }

  ssize_t n = read(fd, *output, size);
  close(fd);

  if (n < 0) {
    free(*output);
    *output = NULL;
    return -1;
  }

  *output_sz = n;
  return 0;
}

int audio_transport_read_sharedmem(const char *name, uint32_t offset,
                                   uint32_t size, uint8_t **output,
                                   size_t *output_sz) {
  if (!name || !output || !output_sz)
    return -1;

  int fd = shm_open(name, O_RDONLY, 0);
  if (fd < 0) {
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return -1;
  }

  if (size == 0) {
    size = st.st_size > offset ? st.st_size - offset : 0;
  }

  if (size == 0) {
    *output = NULL;
    *output_sz = 0;
    close(fd);
    return 0;
  }

  *output = malloc(size);
  if (!*output) {
    close(fd);
    return -1;
  }

  void *mapped = mmap(NULL, offset + size, PROT_READ, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    free(*output);
    *output = NULL;
    close(fd);
    return -1;
  }

  memcpy(*output, (uint8_t *)mapped + offset, size);
  munmap(mapped, offset + size);
  close(fd);

  *output_sz = size;
  return 0;
}

int audio_transport_probe_temp_file(const char *path) {
  if (!path)
    return -1;
  if (validate_temp_file_path(path) != 0) {
    return -1;
  }
  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    return -1;
  }
  close(fd);
  return 0;
}

int audio_transport_probe_file(const char *path) {
  if (!path)
    return -1;
  if (validate_file_path(path) != 0) {
    return -1;
  }
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  close(fd);
  return 0;
}

int audio_transport_probe_sharedmem(const char *name) {
  if (!name)
    return -1;
  int fd = shm_open(name, O_RDONLY, 0);
  if (fd < 0) {
    return -1;
  }
  close(fd);
  return 0;
}

static int load_transport_bytes(int (*reader)(const char *, uint32_t, uint32_t,
                                              uint8_t **, size_t *),
                                const char *path_or_name, uint32_t offset,
                                uint32_t size, AudioTransportResult *result) {
  if (!reader || !path_or_name || !result)
    return -1;
  result->data = NULL;
  result->size = 0;
  result->error = 0;

  uint8_t *data = NULL;
  size_t out_sz = 0;
  int rc = reader(path_or_name, offset, size, &data, &out_sz);
  if (rc != 0) {
    if (data)
      free(data);
    result->error = errno ? errno : EIO;
    return rc;
  }

  result->data = data;
  result->size = out_sz;
  return 0;
}

int audio_transport_load_temp_file(const char *path, uint32_t offset,
                                   uint32_t size,
                                   AudioTransportResult *result) {
  return load_transport_bytes(audio_transport_read_temp_file, path, offset,
                              size, result);
}

int audio_transport_load_file(const char *path, uint32_t offset, uint32_t size,
                              AudioTransportResult *result) {
  return load_transport_bytes(audio_transport_read_file, path, offset, size,
                              result);
}

int audio_transport_load_sharedmem(const char *name, uint32_t offset,
                                   uint32_t size,
                                   AudioTransportResult *result) {
  return load_transport_bytes(audio_transport_read_sharedmem, name, offset,
                              size, result);
}
