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

static AudioResponseCode validate_temp_file_path(const char *path) {
  if (!path || !*path || path[0] != '/')
    return AUDIO_RESPONSE_EINVAL;

  char dirname_buf[4096], basename_buf[4096];
  snprintf(dirname_buf, sizeof(dirname_buf), "%s", path);

  const char *dir = dirname(dirname_buf);

  if (strcmp(dir, "/tmp") != 0 && strcmp(dir, "/dev/shm") != 0) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || strcmp(dir, tmpdir) != 0) {
      return AUDIO_RESPONSE_EINVAL;
    }
  }

  snprintf(basename_buf, sizeof(basename_buf), "%s", path);
  const char *name = basename(basename_buf);

  if (strncmp(name, "tty-audio-protocol-", 19) != 0) {
    return AUDIO_RESPONSE_EINVAL;
  }

  return AUDIO_RESPONSE_OK;
}

static AudioResponseCode validate_file_path(const char *path) {
  if (!path || !*path || path[0] != '/')
    return AUDIO_RESPONSE_EINVAL;
  if (strncmp(path, "/proc", 5) == 0 || strncmp(path, "/sys", 4) == 0 ||
      strncmp(path, "/dev", 4) == 0) {
    return AUDIO_RESPONSE_EINVAL;
  }
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_read_temp_file(const char *path, uint32_t offset,
                                   uint32_t size, uint8_t **output,
                                   size_t *output_sz) {
  if (!path || !output || !output_sz)
    return AUDIO_RESPONSE_EINVAL;

  AudioResponseCode val_rc = validate_temp_file_path(path);
  if (val_rc != AUDIO_RESPONSE_OK) {
    return val_rc;
  }

  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    return AUDIO_RESPONSE_EIO;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (!S_ISREG(st.st_mode)) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (size == 0) {
    size = st.st_size > offset ? st.st_size - offset : 0;
  }

  if (size == 0) {
    *output = NULL;
    *output_sz = 0;
    close(fd);
    unlink(path);
    return AUDIO_RESPONSE_OK;
  }

  *output = malloc(size);
  if (!*output) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (lseek(fd, offset, SEEK_SET) < 0) {
    free(*output);
    *output = NULL;
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  ssize_t n = read(fd, *output, size);
  close(fd);

  if (n < 0) {
    free(*output);
    *output = NULL;
    return AUDIO_RESPONSE_EIO;
  }

  unlink(path);

  *output_sz = n;
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_read_file(const char *path, uint32_t offset, uint32_t size,
                              uint8_t **output, size_t *output_sz) {
  if (!path || !output || !output_sz)
    return AUDIO_RESPONSE_EINVAL;

  AudioResponseCode val_rc = validate_file_path(path);
  if (val_rc != AUDIO_RESPONSE_OK) {
    return val_rc;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return AUDIO_RESPONSE_EIO;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (!S_ISREG(st.st_mode)) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (size == 0) {
    size = st.st_size > offset ? st.st_size - offset : 0;
  }

  if (size == 0) {
    *output = NULL;
    *output_sz = 0;
    close(fd);
    return AUDIO_RESPONSE_OK;
  }

  *output = malloc(size);
  if (!*output) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (lseek(fd, offset, SEEK_SET) < 0) {
    free(*output);
    *output = NULL;
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  ssize_t n = read(fd, *output, size);
  close(fd);

  if (n < 0) {
    free(*output);
    *output = NULL;
    return AUDIO_RESPONSE_EIO;
  }

  *output_sz = n;
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_read_sharedmem(const char *name, uint32_t offset,
                                   uint32_t size, uint8_t **output,
                                   size_t *output_sz) {
  if (!name || !output || !output_sz)
    return AUDIO_RESPONSE_EINVAL;
  if (name[0] != '/')
    return AUDIO_RESPONSE_EINVAL;

  int fd = shm_open(name, O_RDONLY, 0);
  if (fd < 0) {
    return AUDIO_RESPONSE_EIO;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  if (size == 0) {
    size = st.st_size > offset ? st.st_size - offset : 0;
  }

  if (size == 0) {
    *output = NULL;
    *output_sz = 0;
    close(fd);
    return AUDIO_RESPONSE_OK;
  }

  *output = malloc(size);
  if (!*output) {
    close(fd);
    return AUDIO_RESPONSE_EIO;
  }

  size_t done = 0;
  while (done < size) {
    ssize_t n = pread(fd, *output + done, size - done, (off_t)offset + (off_t)done);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      free(*output);
      *output = NULL;
      close(fd);
      return AUDIO_RESPONSE_EIO;
    }
    if (n == 0) {
      free(*output);
      *output = NULL;
      close(fd);
      return AUDIO_RESPONSE_EIO;
    }
    done += (size_t)n;
  }

  close(fd);

  *output_sz = size;
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_probe_temp_file(const char *path) {
  if (!path)
    return AUDIO_RESPONSE_EINVAL;
  AudioResponseCode val_rc = validate_temp_file_path(path);
  if (val_rc != AUDIO_RESPONSE_OK) {
    return val_rc;
  }
  int fd = open(path, O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    return AUDIO_RESPONSE_EIO;
  }
  close(fd);
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_probe_file(const char *path) {
  if (!path)
    return AUDIO_RESPONSE_EINVAL;
  AudioResponseCode val_rc = validate_file_path(path);
  if (val_rc != AUDIO_RESPONSE_OK) {
    return val_rc;
  }
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return AUDIO_RESPONSE_EIO;
  }
  close(fd);
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_probe_sharedmem(const char *name) {
  if (!name)
    return AUDIO_RESPONSE_EINVAL;
  if (name[0] != '/')
    return AUDIO_RESPONSE_EINVAL;
  int fd = shm_open(name, O_RDONLY, 0);
  if (fd < 0) {
    return AUDIO_RESPONSE_EIO;
  }
  close(fd);
  return AUDIO_RESPONSE_OK;
}

static AudioResponseCode load_transport_bytes(AudioResponseCode (*reader)(const char *, uint32_t, uint32_t,
                                              uint8_t **, size_t *),
                                const char *path_or_name, uint32_t offset,
                                uint32_t size, AudioTransportResult *result) {
  if (!reader || !path_or_name || !result)
    return AUDIO_RESPONSE_EIO;
  result->data = NULL;
  result->size = 0;
  result->error = 0;

  uint8_t *data = NULL;
  size_t out_sz = 0;
  AudioResponseCode rc = reader(path_or_name, offset, size, &data, &out_sz);
  if (rc != AUDIO_RESPONSE_OK) {
    if (data)
      free(data);
    result->error = errno ? errno : EIO;
    return rc;
  }

  result->data = data;
  result->size = out_sz;
  return AUDIO_RESPONSE_OK;
}

AudioResponseCode audio_transport_load_temp_file(const char *path, uint32_t offset,
                                   uint32_t size,
                                   AudioTransportResult *result) {
  return load_transport_bytes(audio_transport_read_temp_file, path, offset,
                              size, result);
}

AudioResponseCode audio_transport_load_file(const char *path, uint32_t offset, uint32_t size,
                              AudioTransportResult *result) {
  return load_transport_bytes(audio_transport_read_file, path, offset, size,
                              result);
}

AudioResponseCode audio_transport_load_sharedmem(const char *name, uint32_t offset,
                                   uint32_t size,
                                   AudioTransportResult *result) {
  return load_transport_bytes(audio_transport_read_sharedmem, name, offset,
                              size, result);
}
