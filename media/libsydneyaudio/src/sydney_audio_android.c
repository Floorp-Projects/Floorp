/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <time.h>
#include <jni.h>
#include "sydney_audio.h"

#include "android/log.h"

#ifndef ALOG
#if defined(DEBUG) || defined(FORCE_ALOG)
#define ALOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gecko - SYDNEY_AUDIO" , ## args)
#else
#define ALOG(args...)
#endif
#endif

/* Android implementation based on sydney_audio_mac.c */

#define NANOSECONDS_PER_SECOND     1000000000
#define NANOSECONDS_IN_MILLISECOND 1000000
#define MILLISECONDS_PER_SECOND    1000

/* android.media.AudioTrack */
struct AudioTrack {
  jclass    class;
  jmethodID constructor;
  jmethodID flush;
  jmethodID getminbufsz;
  jmethodID pause;
  jmethodID play;
  jmethodID release;
  jmethodID setvol;
  jmethodID stop;
  jmethodID write;
  jmethodID getpos;
};

enum AudioTrackMode {
  MODE_STATIC = 0,
  MODE_STREAM = 1
};

/* android.media.AudioManager */
enum AudioManagerStream {
  STREAM_VOICE_CALL = 0,
  STREAM_SYSTEM = 1,
  STREAM_RING = 2,
  STREAM_MUSIC = 3,
  STREAM_ALARM = 4,
  STREAM_NOTIFICATION = 5,
  STREAM_DTMF = 8
};

/* android.media.AudioFormat */
enum AudioFormatChannel {
  CHANNEL_OUT_MONO = 4,
  CHANNEL_OUT_STEREO = 12
};

enum AudioFormatEncoding {
  ENCODING_PCM_16BIT = 2,
  ENCODING_PCM_8BIT = 3
};

struct sa_stream {
  jobject output_unit;
  jbyteArray output_buf;
  unsigned int output_buf_size;

  unsigned int rate;
  unsigned int channels;
  unsigned int isPaused;

  int64_t lastStartTime;
  int64_t timePlaying;
  int64_t amountWritten;
  unsigned int bufferSize;

  jclass at_class;
};

static struct AudioTrack at;
extern JNIEnv * GetJNIForThread();

static jclass
init_jni_bindings(JNIEnv *jenv) {
  jclass class = (*jenv)->FindClass(jenv, "android/media/AudioTrack");
  if (!class) {
    return NULL;
  }
  at.constructor = (*jenv)->GetMethodID(jenv, class, "<init>", "(IIIIII)V");
  at.flush       = (*jenv)->GetMethodID(jenv, class, "flush", "()V");
  at.getminbufsz = (*jenv)->GetStaticMethodID(jenv, class, "getMinBufferSize", "(III)I");
  at.pause       = (*jenv)->GetMethodID(jenv, class, "pause", "()V");
  at.play        = (*jenv)->GetMethodID(jenv, class, "play",  "()V");
  at.release     = (*jenv)->GetMethodID(jenv, class, "release",  "()V");
  at.setvol      = (*jenv)->GetMethodID(jenv, class, "setStereoVolume",  "(FF)I");
  at.stop        = (*jenv)->GetMethodID(jenv, class, "stop",  "()V");
  at.write       = (*jenv)->GetMethodID(jenv, class, "write", "([BII)I");
  at.getpos      = (*jenv)->GetMethodID(jenv, class, "getPlaybackHeadPosition", "()I");

  return (*jenv)->NewGlobalRef(jenv, class);
}

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
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }

  s->output_unit = NULL;
  s->output_buf  = NULL;
  s->output_buf_size = 0;
  s->rate        = rate;
  s->channels    = channels;
  s->isPaused    = 0;

  s->lastStartTime = 0;
  s->timePlaying = 0;
  s->amountWritten = 0;

  s->bufferSize = 0;

  *_s = s;
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

  JNIEnv *jenv = GetJNIForThread();
  if (!jenv) {
    return SA_ERROR_NO_DEVICE;
  }

  if ((*jenv)->PushLocalFrame(jenv, 4)) {
    return SA_ERROR_OOM;
  }

  s->at_class = init_jni_bindings(jenv);
  if (!s->at_class) {
    (*jenv)->PopLocalFrame(jenv, NULL);
    return SA_ERROR_NO_DEVICE;
  }

  int32_t chanConfig = s->channels == 1 ?
    CHANNEL_OUT_MONO : CHANNEL_OUT_STEREO;

  jint minsz = (*jenv)->CallStaticIntMethod(jenv, s->at_class, at.getminbufsz,
                                            s->rate, chanConfig, ENCODING_PCM_16BIT);
  if (minsz <= 0) {
    (*jenv)->PopLocalFrame(jenv, NULL);
    return SA_ERROR_INVALID;
  }

  s->bufferSize = s->rate * s->channels * sizeof(int16_t);
  if (s->bufferSize < minsz) {
    s->bufferSize = minsz;
  }

  jobject obj =
    (*jenv)->NewObject(jenv, s->at_class, at.constructor,
                       STREAM_MUSIC,
                       s->rate,
                       chanConfig,
                       ENCODING_PCM_16BIT,
                       s->bufferSize,
                       MODE_STREAM);

  jthrowable exception = (*jenv)->ExceptionOccurred(jenv);
  if (exception) {
    (*jenv)->ExceptionDescribe(jenv);
    (*jenv)->ExceptionClear(jenv);
    (*jenv)->PopLocalFrame(jenv, NULL);
    return SA_ERROR_INVALID;
  }

  if (!obj) {
    (*jenv)->PopLocalFrame(jenv, NULL);
    return SA_ERROR_OOM;
  }

  s->output_unit = (*jenv)->NewGlobalRef(jenv, obj);

  /* arbitrary buffer size.  using a preallocated buffer avoids churning
     the GC every audio write. */
  s->output_buf_size = 4096 * s->channels * sizeof(int16_t);
  jbyteArray buf = (*jenv)->NewByteArray(jenv, s->output_buf_size);
  if (!buf) {
    (*jenv)->ExceptionClear(jenv);
    (*jenv)->DeleteGlobalRef(jenv, s->output_unit);
    (*jenv)->PopLocalFrame(jenv, NULL);
    return SA_ERROR_OOM;
  }

  s->output_buf = (*jenv)->NewGlobalRef(jenv, buf);

  (*jenv)->PopLocalFrame(jenv, NULL);

  ALOG("%p - New stream %u %u bsz=%u min=%u obsz=%u", s, s->rate, s->channels,
       s->bufferSize, minsz, s->output_buf_size);

  return SA_SUCCESS;
}


