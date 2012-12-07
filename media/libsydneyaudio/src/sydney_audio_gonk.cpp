/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <time.h>
extern "C" {
#include "sydney_audio.h"
}

#include "gonk/AudioTrack.h"
#include "android/log.h"

#if defined(DEBUG) || defined(FORCE_ALOG)
#define ALOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gecko - SYDNEY_AUDIO" , ## args)
#else
#define ALOG(args...)
#endif

/* Gonk implementation based on sydney_audio_android.c */

#define NANOSECONDS_PER_SECOND     1000000000
#define NANOSECONDS_IN_MILLISECOND 1000000
#define MILLISECONDS_PER_SECOND    1000

using namespace android;

struct sa_stream {
  AudioTrack *output_unit;

  unsigned int rate;
  unsigned int channels;
  unsigned int isPaused;

  int64_t lastStartTime;
  int64_t timePlaying;
  int64_t amountWritten;
  unsigned int bufferSize;

  int streamType;
};

/*
 * -----------------------------------------------------------------------------
 * Startup and shutdown functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_create_pcm(
  sa_stream_t      ** _s,
  const char        * client_name,
  sa_mode_t           mode,
  sa_pcm_format_t     format,
  unsigned  int       rate,
  unsigned  int       channels
) {

  /*
   * Make sure we return a NULL stream pointer on failure.
   */
  if (_s == NULL) {
    return SA_ERROR_INVALID;
  }
  *_s = NULL;

  if (mode != SA_MODE_WRONLY) {
    return SA_ERROR_NOT_SUPPORTED;
  }
  if (format != SA_PCM_FORMAT_S16_NE) {
    return SA_ERROR_NOT_SUPPORTED;
  }
  if (channels != 1 && channels != 2) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  /*
   * Allocate the instance and required resources.
   */
  sa_stream_t *s;
  if ((s = (sa_stream_t *)malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }

  s->output_unit = NULL;
  s->rate        = rate;
  s->channels    = channels;
  s->isPaused    = 0;

  s->lastStartTime = 0;
  s->timePlaying = 0;
  s->amountWritten = 0;

  s->bufferSize = 0;

  s->streamType = AudioSystem::SYSTEM;

  *_s = s;
  return SA_SUCCESS;
}

/* Assign audio stream type for argument used by AudioTrack class */
int
sa_stream_set_stream_type(sa_stream_t *s,  const sa_stream_type_t stream_type)
{
  if (s->output_unit != NULL) {
    return SA_ERROR_INVALID;
  }

  switch (stream_type)
 {
    case SA_STREAM_TYPE_VOICE_CALL:
      s->streamType = AudioSystem::VOICE_CALL;
      break;
    case SA_STREAM_TYPE_SYSTEM:
      s->streamType = AudioSystem::SYSTEM;
      break;
    case SA_STREAM_TYPE_RING:
      s->streamType = AudioSystem::RING;
      break;
    case SA_STREAM_TYPE_MUSIC:
      s->streamType = AudioSystem::MUSIC;
      break;
    case SA_STREAM_TYPE_ALARM:
      s->streamType = AudioSystem::ALARM;
      break;
    case SA_STREAM_TYPE_NOTIFICATION:
      s->streamType = AudioSystem::NOTIFICATION;
      break;
    case SA_STREAM_TYPE_BLUETOOTH_SCO:
      s->streamType = AudioSystem::BLUETOOTH_SCO;
      break;
    case SA_STREAM_TYPE_ENFORCED_AUDIBLE:
      s->streamType = AudioSystem::ENFORCED_AUDIBLE;
      break;
    case SA_STREAM_TYPE_DTMF:
      s->streamType = AudioSystem::DTMF;
      break;
    case SA_STREAM_TYPE_FM:
      s->streamType = AudioSystem::FM;
      break;
    default:
      return SA_ERROR_INVALID;
 }

  return SA_SUCCESS;
}

int
sa_stream_open(sa_stream_t *s) {

  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (s->output_unit != NULL) {
    return SA_ERROR_INVALID;
  }

  int32_t chanConfig = s->channels == 1 ?
    AudioSystem::CHANNEL_OUT_MONO : AudioSystem::CHANNEL_OUT_STEREO;

  int frameCount;
  if (AudioTrack::getMinFrameCount(&frameCount, s->streamType,
                                   s->rate) != NO_ERROR) {
    return SA_ERROR_INVALID;
  }
  int minsz = frameCount * s->channels * sizeof(int16_t);

  s->bufferSize = s->rate * s->channels * sizeof(int16_t);
  if (s->bufferSize < minsz) {
    s->bufferSize = minsz;
  }

  AudioTrack *track =
    new AudioTrack(s->streamType,
                   s->rate,
                   AudioSystem::PCM_16_BIT,
                   chanConfig,
                   frameCount,
                   0,
                   NULL, NULL,
                   0,
                   0);

  if (track->initCheck() != NO_ERROR) {
    delete track;
    return SA_ERROR_INVALID;
  }

  s->output_unit = track;

  ALOG("%p - New stream %u %u bsz=%u min=%u", s, s->rate, s->channels,
       s->bufferSize, minsz);

  return SA_SUCCESS;
}


