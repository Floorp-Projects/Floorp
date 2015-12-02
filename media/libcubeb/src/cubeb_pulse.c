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
#include <stdio.h>

#ifdef DISABLE_LIBPULSE_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) cubeb_##x
#define LIBPULSE_API_VISIT(X)                   \
  X(pa_channel_map_can_balance)                 \
  X(pa_channel_map_init_auto)                   \
  X(pa_context_connect)                         \
  X(pa_context_disconnect)                      \
  X(pa_context_drain)                           \
  X(pa_context_get_server_info)                 \
  X(pa_context_get_sink_info_by_name)           \
  X(pa_context_get_sink_info_list)              \
  X(pa_context_get_source_info_list)            \
  X(pa_context_get_state)                       \
  X(pa_context_new)                             \
  X(pa_context_rttime_new)                      \
  X(pa_context_set_sink_input_volume)           \
  X(pa_context_set_state_callback)              \
  X(pa_context_unref)                           \
  X(pa_cvolume_set)                             \
  X(pa_cvolume_set_balance)                     \
  X(pa_frame_size)                              \
  X(pa_operation_get_state)                     \
  X(pa_operation_unref)                         \
  X(pa_proplist_gets)                           \
  X(pa_rtclock_now)                             \
  X(pa_stream_begin_write)                      \
  X(pa_stream_cancel_write)                     \
  X(pa_stream_connect_playback)                 \
  X(pa_stream_cork)                             \
  X(pa_stream_disconnect)                       \
  X(pa_stream_get_channel_map)                  \
  X(pa_stream_get_index)                        \
  X(pa_stream_get_latency)                      \
  X(pa_stream_get_sample_spec)                  \
  X(pa_stream_get_state)                        \
  X(pa_stream_get_time)                         \
  X(pa_stream_new)                              \
  X(pa_stream_set_state_callback)               \
  X(pa_stream_set_write_callback)               \
  X(pa_stream_unref)                            \
  X(pa_stream_update_timing_info)               \
  X(pa_stream_write)                            \
  X(pa_sw_volume_from_linear)                   \
  X(pa_threaded_mainloop_free)                  \
  X(pa_threaded_mainloop_get_api)               \
  X(pa_threaded_mainloop_in_thread)             \
  X(pa_threaded_mainloop_lock)                  \
  X(pa_threaded_mainloop_new)                   \
  X(pa_threaded_mainloop_signal)                \
  X(pa_threaded_mainloop_start)                 \
  X(pa_threaded_mainloop_stop)                  \
  X(pa_threaded_mainloop_unlock)                \
  X(pa_threaded_mainloop_wait)                  \
  X(pa_usec_to_bytes)                           \

#define MAKE_TYPEDEF(x) static typeof(x) * cubeb_##x;
LIBPULSE_API_VISIT(MAKE_TYPEDEF);
#undef MAKE_TYPEDEF
#endif

static struct cubeb_ops const pulse_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * libpulse;
  pa_threaded_mainloop * mainloop;
  pa_context * context;
  pa_sink_info * default_sink_info;
  char * context_name;
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
  float volume;
};

const float PULSE_NO_GAIN = -1.0;

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

    if (stm->volume != PULSE_NO_GAIN) {
      uint32_t samples =  size * stm->sample_spec.channels / frame_size ;

      if (stm->sample_spec.format == PA_SAMPLE_S16BE ||
          stm->sample_spec.format == PA_SAMPLE_S16LE) {
        short * b = buffer;
        for (uint32_t i = 0; i < samples; i++) {
          b[i] *= stm->volume;
        }
      } else {
        float * b = buffer;
        for (uint32_t i = 0; i < samples; i++) {
          b[i] *= stm->volume;
        }
      }
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
    if (!PA_CONTEXT_IS_GOOD(WRAP(pa_context_get_state)(ctx->context))) {
      return -1;
    }
    if (stream && !PA_STREAM_IS_GOOD(WRAP(pa_stream_get_state)(stream))) {
      return -1;
    }
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

static void pulse_context_destroy(cubeb * ctx);
static void pulse_destroy(cubeb * ctx);

static int
pulse_context_init(cubeb * ctx)
{
  if (ctx->context) {
    assert(ctx->error == 1);
    pulse_context_destroy(ctx);
  }

  ctx->context = WRAP(pa_context_new)(WRAP(pa_threaded_mainloop_get_api)(ctx->mainloop),
                                      ctx->context_name);
  if (!ctx->context) {
    return -1;
  }
  WRAP(pa_context_set_state_callback)(ctx->context, context_state_callback, ctx);

  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  WRAP(pa_context_connect)(ctx->context, NULL, 0, NULL);

  if (wait_until_context_ready(ctx) != 0) {
    WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);
    pulse_context_destroy(ctx);
    ctx->context = NULL;
    return -1;
  }

  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);

  ctx->error = 0;

  return 0;
}

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

