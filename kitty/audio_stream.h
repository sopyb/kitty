#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#define AUDIO_MAX_STREAMS 256
#define AUDIO_STORAGE_QUOTA (330ULL * 1024 * 1024)

typedef enum {
  AUDIO_FORMAT_S16LE = 0,
  AUDIO_FORMAT_U8 = 1,
  AUDIO_FORMAT_MULAW = 2,
  AUDIO_FORMAT_S24LE = 3,
  AUDIO_FORMAT_S32LE = 4,
  AUDIO_FORMAT_F32LE = 5,
  AUDIO_FORMAT_F64LE = 6,
} AudioFormat;

typedef enum {
  AUDIO_STATE_STOPPED = 0,
  AUDIO_STATE_PLAYING = 1,
  AUDIO_STATE_PAUSED = 2,
} AudioPlaybackState;

struct AudioBackend;

typedef struct AudioStream {
  uint32_t id;
  uint32_t rate;
  uint8_t channels;
  AudioFormat format;
  AudioPlaybackState state;
  uint32_t volume;
  uint32_t preroll_ms;
  uint32_t loop_count;
  uint64_t duration_ms;
  uint64_t current_offset_ms;
  uint8_t *data;
  size_t data_sz;
  size_t data_capacity;
  size_t data_written;
  bool transmission_complete;
  bool autoplay;
  bool playback_paused;
  pthread_t playback_thread;
  bool playback_active;
  bool playback_stop;
  bool playback_failed;
  const struct AudioBackend *backend;
  void *backend_data;
  pthread_mutex_t data_lock;
  pthread_cond_t data_ready;
  size_t playback_offset;
} AudioStream;

typedef struct {
  AudioStream streams[AUDIO_MAX_STREAMS];
  uint32_t active_stream_id;
  uint64_t total_storage;
  size_t stream_count;
} AudioManager;

AudioManager *audio_manager_new(void);
void audio_manager_free(AudioManager *self);
AudioStream *audio_manager_get_or_create_stream(AudioManager *self,
                                                uint32_t id);
AudioStream *audio_manager_get_stream(AudioManager *self, uint32_t id);
void audio_manager_delete_stream(AudioManager *self, uint32_t id);
void audio_manager_delete_all_streams(AudioManager *self);
void audio_stream_append_data(AudioStream *self, const uint8_t *data,
                              size_t sz);
void audio_stream_mark_complete(AudioStream *self);
void audio_stream_free(AudioStream *self);
bool audio_format_from_string(const char *format, AudioFormat *out);
size_t audio_format_bytes_per_sample(AudioFormat format);
