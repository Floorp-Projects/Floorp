/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"

#define CUBEB_STREAM_MAX 16
#define CUBEB_WATCHDOG_MS 10000

#define CUBEB_ALSA_PCM_NAME "default"

#define ALSA_PA_PLUGIN "ALSA <-> PulseAudio PCM I/O Plugin"

/* ALSA is not thread-safe.  snd_pcm_t instances are individually protected
   by the owning cubeb_stream's mutex.  snd_pcm_t creation and destruction
   is not thread-safe until ALSA 1.0.24 (see alsa-lib.git commit 91c9c8f1),
   so those calls must be wrapped in the following mutex. */
static pthread_mutex_t cubeb_alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
static int cubeb_alsa_error_handler_set = 0;

static struct cubeb_ops const alsa_ops;

struct cubeb {
  struct cubeb_ops const * ops;

  pthread_t thread;

  /* Mutex for streams array, must not be held while blocked in poll(2). */
  pthread_mutex_t mutex;

  /* Sparse array of streams managed by this context. */
  cubeb_stream * streams[CUBEB_STREAM_MAX];

  /* fds and nfds are only updated by alsa_run when rebuild is set. */
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

  /* Local configuration with handle_underrun workaround set for PulseAudio
     ALSA plugin.  Will be NULL if the PA ALSA plugin is not in use or the
     workaround is not required. */
  snd_config_t * local_config;
  int is_pa;
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
  if (write(ctx->control_fd_write, "x", 1) < 0) {
    /* ignore write error */
  }
}

static void
set_timeout(struct timeval * timeout, unsigned int ms)
{
  gettimeofday(timeout, NULL);
  timeout->tv_sec += ms / 1000;
  timeout->tv_usec += (ms % 1000) * 1000;
}

static void
alsa_set_stream_state(cubeb_stream * stm, enum stream_state state)
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
alsa_refill_stream(cubeb_stream * stm)
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
alsa_run(cubeb * ctx)
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
      if (read(ctx->control_fd_read, &dummy, 1) < 0) {
        /* ignore read error */
      }

      if (ctx->shutdown) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
      }
    }

    for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
      stm = ctx->streams[i];
      if (stm && stm->state == RUNNING && stm->fds && any_revents(stm->fds, stm->nfds)) {
        alsa_set_stream_state(stm, PROCESSING);
        pthread_mutex_unlock(&ctx->mutex);
        state = alsa_refill_stream(stm);
        pthread_mutex_lock(&ctx->mutex);
        alsa_set_stream_state(stm, state);
      }
    }
  } else if (r == 0) {
    for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
      stm = ctx->streams[i];
      if (stm) {
        if (stm->state == DRAINING && ms_since(&stm->drain_timeout) >= 0) {
          alsa_set_stream_state(stm, INACTIVE);
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
        } else if (stm->state == RUNNING && ms_since(&stm->last_activity) > CUBEB_WATCHDOG_MS) {
          alsa_set_stream_state(stm, ERROR);
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
        }
      }
    }
  }

  pthread_mutex_unlock(&ctx->mutex);

  return 0;
}

static void *
alsa_run_thread(void * context)
{
  cubeb * ctx = context;
  int r;

  do {
    r = alsa_run(ctx);
  } while (r >= 0);

  return NULL;
}

static snd_config_t *
get_slave_pcm_node(snd_config_t * lconf, snd_config_t * root_pcm)
{
  int r;
  snd_config_t * slave_pcm;
  snd_config_t * slave_def;
  snd_config_t * pcm;
  char const * string;
  char node_name[64];

  slave_def = NULL;

  r = snd_config_search(root_pcm, "slave", &slave_pcm);
  if (r < 0) {
    return NULL;
  }

  r = snd_config_get_string(slave_pcm, &string);
  if (r >= 0) {
    r = snd_config_search_definition(lconf, "pcm_slave", string, &slave_def);
    if (r < 0) {
      return NULL;
    }
  }

  do {
    r = snd_config_search(slave_def ? slave_def : slave_pcm, "pcm", &pcm);
    if (r < 0) {
      break;
    }

    r = snd_config_get_string(slave_def ? slave_def : slave_pcm, &string);
    if (r < 0) {
      break;
    }

    r = snprintf(node_name, sizeof(node_name), "pcm.%s", string);
    if (r < 0 || r > (int) sizeof(node_name)) {
      break;
    }
    r = snd_config_search(lconf, node_name, &pcm);
    if (r < 0) {
      break;
    }

    return pcm;
  } while (0);

  if (slave_def) {
    snd_config_delete(slave_def);
  }

  return NULL;
}