int
sa_stream_destroy(sa_stream_t *s) {

  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }

  static bool firstLeaked = 0;
  if (s->output_unit) {
    s->output_unit->stop();
    s->output_unit->flush();
    // XXX: Figure out why we crash if we don't leak the first AudioTrack
    if (firstLeaked)
      delete s->output_unit;
    else
      firstLeaked = true;
  }
  free(s);

  ALOG("%p - Stream destroyed", s);
  return SA_SUCCESS;
}


/*
 * -----------------------------------------------------------------------------
 * Data read and write functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_write(sa_stream_t *s, const void *data, size_t nbytes) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (nbytes == 0) {
    return SA_SUCCESS;
  }

  const char *p = (char *)data;
  ssize_t r = 0;
  size_t wrote = 0;
  do {
    size_t towrite = nbytes - wrote;

    r = s->output_unit->write(p, towrite);
    if (r < 0) {
      ALOG("%p - Write failed %d", s, r);
      break;
    }

    /* AudioTrack::write is blocking when the AudioTrack is playing.  When
       it's not playing, it's a non-blocking call that will return a short
       write when the buffer is full.  Use a short write to indicate a good
       time to start the AudioTrack playing. */
    if (r != towrite) {
      ALOG("%p - Buffer full, starting playback", s);
      sa_stream_resume(s);
    }

    p += r;
    wrote += r;
  } while (wrote < nbytes);

  s->amountWritten += nbytes;

  return r < 0 ? SA_ERROR_INVALID : SA_SUCCESS;
}


/*
 * -----------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_write_size(sa_stream_t *s, size_t *size) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  /* No android API for this, so estimate based on how much we have played and
   * how much we have written. */
  *size = s->bufferSize - ((s->timePlaying * s->channels * s->rate * sizeof(int16_t) /
                            MILLISECONDS_PER_SECOND) - s->amountWritten);

  /* Available buffer space can't exceed bufferSize. */
  if (*size > s->bufferSize) {
    *size = s->bufferSize;
  }
  ALOG("%p - Write Size tp=%lld aw=%u sz=%zu", s, s->timePlaying, s->amountWritten, *size);

  return SA_SUCCESS;
}


int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  uint32_t framePosition;
  if (s->output_unit->getPosition(&framePosition) != NO_ERROR)
    return SA_ERROR_INVALID;

  /* android returns number of frames, so:
     position = frames * (PCM_16_BIT == 2 bytes) * channels
  */
  *pos = framePosition * s->channels * sizeof(int16_t);
  return SA_SUCCESS;
}


int
sa_stream_pause(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  s->isPaused = 1;

  /* Update stats */
  if (s->lastStartTime != 0) {
    /* if lastStartTime is not zero, so playback has started */
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    int64_t ticker = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
    s->timePlaying += ticker - s->lastStartTime;
  }
  ALOG("%p - Pause total time playing: %lld total written: %lld", s,  s->timePlaying, s->amountWritten);

  s->output_unit->pause();
  return SA_SUCCESS;
}


int
sa_stream_resume(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  ALOG("%p - resume", s);

  s->isPaused = 0;

  /* Update stats */
  struct timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  int64_t ticker = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
  s->lastStartTime = ticker;

  s->output_unit->start();
  return SA_SUCCESS;
}


int
sa_stream_drain(sa_stream_t *s)
{
  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

/* This is somewhat of a hack (see bug 693131).  The AudioTrack documentation
   doesn't make it clear how much data must be written before a chunk of data is
   played, and experimentation with short streams required filling the entire
   allocated buffer.  To guarantee that short streams (and the end of longer
   streams) are audible, write an entire bufferSize of silence before sleeping.
   This guarantees the short write logic in sa_stream_write is hit and the
   stream is playing before sleeping.  Note that the sleep duration is
   calculated from the duration of audio written before writing silence. */
  size_t available;
  sa_stream_get_write_size(s, &available);

  void *p = calloc(1, s->bufferSize);
  sa_stream_write(s, p, s->bufferSize);
  free(p);

  /* There is no way with the Android SDK to determine exactly how
     long to playback.  So estimate and sleep for that long. */
  unsigned long long x = (s->bufferSize - available) * 1000 / s->channels / s->rate /
                         sizeof(int16_t) * NANOSECONDS_IN_MILLISECOND;
  ALOG("%p - Drain - flush %u, sleep for %llu ns", s, available, x);

  struct timespec ts = {(time_t)(x / NANOSECONDS_PER_SECOND),
                        (time_t)(x % NANOSECONDS_PER_SECOND)};
  nanosleep(&ts, NULL);
  s->output_unit->flush();

  return SA_SUCCESS;
}


