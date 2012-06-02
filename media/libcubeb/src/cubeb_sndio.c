/*
 * Copyright (c) 2011 Alexandre Ratchov <alex@caoua.org>
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include <poll.h>
#include <pthread.h>
#include <sndio.h>
#include <stdlib.h>
#include <stdio.h>
#include "cubeb/cubeb.h"

#ifdef CUBEB_SNDIO_DEBUG
#define DPR(...) fprintf(stderr, __VA_ARGS__);
#else
#define DPR(...) do {} while(0)
#endif

struct cubeb_stream {
  pthread_t th;			  /* to run real-time audio i/o */
  pthread_mutex_t mtx;		  /* protects hdl and pos */
  struct sio_hdl *hdl;		  /* link us to sndio */
  int active;			  /* cubec_start() called */
  int conv;			  /* need float->s16 conversion */
  unsigned char *buf;		  /* data is prepared here */
  unsigned int nfr;		  /* number of frames in buf */
  unsigned int bpf;		  /* bytes per frame */
  unsigned int pchan;		  /* number of play channels */
  uint64_t rdpos;		  /* frame number Joe hears right now */
  uint64_t wrpos;		  /* number of written frames */
  cubeb_data_callback data_cb;    /* cb to preapare data */
  cubeb_state_callback state_cb;  /* cb to notify about state changes */
  void *arg;			  /* user arg to {data,state}_cb */
};

void
float_to_s16(void *ptr, long nsamp)
{
  int16_t *dst = ptr;
  float *src = ptr;

  while (nsamp-- > 0)
    *(dst++) = *(src++) * 32767;
}

void
cubeb_onmove(void *arg, int delta)
{
  struct cubeb_stream *s = (struct cubeb_stream *)arg;

  s->rdpos += delta;
}

void *
cubeb_mainloop(void *arg)
{
#define MAXFDS 8
  struct pollfd pfds[MAXFDS];
  struct cubeb_stream *s = arg;
  int n, nfds, revents, state;
  size_t start = 0, end = 0;
  long nfr;

  DPR("cubeb_mainloop()\n");
  s->state_cb(s, s->arg, CUBEB_STATE_STARTED);
  pthread_mutex_lock(&s->mtx);
  if (!sio_start(s->hdl)) {
    pthread_mutex_unlock(&s->mtx);
    return NULL;
  }
  DPR("cubeb_mainloop(), started\n");

  start = end = s->nfr;
  for (;;) {
    if (!s->active) {
      DPR("cubeb_mainloop() stopped\n");
      state = CUBEB_STATE_STOPPED;
      break;
    }
    if (start == end) {
      if (end < s->nfr) {
        DPR("cubeb_mainloop() drained\n");
        state = CUBEB_STATE_DRAINED;
        break;
      }
      pthread_mutex_unlock(&s->mtx);
      nfr = s->data_cb(s, s->arg, s->buf, s->nfr);
      pthread_mutex_lock(&s->mtx);
      if (nfr < 0) {
        DPR("cubeb_mainloop() cb err\n");
        state = CUBEB_STATE_ERROR;
        break;
      }
      if (s->conv)
        float_to_s16(s->buf, nfr * s->pchan);
      start = 0;
      end = nfr * s->bpf;
    }
    if (end == 0)
      continue;
    nfds = sio_pollfd(s->hdl, pfds, POLLOUT);    
    if (nfds > 0) {
      pthread_mutex_unlock(&s->mtx);
      n = poll(pfds, nfds, -1);
      pthread_mutex_lock(&s->mtx);
      if (n < 0)
        continue;
    }
    revents = sio_revents(s->hdl, pfds);
    if (revents & POLLHUP)
      break;
    if (revents & POLLOUT) {
      n = sio_write(s->hdl, s->buf + start, end - start);
      if (n == 0) {
        DPR("cubeb_mainloop() werr\n");
        state = CUBEB_STATE_ERROR;
        break;
      }
      s->wrpos = 0;
      start += n;
    }
  }
  sio_stop(s->hdl);
  s->rdpos = s->wrpos;
  pthread_mutex_unlock(&s->mtx);
  s->state_cb(s, s->arg, state);
  return NULL;
}

int
cubeb_init(cubeb **context, char const *context_name)
{
  DPR("cubeb_init(%s)\n", context_name);
  *context = (void *)0xdeadbeef;
  (void)context_name;
  return CUBEB_OK;
}

void
cubeb_destroy(cubeb *context)
{
  DPR("cubeb_destroy()\n");
  (void)context;
}

