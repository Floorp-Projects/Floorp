/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#define _BSD_SOURCE
#define _POSIX_SOURCE
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include "cubeb/cubeb.h"

#define CUBEB_STREAM_MAX 16
#define CUBEB_WATCHDOG_MS 10000
#define UNUSED __attribute__ ((__unused__))

#define ALSA_PA_PLUGIN "ALSA <-> PulseAudio PCM I/O Plugin"

/* ALSA is not thread-safe.  snd_pcm_t instances are individually protected
   by the owning cubeb_stream's mutex.  snd_pcm_t creation and destruction
   is not thread-safe until ALSA 1.0.24 (see alsa-lib.git commit 91c9c8f1),
   so those calls must be wrapped in the following mutex. */
static pthread_mutex_t cubeb_alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
static int cubeb_alsa_error_handler_set = 0;

struct cubeb {
  pthread_t thread;

  /* Mutex for streams array, must not be held while blocked in poll(2). */
  pthread_mutex_t mutex;

  /* Sparse array of streams managed by this context. */
  cubeb_stream * streams[CUBEB_STREAM_MAX];

  /* fds and nfds are only updated by cubeb_run when rebuild is set. */
  struct pollfd * fds;
  nfds_t nfds;
  int rebuild;

  int shutdown;

  /* Control pipe for forcing poll to wake and rebuild fds or recalculate the timeout. */
  int control_fd_read;
  int control_fd_write;

  /* Track number of active streams.  This is limited to CUBEB_STREAM_MAX
     due to resource contraints. */
  unsigned int active_streams;
};

enum stream_state {
  INACTIVE,
  RUNNING,
  DRAINING,
  PROCESSING,
  ERROR
};

struct cubeb_stream {
  cubeb * context;
  pthread_mutex_t mutex;
  snd_pcm_t * pcm;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
  snd_pcm_uframes_t write_position;
  snd_pcm_uframes_t last_position;
  snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;
  cubeb_stream_params params;

  /* Every member after this comment is protected by the owning context's
     mutex rather than the stream's mutex, or is only used on the context's
     run thread. */
  pthread_cond_t cond; /* Signaled when the stream's state is changed. */

  enum stream_state state;

  struct pollfd * saved_fds; /* A copy of the pollfds passed in at init time. */
  struct pollfd * fds; /* Pointer to this waitable's pollfds within struct cubeb's fds. */
  nfds_t nfds;

  struct timeval drain_timeout;

  /* XXX: Horrible hack -- if an active stream has been idle for
     CUBEB_WATCHDOG_MS it will be disabled and the error callback will be
     called.  This works around a bug seen with older versions of ALSA and
     PulseAudio where streams would stop requesting new data despite still
     being logically active and playing. */
  struct timeval last_activity;
};

static int
any_revents(struct pollfd * fds, nfds_t nfds)
{
  nfds_t i;

  for (i = 0; i < nfds; ++i) {
    if (fds[i].revents) {
      return 1;
    }
  }

  return 0;
}

static int
cmp_timeval(struct timeval * a, struct timeval * b)
{
  if (a->tv_sec == b->tv_sec) {
    if (a->tv_usec == b->tv_usec) {
      return 0;
    }
    return a->tv_usec > b->tv_usec ? 1 : -1;
  }
  return a->tv_sec > b->tv_sec ? 1 : -1;
}

static int
timeval_to_relative_ms(struct timeval * tv)
{
  struct timeval now;
  struct timeval dt;
  long long t;
  int r;

  gettimeofday(&now, NULL);
  r = cmp_timeval(tv, &now);
  if (r >= 0) {
    timersub(tv, &now, &dt);
  } else {
    timersub(&now, tv, &dt);
  }
  t = dt.tv_sec;
  t *= 1000;
  t += (dt.tv_usec + 500) / 1000;

  if (t > INT_MAX) {
    t = INT_MAX;
  } else if (t < INT_MIN) {
    t = INT_MIN;
  }

  return r >= 0 ? t : -t;
}

static int
ms_until(struct timeval * tv)
{
  return timeval_to_relative_ms(tv);
}

static int
ms_since(struct timeval * tv)
{
  return -timeval_to_relative_ms(tv);
}