#define LOAD(x) {                               \
    cubeb_##x = dlsym(libpulse, #x);            \
    if (!cubeb_##x) {                           \
      dlclose(libpulse);                        \
      return CUBEB_ERROR;                       \
    }                                           \
  }

  LIBPULSE_API_VISIT(LOAD);
#undef LOAD
#endif

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &pulse_ops;
  ctx->libpulse = libpulse;

  ctx->mainloop = WRAP(pa_threaded_mainloop_new)();
  ctx->default_sink_info = NULL;

  WRAP(pa_threaded_mainloop_start)(ctx->mainloop);

  ctx->context_name = context_name ? strdup(context_name) : NULL;
  if (pulse_context_init(ctx) != 0) {
    pulse_destroy(ctx);
    return CUBEB_ERROR;
  }

  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
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

  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  while (!ctx->default_sink_info) {
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
  }
  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);

  *max_channels = ctx->default_sink_info->channel_map.channels;

  return CUBEB_OK;
}

static int
pulse_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  assert(ctx && rate);

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
pulse_context_destroy(cubeb * ctx)
{
  pa_operation * o;

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

static void
pulse_destroy(cubeb * ctx)
{
  if (ctx->context_name) {
    free(ctx->context_name);
  }
  if (ctx->context) {
    pulse_context_destroy(ctx);
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

  // If the connection failed for some reason, try to reconnect
  if (context->error == 1 && pulse_context_init(context) != 0) {
    return CUBEB_ERROR;
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
  stm->volume = PULSE_NO_GAIN;

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

void volume_success(pa_context *c, int success, void *userdata)
{
  cubeb_stream * stream = userdata;
  assert(success);
  WRAP(pa_threaded_mainloop_signal)(stream->context->mainloop, 0);
}

int
pulse_stream_set_volume(cubeb_stream * stm, float volume)
{
  uint32_t index;
  pa_operation * op;
  pa_volume_t vol;
  pa_cvolume cvol;
  const pa_sample_spec * ss;

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);

  while (!stm->context->default_sink_info) {
    WRAP(pa_threaded_mainloop_wait)(stm->context->mainloop);
  }

  /* if the pulse daemon is configured to use flat volumes,
   * apply our own gain instead of changing the input volume on the sink. */
  if (stm->context->default_sink_info->flags & PA_SINK_FLAT_VOLUME) {
    stm->volume = volume;
  } else {
    ss = WRAP(pa_stream_get_sample_spec)(stm->stream);

    vol = WRAP(pa_sw_volume_from_linear)(volume);
    WRAP(pa_cvolume_set)(&cvol, ss->channels, vol);

    index = WRAP(pa_stream_get_index)(stm->stream);

    op = WRAP(pa_context_set_sink_input_volume)(stm->context->context,
                                                index, &cvol, volume_success,
                                                stm);
    if (op) {
      operation_wait(stm->context, stm->stream, op);
      WRAP(pa_operation_unref)(op);
    }
  }

  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  return CUBEB_OK;
}

int
pulse_stream_set_panning(cubeb_stream * stream, float panning)
{
  const pa_channel_map * map;
  pa_cvolume vol;

  map = WRAP(pa_stream_get_channel_map)(stream->stream);

  if (!WRAP(pa_channel_map_can_balance)(map)) {
    return CUBEB_ERROR;
  }

  WRAP(pa_cvolume_set_balance)(&vol, map, panning);

  return CUBEB_OK;
}

typedef struct {
  char * default_sink_name;
  char * default_source_name;

  cubeb_device_info ** devinfo;
  uint32_t max;
  uint32_t count;
} pulse_dev_list_data;

static cubeb_device_fmt
pulse_format_to_cubeb_format(pa_sample_format_t format)
{
  switch (format) {
  case PA_SAMPLE_S16LE:
    return CUBEB_DEVICE_FMT_S16LE;
  case PA_SAMPLE_S16BE:
    return CUBEB_DEVICE_FMT_S16BE;
  case PA_SAMPLE_FLOAT32LE:
    return CUBEB_DEVICE_FMT_F32LE;
  case PA_SAMPLE_FLOAT32BE:
    return CUBEB_DEVICE_FMT_F32BE;
  default:
    return CUBEB_DEVICE_FMT_F32NE;
  }
}

static void
pulse_ensure_dev_list_data_list_size (pulse_dev_list_data * list_data)
{
  if (list_data->count == list_data->max) {
    list_data->max += 8;
    list_data->devinfo = realloc(list_data->devinfo,
        sizeof(cubeb_device_info) * list_data->max);
  }
}

static cubeb_device_state
pulse_get_state_from_sink_port(pa_sink_port_info * info)
{
  if (info != NULL) {
#if PA_CHECK_VERSION(2, 0, 0)
    if (info->available == PA_PORT_AVAILABLE_NO)
      return CUBEB_DEVICE_STATE_UNPLUGGED;
    else /*if (info->available == PA_PORT_AVAILABLE_YES) + UNKNOWN */
#endif
      return CUBEB_DEVICE_STATE_ENABLED;
  }

  return CUBEB_DEVICE_STATE_DISABLED;
}

static void
pulse_sink_info_cb(pa_context * context, const pa_sink_info * info,
    int eol, void * user_data)
{
  pulse_dev_list_data * list_data = user_data;
  cubeb_device_info * devinfo;
  const char * prop;

  (void)context;

  if (eol || info == NULL)
    return;

  devinfo = calloc(1, sizeof(cubeb_device_info));

  devinfo->device_id = strdup(info->name);
  devinfo->devid = (cubeb_devid)devinfo->device_id;
  devinfo->friendly_name = strdup(info->description);
  prop = WRAP(pa_proplist_gets)(info->proplist, "sysfs.path");
  if (prop)
    devinfo->group_id = strdup(prop);
  prop = WRAP(pa_proplist_gets)(info->proplist, "device.vendor.name");
  if (prop)
    devinfo->vendor_name = strdup(prop);

  devinfo->type = CUBEB_DEVICE_TYPE_OUTPUT;
  devinfo->state = pulse_get_state_from_sink_port(info->active_port);
  devinfo->preferred = strcmp(info->name, list_data->default_sink_name) == 0;

  devinfo->format = CUBEB_DEVICE_FMT_ALL;
  devinfo->default_format = pulse_format_to_cubeb_format(info->sample_spec.format);
  devinfo->max_channels = info->channel_map.channels;
  devinfo->min_rate = 1;
  devinfo->max_rate = PA_RATE_MAX;
  devinfo->default_rate = info->sample_spec.rate;

  devinfo->latency_lo_ms = 40;
  devinfo->latency_hi_ms = 400;

  pulse_ensure_dev_list_data_list_size (list_data);
  list_data->devinfo[list_data->count++] = devinfo;
}

static cubeb_device_state
pulse_get_state_from_source_port(pa_source_port_info * info)
{
  if (info != NULL) {
#if PA_CHECK_VERSION(2, 0, 0)
    if (info->available == PA_PORT_AVAILABLE_NO)
      return CUBEB_DEVICE_STATE_UNPLUGGED;
    else /*if (info->available == PA_PORT_AVAILABLE_YES) + UNKNOWN */
#endif
      return CUBEB_DEVICE_STATE_ENABLED;
  }

  return CUBEB_DEVICE_STATE_DISABLED;
}

static void
pulse_source_info_cb(pa_context * context, const pa_source_info * info,
    int eol, void * user_data)
{
  pulse_dev_list_data * list_data = user_data;
  cubeb_device_info * devinfo;
  const char * prop;

  (void)context;

  if (eol)
    return;

  devinfo = calloc(1, sizeof(cubeb_device_info));

  devinfo->device_id = strdup(info->name);
  devinfo->devid = (cubeb_devid)devinfo->device_id;
  devinfo->friendly_name = strdup(info->description);
  prop = WRAP(pa_proplist_gets)(info->proplist, "sysfs.path");
  if (prop)
    devinfo->group_id = strdup(prop);
  prop = WRAP(pa_proplist_gets)(info->proplist, "device.vendor.name");
  if (prop)
    devinfo->vendor_name = strdup(prop);

  devinfo->type = CUBEB_DEVICE_TYPE_INPUT;
  devinfo->state = pulse_get_state_from_source_port(info->active_port);
  devinfo->preferred = strcmp(info->name, list_data->default_source_name) == 0;

  devinfo->format = CUBEB_DEVICE_FMT_ALL;
  devinfo->default_format = pulse_format_to_cubeb_format(info->sample_spec.format);
  devinfo->max_channels = info->channel_map.channels;
  devinfo->min_rate = 1;
  devinfo->max_rate = PA_RATE_MAX;
  devinfo->default_rate = info->sample_spec.rate;

  devinfo->latency_lo_ms = 1;
  devinfo->latency_hi_ms = 10;

  pulse_ensure_dev_list_data_list_size (list_data);
  list_data->devinfo[list_data->count++] = devinfo;
}

static void
pulse_server_info_cb(pa_context * c, const pa_server_info * i, void * userdata)
{
  pulse_dev_list_data * list_data = userdata;

  (void)c;

  free(list_data->default_sink_name);
  free(list_data->default_source_name);
  list_data->default_sink_name = strdup(i->default_sink_name);
  list_data->default_source_name = strdup(i->default_source_name);
}

static int
pulse_enumerate_devices(cubeb * context, cubeb_device_type type,
                        cubeb_device_collection ** collection)
{
  pulse_dev_list_data user_data = { NULL, NULL, NULL, 0, 0 };
  pa_operation * o;
  uint32_t i;

  o = WRAP(pa_context_get_server_info)(context->context,
      pulse_server_info_cb, &user_data);
  if (o) {
    operation_wait(context, NULL, o);
    WRAP(pa_operation_unref)(o);
  }

  if (type & CUBEB_DEVICE_TYPE_OUTPUT) {
    o = WRAP(pa_context_get_sink_info_list)(context->context,
        pulse_sink_info_cb, &user_data);
    if (o) {
      operation_wait(context, NULL, o);
      WRAP(pa_operation_unref)(o);
    }
  }

  if (type & CUBEB_DEVICE_TYPE_INPUT) {
    o = WRAP(pa_context_get_source_info_list)(context->context,
        pulse_source_info_cb, &user_data);
    if (o) {
      operation_wait(context, NULL, o);
      WRAP(pa_operation_unref)(o);
    }
  }

  *collection = malloc(sizeof(cubeb_device_collection) +
      sizeof(cubeb_device_info*) * (user_data.count > 0 ? user_data.count - 1 : 0));
  (*collection)->count = user_data.count;
  for (i = 0; i < user_data.count; i++)
    (*collection)->device[i] = user_data.devinfo[i];

  free(user_data.devinfo);
  return CUBEB_OK;
}

static struct cubeb_ops const pulse_ops = {
  .init = pulse_init,
  .get_backend_id = pulse_get_backend_id,
  .get_max_channel_count = pulse_get_max_channel_count,
  .get_min_latency = pulse_get_min_latency,
  .get_preferred_sample_rate = pulse_get_preferred_sample_rate,
  .enumerate_devices = pulse_enumerate_devices,
  .destroy = pulse_destroy,
  .stream_init = pulse_stream_init,
  .stream_destroy = pulse_stream_destroy,
  .stream_start = pulse_stream_start,
  .stream_stop = pulse_stream_stop,
  .stream_get_position = pulse_stream_get_position,
  .stream_get_latency = pulse_stream_get_latency,
  .stream_set_volume = pulse_stream_set_volume,
  .stream_set_panning = pulse_stream_set_panning,
  .stream_get_current_device = NULL,
  .stream_device_destroy = NULL,
  .stream_register_device_changed_callback = NULL
};