int
cubeb_stream_init(cubeb *context,
    cubeb_stream **stream,
    char const *stream_name,
    cubeb_stream_params stream_params, unsigned int latency,
    cubeb_data_callback data_callback,
    cubeb_state_callback state_callback,
    void *user_ptr)
{
  struct cubeb_stream *s;
  struct sio_par wpar, rpar;
  DPR("cubeb_stream_init(%s)\n", stream_name);
  size_t size;

  s = malloc(sizeof(struct cubeb_stream));
  if (s == NULL)
    return CUBEB_ERROR;
  s->hdl = sio_open(NULL, SIO_PLAY, 0);
  if (s->hdl == NULL) {
    free(s);
    DPR("cubeb_stream_init(), sio_open() failed\n");
    return CUBEB_ERROR;
  }
  sio_initpar(&wpar);
  wpar.sig = 1;
  wpar.bits = 16;
  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    wpar.le = 1;
    break;
  case CUBEB_SAMPLE_S16BE:
    wpar.le = 0;
    break;
  case CUBEB_SAMPLE_FLOAT32NE:
    wpar.le = SIO_LE_NATIVE;
    break;
  default:
    DPR("cubeb_stream_init() unsupported format\n");
    return CUBEB_ERROR_INVALID_FORMAT;
  }
  wpar.rate = stream_params.rate;
  wpar.pchan = stream_params.channels;
  wpar.appbufsz = latency * wpar.rate / 1000;
  if (!sio_setpar(s->hdl, &wpar) || !sio_getpar(s->hdl, &rpar)) {
    sio_close(s->hdl);
    free(s);
    DPR("cubeb_stream_init(), sio_setpar() failed\n");
    return CUBEB_ERROR;
  }
  if (rpar.bits != wpar.bits || rpar.le != wpar.le ||
      rpar.sig != wpar.sig || rpar.rate != wpar.rate ||
      rpar.pchan != wpar.pchan) {
    sio_close(s->hdl);
    free(s);
    DPR("cubeb_stream_init() unsupported params\n");
    return CUBEB_ERROR_INVALID_FORMAT;
  }
  sio_onmove(s->hdl, cubeb_onmove, s);
  s->active = 0;
  s->nfr = rpar.round;
  s->bpf = rpar.bps * rpar.pchan;
  s->pchan = rpar.pchan;
  s->data_cb = data_callback;
  s->state_cb = state_callback;
  s->arg = user_ptr;
  s->mtx = PTHREAD_MUTEX_INITIALIZER;
  s->rdpos = s->wrpos = 0;
  if (stream_params.format == CUBEB_SAMPLE_FLOAT32LE) {
    s->conv = 1;
    size = rpar.round * rpar.pchan * sizeof(float);
  } else {
    s->conv = 0;
    size = rpar.round * rpar.pchan * rpar.bps;
  }
  s->buf = malloc(size);
  if (s->buf == NULL) {
    sio_close(s->hdl);
    free(s);
    return CUBEB_ERROR;
  }
  *stream = s;
  DPR("cubeb_stream_init() end, ok\n");
  (void)context;
  (void)stream_name;
  return CUBEB_OK;
}

void
cubeb_stream_destroy(cubeb_stream *s)
{
  DPR("cubeb_stream_destroy()\n");
  sio_close(s->hdl);
  free(s);
}

int
cubeb_stream_start(cubeb_stream *s)
{
  int err;

  DPR("cubeb_stream_start()\n");
  s->active = 1;
  err = pthread_create(&s->th, NULL, cubeb_mainloop, s);
  if (err) {
    s->active = 0;
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

int
cubeb_stream_stop(cubeb_stream *s)
{
  void *dummy;

  DPR("cubeb_stream_stop()\n");
  if (s->active) {
    s->active = 0;
    pthread_join(s->th, &dummy);
  }
  return CUBEB_OK;
}

int
cubeb_stream_get_position(cubeb_stream *s, uint64_t *p)
{
  pthread_mutex_lock(&s->mtx);
  DPR("cubeb_stream_get_position() %lld\n", s->rdpos);
  *p = s->rdpos;
  pthread_mutex_unlock(&s->mtx);
  return CUBEB_OK;
}

int
cubeb_stream_set_volume(cubeb_stream *s, float volume)
{
  DPR("cubeb_stream_set_volume(%f)\n", volume);
  pthread_mutex_lock(&s->mtx);
  sio_setvol(s->hdl, SIO_MAXVOL * volume);
  pthread_mutex_unlock(&s->mtx);
  return CUBEB_OK;
}