static void
rebuild(cubeb * ctx)
{
  nfds_t nfds;
  int i;
  nfds_t j;
  cubeb_stream * stm;

  assert(ctx->rebuild);

  /* Always count context's control pipe fd. */
  nfds = 1;
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    stm = ctx->streams[i];
    if (stm) {
      stm->fds = NULL;
      if (stm->state == RUNNING) {
        nfds += stm->nfds;
      }
    }
  }

  free(ctx->fds);
  ctx->fds = calloc(nfds, sizeof(struct pollfd));
  assert(ctx->fds);
  ctx->nfds = nfds;

  /* Include context's control pipe fd. */
  ctx->fds[0].fd = ctx->control_fd_read;
  ctx->fds[0].events = POLLIN | POLLERR;

  for (i = 0, j = 1; i < CUBEB_STREAM_MAX; ++i) {
    stm = ctx->streams[i];
    if (stm && stm->state == RUNNING) {
      memcpy(&ctx->fds[j], stm->saved_fds, stm->nfds * sizeof(struct pollfd));
      stm->fds = &ctx->fds[j];
      j += stm->nfds;
    }
  }

  ctx->rebuild = 0;
}

static void
poll_wake(cubeb * ctx)
{
  write(ctx->control_fd_write, "x", 1);
}

static void
set_timeout(struct timeval * timeout, unsigned int ms)
{
  gettimeofday(timeout, NULL);
  timeout->tv_sec += ms / 1000;
  timeout->tv_usec += (ms % 1000) * 1000;
}

static void
cubeb_set_stream_state(cubeb_stream * stm, enum stream_state state)
{
  cubeb * ctx;
  int r;

  ctx = stm->context;
  stm->state = state;
  r = pthread_cond_broadcast(&stm->cond);
  assert(r == 0);
  ctx->rebuild = 1;
  poll_wake(ctx);
}

static enum stream_state
cubeb_refill_stream(cubeb_stream * stm)
{
  int r;
  unsigned short revents;
  snd_pcm_sframes_t avail;
  long got;
  void * p;
  int draining;

  draining = 0;

  pthread_mutex_lock(&stm->mutex);

  r = snd_pcm_poll_descriptors_revents(stm->pcm, stm->fds, stm->nfds, &revents);
  if (r < 0 || revents != POLLOUT) {
    /* This should be a stream error; it makes no sense for poll(2) to wake
       for this stream and then have the stream report that it's not ready.
       Unfortunately, this does happen, so just bail out and try again. */
    pthread_mutex_unlock(&stm->mutex);
    return RUNNING;
  }

  avail = snd_pcm_avail_update(stm->pcm);
  if (avail == -EPIPE) {
    snd_pcm_recover(stm->pcm, avail, 1);
    avail = snd_pcm_avail_update(stm->pcm);
  }

  /* Failed to recover from an xrun, this stream must be broken. */
  if (avail < 0) {
    pthread_mutex_unlock(&stm->mutex);
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return ERROR;
  }

  /* This should never happen. */
  if ((unsigned int) avail > stm->buffer_size) {
    avail = stm->buffer_size;
  }

  /* poll(2) claims this stream is active, so there should be some space
     available to write.  If avail is still zero here, the stream must be in
     a funky state, so recover and try again. */
  if (avail == 0) {
    snd_pcm_recover(stm->pcm, -EPIPE, 1);
    avail = snd_pcm_avail_update(stm->pcm);
    if (avail <= 0) {
      pthread_mutex_unlock(&stm->mutex);
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
      return ERROR;
    }
  }

  p = calloc(1, snd_pcm_frames_to_bytes(stm->pcm, avail));
  assert(p);

  pthread_mutex_unlock(&stm->mutex);
  got = stm->data_callback(stm, stm->user_ptr, p, avail);
  pthread_mutex_lock(&stm->mutex);
  if (got < 0) {
    pthread_mutex_unlock(&stm->mutex);
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return ERROR;
  }
  if (got > 0) {
    snd_pcm_sframes_t wrote = snd_pcm_writei(stm->pcm, p, got);
    if (wrote == -EPIPE) {
      snd_pcm_recover(stm->pcm, wrote, 1);
      wrote = snd_pcm_writei(stm->pcm, p, got);
    }
    assert(wrote >= 0 && wrote == got);
    stm->write_position += wrote;
    gettimeofday(&stm->last_activity, NULL);
  }
  if (got != avail) {
    long buffer_fill = stm->buffer_size - (avail - got);
    double buffer_time = (double) buffer_fill / stm->params.rate;

    /* Fill the remaining buffer with silence to guarantee one full period
       has been written. */
    snd_pcm_writei(stm->pcm, (char *) p + got, avail - got);

    set_timeout(&stm->drain_timeout, buffer_time * 1000);

    draining = 1;
  }

  free(p);
  pthread_mutex_unlock(&stm->mutex);
  return draining ? DRAINING : RUNNING;
}

