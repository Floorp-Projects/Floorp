/*
 * Copyright © 2012 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <SLES/OpenSLES.h>
#if defined(__ANDROID__)
#include "android/sles_definitions.h"
#include <SLES/OpenSLES_Android.h>
#endif
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"

static struct cubeb_ops const opensl_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * lib;
  SLInterfaceID SL_IID_BUFFERQUEUE;
  SLInterfaceID SL_IID_PLAY;
#if defined(__ANDROID__)
  SLInterfaceID SL_IID_ANDROIDCONFIGURATION;
#endif
  SLObjectItf engObj;
  SLEngineItf eng;
  SLObjectItf outmixObj;
};

#define NELEMS(A) (sizeof(A) / sizeof A[0])
#define NBUFS 4

struct cubeb_stream {
  cubeb * context;
  SLObjectItf playerObj;
  SLPlayItf play;
  SLBufferQueueItf bufq;
  void *queuebuf[NBUFS];
  int queuebuf_idx;
  long queuebuf_len;
  long bytespersec;
  long framesize;
  int draining;

  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
};

static void
bufferqueue_callback(SLBufferQueueItf caller, void * user_ptr)
{
  cubeb_stream * stm = user_ptr;
  SLBufferQueueState state;
  (*stm->bufq)->GetState(stm->bufq, &state);

  if (stm->draining) {
    if (!state.count) {
      stm->draining = 0;
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
    }
    return;
  }

  if (state.count > 1)
    return;

  SLuint32 i;
  for (i = state.count; i < NBUFS; i++) {
    void *buf = stm->queuebuf[stm->queuebuf_idx];
    long written = stm->data_callback(stm, stm->user_ptr,
                                      buf, stm->queuebuf_len / stm->framesize);
    if (written == CUBEB_ERROR) {
      (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_STOPPED);
      return;
    }

    if (written) {
      (*stm->bufq)->Enqueue(stm->bufq, buf, written * stm->framesize);
      stm->queuebuf_idx = (stm->queuebuf_idx + 1) % NBUFS;
    } else if (!i) {
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      return;
    }

    if ((written * stm->framesize) < stm->queuebuf_len) {
      stm->draining = 1;
      return;
    }
  }
}

#if defined(__ANDROID__)
static SLuint32
convert_stream_type_to_sl_stream(cubeb_stream_type stream_type)
{
  switch(stream_type) {
    case CUBEB_STREAM_TYPE_SYSTEM:
      return SL_ANDROID_STREAM_SYSTEM;
    case CUBEB_STREAM_TYPE_MUSIC:
      return SL_ANDROID_STREAM_MEDIA;
    case CUBEB_STREAM_TYPE_NOTIFICATION:
      return SL_ANDROID_STREAM_NOTIFICATION;
    case CUBEB_STREAM_TYPE_ALARM:
      return SL_ANDROID_STREAM_ALARM;
    case CUBEB_STREAM_TYPE_VOICE_CALL:
      return SL_ANDROID_STREAM_VOICE;
    case CUBEB_STREAM_TYPE_RING:
      return SL_ANDROID_STREAM_RING;
    case CUBEB_STREAM_TYPE_ENFORCED_AUDIBLE:
    default:
      return 0xFFFFFFFF;
  }
}
#endif

static void opensl_destroy(cubeb * ctx);

/*static*/ int
opensl_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;

  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &opensl_ops;

  ctx->lib = dlopen("libOpenSLES.so", RTLD_LAZY);
  if (!ctx->lib) {
    free(ctx);
    return CUBEB_ERROR;
  }

  typedef SLresult (*slCreateEngine_t)(SLObjectItf *,
                                       SLuint32,
                                       const SLEngineOption *,
                                       SLuint32,
                                       const SLInterfaceID *,
                                       const SLboolean *);
  slCreateEngine_t f_slCreateEngine =
    (slCreateEngine_t)dlsym(ctx->lib, "slCreateEngine");
  SLInterfaceID SL_IID_ENGINE = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_ENGINE");
  SLInterfaceID SL_IID_OUTPUTMIX = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_OUTPUTMIX");
  ctx->SL_IID_BUFFERQUEUE = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_BUFFERQUEUE");
