/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>
#include <string.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"

#ifdef DISABLE_LIBPULSE_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) cubeb_##x
#define MAKE_TYPEDEF(x) static typeof(x) * cubeb_##x
MAKE_TYPEDEF(pa_channel_map_init_auto);
MAKE_TYPEDEF(pa_context_connect);
MAKE_TYPEDEF(pa_context_disconnect);
MAKE_TYPEDEF(pa_context_drain);
MAKE_TYPEDEF(pa_context_get_state);
MAKE_TYPEDEF(pa_context_new);
MAKE_TYPEDEF(pa_context_rttime_new);
MAKE_TYPEDEF(pa_context_set_state_callback);
MAKE_TYPEDEF(pa_context_unref);
MAKE_TYPEDEF(pa_context_get_sink_info_by_name);
MAKE_TYPEDEF(pa_context_get_server_info);
MAKE_TYPEDEF(pa_frame_size);
MAKE_TYPEDEF(pa_operation_get_state);
MAKE_TYPEDEF(pa_operation_unref);
MAKE_TYPEDEF(pa_rtclock_now);
MAKE_TYPEDEF(pa_stream_begin_write);
MAKE_TYPEDEF(pa_stream_cancel_write);
MAKE_TYPEDEF(pa_stream_connect_playback);
MAKE_TYPEDEF(pa_stream_cork);
MAKE_TYPEDEF(pa_stream_disconnect);
MAKE_TYPEDEF(pa_stream_get_latency);
MAKE_TYPEDEF(pa_stream_get_state);
MAKE_TYPEDEF(pa_stream_get_time);
MAKE_TYPEDEF(pa_stream_new);
MAKE_TYPEDEF(pa_stream_set_state_callback);
MAKE_TYPEDEF(pa_stream_set_write_callback);
MAKE_TYPEDEF(pa_stream_unref);
MAKE_TYPEDEF(pa_stream_update_timing_info);
MAKE_TYPEDEF(pa_stream_write);
MAKE_TYPEDEF(pa_threaded_mainloop_free);
MAKE_TYPEDEF(pa_threaded_mainloop_get_api);
MAKE_TYPEDEF(pa_threaded_mainloop_lock);
MAKE_TYPEDEF(pa_threaded_mainloop_in_thread);
MAKE_TYPEDEF(pa_threaded_mainloop_new);
MAKE_TYPEDEF(pa_threaded_mainloop_signal);
MAKE_TYPEDEF(pa_threaded_mainloop_start);
MAKE_TYPEDEF(pa_threaded_mainloop_stop);
MAKE_TYPEDEF(pa_threaded_mainloop_unlock);
MAKE_TYPEDEF(pa_threaded_mainloop_wait);
MAKE_TYPEDEF(pa_usec_to_bytes);
#undef MAKE_TYPEDEF
#endif

static struct cubeb_ops const pulse_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * libpulse;
  pa_threaded_mainloop * mainloop;
  pa_context * context;
  pa_sink_info * default_sink_info;
  int error;
};

struct cubeb_stream {
  cubeb * context;
  pa_stream * stream;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
  pa_time_event * drain_timer;
  pa_sample_spec sample_spec;
  int shutdown;
};

enum cork_state {
  UNCORK = 0,
  CORK = 1 << 0,
  NOTIFY = 1 << 1
};

static void
sink_info_callback(pa_context * context, const pa_sink_info * info, int eol, void * u)
{
  cubeb * ctx = u;
   if (!eol) {
    ctx->default_sink_info = malloc(sizeof(pa_sink_info));
    memcpy(ctx->default_sink_info, info, sizeof(pa_sink_info));
  }
  WRAP(pa_threaded_mainloop_signal)(ctx->mainloop, 0);
}

static void
server_info_callback(pa_context * context, const pa_server_info * info, void * u)
{
  WRAP(pa_context_get_sink_info_by_name)(context, info->default_sink_name, sink_info_callback, u);
}

static void
context_state_callback(pa_context * c, void * u)
{
  cubeb * ctx = u;
  if (!PA_CONTEXT_IS_GOOD(WRAP(pa_context_get_state)(c))) {
    ctx->error = 1;
  }
  WRAP(pa_threaded_mainloop_signal)(ctx->mainloop, 0);
}

static void
context_notify_callback(pa_context * c, void * u)
{
  cubeb * ctx = u;
  WRAP(pa_threaded_mainloop_signal)(ctx->mainloop, 0);
}

static void
stream_success_callback(pa_stream * s, int success, void * u)
{
  cubeb_stream * stm = u;
  WRAP(pa_threaded_mainloop_signal)(stm->context->mainloop, 0);
}

