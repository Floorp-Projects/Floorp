/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include "sydney_audio.h"

/* Pulseaudio implementation based heavily on sydney_audio_alsa.c */

/*
 * The audio interface is based on a "pull" I/O model, which means you
 * can't just provide a data buffer and tell the audio device to play; you must
 * register a callback and provide data as the device asks for it. To support
 * sydney audio's "write-to-play" style interface, we have to buffer up the
 * data as it arrives and feed it to the callback as required.
 *
 * This is handled by a simple linked list of buffers; data is always written
 * to the tail and read from the head. Each buffer tracks the start and end
 * positions of its contained data. Buffers are allocated when the tail buffer
 * fills, and freed when the head buffer empties. There is always at least one
 * buffer allocated.
 *
 *       s   e      s      e      s  e            + data read
 *    +++#####  ->  ########  ->  ####----        # data written
 *    ^                           ^               - empty
 *    bl_head                     bl_tail
 */

typedef struct sa_buf sa_buf;
struct sa_buf {
  unsigned int      size;
  unsigned int      start;
  unsigned int      end;
  sa_buf          * next;
  unsigned char     data[0];
};

struct sa_stream {
  pa_context*       context;
  pa_stream*        stream;
  pa_sample_spec    sample_spec;
  pa_threaded_mainloop* m;

  pthread_t         thread_id;
  pthread_mutex_t   mutex;

  char              playing;
  int64_t           bytes_written;
  char              client_name[255];

  /* buffer list */
  sa_buf          * bl_head;
  sa_buf          * bl_tail;
  int               n_bufs;
};


/*
 * Use a default buffer size with enough room for one second of audio,
 * assuming stereo data at 44.1kHz with 32 bits per channel, and impose
 * a generous limit on the number of buffers.
 */
#define BUF_SIZE    (2 * 44100 * 4)
#define BUF_LIMIT   5

#if BUF_LIMIT < 2
#error BUF_LIMIT must be at least 2!
#endif

static void audio_callback(void* data);
static void stream_write_callback(pa_stream *stream, size_t length, void *userdata);
static void stream_latency_update_callback(pa_stream *stream, void *userdata);
static void context_state_callback(pa_context *c, void *userdata);
static sa_buf *new_buffer(void);



/*
 * -----------------------------------------------------------------------------
 * Pulseaudio callback functions
 * -----------------------------------------------------------------------------
 */
 
static void context_state_callback(pa_context *c, void *userdata) {
  sa_stream_t* s = (sa_stream_t*)userdata;
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    case PA_CONTEXT_FAILED:
      pa_threaded_mainloop_signal(s->m, 0);
      break;
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
  }
}

static void stream_state_callback(pa_stream *stream, void *userdata) {
  sa_stream_t* s = (sa_stream_t*)userdata;
  switch (pa_stream_get_state(stream)) {

      case PA_STREAM_READY:
      case PA_STREAM_FAILED:
      case PA_STREAM_TERMINATED:
        pa_threaded_mainloop_signal(s->m, 0);
        break;
      case PA_STREAM_UNCONNECTED:
      case PA_STREAM_CREATING:
        break;
  }
}

static void stream_write_callback(pa_stream *stream, size_t length, void *userdata)
{
  sa_stream_t* s = (sa_stream_t*)userdata;
  pa_threaded_mainloop_signal(s->m, 0);
}

