#include "data-types.h"
#include "audio_backend.h"
#include "audio_output.h"
#include "audio_stream.h"
#include <limits.h>
#include <string.h>

#if defined(HAVE_ALSA)
#include <alsa/asoundlib.h>
#endif

#if defined(HAVE_ALSA)
static void apply_volume(uint8_t *data, size_t sz, AudioFormat format,
                         uint32_t volume) {
  if (volume >= 100)
    return;
  float scale = (float)volume / 100.0f;
  size_t i = 0;
  switch (format) {
  case AUDIO_FORMAT_U8: {
    if (volume == 0) {
      memset(data, 128, sz);
      break;
    }
    for (i = 0; i < sz; i++) {
      int sample = (int)data[i] - 128;
      int scaled = (int)(sample * scale);
      if (scaled < -128)
        scaled = -128;
      if (scaled > 127)
        scaled = 127;
      data[i] = (uint8_t)(scaled + 128);
    }
    break;
  }
  case AUDIO_FORMAT_S16LE: {
    if (volume == 0) {
      memset(data, 0, sz);
      break;
    }
    for (i = 0; i + 1 < sz; i += 2) {
      int16_t v = (int16_t)(data[i] | (data[i + 1] << 8));
      int32_t scaled = (int32_t)(v * scale);
      if (scaled < -32768)
        scaled = -32768;
      if (scaled > 32767)
        scaled = 32767;
      data[i] = (uint8_t)(scaled & 0xFF);
      data[i + 1] = (uint8_t)((scaled >> 8) & 0xFF);
    }
    break;
  }
  case AUDIO_FORMAT_S24LE: {
    if (volume == 0) {
      memset(data, 0, sz);
      break;
    }
    for (i = 0; i + 2 < sz; i += 3) {
      int32_t v = (int32_t)(data[i] | (data[i + 1] << 8) | (data[i + 2] << 16));
      if (v & 0x800000)
        v |= ~0xFFFFFF;
      int32_t scaled = (int32_t)(v * scale);
      if (scaled < -8388608)
        scaled = -8388608;
      if (scaled > 8388607)
        scaled = 8388607;
      data[i] = (uint8_t)(scaled & 0xFF);
      data[i + 1] = (uint8_t)((scaled >> 8) & 0xFF);
      data[i + 2] = (uint8_t)((scaled >> 16) & 0xFF);
    }
    break;
  }
  case AUDIO_FORMAT_S32LE: {
    if (volume == 0) {
      memset(data, 0, sz);
      break;
    }
    for (i = 0; i + 3 < sz; i += 4) {
      int32_t v = (int32_t)(data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) |
                            (data[i + 3] << 24));
      int64_t scaled = (int64_t)(v * scale);
      if (scaled < INT32_MIN)
        scaled = INT32_MIN;
      if (scaled > INT32_MAX)
        scaled = INT32_MAX;
      data[i] = (uint8_t)(scaled & 0xFF);
      data[i + 1] = (uint8_t)((scaled >> 8) & 0xFF);
      data[i + 2] = (uint8_t)((scaled >> 16) & 0xFF);
      data[i + 3] = (uint8_t)((scaled >> 24) & 0xFF);
    }
    break;
  }
  case AUDIO_FORMAT_F32LE: {
    if (volume == 0) {
      memset(data, 0, sz);
      break;
    }
    for (i = 0; i + 3 < sz; i += 4) {
      float v;
      memcpy(&v, data + i, sizeof(float));
      v *= scale;
      memcpy(data + i, &v, sizeof(float));
    }
    break;
  }
  case AUDIO_FORMAT_F64LE: {
    if (volume == 0) {
      memset(data, 0, sz);
      break;
    }
    for (i = 0; i + 7 < sz; i += 8) {
      double v;
      memcpy(&v, data + i, sizeof(double));
      v *= scale;
      memcpy(data + i, &v, sizeof(double));
    }
    break;
  }
  default:
    break;
  }
}