static int
cubeb_run(cubeb * ctx)
{
  int r;
  int timeout;
  int i;
  char dummy;
  cubeb_stream * stm;
  enum stream_state state;

  pthread_mutex_lock(&ctx->mutex);

  if (ctx->rebuild) {
    rebuild(ctx);
  }

  /* Wake up at least once per second for the watchdog. */
  timeout = 1000;
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    stm = ctx->streams[i];
    if (stm && stm->state == DRAINING) {
      r = ms_until(&stm->drain_timeout);
      if (r >= 0 && timeout > r) {
        timeout = r;
      }
    }
  }

  pthread_mutex_unlock(&ctx->mutex);
  r = poll(ctx->fds, ctx->nfds, timeout);
  pthread_mutex_lock(&ctx->mutex);

  if (r > 0) {
    if (ctx->fds[0].revents & POLLIN) {
      read(ctx->control_fd_read, &dummy, 1);

      if (ctx->shutdown) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
      }
    }

    for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
      stm = ctx->streams[i];
      if (stm && stm->state == RUNNING && stm->fds && any_revents(stm->fds, stm->nfds)) {
        cubeb_set_stream_state(stm, PROCESSING);
        pthread_mutex_unlock(&ctx->mutex);
        state = cubeb_refill_stream(stm);
        pthread_mutex_lock(&ctx->mutex);
        cubeb_set_stream_state(stm, state);
      }
    }
  } else if (r == 0) {
    for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
      stm = ctx->streams[i];
      if (stm) {
        if (stm->state == DRAINING && ms_since(&stm->drain_timeout) >= 0) {
          cubeb_set_stream_state(stm, INACTIVE);
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
        } else if (stm->state == RUNNING && ms_since(&stm->last_activity) > CUBEB_WATCHDOG_MS) {
          cubeb_set_stream_state(stm, ERROR);
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
        }
      }
    }
  }

  pthread_mutex_unlock(&ctx->mutex);

  return 0;
}

static void *
cubeb_run_thread(void * context)
{
  cubeb * ctx = context;
  int r;

  do {
    r = cubeb_run(ctx);
  } while (r >= 0);

  return NULL;
}

static int
cubeb_locked_pcm_open(snd_pcm_t ** pcm, snd_pcm_stream_t stream)
{
  int r;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  r = snd_pcm_open(pcm, "default", stream, SND_PCM_NONBLOCK);
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  return r;
}

static int
cubeb_locked_pcm_close(snd_pcm_t * pcm)
{
  int r;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  r = snd_pcm_close(pcm);
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  return r;
}

static int
cubeb_register_stream(cubeb * ctx, cubeb_stream * stm)
{
  int i;

  pthread_mutex_lock(&ctx->mutex);
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    if (!ctx->streams[i]) {
      ctx->streams[i] = stm;
      break;
    }
  }
  pthread_mutex_unlock(&ctx->mutex);

  return i == CUBEB_STREAM_MAX;
}

static void
cubeb_unregister_stream(cubeb_stream * stm)
{
  cubeb * ctx;
  int i;

  ctx = stm->context;

  pthread_mutex_lock(&ctx->mutex);
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    if (ctx->streams[i] == stm) {
      ctx->streams[i] = NULL;
      break;
    }
  }
  pthread_mutex_unlock(&ctx->mutex);
}

static void
silent_error_handler(char const * file UNUSED, int line UNUSED, char const * function UNUSED,
                     int err UNUSED, char const * fmt UNUSED, ...)
{
}