static void
stream_drain_callback(pa_mainloop_api * a, pa_time_event * e, struct timeval const * tv, void * u)
{
  cubeb_stream * stm = u;
  /* there's no pa_rttime_free, so use this instead. */
  a->time_free(stm->drain_timer);
  stm->drain_timer = NULL;
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
}

static void
stream_state_callback(pa_stream * s, void * u)
{
  cubeb_stream * stm = u;
  if (!PA_STREAM_IS_GOOD(WRAP(pa_stream_get_state)(s))) {
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
  }
  WRAP(pa_threaded_mainloop_signal)(stm->context->mainloop, 0);
}

static void
stream_request_callback(pa_stream * s, size_t nbytes, void * u)
{
  cubeb_stream * stm;
  void * buffer;
  size_t size;
  int r;
  long got;
  size_t towrite;
  size_t frame_size;

  stm = u;

  if (stm->shutdown)
    return;

  frame_size = WRAP(pa_frame_size)(&stm->sample_spec);

  assert(nbytes % frame_size == 0);

  towrite = nbytes;

  while (towrite) {
    size = towrite;
    r = WRAP(pa_stream_begin_write)(s, &buffer, &size);
    assert(r == 0);
    assert(size > 0);
    assert(size % frame_size == 0);

    got = stm->data_callback(stm, stm->user_ptr, buffer, size / frame_size);
    if (got < 0) {
      WRAP(pa_stream_cancel_write)(s);
      stm->shutdown = 1;
      return;
    }

    r = WRAP(pa_stream_write)(s, buffer, got * frame_size, NULL, 0, PA_SEEK_RELATIVE);
    assert(r == 0);

    if ((size_t) got < size / frame_size) {
      pa_usec_t latency = 0;
      r = WRAP(pa_stream_get_latency)(s, &latency, NULL);
      if (r == -PA_ERR_NODATA) {
        /* this needs a better guess. */
        latency = 100 * PA_USEC_PER_MSEC;
      }
      assert(r == 0 || r == -PA_ERR_NODATA);
      /* pa_stream_drain is useless, see PA bug# 866. this is a workaround. */
      /* arbitrary safety margin: double the current latency. */
      stm->drain_timer = WRAP(pa_context_rttime_new)(stm->context->context, WRAP(pa_rtclock_now)() + 2 * latency, stream_drain_callback, stm);
      stm->shutdown = 1;
      return;
    }

    towrite -= size;
  }

  assert(towrite == 0);
}

static int
wait_until_context_ready(cubeb * ctx)
{
  for (;;) {
    pa_context_state_t state = WRAP(pa_context_get_state)(ctx->context);
    if (!PA_CONTEXT_IS_GOOD(state))
      return -1;
    if (state == PA_CONTEXT_READY)
      break;
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
  }
  return 0;
}

static int
wait_until_stream_ready(cubeb_stream * stm)
{
  for (;;) {
    pa_stream_state_t state = WRAP(pa_stream_get_state)(stm->stream);
    if (!PA_STREAM_IS_GOOD(state))
      return -1;
    if (state == PA_STREAM_READY)
      break;
    WRAP(pa_threaded_mainloop_wait)(stm->context->mainloop);
  }
  return 0;
}

static int
operation_wait(cubeb * ctx, pa_stream * stream, pa_operation * o)
{
  while (WRAP(pa_operation_get_state)(o) == PA_OPERATION_RUNNING) {
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
    if (!PA_CONTEXT_IS_GOOD(WRAP(pa_context_get_state)(ctx->context)))
      return -1;
    if (stream && !PA_STREAM_IS_GOOD(WRAP(pa_stream_get_state)(stream)))
      return -1;
  }
  return 0;
}

static void
stream_cork(cubeb_stream * stm, enum cork_state state)
{
  pa_operation * o;

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  o = WRAP(pa_stream_cork)(stm->stream, state & CORK, stream_success_callback, stm);
  if (o) {
    operation_wait(stm->context, stm->stream, o);
    WRAP(pa_operation_unref)(o);
  }
  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  if (state & NOTIFY) {
    stm->state_callback(stm, stm->user_ptr,
                        state & CORK ? CUBEB_STATE_STOPPED : CUBEB_STATE_STARTED);
  }
}

static void pulse_destroy(cubeb * ctx);