/* Work around PulseAudio ALSA plugin bug where the PA server forces a
   higher than requested latency, but the plugin does not update its (and
   ALSA's) internal state to reflect that, leading to an immediate underrun
   situation.  Inspired by WINE's make_handle_underrun_config.
   Reference: http://mailman.alsa-project.org/pipermail/alsa-devel/2012-July/05 */
static snd_config_t *
init_local_config_with_workaround(char const * pcm_name)
{
  int r;
  snd_config_t * lconf;
  snd_config_t * pcm_node;
  snd_config_t * node;
  char const * string;
  char node_name[64];

  lconf = NULL;

  if (snd_config == NULL) {
    return NULL;
  }

  r = snd_config_copy(&lconf, snd_config);
  if (r < 0) {
    return NULL;
  }

  do {
    r = snd_config_search_definition(lconf, "pcm", pcm_name, &pcm_node);
    if (r < 0) {
      break;
    }

    r = snd_config_get_id(pcm_node, &string);
    if (r < 0) {
      break;
    }

    r = snprintf(node_name, sizeof(node_name), "pcm.%s", string);
    if (r < 0 || r > (int) sizeof(node_name)) {
      break;
    }
    r = snd_config_search(lconf, node_name, &pcm_node);
    if (r < 0) {
      break;
    }

    /* If this PCM has a slave, walk the slave configurations until we reach the bottom. */
    while ((node = get_slave_pcm_node(lconf, pcm_node)) != NULL) {
      pcm_node = node;
    }

    /* Fetch the PCM node's type, and bail out if it's not the PulseAudio plugin. */
    r = snd_config_search(pcm_node, "type", &node);
    if (r < 0) {
      break;
    }

    r = snd_config_get_string(node, &string);
    if (r < 0) {
      break;
    }

    if (strcmp(string, "pulse") != 0) {
      break;
    }

    /* Don't clobber an explicit existing handle_underrun value, set it only
       if it doesn't already exist. */
    r = snd_config_search(pcm_node, "handle_underrun", &node);
    if (r != -ENOENT) {
      break;
    }

    /* Disable pcm_pulse's asynchronous underrun handling. */
    r = snd_config_imake_integer(&node, "handle_underrun", 0);
    if (r < 0) {
      break;
    }

    r = snd_config_add(pcm_node, node);
    if (r < 0) {
      break;
    }

    return lconf;
  } while (0);

  snd_config_delete(lconf);

  return NULL;
}

static int
alsa_locked_pcm_open(snd_pcm_t ** pcm, snd_pcm_stream_t stream, snd_config_t * local_config)
{
  int r;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  if (local_config) {
    r = snd_pcm_open_lconf(pcm, CUBEB_ALSA_PCM_NAME, stream, SND_PCM_NONBLOCK, local_config);
  } else {
    r = snd_pcm_open(pcm, CUBEB_ALSA_PCM_NAME, stream, SND_PCM_NONBLOCK);
  }
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  return r;
}

static int
alsa_locked_pcm_close(snd_pcm_t * pcm)
{
  int r;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  r = snd_pcm_close(pcm);
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  return r;
}

static int
alsa_register_stream(cubeb * ctx, cubeb_stream * stm)
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
alsa_unregister_stream(cubeb_stream * stm)
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
silent_error_handler(char const * file, int line, char const * function,
                     int err, char const * fmt, ...)
{
}

