/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"

#define NELEMS(x) ((int) (sizeof(x) / sizeof(x[0])))

struct cubeb {
  struct cubeb_ops * ops;
};

struct cubeb_stream {
  struct cubeb * context;
};

#if defined(USE_PULSE)
int pulse_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_JACK)
int jack_init (cubeb ** context, char const * context_name);
#endif
#if defined(USE_ALSA)
int alsa_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AUDIOQUEUE)
int audioqueue_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AUDIOUNIT)
int audiounit_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_DIRECTSOUND)
int directsound_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_WINMM)
int winmm_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_WASAPI)
int wasapi_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_SNDIO)
int sndio_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_OPENSL)
int opensl_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AUDIOTRACK)
int audiotrack_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_KAI)
int kai_init(cubeb ** context, char const * context_name);
#endif

int
validate_stream_params(cubeb_stream_params stream_params)
{
  if (stream_params.rate < 1000 || stream_params.rate > 192000 ||
      stream_params.channels < 1 || stream_params.channels > 8) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
  case CUBEB_SAMPLE_S16BE:
  case CUBEB_SAMPLE_FLOAT32LE:
  case CUBEB_SAMPLE_FLOAT32BE:
    return CUBEB_OK;
  }

  return CUBEB_ERROR_INVALID_FORMAT;
}

int
validate_latency(int latency)
{
  if (latency < 1 || latency > 2000) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }
  return CUBEB_OK;
}

int
cubeb_init(cubeb ** context, char const * context_name)
{
  int (* init[])(cubeb **, char const *) = {
#if defined(USE_JACK)
    jack_init,
#endif
#if defined(USE_PULSE)
    pulse_init,
#endif
#if defined(USE_ALSA)
    alsa_init,
#endif
#if defined(USE_AUDIOUNIT)
    audiounit_init,
#endif
#if defined(USE_AUDIOQUEUE)
    audioqueue_init,
#endif
#if defined(USE_WASAPI)
    wasapi_init,
#endif
#if defined(USE_WINMM)
    winmm_init,
#endif
#if defined(USE_DIRECTSOUND)
    directsound_init,
#endif
#if defined(USE_SNDIO)
    sndio_init,
#endif
#if defined(USE_OPENSL)
    opensl_init,
#endif
#if defined(USE_AUDIOTRACK)
    audiotrack_init,
#endif
#if defined(USE_KAI)
    kai_init,
#endif
  };
  int i;

  if (!context) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  for (i = 0; i < NELEMS(init); ++i) {
    if (init[i](context, context_name) == CUBEB_OK) {
      /* Assert that the minimal API is implemented. */
#define OK(fn) assert((* context)->ops->fn)
      OK(get_backend_id);
      OK(destroy);
      OK(stream_init);
      OK(stream_destroy);
      OK(stream_start);
      OK(stream_stop);
      OK(stream_get_position);
      return CUBEB_OK;
    }
  }

  return CUBEB_ERROR;
}

char const *
cubeb_get_backend_id(cubeb * context)
{
  if (!context) {
    return NULL;
  }

  return context->ops->get_backend_id(context);
}

int
cubeb_get_max_channel_count(cubeb * context, uint32_t * max_channels)
{
  if (!context || !max_channels) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!context->ops->get_max_channel_count) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->get_max_channel_count(context, max_channels);
}

int
cubeb_get_min_latency(cubeb * context, cubeb_stream_params params, uint32_t * latency_ms)
{
  if (!context || !latency_ms) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!context->ops->get_min_latency) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->get_min_latency(context, params, latency_ms);
}

int
cubeb_get_preferred_sample_rate(cubeb * context, uint32_t * rate)
{
  if (!context || !rate) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!context->ops->get_preferred_sample_rate) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->get_preferred_sample_rate(context, rate);
}

void
cubeb_destroy(cubeb * context)
{
  if (!context) {
    return;
  }

  context->ops->destroy(context);
}

