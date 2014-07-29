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
#include <pthread.h>
#include <SLES/OpenSLES.h>
#include <math.h>
#if defined(__ANDROID__)
#include <sys/system_properties.h>
#include "android/sles_definitions.h"
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Cubeb_OpenSL" , ## args)
#define ANDROID_VERSION_GINGERBREAD_MR1 10
#endif
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb_resampler.h"
#include "cubeb-sles.h"

static struct cubeb_ops const opensl_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * lib;
  void * libmedia;
  int32_t (* get_output_latency)(uint32_t * latency, int stream_type);
  SLInterfaceID SL_IID_BUFFERQUEUE;
  SLInterfaceID SL_IID_PLAY;
#if defined(__ANDROID__)
  SLInterfaceID SL_IID_ANDROIDCONFIGURATION;
#endif
  SLInterfaceID SL_IID_VOLUME;
  SLObjectItf engObj;
  SLEngineItf eng;
  SLObjectItf outmixObj;
};

#define NELEMS(A) (sizeof(A) / sizeof A[0])
#define NBUFS 4
#define AUDIO_STREAM_TYPE_MUSIC 3

struct cubeb_stream {
  cubeb * context;
  pthread_mutex_t mutex;
  SLObjectItf playerObj;
  SLPlayItf play;
  SLBufferQueueItf bufq;
  SLVolumeItf volume;
  uint8_t *queuebuf[NBUFS];
  int queuebuf_idx;
  long queuebuf_len;
  long bytespersec;
  long framesize;
  long written;
  int draining;
  cubeb_stream_type stream_type;

  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;

  cubeb_resampler * resampler;
  unsigned int inputrate;
  unsigned int outputrate;
  unsigned int latency;
};

static void
play_callback(SLPlayItf caller, void * user_ptr, SLuint32 event)
{
  cubeb_stream * stm = user_ptr;
  assert(stm);
  switch (event) {
    case SL_PLAYEVENT_HEADATMARKER:
      pthread_mutex_lock(&stm->mutex);
      assert(stm->draining);
      stm->draining = 0;
      pthread_mutex_unlock(&stm->mutex);
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      break;
    default:
      break;
  }
}