/*static*/ int
alsa_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;
  int r;
  int i;
  int fd[2];
  pthread_attr_t attr;
  snd_pcm_t * dummy;

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

  ctx->ops = &alsa_ops;

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

  /* Force an early rebuild when alsa_run is first called to ensure fds and
     nfds have been initialized. */
  ctx->rebuild = 1;

  r = pthread_attr_init(&attr);
  assert(r == 0);

  r = pthread_attr_setstacksize(&attr, 256 * 1024);
  assert(r == 0);

  r = pthread_create(&ctx->thread, &attr, alsa_run_thread, ctx);
  assert(r == 0);

  r = pthread_attr_destroy(&attr);
  assert(r == 0);

  /* Open a dummy PCM to force the configuration space to be evaluated so that
     init_local_config_with_workaround can find and modify the default node. */
  r = alsa_locked_pcm_open(&dummy, SND_PCM_STREAM_PLAYBACK, NULL);
  if (r >= 0) {
    alsa_locked_pcm_close(dummy);
  }
  ctx->is_pa = 0;
  pthread_mutex_lock(&cubeb_alsa_mutex);
  ctx->local_config = init_local_config_with_workaround(CUBEB_ALSA_PCM_NAME);
  pthread_mutex_unlock(&cubeb_alsa_mutex);
  if (ctx->local_config) {
    ctx->is_pa = 1;
    r = alsa_locked_pcm_open(&dummy, SND_PCM_STREAM_PLAYBACK, ctx->local_config);
    /* If we got a local_config, we found a PA PCM.  If opening a PCM with that
       config fails with EINVAL, the PA PCM is too old for this workaround. */
    if (r == -EINVAL) {
      pthread_mutex_lock(&cubeb_alsa_mutex);
      snd_config_delete(ctx->local_config);
      pthread_mutex_unlock(&cubeb_alsa_mutex);
      ctx->local_config = NULL;
    } else if (r >= 0) {
      alsa_locked_pcm_close(dummy);
    }
  }

  *context = ctx;

  return CUBEB_OK;
}

static char const *
alsa_get_backend_id(cubeb * ctx)
{
  return "alsa";
}

static void
alsa_destroy(cubeb * ctx)
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

  if (ctx->local_config) {
    pthread_mutex_lock(&cubeb_alsa_mutex);
    snd_config_delete(ctx->local_config);
    pthread_mutex_unlock(&cubeb_alsa_mutex);
  }

  free(ctx);
}

static void alsa_stream_destroy(cubeb_stream * stm);