#if defined(__ANDROID__)
  ctx->SL_IID_ANDROIDCONFIGURATION = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_ANDROIDCONFIGURATION");
#endif
  ctx->SL_IID_PLAY = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_PLAY");
  if (!f_slCreateEngine ||
      !SL_IID_ENGINE ||
      !SL_IID_OUTPUTMIX ||
      !ctx->SL_IID_BUFFERQUEUE ||
#if defined(__ANDROID__)
      !ctx->SL_IID_ANDROIDCONFIGURATION ||
#endif
      !ctx->SL_IID_PLAY) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }


  const SLEngineOption opt[] = {{SL_ENGINEOPTION_THREADSAFE, SL_BOOLEAN_TRUE}};

  SLresult res;
  res = f_slCreateEngine(&ctx->engObj, 1, opt, 0, NULL, NULL);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = (*ctx->engObj)->Realize(ctx->engObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = (*ctx->engObj)->GetInterface(ctx->engObj, SL_IID_ENGINE, &ctx->eng);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  const SLInterfaceID idsom[] = {SL_IID_OUTPUTMIX};
  const SLboolean reqom[] = {SL_BOOLEAN_TRUE};
  res = (*ctx->eng)->CreateOutputMix(ctx->eng, &ctx->outmixObj, 1, idsom, reqom);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = (*ctx->outmixObj)->Realize(ctx->outmixObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  *context = ctx;

  return CUBEB_OK;
}

static char const *
opensl_get_backend_id(cubeb * ctx)
{
  return "opensl";
}

static int
opensl_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);
  /* The android mixer handles up to two channels, see
  http://androidxref.com/4.2.2_r1/xref/frameworks/av/services/audioflinger/AudioFlinger.h#67 */
  *max_channels = 2;

  return CUBEB_OK;
}

static void
opensl_destroy(cubeb * ctx)
{
  if (ctx->outmixObj)
    (*ctx->outmixObj)->Destroy(ctx->outmixObj);
  if (ctx->engObj)
    (*ctx->engObj)->Destroy(ctx->engObj);
  dlclose(ctx->lib);
  free(ctx);
}

static void opensl_stream_destroy(cubeb_stream * stm);

