/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>
#include <sys/audio.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include "sydney_audio.h"

#ifndef DEFAULT_AUDIO_DEVICE
#define DEFAULT_AUDIO_DEVICE "/dev/paud0/1"
#endif

#define LOOP_WHILE_EINTR(v,func) do { (v) = (func); } \
                while ((v) == -1 && errno == EINTR);

typedef struct sa_buf sa_buf;
struct sa_buf {
  unsigned int      size; /* the size of data */
  sa_buf            *next;
  unsigned char     data[]; /* sound data */
};

struct sa_stream 
{
  int               audio_fd;
  pthread_mutex_t   mutex;
  pthread_t         thread_id;
  int               playing;
  int64_t           bytes_played;

  /* audio format info */
  /* default setting */
  unsigned int      default_n_channels;
  unsigned int      default_rate;
  unsigned int      default_precision;

  /* used settings */
  unsigned int      rate;
  unsigned int      n_channels;
  unsigned int      precision;

  /* buffer list */
  sa_buf            *bl_head;
  sa_buf            *bl_tail;
};

/* Use a default buffer size with enough room for one second of audio,
 * assuming stereo data at 44.1kHz with 32 bits per channel, and impose
 * a generous limit on the number of buffers.
 */
#define BUF_SIZE    (2 * 44100 * 4)

static void* audio_callback(void* s);

static sa_buf *new_buffer(int size);

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
) 
{
  sa_stream_t   * s = 0;

  /* Make sure we return a NULL stream pointer on failure. */
  if (_s == NULL) 
    return SA_ERROR_INVALID;

  *_s = NULL;

  if (mode != SA_MODE_WRONLY) 
    return SA_ERROR_NOT_SUPPORTED;

  if (format != SA_PCM_FORMAT_S16_LE) 
    return SA_ERROR_NOT_SUPPORTED;

  /*
   * Allocate the instance and required resources.
   */
  if ((s = malloc(sizeof(sa_stream_t))) == NULL) 
    return SA_ERROR_OOM;

  if (pthread_mutex_init(&s->mutex, NULL) != 0) {
    free(s);
    return SA_ERROR_SYSTEM;
  }

  s->audio_fd = NULL;
  s->rate = rate;
  s->n_channels = n_channels;
  s->precision = 16;

  s->playing = 0;
  s->bytes_played = 0;
  s->bl_tail = s->bl_head = NULL;

  *_s = s;

  return SA_SUCCESS;
}


int
sa_stream_open(sa_stream_t *s) 
{
  int fd,err;
  char *device_name;

  audio_init init;
  audio_control control;
  audio_change change;

  device_name = DEFAULT_AUDIO_DEVICE;

  if (s == NULL) 
    return SA_ERROR_NO_INIT;

  if (s->audio_fd != NULL) 
    return SA_ERROR_INVALID;

  fd = open(device_name,O_WRONLY | O_NONBLOCK);
  if (fd >= 0) 
  {
     close (fd);
     fd = open (device_name, O_WRONLY);
  }

  if ( fd < 0 )
  {
    printf("Open %s failed:%s ",device_name,strerror(errno));
    return SA_ERROR_NO_DEVICE;
  }
  
  init.srate = s->rate;
  init.channels = s->n_channels;
  init.mode = AUDIO_PCM;
  init.flags = AUDIO_BIG_ENDIAN | AUDIO_TWOS_COMPLEMENT;
  init.operation = AUDIO_PLAY;

  if (ioctl(s->audio_fd, AUDIO_INIT, &init) < 0) {
      close(s->audio_fd);
      return 0;
  }

  change.balance = 0x3fff0000;
  change.volume = 0x3fff0000;
  change.monitor = AUDIO_IGNORE;
  change.input = AUDIO_IGNORE;
  change.output = AUDIO_OUTPUT_1;

  control.ioctl_request = AUDIO_CHANGE;
  control.position = 0;
  control.request_info = &change;

  if (ioctl(s->audio_fd, AUDIO_CONTROL, &control) < 0) {
      close(s->audio_fd);
      return 0;
  }

  control.ioctl_request = AUDIO_START;
  control.request_info = NULL;

  if (ioctl(s->audio_fd, AUDIO_CONTROL, &control) < 0) {
      close(s->audio_fd);
      return 0;
  }

  return SA_SUCCESS;
}

