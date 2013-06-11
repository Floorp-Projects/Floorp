/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"

#define NBUFS 4

static struct cubeb_ops const audiounit_ops;

struct cubeb {
  struct cubeb_ops const * ops;
};

struct cubeb_stream {
  cubeb * context;
  AudioUnit unit;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
  AudioStreamBasicDescription sample_spec;
  pthread_mutex_t mutex;
  uint64_t frames_played;
  uint64_t frames_queued;
  int shutdown;
  int draining;
};

static OSStatus
audiounit_output_callback(void * user_ptr, AudioUnitRenderActionFlags * flags,
                          AudioTimeStamp const * tstamp, UInt32 bus, UInt32 nframes,
                          AudioBufferList * bufs)
{
  cubeb_stream * stm;
  unsigned char * buf;
  long got;
  OSStatus r;

  assert(bufs->mNumberBuffers == 1);
  buf = bufs->mBuffers[0].mData;

  stm = user_ptr;

  pthread_mutex_lock(&stm->mutex);

  if (stm->draining || stm->shutdown) {
    pthread_mutex_unlock(&stm->mutex);
    if (stm->draining) {
      r = AudioOutputUnitStop(stm->unit);
      assert(r == 0);
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
    }
    return noErr;
  }

  pthread_mutex_unlock(&stm->mutex);
  got = stm->data_callback(stm, stm->user_ptr, buf, nframes);
  pthread_mutex_lock(&stm->mutex);
  if (got < 0) {
    // XXX handle this case.
    assert(false);
    pthread_mutex_unlock(&stm->mutex);
    return noErr;
  }

  if ((UInt32) got < nframes) {
    size_t got_bytes = got * stm->sample_spec.mBytesPerFrame;
    size_t rem_bytes = (nframes - got) * stm->sample_spec.mBytesPerFrame;

    stm->draining = 1;

    memset(buf + got_bytes, 0, rem_bytes);
  }

  stm->frames_played = stm->frames_queued;
  stm->frames_queued += got;
  pthread_mutex_unlock(&stm->mutex);

  return noErr;
}

/*static*/ int
audiounit_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;

  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &audiounit_ops;

  *context = ctx;

  return CUBEB_OK;
}

static char const *
audiounit_get_backend_id(cubeb * ctx)
{
  return "audiounit";
}


int
audiounit_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  UInt32 size;
  OSStatus r;
  AudioDeviceID output_device_id;
  AudioStreamBasicDescription stream_format;
  AudioObjectPropertyAddress output_device_address = {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  AudioObjectPropertyAddress stream_format_address = {
    kAudioDevicePropertyStreamFormat,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  assert(ctx && max_channels);

  /* Get the output device. */
  size = sizeof(output_device_id);

  r = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &output_device_address,
                                 0,
                                 0,
                                 &size,
                                 &output_device_id);

  if (r != noErr) {
    return CUBEB_ERROR;
  }

  size = sizeof(stream_format);

  r = AudioObjectGetPropertyData(output_device_id,
                                 &stream_format_address,
                                 0,
                                 NULL,
                                 &size,
                                 &stream_format);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  *max_channels = stream_format.mChannelsPerFrame;

  return CUBEB_OK;
}

static void
audiounit_destroy(cubeb * ctx)
{
  free(ctx);
}

static void audiounit_stream_destroy(cubeb_stream * stm);