static int
opensl_stream_init(cubeb * ctx, cubeb_stream ** stream, char const * stream_name,
                  cubeb_stream_params stream_params, unsigned int latency,
                  cubeb_data_callback data_callback, cubeb_state_callback state_callback,
                  void * user_ptr)
{
  cubeb_stream * stm;

  assert(ctx);

  *stream = NULL;

  if (stream_params.rate < 8000 || stream_params.rate > 48000 ||
      stream_params.channels < 1 || stream_params.channels > 32 ||
      latency < 1 || latency > 2000) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  SLDataFormat_PCM format;

  format.formatType = SL_DATAFORMAT_PCM;
  format.numChannels = stream_params.channels;
  // samplesPerSec is in milliHertz
  format.samplesPerSec = stream_params.rate * 1000;
  format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  format.channelMask = stream_params.channels == 1 ?
                       SL_SPEAKER_FRONT_CENTER :
                       SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    format.endianness = SL_BYTEORDER_LITTLEENDIAN;
    break;
  case CUBEB_SAMPLE_S16BE:
    format.endianness = SL_BYTEORDER_BIGENDIAN;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = ctx;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;

  stm->framesize = stream_params.channels * sizeof(int16_t);
  stm->bytespersec = stream_params.rate * stm->framesize;
  stm->queuebuf_len = (stm->bytespersec * latency) / (1000 * NBUFS);
  stm->queuebuf_len += stm->framesize - (stm->queuebuf_len % stm->framesize);
  int i;
  for (i = 0; i < NBUFS; i++) {
    stm->queuebuf[i] = malloc(stm->queuebuf_len);
    assert(stm->queuebuf[i]);
  }

  SLDataLocator_BufferQueue loc_bufq;
  loc_bufq.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
  loc_bufq.numBuffers = NBUFS;
  SLDataSource source;
  source.pLocator = &loc_bufq;
  source.pFormat = &format;

  SLDataLocator_OutputMix loc_outmix;
  loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
  loc_outmix.outputMix = ctx->outmixObj;
  SLDataSink sink;
  sink.pLocator = &loc_outmix;
  sink.pFormat = NULL;

#if defined(__ANDROID__)
  const SLInterfaceID ids[] = {ctx->SL_IID_BUFFERQUEUE, ctx->SL_IID_ANDROIDCONFIGURATION};
  const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
#else
  const SLInterfaceID ids[] = {ctx->SL_IID_BUFFERQUEUE};
  const SLboolean req[] = {SL_BOOLEAN_TRUE};
#endif
  assert(NELEMS(ids) == NELEMS(req));
  SLresult res = (*ctx->eng)->CreateAudioPlayer(ctx->eng, &stm->playerObj,
                                                &source, &sink, NELEMS(ids), ids, req);
  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

#if defined(__ANDROID__)
  SLuint32 stream_type = convert_stream_type_to_sl_stream(stream_params.stream_type);
  if (stream_type != 0xFFFFFFFF) {
    SLAndroidConfigurationItf playerConfig;
    res = (*stm->playerObj)->GetInterface(stm->playerObj,
          ctx->SL_IID_ANDROIDCONFIGURATION, &playerConfig);
    res = (*playerConfig)->SetConfiguration(playerConfig,
          SL_ANDROID_KEY_STREAM_TYPE, &stream_type, sizeof(SLint32));
    if (res != SL_RESULT_SUCCESS) {
      opensl_stream_destroy(stm);
      return CUBEB_ERROR;
    }
  }
#endif

  res = (*stm->playerObj)->Realize(stm->playerObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  res = (*stm->playerObj)->GetInterface(stm->playerObj, ctx->SL_IID_PLAY, &stm->play);
  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  res = (*stm->playerObj)->GetInterface(stm->playerObj, ctx->SL_IID_BUFFERQUEUE,
                                    &stm->bufq);
  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  res = (*stm->bufq)->RegisterCallback(stm->bufq, bufferqueue_callback, stm);
  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
opensl_stream_destroy(cubeb_stream * stm)
{
  if (stm->playerObj)
    (*stm->playerObj)->Destroy(stm->playerObj);
  int i;
  for (i = 0; i < NBUFS; i++) {
    free(stm->queuebuf[i]);
  }
  free(stm);
}

static int
opensl_stream_start(cubeb_stream * stm)
{
  SLresult res = (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_PLAYING);
  if (res != SL_RESULT_SUCCESS)
    return CUBEB_ERROR;
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);
  bufferqueue_callback(NULL, stm);
  return CUBEB_OK;
}

static int
opensl_stream_stop(cubeb_stream * stm)
{
  SLresult res = (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_PAUSED);
  if (res != SL_RESULT_SUCCESS)
    return CUBEB_ERROR;
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  return CUBEB_OK;
}

static int
opensl_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  SLmillisecond msec;
  SLresult res = (*stm->play)->GetPosition(stm->play, &msec);
  if (res != SL_RESULT_SUCCESS)
    return CUBEB_ERROR;
  *position = (stm->bytespersec / (1000 * stm->framesize)) * msec;
  return CUBEB_OK;
}

static struct cubeb_ops const opensl_ops = {
  .init = opensl_init,
  .get_backend_id = opensl_get_backend_id,
  .get_max_channel_count = opensl_get_max_channel_count,
  .destroy = opensl_destroy,
  .stream_init = opensl_stream_init,
  .stream_destroy = opensl_stream_destroy,
  .stream_start = opensl_stream_start,
  .stream_stop = opensl_stream_stop,
  .stream_get_position = opensl_stream_get_position
};
