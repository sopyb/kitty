#include "audio_container_raw.h"
#include "audio_stream.h"
#include <string.h>

static bool raw_init(struct AudioStream *stream, const char *format_spec) {
  if (!stream || !format_spec || format_spec[0] == '\0') return false;

  stream->container = AUDIO_CONTAINER_RAW;

  if (strcmp(format_spec, "s16le") == 0) {
    stream->format = AUDIO_FORMAT_S16LE;
    return true;
  }
  if (strcmp(format_spec, "u8") == 0) {
    stream->format = AUDIO_FORMAT_U8;
    return true;
  }
  if (strcmp(format_spec, "mulaw") == 0) {
    stream->format = AUDIO_FORMAT_MULAW;
    return true;
  }
  if (strcmp(format_spec, "s24le") == 0) {
    stream->format = AUDIO_FORMAT_S24LE;
    return true;
  }
  if (strcmp(format_spec, "s32le") == 0) {
    stream->format = AUDIO_FORMAT_S32LE;
    return true;
  }
  if (strcmp(format_spec, "f32le") == 0) {
    stream->format = AUDIO_FORMAT_F32LE;
    return true;
  }
  if (strcmp(format_spec, "f64le") == 0) {
    stream->format = AUDIO_FORMAT_F64LE;
    return true;
  }
  return false;
}

static void raw_append_data(struct AudioStream *stream, const uint8_t *data, size_t sz) {
  audio_stream_append_pcm(stream, data, sz);
}

static void raw_free(struct AudioStream *stream) {
  // Raw container requires no specific cleanup
  (void)stream;
}

const AudioContainerHandler audio_container_raw_handler = {
    .init = raw_init,
    .append_data = raw_append_data,
    .free = raw_free
};