static void stream_latency_update_callback(pa_stream *stream, void *userdata)
{
  sa_stream_t* s = (sa_stream_t*)userdata;
  pa_threaded_mainloop_signal(s->m, 0);
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
  unsigned  int       n_channels
) {
  sa_stream_t   * s = 0;
  char *server = NULL;

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
  if (format != SA_PCM_FORMAT_S16_LE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  /*
   * Allocate the instance and required resources.
   */
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) {
    return SA_ERROR_OOM;
  }
  if ((s->bl_head = new_buffer()) == NULL) {
    free(s);
    return SA_ERROR_OOM;
  }

  if (pthread_mutex_init(&s->mutex, NULL) != 0) {
    free(s->bl_head);
    free(s);
    return SA_ERROR_SYSTEM;
  }

  s->stream        = NULL;
  s->m             = NULL;
  s->thread_id     = 0;
  s->playing       = 0;
  s->bytes_written = 0;

  s->bl_tail       = s->bl_head;
  s->n_bufs        = 1;

  s->sample_spec.format = PA_SAMPLE_S16LE;
  s->sample_spec.channels = n_channels;
  s->sample_spec.rate = rate;

  strcpy(s->client_name, client_name);

  /* Set up a new main loop */
  s->m = pa_threaded_mainloop_new();
  pa_threaded_mainloop_start(s->m);

  pa_threaded_mainloop_lock(s->m);

  /* Create a new connection context */
  if (!(s->context = pa_context_new(pa_threaded_mainloop_get_api(s->m), "OggPlay"))) {
    fprintf(stderr, "pa_context_new() failed.\n");
    goto unlock_and_fail;
  }
  pa_context_set_state_callback(s->context, context_state_callback, s);

  pa_context_connect(s->context, server, 0, NULL);

  /* Wait until the context is ready */
  pa_threaded_mainloop_wait(s->m);
  if (pa_context_get_state(s->context) != PA_CONTEXT_READY) {
    fprintf(stderr, "creating Pulseaudio Context failed\n");
    goto unlock_and_fail;
  }
  pa_threaded_mainloop_unlock(s->m);

  *_s = s;
  return SA_SUCCESS;

unlock_and_fail:
  pa_threaded_mainloop_unlock(s->m);
  free(s);
  return SA_ERROR_OOM;
}

int
sa_stream_open(sa_stream_t *s) {
  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (s->stream != NULL) {
    return SA_ERROR_INVALID;
  }

  pa_threaded_mainloop_lock(s->m);
  if (!(s->stream = pa_stream_new(s->context, s->client_name, &s->sample_spec, NULL))) {
    fprintf(stderr, "pa_stream_new() failed: %s\n", pa_strerror(pa_context_errno(s->context)));
    goto unlock_and_fail;
  }

  pa_stream_set_state_callback(s->stream, stream_state_callback, s);
  pa_stream_set_write_callback(s->stream, stream_write_callback, s);
  pa_stream_set_latency_update_callback(s->stream, stream_latency_update_callback, s);

  if (pa_stream_connect_playback(s->stream, NULL, NULL, 0, NULL, NULL) < 0) {
    fprintf(stderr, "pa_stream_connect_playback() failed: %s\n", pa_strerror(pa_context_errno(s->context)));
    goto unlock_and_fail;
  }

  /* Wait until the stream is ready */
  pa_threaded_mainloop_wait(s->m);

  if (pa_stream_get_state(s->stream) != PA_STREAM_READY) {
    fprintf(stderr, "Failed to connect stream: %s", pa_strerror(pa_context_errno(s->context)));
    goto unlock_and_fail;
  }
  pa_threaded_mainloop_unlock(s->m);

  if (!s->stream)
    return SA_ERROR_NO_DEVICE;
  return SA_SUCCESS;

unlock_and_fail:
  pa_threaded_mainloop_unlock(s->m);
  return SA_ERROR_NO_DEVICE;
}

int
sa_stream_destroy(sa_stream_t *s) {
  if (s == NULL) {
    return SA_SUCCESS;
  }

  pthread_mutex_lock(&s->mutex);
  s->thread_id = 0;
  pthread_mutex_unlock(&s->mutex);

  pa_threaded_mainloop_lock(s->m);
  pa_stream_disconnect(s->stream);
  s->stream = NULL;
  pa_context_disconnect(s->context);
  pa_context_unref(s->context);
  s->context = NULL;
  pa_threaded_mainloop_unlock(s->m);

  pa_threaded_mainloop_stop(s->m);
  pa_threaded_mainloop_free(s->m);

  pthread_mutex_destroy(&s->mutex);

  while (s->bl_head != NULL) {
    sa_buf  * next = s->bl_head->next;
    free(s->bl_head);
    s->bl_head = next;
  }
  free(s);

  return SA_SUCCESS;
}



