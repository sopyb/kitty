#include "audio_output.h"
#include "audio_backend.h"
#include "audio_stream.h"
#include "data-types.h"

bool audio_output_start(AudioStream *stream) {
  if (!stream || stream->playback_active)
    return false;
  if (stream->playback_failed)
    return false;
  if (!stream->data || stream->data_sz == 0)
    return false;

  pthread_mutex_lock(&stream->data_lock);
  stream->playback_stop = false;
  stream->playback_paused = false;
  stream->playback_active = true;
  stream->state = AUDIO_STATE_PLAYING;
  pthread_mutex_unlock(&stream->data_lock);

  if (!stream->backend) {
    stream->backend = audio_backend_select();
  }
  if (!stream->backend) {
    log_error("Audio playback not available: no backend available");
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    stream->playback_failed = true;
    pthread_mutex_unlock(&stream->data_lock);
    return false;
  }

  if (!stream->backend->start(stream)) {
    pthread_mutex_lock(&stream->data_lock);
    stream->playback_active = false;
    stream->state = AUDIO_STATE_STOPPED;
    stream->playback_failed = true;
    pthread_mutex_unlock(&stream->data_lock);
    return false;
  }
  return true;
}

void audio_output_stop(AudioStream *stream) {
  if (!stream || !stream->playback_active)
    return;
  if (stream->backend) {
    stream->backend->stop(stream);
  }
  pthread_mutex_lock(&stream->data_lock);
  stream->playback_active = false;
  stream->playback_paused = false;
  pthread_mutex_unlock(&stream->data_lock);
  stream->backend = NULL;
}

void audio_output_pause(AudioStream *stream) {
  if (!stream)
    return;
  pthread_mutex_lock(&stream->data_lock);
  stream->playback_paused = true;
  stream->state = AUDIO_STATE_PAUSED;
  pthread_cond_broadcast(&stream->data_ready);
  pthread_mutex_unlock(&stream->data_lock);
  if (stream->backend && stream->backend->pause) {
    stream->backend->pause(stream);
  }
}

void audio_output_resume(AudioStream *stream) {
  if (!stream)
    return;
  if (stream->backend && stream->backend->resume) {
    stream->backend->resume(stream);
  }
  pthread_mutex_lock(&stream->data_lock);
  stream->playback_paused = false;
  stream->state = AUDIO_STATE_PLAYING;
  pthread_cond_broadcast(&stream->data_ready);
  pthread_mutex_unlock(&stream->data_lock);
}