static void *audio_playback_thread(void *arg) {
  AudioStream *stream = (AudioStream *)arg;
  if (!stream)
    return NULL;

  snd_pcm_t *handle = NULL;
  snd_pcm_format_t pcm_format = SND_PCM_FORMAT_S16_LE;

  switch (stream->format) {
  case AUDIO_FORMAT_U8:
    pcm_format = SND_PCM_FORMAT_U8;
    break;
  case AUDIO_FORMAT_MULAW:
    pcm_format = SND_PCM_FORMAT_MU_LAW;
    break;
  case AUDIO_FORMAT_S16LE:
    pcm_format = SND_PCM_FORMAT_S16_LE;
    break;
  case AUDIO_FORMAT_S24LE:
    pcm_format = SND_PCM_FORMAT_S24_LE;
    break;
  case AUDIO_FORMAT_S32LE:
    pcm_format = SND_PCM_FORMAT_S32_LE;
    break;
  case AUDIO_FORMAT_F32LE:
    pcm_format = SND_PCM_FORMAT_FLOAT_LE;
    break;
  case AUDIO_FORMAT_F64LE:
    pcm_format = SND_PCM_FORMAT_FLOAT64_LE;
    break;
  default:
    log_error("Unsupported audio format for ALSA playback");
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&stream->data_lock);
    return NULL;
  }

  if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    log_error("Failed to open ALSA default device for audio playback");
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&stream->data_lock);
    return NULL;
  }

  if (snd_pcm_set_params(handle, pcm_format, SND_PCM_ACCESS_RW_INTERLEAVED,
                         stream->channels ? stream->channels : 2,
                         stream->rate ? stream->rate : 44100, 1, 100000) < 0) {
    log_error("Failed to configure ALSA playback parameters");
    snd_pcm_close(handle);
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&stream->data_lock);
    return NULL;
  }

  stream->backend_data = handle;

  size_t bytes_per_sample = audio_format_bytes_per_sample(stream->format);
  if (bytes_per_sample == 0 || stream->channels == 0) {
    log_error("Invalid audio format for playback");
    snd_pcm_close(handle);
    stream->backend_data = NULL;
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&stream->data_lock);
    return NULL;
  }
  size_t frame_size = bytes_per_sample * stream->channels;
  uint8_t buffer[8192];
  size_t max_chunk = sizeof(buffer);
  if (max_chunk < frame_size) {
    log_error("Audio frame size too large for playback buffer");
    snd_pcm_close(handle);
    stream->backend_data = NULL;
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&stream->data_lock);
    return NULL;
  }
  max_chunk -= max_chunk % frame_size;

  while (!stream->playback_stop) {
    bool do_flush = false;
    pthread_mutex_lock(&stream->data_lock);
    if (stream->flush_playback) {
      do_flush = true;
      stream->flush_playback = false;
    }
    if (do_flush) {
      pthread_mutex_unlock(&stream->data_lock);
      snd_pcm_drop(handle);
      snd_pcm_prepare(handle);
      continue;
    }
    while (!stream->playback_stop && stream->playback_paused) {
      stream->state = AUDIO_STATE_PAUSED;
      pthread_cond_wait(&stream->data_ready, &stream->data_lock);
    }
    if (stream->playback_stop) {
      pthread_mutex_unlock(&stream->data_lock);
      break;
    }
    while (!stream->playback_stop &&
           stream->playback_offset >= stream->data_sz &&
           !stream->transmission_complete) {
      pthread_cond_wait(&stream->data_ready, &stream->data_lock);
    }
    if (stream->playback_stop) {
      pthread_mutex_unlock(&stream->data_lock);
      break;
    }
    if (!stream->data) {
      pthread_mutex_unlock(&stream->data_lock);
      break;
    }
    size_t available = stream->data_sz - stream->playback_offset;
    if (available < frame_size) {
      bool done = stream->transmission_complete;
      bool should_loop = false;
      if (done && stream->loop_count > 0 && stream->data_sz >= frame_size) {
        should_loop = true;
        if (stream->loop_count != UINT32_MAX && stream->loop_count > 0) {
          stream->loop_count -= 1;
        }
        stream->playback_offset = 0;
      }
      pthread_mutex_unlock(&stream->data_lock);
      if (should_loop) {
        continue;
      }
      if (done)
        break;
      continue;
    }
    size_t chunk = available > max_chunk ? max_chunk : available;
    chunk -= chunk % frame_size;
    if (chunk == 0) {
      pthread_mutex_unlock(&stream->data_lock);
      continue;
    }
    memcpy(buffer, stream->data + stream->playback_offset, chunk);
    uint32_t volume = stream->volume;
    stream->playback_offset += chunk;
    pthread_mutex_unlock(&stream->data_lock);

    if (volume != 100) {
      apply_volume(buffer, chunk, stream->format, volume);
    }

    size_t frames_left = chunk / frame_size;
    const uint8_t *cursor = buffer;
    while (frames_left > 0 && !stream->playback_stop) {
      snd_pcm_sframes_t written = snd_pcm_writei(handle, cursor, frames_left);
      if (written < 0) {
        if (stream->playback_stop)
          break;
        written = snd_pcm_recover(handle, (int)written, 1);
        if (written < 0) {
          if (!stream->playback_stop && written != -EBADFD) {
            log_error("ALSA playback error: %s", snd_strerror((int)written));
          }
          stream->playback_stop = true;
          break;
        }
        continue;
      }
      cursor += (size_t)written * frame_size;
      frames_left -= (size_t)written;
    }
  }

  if (stream->playback_stop) {
    snd_pcm_drop(handle);
  } else {
    snd_pcm_drain(handle);
  }
  snd_pcm_close(handle);
  stream->backend_data = NULL;

  pthread_mutex_lock(&stream->data_lock);
  stream->playback_active = false;
  stream->state = AUDIO_STATE_STOPPED;
  pthread_mutex_unlock(&stream->data_lock);
  return NULL;
}
#endif