/*
 * -----------------------------------------------------------------------------
 * Data read and write functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_write(sa_stream_t *s, const void *data, size_t nbytes) {
  int result = SA_SUCCESS;

  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (nbytes == 0) {
    return SA_SUCCESS;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * Append the new data to the end of our buffer list.
   */
  while (1) {
    unsigned int avail = s->bl_tail->size - s->bl_tail->end;

    if (nbytes <= avail) {

      /*
       * The new data will fit into the current tail buffer, so
       * just copy it in and we're done.
       */
      memcpy(s->bl_tail->data + s->bl_tail->end, data, nbytes);
      s->bl_tail->end += nbytes;
      break;

    } else {

      /*
       * Copy what we can into the tail and allocate a new buffer
       * for the rest.
       */
      memcpy(s->bl_tail->data + s->bl_tail->end, data, avail);
      s->bl_tail->end += avail;
      data = ((unsigned char *)data) + avail;
      nbytes -= avail;

      /* 
       * If we still have data left to copy but we've hit the limit of
       * allowable buffer allocations, we need to spin for a bit to allow
       * the audio callback function to slurp some more data up.
       */
      if (nbytes > 0 && s->n_bufs == BUF_LIMIT) {
        if (!s->playing) {
          /*
           * We haven't even started playing yet! That means the
           * BUF_SIZE/BUF_LIMIT values are too low... Not much we can
           * do here; spinning won't help because the audio callback
           * hasn't been enabled yet. Oh well, error time.
           */
          printf("Too much audio data received before audio device enabled!\n");
          result = SA_ERROR_SYSTEM;
          break;
        }
        while (s->n_bufs == BUF_LIMIT) {
          struct timespec ts = {0, 1000000};
          pthread_mutex_unlock(&s->mutex);
          nanosleep(&ts, NULL);
          pthread_mutex_lock(&s->mutex);
        }
      }

      /* 
       * Allocate a new tail buffer, and go 'round again to fill it up.
       */
      if ((s->bl_tail->next = new_buffer()) == NULL) {
        result = SA_ERROR_OOM;
        break;
      }
      s->n_bufs++;
      s->bl_tail = s->bl_tail->next;
    
    } /* if (nbytes <= avail), else */

  } /* while (1) */
  pthread_mutex_unlock(&s->mutex);
  /*
   * Once we have our first block of audio data, enable the audio callback
   * function. This doesn't need to be protected by the mutex, because
   * s->playing is not used in the audio callback thread, and it's probably
   * better not to be inside the lock when we enable the audio callback.
   */
  if (!s->playing) {
    s->playing = 1;
    if (pthread_create(&s->thread_id, NULL, (void *)audio_callback, s) != 0) {
      result = SA_ERROR_SYSTEM;
    }
  }
  return result;
}

static void audio_callback(void* data)
{
  sa_stream_t* s = (sa_stream_t*)data;
  unsigned int bytes_per_frame = s->sample_spec.channels * pa_sample_size(&s->sample_spec);
  size_t buffer_size = s->sample_spec.rate * bytes_per_frame;
  char* buffer = malloc(buffer_size);

  while(1) {
    char* dst = buffer;
    size_t bytes_to_copy, bytes;

    pa_threaded_mainloop_lock(s->m);
    while(1) {
      if (s == NULL || s->stream == NULL) {
        if (s != NULL && s->m != NULL) 
          pa_threaded_mainloop_unlock(s->m);
        goto free_buffer;
      }
      if ((bytes_to_copy = pa_stream_writable_size(s->stream)) == (size_t) -1) {
        fprintf(stderr, "pa_stream_writable_size() failed: %s", pa_strerror(pa_context_errno(s->context)));
        pa_threaded_mainloop_unlock(s->m);
        goto free_buffer;
      }
      if(bytes_to_copy > 0)
        break;
      pa_threaded_mainloop_wait(s->m);
    }
    pa_threaded_mainloop_unlock(s->m);
    if (bytes_to_copy > buffer_size)
      bytes_to_copy = buffer_size;
    bytes = bytes_to_copy;

    pthread_mutex_lock(&s->mutex);
    if (!s->thread_id) {
      pthread_mutex_unlock(&s->mutex);
      break;
    }
    /*
     * Consume data from the start of the buffer list.
     */
    while (1) {
      unsigned int avail = s->bl_head->end - s->bl_head->start;
      assert(s->bl_head->start <= s->bl_head->end);

      if (avail >= bytes_to_copy) {
        /*
         * We have all we need in the head buffer, so just grab it and go.
         */
        memcpy(dst, s->bl_head->data + s->bl_head->start, bytes_to_copy);
        s->bl_head->start += bytes_to_copy;
        break;
    
      } else {
        sa_buf* next = 0;
        /*
         * Copy what we can from the head and move on to the next buffer.
         */
        memcpy(dst, s->bl_head->data + s->bl_head->start, avail);
        s->bl_head->start += avail;
        dst += avail;
        bytes_to_copy -= avail;
        /*
         * We want to free the now-empty buffer, but not if it's also the
         * current tail. If it is the tail, we don't have enough data to fill
         * the destination buffer, so we write less and give up.
         */
        next = s->bl_head->next;
        if (next == NULL) {
          bytes = bytes-bytes_to_copy;
          break;
        }
        free(s->bl_head);
        s->bl_head = next;
        s->n_bufs--;
      } /* if (avail >= bytes_to_copy), else */
    } /* while (1) */

    if(bytes > 0) {
      pa_threaded_mainloop_lock(s->m);
      if (pa_stream_write(s->stream, buffer, bytes, NULL, 0, PA_SEEK_RELATIVE) < 0) {
        fprintf(stderr, "pa_stream_write() failed: %s", pa_strerror(pa_context_errno(s->context)));
        pa_threaded_mainloop_unlock(s->m);
        return;
      }
      pa_stream_update_timing_info(s->stream, NULL, NULL);
      s->bytes_written += bytes;
      pa_threaded_mainloop_unlock(s->m);
    }
    pthread_mutex_unlock(&s->mutex);
  }
free_buffer:
  free(buffer);
}