static int
alsa_stream_init(cubeb * ctx, cubeb_stream ** stream, char const * stream_name,
                 cubeb_stream_params stream_params, unsigned int latency,
                 cubeb_data_callback data_callback, cubeb_state_callback state_callback,
                 void * user_ptr)
{
  cubeb_stream * stm;
  int r;
  snd_pcm_format_t format;

  assert(ctx && stream);

  *stream = NULL;

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

  r = alsa_locked_pcm_open(&stm->pcm, SND_PCM_STREAM_PLAYBACK, ctx->local_config);
  if (r < 0) {
    alsa_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  r = snd_pcm_nonblock(stm->pcm, 1);
  assert(r == 0);

  /* Ugly hack: the PA ALSA plugin allows buffer configurations that can't
     possibly work.  See https://bugzilla.mozilla.org/show_bug.cgi?id=761274.
     Only resort to this hack if the handle_underrun workaround failed. */
  if (!ctx->local_config && ctx->is_pa) {
    latency = latency < 500 ? 500 : latency;
  }

  r = snd_pcm_set_params(stm->pcm, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                         stm->params.channels, stm->params.rate, 1,
                         latency * 1000);
  if (r < 0) {
    alsa_stream_destroy(stm);
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

  if (alsa_register_stream(ctx, stm) != 0) {
    alsa_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
alsa_stream_destroy(cubeb_stream * stm)
{
  int r;
  cubeb * ctx;

  assert(stm && (stm->state == INACTIVE || stm->state == ERROR));

  ctx = stm->context;

  pthread_mutex_lock(&stm->mutex);
  if (stm->pcm) {
    alsa_locked_pcm_close(stm->pcm);
    stm->pcm = NULL;
  }
  free(stm->saved_fds);
  pthread_mutex_unlock(&stm->mutex);
  pthread_mutex_destroy(&stm->mutex);

  r = pthread_cond_destroy(&stm->cond);
  assert(r == 0);

  alsa_unregister_stream(stm);

  pthread_mutex_lock(&ctx->mutex);
  assert(ctx->active_streams >= 1);
  ctx->active_streams -= 1;
  pthread_mutex_unlock(&ctx->mutex);

  free(stm);
}

static int
alsa_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  int rv;
  cubeb_stream * stm;
  snd_pcm_hw_params_t* hw_params;
  cubeb_stream_params params;
  params.rate = 44100;
  params.format = CUBEB_SAMPLE_FLOAT32NE;
  params.channels = 2;

  snd_pcm_hw_params_alloca(&hw_params);

  assert(ctx);

  rv = alsa_stream_init(ctx, &stm, "", params, 100, NULL, NULL, NULL);
  if (rv != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  rv = snd_pcm_hw_params_any(stm->pcm, hw_params);
  if (rv < 0) {
    return CUBEB_ERROR;
  }

  rv = snd_pcm_hw_params_get_channels_max(hw_params, max_channels);
  if (rv < 0) {
    return CUBEB_ERROR;
  }

  alsa_stream_destroy(stm);

  return CUBEB_OK;
}

static int
alsa_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate) {
  int rv, dir;
  snd_pcm_t * pcm;
  snd_pcm_hw_params_t * hw_params;

  snd_pcm_hw_params_alloca(&hw_params);

  /* get a pcm, disabling resampling, so we get a rate the
   * hardware/dmix/pulse/etc. supports. */
  rv = snd_pcm_open(&pcm, "", SND_PCM_STREAM_PLAYBACK | SND_PCM_NO_AUTO_RESAMPLE, 0);
  if (rv < 0) {
    return CUBEB_ERROR;
  }

  rv = snd_pcm_hw_params_any(pcm, hw_params);
  if (rv < 0) {
    snd_pcm_close(pcm);
    return CUBEB_ERROR;
  }

  rv = snd_pcm_hw_params_get_rate(hw_params, rate, &dir);
  if (rv >= 0) {
    /* There is a default rate: use it. */
    snd_pcm_close(pcm);
    return CUBEB_OK;
  }

  /* Use a common rate, alsa may adjust it based on hw/etc. capabilities. */
  *rate = 44100;

  rv = snd_pcm_hw_params_set_rate_near(pcm, hw_params, rate, NULL);
  if (rv < 0) {
    snd_pcm_close(pcm);
    return CUBEB_ERROR;
  }

  snd_pcm_close(pcm);

  return CUBEB_OK;
}

static int
alsa_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_ms)
{
  /* This is found to be an acceptable minimum, even on a super low-end
  * machine. */
  *latency_ms = 40;

  return CUBEB_OK;
}

static int
alsa_stream_start(cubeb_stream * stm)
{
  cubeb * ctx;

  assert(stm);
  ctx = stm->context;

  pthread_mutex_lock(&stm->mutex);
  snd_pcm_pause(stm->pcm, 0);
  gettimeofday(&stm->last_activity, NULL);
  pthread_mutex_unlock(&stm->mutex);

  pthread_mutex_lock(&ctx->mutex);
  if (stm->state != INACTIVE) {
    pthread_mutex_unlock(&ctx->mutex);
    return CUBEB_ERROR;
  }
  alsa_set_stream_state(stm, RUNNING);
  pthread_mutex_unlock(&ctx->mutex);

  return CUBEB_OK;
}

static int
alsa_stream_stop(cubeb_stream * stm)
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

  alsa_set_stream_state(stm, INACTIVE);
  pthread_mutex_unlock(&ctx->mutex);

  pthread_mutex_lock(&stm->mutex);
  snd_pcm_pause(stm->pcm, 1);
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
}

static int
alsa_stream_get_position(cubeb_stream * stm, uint64_t * position)
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

int
alsa_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  snd_pcm_sframes_t delay;
  /* This function returns the delay in frames until a frame written using
     snd_pcm_writei is sent to the DAC. The DAC delay should be < 1ms anyways. */
  if (snd_pcm_delay(stm->pcm, &delay)) {
    return CUBEB_ERROR;
  }

  *latency = delay;

  return CUBEB_OK;
}

static struct cubeb_ops const alsa_ops = {
  .init = alsa_init,
  .get_backend_id = alsa_get_backend_id,
  .get_max_channel_count = alsa_get_max_channel_count,
  .get_min_latency = alsa_get_min_latency,
  .get_preferred_sample_rate = alsa_get_preferred_sample_rate,
  .destroy = alsa_destroy,
  .stream_init = alsa_stream_init,
  .stream_destroy = alsa_stream_destroy,
  .stream_start = alsa_stream_start,
  .stream_stop = alsa_stream_stop,
  .stream_get_position = alsa_stream_get_position,
  .stream_get_latency = alsa_stream_get_latency
};