static bool alsa_available(void) {
#if defined(HAVE_ALSA)
  return true;
#else
  return false;
#endif
}

static bool alsa_start(AudioStream *stream) {
#if defined(HAVE_ALSA)
  if (pthread_create(&stream->playback_thread, NULL, audio_playback_thread,
                     stream) != 0) {
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    pthread_mutex_unlock(&stream->data_lock);
    return false;
  }
  return true;
#else
  (void)stream;
  log_error("Audio playback not available: ALSA support missing");
  return false;
#endif
}

static void alsa_pause(AudioStream *stream) {
#if defined(HAVE_ALSA)
  if (!stream)
    return;
  snd_pcm_t *handle = (snd_pcm_t *)stream->backend_data;
  if (handle) {
    snd_pcm_pause(handle, 1);
  }
#else
  (void)stream;
#endif
}

static void alsa_resume(AudioStream *stream) {
#if defined(HAVE_ALSA)
  if (!stream)
    return;
  snd_pcm_t *handle = (snd_pcm_t *)stream->backend_data;
  if (handle) {
    snd_pcm_pause(handle, 0);
  }
#else
  (void)stream;
#endif
}

static void alsa_stop(AudioStream *stream) {
  if (!stream)
    return;
#if defined(HAVE_ALSA)
  snd_pcm_t *handle = (snd_pcm_t *)stream->backend_data;
  if (handle) {
    snd_pcm_drop(handle);
  }
#endif
  pthread_mutex_lock(&stream->data_lock);
  stream->playback_stop = true;
  pthread_cond_broadcast(&stream->data_ready);
  pthread_mutex_unlock(&stream->data_lock);
  if (stream->playback_active) {
    pthread_join(stream->playback_thread, NULL);
  }
  pthread_mutex_lock(&stream->data_lock);
  stream->playback_active = false;
  stream->state = AUDIO_STATE_STOPPED;
  pthread_mutex_unlock(&stream->data_lock);
}

const AudioBackend *audio_backend_alsa(void) {
  static const AudioBackend backend = {
      .name = "alsa",
      .available = alsa_available,
      .start = alsa_start,
      .pause = alsa_pause,
      .resume = alsa_resume,
      .stop = alsa_stop,
  };
  return &backend;
}