/*static*/ int
pulse_init(cubeb ** context, char const * context_name)
{
  void * libpulse = NULL;
  cubeb * ctx;

  *context = NULL;

#ifndef DISABLE_LIBPULSE_DLOPEN
  libpulse = dlopen("libpulse.so.0", RTLD_LAZY);
  if (!libpulse) {
    return CUBEB_ERROR;
  }

#define LOAD(x) do { \
    cubeb_##x = dlsym(libpulse, #x); \
    if (!cubeb_##x) { \
      dlclose(libpulse); \
      return CUBEB_ERROR; \
    } \
  } while(0)
  LOAD(pa_channel_map_init_auto);
  LOAD(pa_context_connect);
  LOAD(pa_context_disconnect);
  LOAD(pa_context_drain);
  LOAD(pa_context_get_state);
  LOAD(pa_context_new);
  LOAD(pa_context_rttime_new);
  LOAD(pa_context_set_state_callback);
  LOAD(pa_context_get_sink_info_by_name);
  LOAD(pa_context_get_server_info);
  LOAD(pa_context_unref);
  LOAD(pa_frame_size);
  LOAD(pa_operation_get_state);
  LOAD(pa_operation_unref);
  LOAD(pa_rtclock_now);
  LOAD(pa_stream_begin_write);
  LOAD(pa_stream_cancel_write);
  LOAD(pa_stream_connect_playback);
  LOAD(pa_stream_cork);
  LOAD(pa_stream_disconnect);
  LOAD(pa_stream_get_latency);
  LOAD(pa_stream_get_state);
  LOAD(pa_stream_get_time);
  LOAD(pa_stream_new);
  LOAD(pa_stream_set_state_callback);
  LOAD(pa_stream_set_write_callback);
  LOAD(pa_stream_unref);
  LOAD(pa_stream_update_timing_info);
  LOAD(pa_stream_write);
  LOAD(pa_threaded_mainloop_free);
  LOAD(pa_threaded_mainloop_get_api);
  LOAD(pa_threaded_mainloop_lock);
  LOAD(pa_threaded_mainloop_in_thread);
  LOAD(pa_threaded_mainloop_new);
  LOAD(pa_threaded_mainloop_signal);
  LOAD(pa_threaded_mainloop_start);
  LOAD(pa_threaded_mainloop_stop);
  LOAD(pa_threaded_mainloop_unlock);
  LOAD(pa_threaded_mainloop_wait);
  LOAD(pa_usec_to_bytes);
#undef LOAD
#endif

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &pulse_ops;
  ctx->libpulse = libpulse;

  ctx->mainloop = WRAP(pa_threaded_mainloop_new)();
  ctx->context = WRAP(pa_context_new)(WRAP(pa_threaded_mainloop_get_api)(ctx->mainloop), context_name);
  ctx->default_sink_info = NULL;

  WRAP(pa_context_set_state_callback)(ctx->context, context_state_callback, ctx);
  WRAP(pa_threaded_mainloop_start)(ctx->mainloop);

  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  WRAP(pa_context_connect)(ctx->context, NULL, 0, NULL);

  if (wait_until_context_ready(ctx) != 0) {
    WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);
    pulse_destroy(ctx);
    return CUBEB_ERROR;
  }
  WRAP(pa_context_get_server_info)(ctx->context, server_info_callback, ctx);
  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);

  *context = ctx;

  return CUBEB_OK;
}

static char const *
pulse_get_backend_id(cubeb * ctx)
{
  return "pulse";
}

static int
pulse_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);

  while (!ctx->default_sink_info) {
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
  }

  *max_channels = ctx->default_sink_info->channel_map.channels;

  return CUBEB_OK;
}

static int
pulse_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  while (!ctx->default_sink_info) {
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
  }
  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);

  *rate = ctx->default_sink_info->sample_spec.rate;

  return CUBEB_OK;
}

static int
pulse_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_ms)
{
  // According to PulseAudio developers, this is a safe minimum.
  *latency_ms = 40;

  return CUBEB_OK;
}

static void
pulse_destroy(cubeb * ctx)
{
  pa_operation * o;

  if (ctx->context) {
    WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
    o = WRAP(pa_context_drain)(ctx->context, context_notify_callback, ctx);
    if (o) {
      operation_wait(ctx, NULL, o);
      WRAP(pa_operation_unref)(o);
    }
    WRAP(pa_context_set_state_callback)(ctx->context, NULL, NULL);
    WRAP(pa_context_disconnect)(ctx->context);
    WRAP(pa_context_unref)(ctx->context);
    WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);
  }

  if (ctx->mainloop) {
    WRAP(pa_threaded_mainloop_stop)(ctx->mainloop);
    WRAP(pa_threaded_mainloop_free)(ctx->mainloop);
  }

  if (ctx->libpulse) {
    dlclose(ctx->libpulse);
  }
  if (ctx->default_sink_info) {
    free(ctx->default_sink_info);
  }
  free(ctx);
}

static void pulse_stream_destroy(cubeb_stream * stm);