static int
pcm_uses_pulseaudio_plugin(snd_pcm_t * pcm)
{
  snd_output_t * out;
  char * buf;
  size_t bufsz;
  int r;

  snd_output_buffer_open(&out);
  snd_pcm_dump(pcm, out);
  bufsz = snd_output_buffer_string(out, &buf);
  r = bufsz >= strlen(ALSA_PA_PLUGIN) &&
      strncmp(buf, ALSA_PA_PLUGIN, strlen(ALSA_PA_PLUGIN)) == 0;
  snd_output_close(out);

  return r;
}

int
cubeb_init(cubeb ** context, char const * context_name UNUSED)
{
  cubeb * ctx;
  int r;
  int i;
  int fd[2];
  pthread_attr_t attr;

  assert(context);
  *context = NULL;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  if (!cubeb_alsa_error_handler_set) {
    snd_lib_error_set_handler(silent_error_handler);
    cubeb_alsa_error_handler_set = 1;
  }
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  r = pthread_mutex_init(&ctx->mutex, NULL);
  assert(r == 0);

  r = pipe(fd);
  assert(r == 0);

  for (i = 0; i < 2; ++i) {
    fcntl(fd[i], F_SETFD, fcntl(fd[i], F_GETFD) | FD_CLOEXEC);
    fcntl(fd[i], F_SETFL, fcntl(fd[i], F_GETFL) | O_NONBLOCK);
  }

  ctx->control_fd_read = fd[0];
  ctx->control_fd_write = fd[1];

  /* Force an early rebuild when cubeb_run is first called to ensure fds and
     nfds have been initialized. */
  ctx->rebuild = 1;

  r = pthread_attr_init(&attr);
  assert(r == 0);

  r = pthread_attr_setstacksize(&attr, 256 * 1024);
  assert(r == 0);

  r = pthread_create(&ctx->thread, &attr, cubeb_run_thread, ctx);
  assert(r == 0);

  r = pthread_attr_destroy(&attr);
  assert(r == 0);

  *context = ctx;

  return CUBEB_OK;
}

char const *
cubeb_get_backend_id(cubeb * ctx UNUSED)
{
  return "alsa";
}

void
cubeb_destroy(cubeb * ctx)
{
  int r;

  assert(ctx);

  pthread_mutex_lock(&ctx->mutex);
  ctx->shutdown = 1;
  poll_wake(ctx);
  pthread_mutex_unlock(&ctx->mutex);

  r = pthread_join(ctx->thread, NULL);
  assert(r == 0);

  close(ctx->control_fd_read);
  close(ctx->control_fd_write);
  pthread_mutex_destroy(&ctx->mutex);
  free(ctx->fds);

  free(ctx);
}

