#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
  AUDIO_CONTAINER_UNKNOWN = 0,
  AUDIO_CONTAINER_RAW = 1,
} AudioContainer;

struct AudioStream;

typedef struct AudioContainerHandler {
  bool (*init)(struct AudioStream *stream, const char *format_spec);
  void (*append_data)(struct AudioStream *stream, const uint8_t *data, size_t sz);
  void (*free)(struct AudioStream *stream);
} AudioContainerHandler;

const AudioContainerHandler* audio_container_get_handler(AudioContainer type);