static int
pulse_stream_init(cubeb * context, cubeb_stream ** stream, char const * stream_name,
                  cubeb_stream_params stream_params, unsigned int latency,
                  cubeb_data_callback data_callback, cubeb_state_callback state_callback,
                  void * user_ptr)
{
  pa_sample_spec ss;
  cubeb_stream * stm;
  pa_operation * o;
  pa_buffer_attr battr;
  int r;

  assert(context);

  *stream = NULL;

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    ss.format = PA_SAMPLE_S16LE;
    break;
  case CUBEB_SAMPLE_S16BE:
    ss.format = PA_SAMPLE_S16BE;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    ss.format = PA_SAMPLE_FLOAT32LE;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    ss.format = PA_SAMPLE_FLOAT32BE;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  ss.rate = stream_params.rate;
  ss.channels = stream_params.channels;

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = context;

  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;

  stm->sample_spec = ss;

  battr.maxlength = -1;
  battr.tlength = WRAP(pa_usec_to_bytes)(latency * PA_USEC_PER_MSEC, &stm->sample_spec);
  battr.prebuf = -1;
  battr.minreq = battr.tlength / 4;
  battr.fragsize = -1;

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  stm->stream = WRAP(pa_stream_new)(stm->context->context, stream_name, &ss, NULL);
  if (!stm->stream) {
    pulse_stream_destroy(stm);
    return CUBEB_ERROR;
  }
  WRAP(pa_stream_set_state_callback)(stm->stream, stream_state_callback, stm);
  WRAP(pa_stream_set_write_callback)(stm->stream, stream_request_callback, stm);
  WRAP(pa_stream_connect_playback)(stm->stream, NULL, &battr,
                             PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
                             PA_STREAM_START_CORKED,
                             NULL, NULL);

  r = wait_until_stream_ready(stm);
  if (r == 0) {
    /* force a timing update now, otherwise timing info does not become valid
       until some point after initialization has completed. */
    o = WRAP(pa_stream_update_timing_info)(stm->stream, stream_success_callback, stm);
    if (o) {
      r = operation_wait(stm->context, stm->stream, o);
      WRAP(pa_operation_unref)(o);
    }
  }
  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  if (r != 0) {
    pulse_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
pulse_stream_destroy(cubeb_stream * stm)
{
  if (stm->stream) {
    stream_cork(stm, CORK);

    WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);

    if (stm->drain_timer) {
      /* there's no pa_rttime_free, so use this instead. */
      WRAP(pa_threaded_mainloop_get_api)(stm->context->mainloop)->time_free(stm->drain_timer);
    }

    WRAP(pa_stream_set_state_callback)(stm->stream, NULL, NULL);
    WRAP(pa_stream_disconnect)(stm->stream);
    WRAP(pa_stream_unref)(stm->stream);
    WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
  }

  free(stm);
}

static int
pulse_stream_start(cubeb_stream * stm)
{
  stream_cork(stm, UNCORK | NOTIFY);
  return CUBEB_OK;
}

static int
pulse_stream_stop(cubeb_stream * stm)
{
  stream_cork(stm, CORK | NOTIFY);
  return CUBEB_OK;
}

static int
pulse_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  int r, in_thread;
  pa_usec_t r_usec;
  uint64_t bytes;

  in_thread = WRAP(pa_threaded_mainloop_in_thread)(stm->context->mainloop);

  if (!in_thread) {
    WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  }
  r = WRAP(pa_stream_get_time)(stm->stream, &r_usec);
  if (!in_thread) {
    WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
  }

  if (r != 0) {
    return CUBEB_ERROR;
  }

  bytes = WRAP(pa_usec_to_bytes)(r_usec, &stm->sample_spec);
  *position = bytes / WRAP(pa_frame_size)(&stm->sample_spec);

  return CUBEB_OK;
}

int
pulse_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  pa_usec_t r_usec;
  int negative, r;

  if (!stm) {
    return CUBEB_ERROR;
  }

  r = WRAP(pa_stream_get_latency)(stm->stream, &r_usec, &negative);
  assert(!negative);
  if (r) {
    return CUBEB_ERROR;
  }

  *latency = r_usec * stm->sample_spec.rate / PA_USEC_PER_SEC;
  return CUBEB_OK;
}

static struct cubeb_ops const pulse_ops = {
  .init = pulse_init,
  .get_backend_id = pulse_get_backend_id,
  .get_max_channel_count = pulse_get_max_channel_count,
  .get_min_latency = pulse_get_min_latency,
  .get_preferred_sample_rate = pulse_get_preferred_sample_rate,
  .destroy = pulse_destroy,
  .stream_init = pulse_stream_init,
  .stream_destroy = pulse_stream_destroy,
  .stream_start = pulse_stream_start,
  .stream_stop = pulse_stream_stop,
  .stream_get_position = pulse_stream_get_position,
  .stream_get_latency = pulse_stream_get_latency
};