/*
 * -----------------------------------------------------------------------------
 * Extension functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_set_volume_abs(sa_stream_t *s, float vol) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  s->output_unit->setVolume(vol, vol);
  return SA_SUCCESS;
}

/*
 * -----------------------------------------------------------------------------
 * Unsupported functions
 * -----------------------------------------------------------------------------
 */
#define UNSUPPORTED(func)   func { return SA_ERROR_NOT_SUPPORTED; }

UNSUPPORTED(int sa_stream_create_opaque(sa_stream_t **s, const char *client_name, sa_mode_t mode, const char *codec))
UNSUPPORTED(int sa_stream_set_write_lower_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_read_lower_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_write_upper_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_read_upper_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_channel_map(sa_stream_t *s, const sa_channel_t map[], unsigned int n))
UNSUPPORTED(int sa_stream_set_xrun_mode(sa_stream_t *s, sa_xrun_mode_t mode))
UNSUPPORTED(int sa_stream_set_non_interleaved(sa_stream_t *s, int enable))
UNSUPPORTED(int sa_stream_set_dynamic_rate(sa_stream_t *s, int enable))
UNSUPPORTED(int sa_stream_set_driver(sa_stream_t *s, const char *driver))
UNSUPPORTED(int sa_stream_start_thread(sa_stream_t *s, sa_event_callback_t callback))
UNSUPPORTED(int sa_stream_stop_thread(sa_stream_t *s))
UNSUPPORTED(int sa_stream_change_device(sa_stream_t *s, const char *device_name))
UNSUPPORTED(int sa_stream_change_read_volume(sa_stream_t *s, const int32_t vol[], unsigned int n))
UNSUPPORTED(int sa_stream_change_write_volume(sa_stream_t *s, const int32_t vol[], unsigned int n))
UNSUPPORTED(int sa_stream_change_rate(sa_stream_t *s, unsigned int rate))
UNSUPPORTED(int sa_stream_change_meta_data(sa_stream_t *s, const char *name, const void *data, size_t size))
UNSUPPORTED(int sa_stream_change_user_data(sa_stream_t *s, const void *value))
UNSUPPORTED(int sa_stream_set_adjust_rate(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_nchannels(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_pcm_format(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_watermarks(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_get_mode(sa_stream_t *s, sa_mode_t *access_mode))
UNSUPPORTED(int sa_stream_get_codec(sa_stream_t *s, char *codec, size_t *size))
UNSUPPORTED(int sa_stream_get_pcm_format(sa_stream_t *s, sa_pcm_format_t *format))
UNSUPPORTED(int sa_stream_get_rate(sa_stream_t *s, unsigned int *rate))
UNSUPPORTED(int sa_stream_get_nchannels(sa_stream_t *s, int *nchannels))
UNSUPPORTED(int sa_stream_get_user_data(sa_stream_t *s, void **value))
UNSUPPORTED(int sa_stream_get_write_lower_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_read_lower_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_write_upper_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_read_upper_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_channel_map(sa_stream_t *s, sa_channel_t map[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_xrun_mode(sa_stream_t *s, sa_xrun_mode_t *mode))
UNSUPPORTED(int sa_stream_get_non_interleaved(sa_stream_t *s, int *enabled))
UNSUPPORTED(int sa_stream_get_dynamic_rate(sa_stream_t *s, int *enabled))
UNSUPPORTED(int sa_stream_get_driver(sa_stream_t *s, char *driver_name, size_t *size))
UNSUPPORTED(int sa_stream_get_device(sa_stream_t *s, char *device_name, size_t *size))
UNSUPPORTED(int sa_stream_get_read_volume(sa_stream_t *s, int32_t vol[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_write_volume(sa_stream_t *s, int32_t vol[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_meta_data(sa_stream_t *s, const char *name, void*data, size_t *size))
UNSUPPORTED(int sa_stream_get_adjust_rate(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_nchannels(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_pcm_format(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_watermarks(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_state(sa_stream_t *s, sa_state_t *state))
UNSUPPORTED(int sa_stream_get_event_error(sa_stream_t *s, sa_error_t *error))
UNSUPPORTED(int sa_stream_get_event_notify(sa_stream_t *s, sa_notify_t *notify))
UNSUPPORTED(int sa_stream_read(sa_stream_t *s, void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_read_ni(sa_stream_t *s, unsigned int channel, void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_write_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_pwrite(sa_stream_t *s, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence))
UNSUPPORTED(int sa_stream_pwrite_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence))
UNSUPPORTED(int sa_stream_get_read_size(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_volume_abs(sa_stream_t *s, float *vol))
UNSUPPORTED(int sa_stream_get_min_write(sa_stream_t *s, size_t *size))

const char *sa_strerror(int code) { return NULL; }