int
sa_stream_destroy(sa_stream_t *s) 
{
  int result; 

  if (s == NULL) 
    return SA_SUCCESS;


  pthread_mutex_lock(&s->mutex);

  result = SA_SUCCESS;

  /*
   * Shut down the audio output device.
   * and release resources
   */
  if (s->audio_fd != NULL) 
  {
    if (close(s->audio_fd) < 0) 
    {
      perror("Close aix audio fd failed");
      result = SA_ERROR_SYSTEM;
    }
  }

  s->thread_id = 0;

  while (s->bl_head != NULL) {
    sa_buf  * next = s->bl_head->next;
    free(s->bl_head);
    s->bl_head = next;
  }

  pthread_mutex_unlock(&s->mutex);

  if (pthread_mutex_destroy(&s->mutex) != 0) {
    result = SA_ERROR_SYSTEM;
  }

  free(s);

  return result;
}

/*
 * -----------------------------------------------------------------------------
 * Data read and write functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_write(sa_stream_t *s, const void *data, size_t nbytes) 
{

  int result;
  sa_buf *buf;

  if (s == NULL || s->audio_fd == NULL) 
    return SA_ERROR_NO_INIT;

  if (nbytes == 0) 
    return SA_SUCCESS;


 /*
  * Append the new data to the end of our buffer list.
  */
  result = SA_SUCCESS;
  buf = new_buffer(nbytes);

  if (buf == NULL)
    return SA_ERROR_OOM;

  memcpy(buf->data,data, nbytes);

  pthread_mutex_lock(&s->mutex);
  if (!s->bl_head)
    s->bl_head = buf;
  else
    s->bl_tail->next = buf;

  s->bl_tail = buf;

  pthread_mutex_unlock(&s->mutex);

 /*
  * Once we have our first block of audio data, enable the audio callback
  * function. This doesn't need to be protected by the mutex, because
  * s->playing is not used in the audio callback thread, and it's probably
  * better not to be inside the lock when we enable the audio callback.
  */
  if (!s->playing) {
    s->playing = 1;
    if (pthread_create(&s->thread_id, NULL, audio_callback, s) != 0) {
      result = SA_ERROR_SYSTEM;
    }
  } 

  return result;
}

static void* 
audio_callback(void* data)
{
  sa_stream_t* s = (sa_stream_t*)data;
  sa_buf *buf;
  int fd,nbytes_written,bytes,nbytes;

  fd = s->audio_fd;

  while (1)
  { 
    if (s->thread_id == 0)
      break;

    pthread_mutex_lock(&s->mutex);
    while (s->bl_head) 
    {
      buf = s->bl_head;
      s->bl_head = s->bl_head->next;

      nbytes_written = 0; 
      nbytes = buf->size;

      while (nbytes_written < nbytes)
      {
        LOOP_WHILE_EINTR(bytes,(write(fd, (void *)((buf->data)+nbytes_written), nbytes-nbytes_written)));

        nbytes_written += bytes;
        if (nbytes_written != nbytes)
          printf("AixAudio\tWrite completed short - %d vs %d. Write more data\n",nbytes_written,nbytes);
      }

      free(buf);
      s->bytes_played += nbytes;
     }
     pthread_mutex_unlock(&s->mutex);
   }

  return NULL;
}

/*
 * -----------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_write_size(sa_stream_t *s, size_t *size) 
{
  sa_buf  * b;
  size_t    used = 0;

  if (s == NULL ) 
    return SA_ERROR_NO_INIT;

  /* there is no interface to get the avaiable writing buffer size
   * in aix audio, we return max size here to force sa_stream_write() to
   * be called when there is data to be played
   */
  *size = BUF_SIZE; 

  return SA_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * General query and support functions
 * -----------------------------------------------------------------------------
 */

int
sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos) 
{
  if (s == NULL) {
    return SA_ERROR_NO_INIT;
  }
  if (position != SA_POSITION_WRITE_SOFTWARE) {
    return SA_ERROR_NOT_SUPPORTED;
  }

  pthread_mutex_lock(&s->mutex);
  *pos = s->bytes_played;
  pthread_mutex_unlock(&s->mutex);
  return SA_SUCCESS;
}

static sa_buf *
new_buffer(int size) 
{
  sa_buf  * b = malloc(sizeof(sa_buf) + size);
  if (b != NULL) {
    b->size  = size;
    b->next  = NULL;
  }
  return b;
}

/*
 * -----------------------------------------------------------------------------
 * Unsupported functions
 * -----------------------------------------------------------------------------
 */
#define UNSUPPORTED(func)   func { return SA_ERROR_NOT_SUPPORTED; }

UNSUPPORTED(int sa_stream_pause(sa_stream_t *s)) 
UNSUPPORTED(int sa_stream_resume(sa_stream_t *s)) 
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