int
cubeb_stream_init(cubeb * context, cubeb_stream ** stream, char const * stream_name,
                  cubeb_stream_params stream_params, unsigned int latency,
                  cubeb_data_callback data_callback,
                  cubeb_state_callback state_callback,
                  void * user_ptr)
{
  int r;

  if (!context || !stream) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if ((r = validate_stream_params(stream_params)) != CUBEB_OK ||
      (r = validate_latency(latency)) != CUBEB_OK) {
    return r;
  }

  return context->ops->stream_init(context, stream, stream_name,
                                   stream_params, latency,
                                   data_callback,
                                   state_callback,
                                   user_ptr);
}

void
cubeb_stream_destroy(cubeb_stream * stream)
{
  if (!stream) {
    return;
  }

  stream->context->ops->stream_destroy(stream);
}

int
cubeb_stream_start(cubeb_stream * stream)
{
  if (!stream) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  return stream->context->ops->stream_start(stream);
}

int
cubeb_stream_stop(cubeb_stream * stream)
{
  if (!stream) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  return stream->context->ops->stream_stop(stream);
}

int
cubeb_stream_get_position(cubeb_stream * stream, uint64_t * position)
{
  if (!stream || !position) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  return stream->context->ops->stream_get_position(stream, position);
}

int
cubeb_stream_get_latency(cubeb_stream * stream, uint32_t * latency)
{
  if (!stream || !latency) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_get_latency) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_get_latency(stream, latency);
}

int
cubeb_stream_set_volume(cubeb_stream * stream, float volume)
{
  if (!stream || volume > 1.0 || volume < 0.0) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_set_volume) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_set_volume(stream, volume);
}

int cubeb_stream_set_panning(cubeb_stream * stream, float panning)
{
  if (!stream || panning < -1.0 || panning > 1.0) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_set_panning) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_set_panning(stream, panning);
}

int cubeb_stream_get_current_device(cubeb_stream * stream,
                                    cubeb_device ** const device)
{
  if (!stream || !device) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_get_current_device) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_get_current_device(stream, device);
}

int cubeb_stream_device_destroy(cubeb_stream * stream,
                                cubeb_device * device)
{
  if (!stream || !device) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_device_destroy) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_device_destroy(stream, device);
}

int cubeb_stream_register_device_changed_callback(cubeb_stream * stream,
                                                  cubeb_device_changed_callback device_changed_callback)
{
  if (!stream || !device_changed_callback) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_register_device_changed_callback) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_register_device_changed_callback(stream, device_changed_callback);
}

int cubeb_enumerate_devices(cubeb * context,
                            cubeb_device_type devtype,
                            cubeb_device_collection ** collection)
{
  if ((devtype & (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) == 0)
    return CUBEB_ERROR_INVALID_PARAMETER;
  if (collection == NULL)
    return CUBEB_ERROR_INVALID_PARAMETER;
  if (!context->ops->enumerate_devices)
    return CUBEB_ERROR_NOT_SUPPORTED;

  return context->ops->enumerate_devices(context, devtype, collection);
}

int cubeb_device_collection_destroy(cubeb_device_collection * collection)
{
  uint32_t i;

  if (collection == NULL)
    return CUBEB_ERROR_INVALID_PARAMETER;

  for (i = 0; i < collection->count; i++)
    cubeb_device_info_destroy(collection->device[i]);

  free(collection);
  return CUBEB_OK;
}

int cubeb_device_info_destroy(cubeb_device_info * info)
{
  free(info->device_id);
  free(info->friendly_name);
  free(info->group_id);
  free(info->vendor_name);

  free(info);
  return CUBEB_OK;
}

int cubeb_register_device_collection_changed(cubeb * context,
                                             cubeb_device_collection_changed_callback callback,
                                             void * user_ptr)
{
  return CUBEB_ERROR_NOT_SUPPORTED;
}