int
cubeb_stream_init(cubeb * ctx, cubeb_stream ** stream, char const * stream_name UNUSED,
                  cubeb_stream_params stream_params, unsigned int latency,
                  cubeb_data_callback data_callback, cubeb_state_callback state_callback,
                  void * user_ptr)
{
  cubeb_stream * stm;
  int r;
  snd_pcm_format_t format;

  assert(ctx && stream);

  *stream = NULL;

  if (stream_params.rate < 1 || stream_params.rate > 192000 ||
      stream_params.channels < 1 || stream_params.channels > 32 ||
      latency < 1 || latency > 2000) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    format = SND_PCM_FORMAT_S16_LE;
    break;
  case CUBEB_SAMPLE_S16BE:
    format = SND_PCM_FORMAT_S16_BE;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    format = SND_PCM_FORMAT_FLOAT_LE;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    format = SND_PCM_FORMAT_FLOAT_BE;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  pthread_mutex_lock(&ctx->mutex);
  if (ctx->active_streams >= CUBEB_STREAM_MAX) {
    pthread_mutex_unlock(&ctx->mutex);
    return CUBEB_ERROR;
  }
  ctx->active_streams += 1;
  pthread_mutex_unlock(&ctx->mutex);

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = ctx;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->params = stream_params;
  stm->state = INACTIVE;

  r = pthread_mutex_init(&stm->mutex, NULL);
  assert(r == 0);

  r = cubeb_locked_pcm_open(&stm->pcm, SND_PCM_STREAM_PLAYBACK);
  if (r < 0) {
    cubeb_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  r = snd_pcm_nonblock(stm->pcm, 1);
  assert(r == 0);

  /* Ugly hack: the PA ALSA plugin allows buffer configurations that can't
     possibly work.  See https://bugzilla.mozilla.org/show_bug.cgi?id=761274 */
  if (pcm_uses_pulseaudio_plugin(stm->pcm)) {
    latency = latency < 200 ? 200 : latency;
  }

  r = snd_pcm_set_params(stm->pcm, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                         stm->params.channels, stm->params.rate, 1,
                         latency * 1000);
  if (r < 0) {
    cubeb_stream_destroy(stm);
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  r = snd_pcm_get_params(stm->pcm, &stm->buffer_size, &stm->period_size);
  assert(r == 0);

  stm->nfds = snd_pcm_poll_descriptors_count(stm->pcm);
  assert(stm->nfds > 0);

  stm->saved_fds = calloc(stm->nfds, sizeof(struct pollfd));
  assert(stm->saved_fds);
  r = snd_pcm_poll_descriptors(stm->pcm, stm->saved_fds, stm->nfds);
  assert((nfds_t) r == stm->nfds);

  r = pthread_cond_init(&stm->cond, NULL);
  assert(r == 0);

  if (cubeb_register_stream(ctx, stm) != 0) {
    cubeb_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

void
cubeb_stream_destroy(cubeb_stream * stm)
{
  int r;
  cubeb * ctx;

  assert(stm && (stm->state == INACTIVE || stm->state == ERROR));

  ctx = stm->context;

  pthread_mutex_lock(&stm->mutex);
  if (stm->pcm) {
    cubeb_locked_pcm_close(stm->pcm);
    stm->pcm = NULL;
  }
  free(stm->saved_fds);
  pthread_mutex_unlock(&stm->mutex);
  pthread_mutex_destroy(&stm->mutex);

  r = pthread_cond_destroy(&stm->cond);
  assert(r == 0);

  cubeb_unregister_stream(stm);

  pthread_mutex_lock(&ctx->mutex);
  assert(ctx->active_streams >= 1);
  ctx->active_streams -= 1;
  pthread_mutex_unlock(&ctx->mutex);

  free(stm);
}

int
cubeb_stream_start(cubeb_stream * stm)
{
  cubeb * ctx;

  assert(stm);
  ctx = stm->context;

  pthread_mutex_lock(&stm->mutex);
  snd_pcm_pause(stm->pcm, 0);
  pthread_mutex_unlock(&stm->mutex);

  pthread_mutex_lock(&ctx->mutex);
  cubeb_set_stream_state(stm, RUNNING);
  pthread_mutex_unlock(&ctx->mutex);

  return CUBEB_OK;
}

int
cubeb_stream_stop(cubeb_stream * stm)
{
  cubeb * ctx;
  int r;

  assert(stm);
  ctx = stm->context;

  pthread_mutex_lock(&ctx->mutex);
  while (stm->state == PROCESSING) {
    r = pthread_cond_wait(&stm->cond, &ctx->mutex);
    assert(r == 0);
  }

  cubeb_set_stream_state(stm, INACTIVE);
  pthread_mutex_unlock(&ctx->mutex);

  pthread_mutex_lock(&stm->mutex);
  snd_pcm_pause(stm->pcm, 1);
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
}

int
cubeb_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  snd_pcm_sframes_t delay;

  assert(stm && position);

  pthread_mutex_lock(&stm->mutex);

  delay = -1;
  if (snd_pcm_state(stm->pcm) != SND_PCM_STATE_RUNNING ||
      snd_pcm_delay(stm->pcm, &delay) != 0) {
    *position = stm->last_position;
    pthread_mutex_unlock(&stm->mutex);
    return CUBEB_OK;
  }

  assert(delay >= 0);

  *position = 0;
  if (stm->write_position >= (snd_pcm_uframes_t) delay) {
    *position = stm->write_position - delay;
  }

  stm->last_position = *position;

  pthread_mutex_unlock(&stm->mutex);
  return CUBEB_OK;
}