/*
 * -----------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */


int
sa_stream_get_write_size(sa_stream_t *s, size_t *size) {
  sa_buf  * b;
  size_t    used = 0;

  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }

  pthread_mutex_lock(&s->mutex);

  /*
   * Sum up the used portions of our buffers and subtract that from
   * the pre-defined max allowed allocation.
   */
  for (b = s->bl_head; b != NULL; b = b->next) {
    used += b->end - b->start;
  }
  *size = BUF_SIZE * BUF_LIMIT - used;

  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}


int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) {
  pa_usec_t usec;
  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (position != SA_POSITION_WRITE_SOFTWARE) {
    return SA_ERROR_NOT_SUPPORTED;
  }
  pa_threaded_mainloop_lock(s->m);
  if(pa_stream_get_time(s->stream,  &usec) != PA_ERR_NODATA) {
    *pos = pa_usec_to_bytes(usec, &s->sample_spec);
  }
  else {
    *pos  = s->bytes_written;
  }
  pa_threaded_mainloop_unlock(s->m);
  return SA_SUCCESS;
}


int
sa_stream_pause(sa_stream_t *s) {
  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }
  return SA_SUCCESS;
}


int
sa_stream_resume(sa_stream_t *s) {
  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }

  pa_threaded_mainloop_lock(s->m);
  s->bytes_written = 0;
  pa_threaded_mainloop_unlock(s->m);
  return SA_SUCCESS;
}


static sa_buf *
new_buffer(void) {
  sa_buf  * b = malloc(sizeof(sa_buf) + BUF_SIZE);
  if (b != NULL) {
    b->size  = BUF_SIZE;
    b->start = 0;
    b->end   = 0;
    b->next  = NULL;
  }
  return b;
}



/*
 * -----------------------------------------------------------------------------
 * Extension functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_set_volume_abs(sa_stream_t *s, float vol) {
  pa_cvolume cv;

  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }
  pa_cvolume_set(&cv, s->sample_spec.channels, pa_sw_volume_from_dB(vol));

  return SA_SUCCESS;
}


int
sa_stream_get_volume_abs(sa_stream_t *s, float *vol) {
  if (s == NULL || s->stream == NULL) {
    return SA_ERROR_NO_INIT;
  }
  printf("sa_stream_get_volume_abs not implemented\n");
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
UNSUPPORTED(int sa_stream_drain(sa_stream_t *s))
UNSUPPORTED(int sa_stream_get_min_write(sa_stream_t *s, size_t *size))

const char *sa_strerror(int code) { return NULL; }