static void
bufferqueue_callback(SLBufferQueueItf caller, void * user_ptr)
{
  cubeb_stream * stm = user_ptr;
  assert(stm);
  SLBufferQueueState state;
  SLresult res;

  res = (*stm->bufq)->GetState(stm->bufq, &state);
  assert(res == SL_RESULT_SUCCESS);

  if (state.count > 1)
    return;

  SLuint32 i;
  for (i = state.count; i < NBUFS; i++) {
    uint8_t *buf = stm->queuebuf[stm->queuebuf_idx];
    long written = 0;
    pthread_mutex_lock(&stm->mutex);
    int draining = stm->draining;
    pthread_mutex_unlock(&stm->mutex);

    if (!draining) {
      written = cubeb_resampler_fill(stm->resampler, buf,
                                        stm->queuebuf_len / stm->framesize);
      if (written < 0 || written * stm->framesize > stm->queuebuf_len) {
        (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_STOPPED);
        return;
      }
    }

    // Keep sending silent data even in draining mode to prevent the audio
    // back-end from being stopped automatically by OpenSL/ES.
    memset(buf + written * stm->framesize, 0, stm->queuebuf_len - written * stm->framesize);
    res = (*stm->bufq)->Enqueue(stm->bufq, buf, stm->queuebuf_len);
    assert(res == SL_RESULT_SUCCESS);
    stm->queuebuf_idx = (stm->queuebuf_idx + 1) % NBUFS;
    if (written > 0) {
      pthread_mutex_lock(&stm->mutex);
      stm->written += written;
      pthread_mutex_unlock(&stm->mutex);
    }

    if (!draining && written * stm->framesize < stm->queuebuf_len) {
      pthread_mutex_lock(&stm->mutex);
      int64_t written_duration = INT64_C(1000) * stm->written * stm->framesize / stm->bytespersec;
      stm->draining = 1;
      pthread_mutex_unlock(&stm->mutex);
      // Use SL_PLAYEVENT_HEADATMARKER event from slPlayCallback of SLPlayItf
      // to make sure all the data has been processed.
      (*stm->play)->SetMarkerPosition(stm->play, (SLmillisecond)written_duration);
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
    case CUBEB_STREAM_TYPE_SYSTEM_ENFORCED:
      return SL_ANDROID_STREAM_SYSTEM_ENFORCED;
    default:
      return 0xFFFFFFFF;
  }
}
#endif

static void opensl_destroy(cubeb * ctx);

#if defined(__ANDROID__)

static int
get_android_version(void)
{
  char version_string[PROP_VALUE_MAX];

  memset(version_string, 0, PROP_VALUE_MAX);

  int len = __system_property_get("ro.build.version.sdk", version_string);
  if (len <= 0) {
    LOG("Failed to get Android version!\n");
    return len;
  }

  return (int)strtol(version_string, NULL, 10);
}
#endif

/*static*/ int
opensl_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;

#if defined(__ANDROID__)
  int android_version = get_android_version();
  if (android_version > 0 && android_version <= ANDROID_VERSION_GINGERBREAD_MR1) {
    // Don't even attempt to run on Gingerbread and lower
    return CUBEB_ERROR;
  }
#endif

  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &opensl_ops;

  ctx->lib = dlopen("libOpenSLES.so", RTLD_LAZY);
  ctx->libmedia = dlopen("libmedia.so", RTLD_LAZY);
  if (!ctx->lib || !ctx->libmedia) {
    free(ctx);
    return CUBEB_ERROR;
  }

  /* Get the latency, in ms, from AudioFlinger */
  /* status_t AudioSystem::getOutputLatency(uint32_t* latency,
   *                                        audio_stream_type_t streamType) */
  /* First, try the most recent signature. */
  ctx->get_output_latency =
    dlsym(ctx->libmedia, "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t");
  if (!ctx->get_output_latency) {
    /* in case of failure, try the legacy version. */
    /* status_t AudioSystem::getOutputLatency(uint32_t* latency,
     *                                        int streamType) */
    ctx->get_output_latency =
      dlsym(ctx->libmedia, "_ZN7android11AudioSystem16getOutputLatencyEPji");
    if (!ctx->get_output_latency) {
      opensl_destroy(ctx);
      return CUBEB_ERROR;
    }
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
  ctx->SL_IID_VOLUME = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_VOLUME");
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
  res = cubeb_get_sles_engine(&ctx->engObj, 1, opt, 0, NULL, NULL);

  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = cubeb_realize_sles_engine(ctx->engObj);
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

static int
opensl_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  /* https://android.googlesource.com/platform/ndk.git/+/master/docs/opensles/index.html
   * We don't want to deal with JNI here (and we don't have Java on b2g anyways),
   * so we just dlopen the library and get the two symbols we need. */
  int rv;
  void * libmedia;
  uint32_t (*get_primary_output_samplingrate)();
  uint32_t (*get_output_samplingrate)(int * samplingRate, int streamType);
  uint32_t primary_sampling_rate;

  libmedia = dlopen("libmedia.so", RTLD_LAZY);
  if (!libmedia) {
    return CUBEB_ERROR;
  }

  /* uint32_t AudioSystem::getPrimaryOutputSamplingRate(void) */
  get_primary_output_samplingrate =
    dlsym(libmedia, "_ZN7android11AudioSystem28getPrimaryOutputSamplingRateEv");
  if (!get_primary_output_samplingrate) {
    /* fallback to
     * status_t AudioSystem::getOutputSamplingRate(int* samplingRate, int streamType)
     * if we cannot find getPrimaryOutputSamplingRate. */
    get_output_samplingrate =
      dlsym(libmedia, "_ZN7android11AudioSystem21getOutputSamplingRateEPj19audio_stream_type_t");
    if (!get_output_samplingrate) {
      /* Another signature exists, with a int instead of an audio_stream_type_t */
      get_output_samplingrate =
        dlsym(libmedia, "_ZN7android11AudioSystem21getOutputSamplingRateEPii");
      if (!get_output_samplingrate) {
        dlclose(libmedia);
        return CUBEB_ERROR;
      }
    }
  }

  if (get_primary_output_samplingrate) {
    *rate = get_primary_output_samplingrate();
  } else {
    /* We don't really know about the type, here, so we just pass music. */
    rv = get_output_samplingrate((int *) rate, AUDIO_STREAM_TYPE_MUSIC);
    if (rv) {
      dlclose(libmedia);
      return CUBEB_ERROR;
    }
  }

  dlclose(libmedia);

  /* Depending on which method we called above, we can get a zero back, yet have
   * a non-error return value, especially if the audio system is not
   * ready/shutting down (i.e. when we can't get our hand on the AudioFlinger
   * thread). */
  if (*rate == 0) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
opensl_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_ms)
{
  /* https://android.googlesource.com/platform/ndk.git/+/master/docs/opensles/index.html
   * We don't want to deal with JNI here (and we don't have Java on b2g anyways),
   * so we just dlopen the library and get the two symbols we need. */

  int rv;
  void * libmedia;
  size_t (*get_primary_output_frame_count)(void);
  int (*get_output_frame_count)(size_t * frameCount, int streamType);
  uint32_t primary_sampling_rate;
  size_t primary_buffer_size;

  rv = opensl_get_preferred_sample_rate(ctx, &primary_sampling_rate);

  if (rv) {
    return CUBEB_ERROR;
  }

  libmedia = dlopen("libmedia.so", RTLD_LAZY);
  if (!libmedia) {
    return CUBEB_ERROR;
  }

  /* JB variant */
  /* size_t AudioSystem::getPrimaryOutputFrameCount(void) */
  get_primary_output_frame_count =
    dlsym(libmedia, "_ZN7android11AudioSystem26getPrimaryOutputFrameCountEv");
  if (!get_primary_output_frame_count) {
    /* ICS variant */
    /* status_t AudioSystem::getOutputFrameCount(int* frameCount, int streamType) */
    get_output_frame_count =
      dlsym(libmedia, "_ZN7android11AudioSystem19getOutputFrameCountEPii");
    if (!get_output_frame_count) {
      dlclose(libmedia);
      return CUBEB_ERROR;
    }
  }

  if (get_primary_output_frame_count) {
    primary_buffer_size = get_primary_output_frame_count();
  } else {
    if (get_output_frame_count(&primary_buffer_size, params.stream_type) != 0) {
      return CUBEB_ERROR;
    }
  }

  /* To get a fast track in Android's mixer, we need to be at the native
   * samplerate, which is device dependant. Some devices might be able to
   * resample when playing a fast track, but it's pretty rare. */
  *latency_ms = NBUFS * primary_buffer_size / (primary_sampling_rate / 1000);

  dlclose(libmedia);

  return CUBEB_OK;
}

static void
opensl_destroy(cubeb * ctx)
{
  if (ctx->outmixObj)
    (*ctx->outmixObj)->Destroy(ctx->outmixObj);
  if (ctx->engObj)
    cubeb_destroy_sles_engine(&ctx->engObj);
  dlclose(ctx->lib);
  dlclose(ctx->libmedia);
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

  if (stream_params.channels < 1 || stream_params.channels > 32 ||
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

  stm->inputrate = stream_params.rate;
  stm->latency = latency;
  stm->stream_type = stream_params.stream_type;
  stm->framesize = stream_params.channels * sizeof(int16_t);

  int r = pthread_mutex_init(&stm->mutex, NULL);
  assert(r == 0);

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
  const SLInterfaceID ids[] = {ctx->SL_IID_BUFFERQUEUE,
                               ctx->SL_IID_VOLUME,
                               ctx->SL_IID_ANDROIDCONFIGURATION};
  const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
#else
  const SLInterfaceID ids[] = {ctx->SL_IID_BUFFERQUEUE, ctx->SL_IID_VOLUME};
  const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
#endif
  assert(NELEMS(ids) == NELEMS(req));
  SLresult res = (*ctx->eng)->CreateAudioPlayer(ctx->eng, &stm->playerObj,
                                                &source, &sink, NELEMS(ids), ids, req);

  uint32_t preferred_sampling_rate = stm->inputrate;
  // Sample rate not supported? Try again with primary sample rate!
  if (res == SL_RESULT_CONTENT_UNSUPPORTED) {
    if (opensl_get_preferred_sample_rate(ctx, &preferred_sampling_rate)) {
      opensl_stream_destroy(stm);
      return CUBEB_ERROR;
    }

    format.samplesPerSec = preferred_sampling_rate * 1000;
    res = (*ctx->eng)->CreateAudioPlayer(ctx->eng, &stm->playerObj,
                                         &source, &sink, NELEMS(ids), ids, req);
  }

  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  stm->outputrate = preferred_sampling_rate;
  stm->bytespersec = preferred_sampling_rate * stm->framesize;
  stm->queuebuf_len = (stm->bytespersec * latency) / (1000 * NBUFS);
  // round up to the next multiple of stm->framesize, if needed.
  if (stm->queuebuf_len % stm->framesize) {
    stm->queuebuf_len += stm->framesize - (stm->queuebuf_len % stm->framesize);
  }

  stm->resampler = cubeb_resampler_create(stm, stream_params,
                                          preferred_sampling_rate,
                                          data_callback,
                                          stm->queuebuf_len / stm->framesize,
                                          user_ptr,
                                          CUBEB_RESAMPLER_QUALITY_DEFAULT);

  if (!stm->resampler) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  int i;
  for (i = 0; i < NBUFS; i++) {
    stm->queuebuf[i] = malloc(stm->queuebuf_len);
    assert(stm->queuebuf[i]);
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

  res = (*stm->playerObj)->GetInterface(stm->playerObj, ctx->SL_IID_VOLUME,
                                        &stm->volume);

  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  res = (*stm->play)->RegisterCallback(stm->play, play_callback, stm);
  if (res != SL_RESULT_SUCCESS) {
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  res = (*stm->play)->SetCallbackEventsMask(stm->play, (SLuint32)SL_PLAYEVENT_HEADATMARKER);
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
  pthread_mutex_destroy(&stm->mutex);

  cubeb_resampler_destroy(stm->resampler);

  free(stm);
}

static int
opensl_stream_start(cubeb_stream * stm)
{
  /* To refill the queues before starting playback in order to avoid racing
  * with refills started by SetPlayState on OpenSLES ndk threads. */
  bufferqueue_callback(NULL, stm);
  SLresult res = (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_PLAYING);
  if (res != SL_RESULT_SUCCESS)
    return CUBEB_ERROR;
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);
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
  uint64_t samplerate;
  SLresult res;
  int rv;
  uint32_t mixer_latency;

  res = (*stm->play)->GetPosition(stm->play, &msec);
  if (res != SL_RESULT_SUCCESS)
    return CUBEB_ERROR;

  samplerate = stm->inputrate;

  rv = stm->context->get_output_latency(&mixer_latency, stm->stream_type);
  if (rv) {
    return CUBEB_ERROR;
  }

  int64_t compensated_position = samplerate * (msec - mixer_latency) / 1000;
  pthread_mutex_lock(&stm->mutex);
  int64_t maximum_position = stm->written * (int64_t)stm->inputrate / stm->outputrate;
  pthread_mutex_unlock(&stm->mutex);
  assert(maximum_position >= 0);

  if (compensated_position < 0) {
    *position = 0;
  } else if(compensated_position > maximum_position) {
    *position = maximum_position;
  } else {
    *position = compensated_position;
  }
  return CUBEB_OK;
}

int
opensl_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  int rv;
  uint32_t mixer_latency; // The latency returned by AudioFlinger is in ms.

  /* audio_stream_type_t is an int, so this is okay. */
  rv = stm->context->get_output_latency(&mixer_latency, stm->stream_type);
  if (rv) {
    return CUBEB_ERROR;
  }

  *latency = stm->latency * stm->inputrate / 1000 + // OpenSL latency
             mixer_latency * stm->inputrate / 1000; // AudioFlinger latency

  return CUBEB_OK;
}

int
opensl_stream_set_volume(cubeb_stream * stm, float volume)
{
  SLresult res;
  SLmillibel max_level, millibels;

  res = (*stm->volume)->GetMaxVolumeLevel(stm->volume, &max_level);

  if (res != SL_RESULT_SUCCESS) {
    return CUBEB_ERROR;
  }

  /* convert to millibels */
  millibels = lroundf(2000.f * log10f(volume));
  /* clamp to supported range */
  if (millibels > max_level) {
   millibels = max_level;
  }

  res = (*stm->volume)->SetVolumeLevel(stm->volume, millibels);

  if (res != SL_RESULT_SUCCESS) {
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

int
opensl_stream_set_panning(cubeb_stream * stream, float panning)
{
  assert(0 && "not implemented.");

  return CUBEB_OK;
}

static struct cubeb_ops const opensl_ops = {
  .init = opensl_init,
  .get_backend_id = opensl_get_backend_id,
  .get_max_channel_count = opensl_get_max_channel_count,
  .get_min_latency = opensl_get_min_latency,
  .get_preferred_sample_rate = opensl_get_preferred_sample_rate,
  .destroy = opensl_destroy,
  .stream_init = opensl_stream_init,
  .stream_destroy = opensl_stream_destroy,
  .stream_start = opensl_stream_start,
  .stream_stop = opensl_stream_stop,
  .stream_get_position = opensl_stream_get_position,
  .stream_get_latency = opensl_stream_get_latency,
  .stream_set_volume = opensl_stream_set_volume,
  .stream_set_panning = opensl_stream_set_panning
};
