#include "audio_stream.h"
#include "audio_output.h"
#include <stdlib.h>
#include <string.h>
#include "audio_container.h"
#include "audio_container_raw.h"

AudioManager *audio_manager_new(void) {
  AudioManager *self = calloc(1, sizeof(AudioManager));
  if (self) {
    self->active_stream_id = 0;
    self->total_storage = 0;
    self->stream_count = 0;
  }
  return self;
}

void audio_manager_free(AudioManager *self) {
  if (!self)
    return;
  for (size_t i = 0; i < self->stream_count; i++) {
    audio_stream_free(&self->streams[i]);
  }
  free(self);
}

static int audio_manager_find_stream_index(AudioManager *self, uint32_t id) {
  for (size_t i = 0; i < self->stream_count; i++) {
    if (self->streams[i].id == id)
      return (int)i;
  }
  return -1;
}

AudioStream *audio_manager_get_or_create_stream(AudioManager *self,
                                                uint32_t id) {
  if (!self || id == 0)
    return NULL;

  int idx = audio_manager_find_stream_index(self, id);
  if (idx >= 0) {
    return &self->streams[idx];
  }

  if (self->stream_count >= AUDIO_MAX_STREAMS) {
    return NULL;
  }

  AudioStream *stream = &self->streams[self->stream_count++];
  memset(stream, 0, sizeof(*stream));
  stream->id = id;
  stream->volume = 100;
  stream->preroll_ms = 200;
  stream->state = AUDIO_STATE_STOPPED;
  stream->autoplay = 1;
  stream->playback_paused = false;
  if (pthread_mutex_init(&stream->data_lock, NULL) != 0) {
    self->stream_count--;
    memset(stream, 0, sizeof(*stream));
    return NULL;
  }
  if (pthread_cond_init(&stream->data_ready, NULL) != 0) {
    pthread_mutex_destroy(&stream->data_lock);
    self->stream_count--;
    memset(stream, 0, sizeof(*stream));
    return NULL;
  }
  return stream;
}

AudioStream *audio_manager_get_stream(AudioManager *self, uint32_t id) {
  if (!self)
    return NULL;
  int idx = audio_manager_find_stream_index(self, id);
  return idx >= 0 ? &self->streams[idx] : NULL;
}

void audio_manager_delete_stream(AudioManager *self, uint32_t id) {
  if (!self)
    return;
  int idx = audio_manager_find_stream_index(self, id);
  if (idx >= 0) {
    AudioStream *stream = &self->streams[idx];
    if (stream->id == self->active_stream_id) {
      self->active_stream_id = 0;
    }
    if (self->total_storage >= stream->data_written) {
      self->total_storage -= stream->data_written;
    } else {
      self->total_storage = 0;
    }
    audio_stream_free(stream);
    if (idx < (int)self->stream_count - 1) {
      memmove(&self->streams[idx], &self->streams[idx + 1],
              (self->stream_count - idx - 1) * sizeof(AudioStream));
    }
    self->stream_count--;
  }
}

void audio_manager_delete_all_streams(AudioManager *self) {
  if (!self)
    return;
  for (size_t i = 0; i < self->stream_count; i++) {
    if (self->total_storage >= self->streams[i].data_written) {
      self->total_storage -= self->streams[i].data_written;
    } else {
      self->total_storage = 0;
    }
    audio_stream_free(&self->streams[i]);
  }
  self->stream_count = 0;
  self->active_stream_id = 0;
  self->total_storage = 0;
}

void audio_stream_append_pcm(AudioStream *self, const uint8_t *data,
                             size_t sz) {
  if (!self || !data || sz == 0)
    return;

  pthread_mutex_lock(&self->data_lock);
  size_t needed = self->data_written + sz;
  if (needed > self->data_capacity) {
    size_t new_capacity = self->data_capacity ? self->data_capacity * 2 : 65536;
    while (new_capacity < needed) {
      new_capacity *= 2;
    }
    uint8_t *new_data = realloc(self->data, new_capacity);
    if (!new_data) {
      pthread_mutex_unlock(&self->data_lock);
      return;
    }
    self->data = new_data;
    self->data_capacity = new_capacity;
  }

  memcpy(self->data + self->data_written, data, sz);
  self->data_written += sz;
  self->data_sz = self->data_written;
  pthread_cond_signal(&self->data_ready);
  pthread_mutex_unlock(&self->data_lock);
}

void audio_stream_append_data(AudioStream *self, const uint8_t *data, size_t sz) {
  if (self && self->container_handler && self->container_handler->append_data) {
    self->container_handler->append_data(self, data, sz);
  }
}

void audio_stream_mark_complete(AudioStream *self) {
  if (!self)
    return;
  pthread_mutex_lock(&self->data_lock);
  self->transmission_complete = true;
  pthread_cond_broadcast(&self->data_ready);
  pthread_mutex_unlock(&self->data_lock);
}

void audio_stream_free(AudioStream *self) {
  if (!self)
    return;
  audio_output_stop(self);
  if (self->data) {
    free(self->data);
    self->data = NULL;
  }
  self->data_capacity = 0;
  self->data_written = 0;
  self->data_sz = 0;
  self->playback_offset = 0;
  self->transmission_complete = false;
  self->playback_failed = false;
  self->playback_paused = false;
  self->backend = NULL;
  
  if (self->container_handler && self->container_handler->free) {
    self->container_handler->free(self);
  }
  self->container_handler = NULL;
  self->container_data = NULL;
  pthread_cond_destroy(&self->data_ready);
  pthread_mutex_destroy(&self->data_lock);
}

const AudioContainerHandler* audio_container_get_handler(AudioContainer type) {
  switch (type) {
    case AUDIO_CONTAINER_RAW:
      return &audio_container_raw_handler;
    default:
      return NULL;
  }
}

bool audio_stream_set_format(AudioStream *stream, const char *format_string) {
  if (!stream || !format_string || format_string[0] == '\0') return false;

  const char *slash = strchr(format_string, '/');
  if (!slash) return false;

  size_t container_len = slash - format_string;
  const char *format_spec = slash + 1;

  AudioContainer type = AUDIO_CONTAINER_UNKNOWN;

  if (container_len == 3 && strncmp(format_string, "raw", 3) == 0) {
    type = AUDIO_CONTAINER_RAW;
  }

  stream->container = type;
  stream->container_handler = audio_container_get_handler(type);

  if (stream->container_handler && stream->container_handler->init) {
    return stream->container_handler->init(stream, format_spec);
  }

  return false;
}

size_t audio_format_bytes_per_sample(AudioFormat format) {
  switch (format) {
  case AUDIO_FORMAT_U8:
  case AUDIO_FORMAT_MULAW:
    return 1;
  case AUDIO_FORMAT_S16LE:
    return 2;
  case AUDIO_FORMAT_S24LE:
    return 3;
  case AUDIO_FORMAT_S32LE:
  case AUDIO_FORMAT_F32LE:
    return 4;
  case AUDIO_FORMAT_F64LE:
    return 8;
  default:
    return 0;
  }
}