static int
audiounit_stream_init(cubeb * context, cubeb_stream ** stream, char const * stream_name,
                      cubeb_stream_params stream_params, unsigned int latency,
                      cubeb_data_callback data_callback, cubeb_state_callback state_callback,
                      void * user_ptr)
{
  AudioStreamBasicDescription ss;
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  ComponentDescription desc;
  Component comp;
#else
  AudioComponentDescription desc;
  AudioComponent comp;
#endif
  cubeb_stream * stm;
  AURenderCallbackStruct input;
  unsigned int buffer_size;
  OSStatus r;

  assert(context);
  *stream = NULL;

  memset(&ss, 0, sizeof(ss));
  ss.mFormatFlags = 0;

  switch (stream_params.format) {
  case CUBEB_SAMPLE_S16LE:
    ss.mBitsPerChannel = 16;
    ss.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
    break;
  case CUBEB_SAMPLE_S16BE:
    ss.mBitsPerChannel = 16;
    ss.mFormatFlags |= kAudioFormatFlagIsSignedInteger |
                       kAudioFormatFlagIsBigEndian;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    ss.mBitsPerChannel = 32;
    ss.mFormatFlags |= kAudioFormatFlagIsFloat;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    ss.mBitsPerChannel = 32;
    ss.mFormatFlags |= kAudioFormatFlagIsFloat |
                       kAudioFormatFlagIsBigEndian;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  ss.mFormatID = kAudioFormatLinearPCM;
  ss.mFormatFlags |= kLinearPCMFormatFlagIsPacked;
  ss.mSampleRate = stream_params.rate;
  ss.mChannelsPerFrame = stream_params.channels;

  ss.mBytesPerFrame = (ss.mBitsPerChannel / 8) * ss.mChannelsPerFrame;
  ss.mFramesPerPacket = 1;
  ss.mBytesPerPacket = ss.mBytesPerFrame * ss.mFramesPerPacket;

  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  comp = FindNextComponent(NULL, &desc);
#else
  comp = AudioComponentFindNext(NULL, &desc);
#endif
  assert(comp);

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;

  stm->sample_spec = ss;

  r = pthread_mutex_init(&stm->mutex, NULL);
  assert(r == 0);

  stm->frames_played = 0;
  stm->frames_queued = 0;

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  r = OpenAComponent(comp, &stm->unit);
#else
  r = AudioComponentInstanceNew(comp, &stm->unit);
#endif
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  input.inputProc = audiounit_output_callback;
  input.inputProcRefCon = stm;
  r = AudioUnitSetProperty(stm->unit, kAudioUnitProperty_SetRenderCallback,
                           kAudioUnitScope_Global, 0, &input, sizeof(input));
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  r = AudioUnitSetProperty(stm->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                           0, &ss, sizeof(ss));
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  buffer_size = ss.mSampleRate / 1000.0 * latency * ss.mBytesPerFrame / NBUFS;
  if (buffer_size % ss.mBytesPerFrame != 0) {
    buffer_size += ss.mBytesPerFrame - (buffer_size % ss.mBytesPerFrame);
  }
  assert(buffer_size % ss.mBytesPerFrame == 0);

  r = AudioUnitInitialize(stm->unit);
  if (r != 0) {
    audiounit_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
audiounit_stream_destroy(cubeb_stream * stm)
{
  int r;

  stm->shutdown = 1;

  if (stm->unit) {
    AudioOutputUnitStop(stm->unit);
    AudioUnitUninitialize(stm->unit);
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    CloseComponent(stm->unit);
#else
    AudioComponentInstanceDispose(stm->unit);
#endif
  }

  r = pthread_mutex_destroy(&stm->mutex);
  assert(r == 0);

  free(stm);
}

static int
audiounit_stream_start(cubeb_stream * stm)
{
  OSStatus r;
  r = AudioOutputUnitStart(stm->unit);
  assert(r == 0);
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);
  return CUBEB_OK;
}

static int
audiounit_stream_stop(cubeb_stream * stm)
{
  OSStatus r;
  r = AudioOutputUnitStop(stm->unit);
  assert(r == 0);
  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  return CUBEB_OK;
}

static int
audiounit_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  pthread_mutex_lock(&stm->mutex);
  *position = stm->frames_played;
  pthread_mutex_unlock(&stm->mutex);
  return CUBEB_OK;
}

static struct cubeb_ops const audiounit_ops = {
  .init = audiounit_init,
  .get_backend_id = audiounit_get_backend_id,
  .get_max_channel_count = audiounit_get_max_channel_count,
  .destroy = audiounit_destroy,
  .stream_init = audiounit_stream_init,
  .stream_destroy = audiounit_stream_destroy,
  .stream_start = audiounit_stream_start,
  .stream_stop = audiounit_stream_stop,
  .stream_get_position = audiounit_stream_get_position
};
