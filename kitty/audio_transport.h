#pragma once

#include "audio_response.h"
#include <stdint.h>
#include <stdlib.h>

typedef enum {
  AUDIO_TRANSPORT_INLINE = 'd',
  AUDIO_TRANSPORT_TEMP_FILE = 't',
  AUDIO_TRANSPORT_FILE = 'f',
  AUDIO_TRANSPORT_SHAREDMEM = 's',
} AudioTransportType;

typedef struct {
  uint8_t *data;
  size_t size;
  int error;
} AudioTransportResult;

AudioResponseCode audio_transport_read_temp_file(const char *path, uint32_t offset,
                                   uint32_t size, uint8_t **output,
                                   size_t *output_sz);
AudioResponseCode audio_transport_read_file(const char *path, uint32_t offset, uint32_t size,
                              uint8_t **output, size_t *output_sz);
AudioResponseCode audio_transport_read_sharedmem(const char *name, uint32_t offset,
                                   uint32_t size, uint8_t **output,
                                   size_t *output_sz);
AudioResponseCode audio_transport_probe_temp_file(const char *path);
AudioResponseCode audio_transport_probe_file(const char *path);
AudioResponseCode audio_transport_probe_sharedmem(const char *name);
AudioResponseCode audio_transport_load_temp_file(const char *path, uint32_t offset,
                                   uint32_t size, AudioTransportResult *result);
AudioResponseCode audio_transport_load_file(const char *path, uint32_t offset, uint32_t size,
                              AudioTransportResult *result);
AudioResponseCode audio_transport_load_sharedmem(const char *name, uint32_t offset,
                                   uint32_t size, AudioTransportResult *result);
