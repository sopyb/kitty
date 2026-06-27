#pragma once

#include <stdbool.h>

typedef struct AudioStream AudioStream;

typedef struct AudioBackend {
  const char *name;
  bool (*available)(void);
  bool (*start)(AudioStream *stream);
  void (*pause)(AudioStream *stream);
  void (*resume)(AudioStream *stream);
  void (*stop)(AudioStream *stream);
} AudioBackend;

const AudioBackend *audio_backend_select(void);
const AudioBackend *audio_backend_alsa(void);