int
sa_stream_destroy(sa_stream_t *s) {

  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }

  JNIEnv *jenv = GetJNIForThread();
  if (!jenv) {
    return SA_SUCCESS;
  }

  if (s->output_buf) {
    (*jenv)->DeleteGlobalRef(jenv, s->output_buf);
  }
  if (s->output_unit) {
    (*jenv)->CallVoidMethod(jenv, s->output_unit, at.stop);
    (*jenv)->CallVoidMethod(jenv, s->output_unit, at.flush);
    (*jenv)->CallVoidMethod(jenv, s->output_unit, at.release);
    (*jenv)->DeleteGlobalRef(jenv, s->output_unit);
  }
  if (s->at_class) {
    (*jenv)->DeleteGlobalRef(jenv, s->at_class);
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
  JNIEnv *jenv = GetJNIForThread();
  if ((*jenv)->PushLocalFrame(jenv, 2)) {
    return SA_ERROR_OOM;
  }

  unsigned char *p = data;
  jint r = 0;
  size_t wrote = 0;
  do {
    size_t towrite = nbytes - wrote;
    if (towrite > s->output_buf_size) {
      towrite = s->output_buf_size;
    }
    (*jenv)->SetByteArrayRegion(jenv, s->output_buf, 0, towrite, p);

    r = (*jenv)->CallIntMethod(jenv,
                               s->output_unit,
                               at.write,
                               s->output_buf,
                               0,
                               towrite);
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

  ALOG("%p - Wrote %u", s,  nbytes);
  s->amountWritten += nbytes;

  (*jenv)->PopLocalFrame(jenv, NULL);

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

  JNIEnv *jenv = GetJNIForThread();
  *pos  = (*jenv)->CallIntMethod(jenv, s->output_unit, at.getpos);

  /* android returns number of frames, so:
     position = frames * (PCM_16_BIT == 2 bytes) * channels
  */
  *pos *= s->channels * sizeof(int16_t);
  return SA_SUCCESS;
}


int
sa_stream_pause(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  JNIEnv *jenv = GetJNIForThread();
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

  (*jenv)->CallVoidMethod(jenv, s->output_unit, at.pause);
  return SA_SUCCESS;
}


int
sa_stream_resume(sa_stream_t *s) {

  if (s == NULL || s->output_unit == NULL) {
    return SA_ERROR_NO_INIT;
  }

  ALOG("%p - resume", s);

  JNIEnv *jenv = GetJNIForThread();
  s->isPaused = 0;

  /* Update stats */
  struct timespec current_time;
  clock_gettime(CLOCK_REALTIME, &current_time);
  int64_t ticker = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
  s->lastStartTime = ticker;

  (*jenv)->CallVoidMethod(jenv, s->output_unit, at.play);
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

  struct timespec ts = {x / NANOSECONDS_PER_SECOND, x % NANOSECONDS_PER_SECOND};
  nanosleep(&ts, NULL);

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

  JNIEnv *jenv = GetJNIForThread();
  (*jenv)->CallIntMethod(jenv, s->output_unit, at.setvol,
                         (jfloat)vol, (jfloat)vol);

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

