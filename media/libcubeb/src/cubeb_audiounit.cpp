/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG

#include <TargetConditionals.h>
#include <assert.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdlib.h>
#include <AudioUnit/AudioUnit.h>
#if !TARGET_OS_IPHONE
#include <AvailabilityMacros.h>
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/HostTime.h>
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb_panner.h"
#if !TARGET_OS_IPHONE
#include "cubeb_osx_run_loop.h"
#endif
#include "cubeb_resampler.h"
#include "cubeb_ring_array.h"
#include "cubeb_utils.h"
#include <algorithm>
#include <atomic>

#if !defined(kCFCoreFoundationVersionNumber10_7)
/* From CoreFoundation CFBase.h */
#define kCFCoreFoundationVersionNumber10_7 635.00
#endif

#if !TARGET_OS_IPHONE && MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#define AudioComponent Component
#define AudioComponentDescription ComponentDescription
#define AudioComponentFindNext FindNextComponent
#define AudioComponentInstanceNew OpenAComponent
#define AudioComponentInstanceDispose CloseComponent
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101000
typedef UInt32  AudioFormatFlags;
#endif

#define CUBEB_STREAM_MAX 8

#define AU_OUT_BUS    0
#define AU_IN_BUS     1

#define PRINT_ERROR_CODE(str, r) do {                                \
  LOG("System call failed: %s (rv: %d)", str, r);                    \
} while(0)

/* Testing empirically, some headsets report a minimal latency that is very
 * low, but this does not work in practice. Lie and say the minimum is 256
 * frames. */
const uint32_t SAFE_MIN_LATENCY_FRAMES = 256;
const uint32_t SAFE_MAX_LATENCY_FRAMES = 512;

void audiounit_stream_stop_internal(cubeb_stream * stm);
void audiounit_stream_start_internal(cubeb_stream * stm);
static void close_audiounit_stream(cubeb_stream * stm);
static int setup_audiounit_stream(cubeb_stream * stm);

extern cubeb_ops const audiounit_ops;

struct cubeb {
  cubeb_ops const * ops;
  owned_critical_section mutex;
  std::atomic<int> active_streams;
  int limit_streams;
  cubeb_device_collection_changed_callback collection_changed_callback;
  void * collection_changed_user_ptr;
  /* Differentiate input from output devices. */
  cubeb_device_type collection_changed_devtype;
  uint32_t devtype_device_count;
  AudioObjectID * devtype_device_array;
};

class auto_array_wrapper
{
public:
  explicit auto_array_wrapper(auto_array<float> * ar)
  : float_ar(ar)
  , short_ar(nullptr)
  {assert((float_ar && !short_ar) || (!float_ar && short_ar));}

  explicit auto_array_wrapper(auto_array<short> * ar)
  : float_ar(nullptr)
  , short_ar(ar)
  {assert((float_ar && !short_ar) || (!float_ar && short_ar));}

  ~auto_array_wrapper() {
    auto_lock l(lock);
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    delete float_ar;
    delete short_ar;
  }

  void push(void * elements, size_t length){
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    auto_lock l(lock);
    if (float_ar)
      return float_ar->push(static_cast<float*>(elements), length);
    return short_ar->push(static_cast<short*>(elements), length);
  }

  size_t length() {
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    auto_lock l(lock);
    if (float_ar)
      return float_ar->length();
    return short_ar->length();
  }

  void push_silence(size_t length) {
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    auto_lock l(lock);
    if (float_ar)
      return float_ar->push_silence(length);
    return short_ar->push_silence(length);
  }

  bool pop(void * elements, size_t length) {
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    auto_lock l(lock);
    if (float_ar)
      return float_ar->pop(static_cast<float*>(elements), length);
    return short_ar->pop(static_cast<short*>(elements), length);
  }

  void * data() {
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    auto_lock l(lock);
    if (float_ar)
      return float_ar->data();
    return short_ar->data();
  }

  void clear() {
    assert((float_ar && !short_ar) || (!float_ar && short_ar));
    auto_lock l(lock);
    if (float_ar) {
      float_ar->clear();
    } else {
      short_ar->clear();
    }
  }

private:
  auto_array<float> * float_ar;
  auto_array<short> * short_ar;
  owned_critical_section lock;
};

struct cubeb_stream {
  cubeb * context;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  cubeb_device_changed_callback device_changed_callback;
  /* Stream creation parameters */
  cubeb_stream_params input_stream_params;
  cubeb_stream_params output_stream_params;
  cubeb_devid input_device;
  cubeb_devid output_device;
  /* User pointer of data_callback */
  void * user_ptr;
  /* Format descriptions */
  AudioStreamBasicDescription input_desc;
  AudioStreamBasicDescription output_desc;
  /* I/O AudioUnits */
  AudioUnit input_unit;
  AudioUnit output_unit;
  /* Sample rate of input device*/
  Float64 input_hw_rate;
  owned_critical_section mutex;
  /* Hold the input samples in every
   * input callback iteration */
  auto_array_wrapper * input_linear_buffer;
  /* Frames on input buffer */
  std::atomic<uint32_t> input_buffer_frames;
  /* Frame counters */
  uint64_t frames_played;
  uint64_t frames_queued;
  std::atomic<int64_t> frames_read;
  std::atomic<bool> shutdown;
  std::atomic<bool> draining;
  /* Latency requested by the user. */
  uint64_t latency_frames;
  std::atomic<uint64_t> current_latency_frames;
  uint64_t hw_latency_frames;
  std::atomic<float> panning;
  cubeb_resampler * resampler;
  /* This is the number of output callback we got in a row. This is usually one,
   * but can be two when the input and output rate are different, and more when
   * a device has been plugged or unplugged, as there can be some time before
   * the device is ready. */
  std::atomic<int> output_callback_in_a_row;
  /* This is true if a device change callback is currently running.  */
  std::atomic<bool> switching_device;
};

bool has_input(cubeb_stream * stm)
{
  return stm->input_stream_params.rate != 0;
}

bool has_output(cubeb_stream * stm)
{
  return stm->output_stream_params.rate != 0;
}

#if TARGET_OS_IPHONE
typedef UInt32 AudioDeviceID;
typedef UInt32 AudioObjectID;

#define AudioGetCurrentHostTime mach_absolute_time

uint64_t
AudioConvertHostTimeToNanos(uint64_t host_time)
{
  static struct mach_timebase_info timebase_info;
  static bool initialized = false;
  if (!initialized) {
    mach_timebase_info(&timebase_info);
    initialized = true;
  }

  long double answer = host_time;
  if (timebase_info.numer != timebase_info.denom) {
    answer *= timebase_info.numer;
    answer /= timebase_info.denom;
  }
  return (uint64_t)answer;
}
#endif

static int64_t
audiotimestamp_to_latency(AudioTimeStamp const * tstamp, cubeb_stream * stream)
{
  if (!(tstamp->mFlags & kAudioTimeStampHostTimeValid)) {
    return 0;
  }

  uint64_t pres = AudioConvertHostTimeToNanos(tstamp->mHostTime);
  uint64_t now = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

  return ((pres - now) * stream->output_desc.mSampleRate) / 1000000000LL;
}

static void
audiounit_make_silent(AudioBuffer * ioData)
{
  memset(ioData->mData, 0, ioData->mDataByteSize);
}

static OSStatus
audiounit_render_input(cubeb_stream * stm,
                       AudioUnitRenderActionFlags * flags,
                       AudioTimeStamp const * tstamp,
                       UInt32 bus,
                       UInt32 input_frames)
{
  /* Create the AudioBufferList to store input. */
  AudioBufferList input_buffer_list;
  input_buffer_list.mBuffers[0].mDataByteSize =
      stm->input_desc.mBytesPerFrame * input_frames;
  input_buffer_list.mBuffers[0].mData = nullptr;
  input_buffer_list.mBuffers[0].mNumberChannels = stm->input_desc.mChannelsPerFrame;
  input_buffer_list.mNumberBuffers = 1;

  /* Render input samples */
  OSStatus r = AudioUnitRender(stm->input_unit,
                               flags,
                               tstamp,
                               bus,
                               input_frames,
                               &input_buffer_list);

  if (r != noErr) {
    PRINT_ERROR_CODE("AudioUnitRender", r);
    audiounit_make_silent(&input_buffer_list.mBuffers[0]);
    return r;
  }

  /* Copy input data in linear buffer. */
  stm->input_linear_buffer->push(input_buffer_list.mBuffers[0].mData,
                                 input_frames * stm->input_desc.mChannelsPerFrame);

  LOGV("(%p) input:  buffers %d, size %d, channels %d, frames %d.",
       stm, input_buffer_list.mNumberBuffers,
       input_buffer_list.mBuffers[0].mDataByteSize,
       input_buffer_list.mBuffers[0].mNumberChannels,
       input_frames);

  /* Advance input frame counter. */
  assert(input_frames > 0);
  stm->frames_read += input_frames;

  return noErr;
}

static OSStatus
audiounit_input_callback(void * user_ptr,
                         AudioUnitRenderActionFlags * flags,
                         AudioTimeStamp const * tstamp,
                         UInt32 bus,
                         UInt32 input_frames,
                         AudioBufferList * /* bufs */)
{
  cubeb_stream * stm = static_cast<cubeb_stream *>(user_ptr);
  long outframes;

  assert(stm->input_unit != NULL);
  assert(AU_IN_BUS == bus);

  if (stm->shutdown) {
    LOG("(%p) input shutdown", stm);
    return noErr;
  }

  OSStatus r = audiounit_render_input(stm, flags, tstamp, bus, input_frames);
  if (r != noErr) {
    return r;
  }

  // Full Duplex. We'll call data_callback in the AudioUnit output callback.
  if (stm->output_unit != NULL) {
    // This happens when we're finally getting a new input callback after having
    // switched device, we can clear the input buffer now, only keeping the data
    // we just got.
    if (stm->output_callback_in_a_row > 2) {
      stm->input_linear_buffer->pop(
          nullptr,
          stm->input_linear_buffer->length() -
          input_frames * stm->input_stream_params.channels);
    }

    stm->output_callback_in_a_row = 0;

    return noErr;
  }

  /* Input only. Call the user callback through resampler.
     Resampler will deliver input buffer in the correct rate. */
  assert(input_frames <= stm->input_linear_buffer->length() / stm->input_desc.mChannelsPerFrame);
  long total_input_frames = stm->input_linear_buffer->length() / stm->input_desc.mChannelsPerFrame;
  outframes = cubeb_resampler_fill(stm->resampler,
                                   stm->input_linear_buffer->data(),
                                   &total_input_frames,
                                   NULL,
                                   0);
  // Reset input buffer
  stm->input_linear_buffer->clear();

  if (outframes < 0 || outframes != input_frames) {
    stm->shutdown = true;
    return noErr;
  }

  return noErr;
}

static OSStatus
audiounit_output_callback(void * user_ptr,
                          AudioUnitRenderActionFlags * /* flags */,
                          AudioTimeStamp const * tstamp,
                          UInt32 bus,
                          UInt32 output_frames,
                          AudioBufferList * outBufferList)
{
  assert(AU_OUT_BUS == bus);
  assert(outBufferList->mNumberBuffers == 1);

  cubeb_stream * stm = static_cast<cubeb_stream *>(user_ptr);

  stm->output_callback_in_a_row++;

  LOGV("(%p) output: buffers %d, size %d, channels %d, frames %d.",
       stm, outBufferList->mNumberBuffers,
       outBufferList->mBuffers[0].mDataByteSize,
       outBufferList->mBuffers[0].mNumberChannels, output_frames);

  long outframes = 0, input_frames = 0;
  void * output_buffer = NULL, * input_buffer = NULL;

  if (stm->shutdown) {
    LOG("(%p) output shutdown.", stm);
    audiounit_make_silent(&outBufferList->mBuffers[0]);
    return noErr;
  }

  stm->current_latency_frames = audiotimestamp_to_latency(tstamp, stm);
  if (stm->draining) {
    OSStatus r = AudioOutputUnitStop(stm->output_unit);
    assert(r == 0);
    if (stm->input_unit) {
      r = AudioOutputUnitStop(stm->input_unit);
      assert(r == 0);
    }
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
    audiounit_make_silent(&outBufferList->mBuffers[0]);
    return noErr;
  }
  /* Get output buffer. */
  output_buffer = outBufferList->mBuffers[0].mData;
  /* If Full duplex get also input buffer */
  if (stm->input_unit != NULL) {
    /* If the output callback came first and this is a duplex stream, we need to
     * fill in some additional silence in the resampler.
     * Otherwise, if we had more than two callback in a row, or we're currently
     * switching, we add some silence as well to compensate for the fact that
     * we're lacking some input data. */
    if (stm->frames_read == 0 ||
        (stm->input_linear_buffer->length() == 0 &&
        (stm->output_callback_in_a_row > 2 || stm->switching_device))) {
      LOG("(%p) Output callback came first send silent.", stm);
      stm->input_linear_buffer->push_silence(stm->input_buffer_frames *
                                             stm->input_desc.mChannelsPerFrame);
    }
    // The input buffer
    input_buffer = stm->input_linear_buffer->data();
    // Number of input frames in the buffer
    input_frames = stm->input_linear_buffer->length() / stm->input_desc.mChannelsPerFrame;
  }

  /* Call user callback through resampler. */
  outframes = cubeb_resampler_fill(stm->resampler,
                                   input_buffer,
                                   input_buffer ? &input_frames : NULL,
                                   output_buffer,
                                   output_frames);

  if (input_buffer) {
    stm->input_linear_buffer->pop(nullptr, input_frames * stm->input_desc.mChannelsPerFrame);
  }

  if (outframes < 0) {
    stm->shutdown = true;
    return noErr;
  }

  size_t outbpf = stm->output_desc.mBytesPerFrame;
  stm->draining = outframes < output_frames;
  stm->frames_played = stm->frames_queued;
  stm->frames_queued += outframes;

  AudioFormatFlags outaff = stm->output_desc.mFormatFlags;
  float panning = (stm->output_desc.mChannelsPerFrame == 2) ?
      stm->panning.load(std::memory_order_relaxed) : 0.0f;

  /* Post process output samples. */
  if (stm->draining) {
    /* Clear missing frames (silence) */
    memset((uint8_t*)output_buffer + outframes * outbpf, 0, (output_frames - outframes) * outbpf);
  }
  /* Pan stereo. */
  if (panning != 0.0f) {
    if (outaff & kAudioFormatFlagIsFloat) {
      cubeb_pan_stereo_buffer_float((float*)output_buffer, outframes, panning);
    } else if (outaff & kAudioFormatFlagIsSignedInteger) {
      cubeb_pan_stereo_buffer_int((short*)output_buffer, outframes, panning);
    }
  }
  return noErr;
}

extern "C" {
int
audiounit_init(cubeb ** context, char const * /* context_name */)
{
  cubeb * ctx;

  *context = NULL;

  ctx = (cubeb *)calloc(1, sizeof(cubeb));
  assert(ctx);
  // Placement new to call the ctors of cubeb members.
  new (ctx) cubeb();

  ctx->ops = &audiounit_ops;

  ctx->active_streams = 0;

  ctx->limit_streams = kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber10_7;
#if !TARGET_OS_IPHONE
  cubeb_set_coreaudio_notification_runloop();
#endif

  *context = ctx;

  return CUBEB_OK;
}
}

static char const *
audiounit_get_backend_id(cubeb * /* ctx */)
{
  return "audiounit";
}

#if !TARGET_OS_IPHONE
static int
audiounit_get_output_device_id(AudioDeviceID * device_id)
{
  UInt32 size;
  OSStatus r;
  AudioObjectPropertyAddress output_device_address = {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  size = sizeof(*device_id);

  r = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &output_device_address,
                                 0,
                                 NULL,
                                 &size,
                                 device_id);
  if (r != noErr) {
    PRINT_ERROR_CODE("output_device_id", r);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
audiounit_get_input_device_id(AudioDeviceID * device_id)
{
  UInt32 size;
  OSStatus r;
  AudioObjectPropertyAddress input_device_address = {
    kAudioHardwarePropertyDefaultInputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  size = sizeof(*device_id);

  r = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &input_device_address,
                                 0,
                                 NULL,
                                 &size,
                                 device_id);
  if (r != noErr) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static OSStatus
audiounit_property_listener_callback(AudioObjectID /* id */, UInt32 address_count,
                                     const AudioObjectPropertyAddress * addresses,
                                     void * user)
{
  cubeb_stream * stm = (cubeb_stream*) user;
  int rv;
  bool was_running = false;

  stm->switching_device = true;

  // Note if the stream was running or not
  was_running = !stm->shutdown;

  LOG("(%p) Audio device changed, %d events.", stm, address_count);
  if (g_log_level) {
    for (UInt32 i = 0; i < address_count; i++) {
      switch(addresses[i].mSelector) {
        case kAudioHardwarePropertyDefaultOutputDevice:
          LOG("(%p) %d mSelector == kAudioHardwarePropertyDefaultOutputDevice", stm, i);
          break;
        case kAudioHardwarePropertyDefaultInputDevice:
          LOG("(%p) %d mSelector == kAudioHardwarePropertyDefaultInputDevice", stm, i);
          break;
        case kAudioDevicePropertyDataSource:
          LOG("(%p) %d mSelector == kAudioHardwarePropertyDataSource", stm, i);
          break;
      }
    }
  }

  for (UInt32 i = 0; i < address_count; i++) {
    switch(addresses[i].mSelector) {
    case kAudioHardwarePropertyDefaultOutputDevice:
    case kAudioHardwarePropertyDefaultInputDevice:
      /* fall through */
    case kAudioDevicePropertyDataSource: {
        auto_lock lock(stm->mutex);
        if (stm->device_changed_callback) {
          stm->device_changed_callback(stm->user_ptr);
        }
        break;
      }
    }
  }

  // This means the callback won't be called again.
  audiounit_stream_stop_internal(stm);

  {
    auto_lock lock(stm->mutex);
    close_audiounit_stream(stm);
    rv = setup_audiounit_stream(stm);
    if (rv != CUBEB_OK) {
      LOG("(%p) Could not reopen a stream after switching.", stm);
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
      return noErr;
    }

    stm->frames_read = 0;

    // If the stream was running, start it again.
    if (was_running) {
      audiounit_stream_start_internal(stm);
    }
  }

  stm->switching_device = false;

  return noErr;
}

OSStatus
audiounit_add_listener(cubeb_stream * stm, AudioDeviceID id, AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope, AudioObjectPropertyListenerProc listener)
{
  AudioObjectPropertyAddress address = {
      selector,
      scope,
      kAudioObjectPropertyElementMaster
  };

  return AudioObjectAddPropertyListener(id, &address, listener, stm);
}

OSStatus
audiounit_remove_listener(cubeb_stream * stm, AudioDeviceID id,
                          AudioObjectPropertySelector selector,
                          AudioObjectPropertyScope scope,
                          AudioObjectPropertyListenerProc listener)
{
  AudioObjectPropertyAddress address = {
      selector,
      scope,
      kAudioObjectPropertyElementMaster
  };

  return AudioObjectRemovePropertyListener(id, &address, listener, stm);
}

static int
audiounit_install_device_changed_callback(cubeb_stream * stm)
{
  OSStatus r;

  if (stm->output_unit) {
    /* This event will notify us when the data source on the same device changes,
     * for example when the user plugs in a normal (non-usb) headset in the
     * headphone jack. */
    AudioDeviceID output_dev_id;
    r = audiounit_get_output_device_id(&output_dev_id);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    r = audiounit_add_listener(stm, output_dev_id, kAudioDevicePropertyDataSource,
        kAudioDevicePropertyScopeOutput, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    /* This event will notify us when the default audio device changes,
     * for example when the user plugs in a USB headset and the system chooses it
     * automatically as the default, or when another device is chosen in the
     * dropdown list. */
    r = audiounit_add_listener(stm, kAudioObjectSystemObject, kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }
  }

  if (stm->input_unit) {
    /* This event will notify us when the data source on the input device changes. */
    AudioDeviceID input_dev_id;
    r = audiounit_get_input_device_id(&input_dev_id);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    r = audiounit_add_listener(stm, input_dev_id, kAudioDevicePropertyDataSource,
        kAudioDevicePropertyScopeInput, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    /* This event will notify us when the default input device changes. */
    r = audiounit_add_listener(stm, kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }
  }

  return CUBEB_OK;
}

static int
audiounit_uninstall_device_changed_callback(cubeb_stream * stm)
{
  OSStatus r;

  if (stm->output_unit) {
    AudioDeviceID output_dev_id;
    r = audiounit_get_output_device_id(&output_dev_id);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    r = audiounit_remove_listener(stm, output_dev_id, kAudioDevicePropertyDataSource,
        kAudioDevicePropertyScopeOutput, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    r = audiounit_remove_listener(stm, kAudioObjectSystemObject, kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }
  }

  if (stm->input_unit) {
    AudioDeviceID input_dev_id;
    r = audiounit_get_input_device_id(&input_dev_id);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    r = audiounit_remove_listener(stm, input_dev_id, kAudioDevicePropertyDataSource,
        kAudioDevicePropertyScopeInput, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    r = audiounit_remove_listener(stm, kAudioObjectSystemObject, kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal, &audiounit_property_listener_callback);
    if (r != noErr) {
      return CUBEB_ERROR;
    }
  }
  return CUBEB_OK;
}

/* Get the acceptable buffer size (in frames) that this device can work with. */
static int
audiounit_get_acceptable_latency_range(AudioValueRange * latency_range)
{
  UInt32 size;
  OSStatus r;
  AudioDeviceID output_device_id;
  AudioObjectPropertyAddress output_device_buffer_size_range = {
    kAudioDevicePropertyBufferFrameSizeRange,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    LOG("Could not get default output device id.");
    return CUBEB_ERROR;
  }

  /* Get the buffer size range this device supports */
  size = sizeof(*latency_range);

  r = AudioObjectGetPropertyData(output_device_id,
                                 &output_device_buffer_size_range,
                                 0,
                                 NULL,
                                 &size,
                                 latency_range);
  if (r != noErr) {
    PRINT_ERROR_CODE("AudioObjectGetPropertyData/buffer size range", r);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}
#endif /* !TARGET_OS_IPHONE */

static AudioObjectID
audiounit_get_default_device_id(cubeb_device_type type)
{
  AudioObjectPropertyAddress adr = { 0, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
  AudioDeviceID devid;
  UInt32 size;

  if (type == CUBEB_DEVICE_TYPE_OUTPUT) {
    adr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
  } else if (type == CUBEB_DEVICE_TYPE_INPUT) {
    adr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
  } else {
    return kAudioObjectUnknown;
  }

  size = sizeof(AudioDeviceID);
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &adr, 0, NULL, &size, &devid) != noErr) {
    return kAudioObjectUnknown;
  }

  return devid;
}

int
audiounit_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
#if TARGET_OS_IPHONE
  //TODO: [[AVAudioSession sharedInstance] maximumOutputNumberOfChannels]
  *max_channels = 2;
#else
  UInt32 size;
  OSStatus r;
  AudioDeviceID output_device_id;
  AudioStreamBasicDescription stream_format;
  AudioObjectPropertyAddress stream_format_address = {
    kAudioDevicePropertyStreamFormat,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  assert(ctx && max_channels);

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
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
    PRINT_ERROR_CODE("AudioObjectPropertyAddress/StreamFormat", r);
    return CUBEB_ERROR;
  }

  *max_channels = stream_format.mChannelsPerFrame;
#endif
  return CUBEB_OK;
}

static int
audiounit_get_min_latency(cubeb * /* ctx */,
                          cubeb_stream_params /* params */,
                          uint32_t * latency_frames)
{
#if TARGET_OS_IPHONE
  //TODO: [[AVAudioSession sharedInstance] inputLatency]
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  AudioValueRange latency_range;
  if (audiounit_get_acceptable_latency_range(&latency_range) != CUBEB_OK) {
    LOG("Could not get acceptable latency range.");
    return CUBEB_ERROR;
  }

  *latency_frames = std::max<uint32_t>(latency_range.mMinimum,
                                       SAFE_MIN_LATENCY_FRAMES);
#endif

  return CUBEB_OK;
}

static int
audiounit_get_preferred_sample_rate(cubeb * /* ctx */, uint32_t * rate)
{
#if TARGET_OS_IPHONE
  //TODO
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  UInt32 size;
  OSStatus r;
  Float64 fsamplerate;
  AudioDeviceID output_device_id;
  AudioObjectPropertyAddress samplerate_address = {
    kAudioDevicePropertyNominalSampleRate,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  size = sizeof(fsamplerate);
  r = AudioObjectGetPropertyData(output_device_id,
                                 &samplerate_address,
                                 0,
                                 NULL,
                                 &size,
                                 &fsamplerate);

  if (r != noErr) {
    return CUBEB_ERROR;
  }

  *rate = static_cast<uint32_t>(fsamplerate);
#endif
  return CUBEB_OK;
}

static OSStatus audiounit_remove_device_listener(cubeb * context);

static void
audiounit_destroy(cubeb * ctx)
{
  // Disabling this assert for bug 1083664 -- we seem to leak a stream
  // assert(ctx->active_streams == 0);

  /* Unregister the callback if necessary. */
  if(ctx->collection_changed_callback) {
    auto_lock lock(ctx->mutex);
    audiounit_remove_device_listener(ctx);
  }

  ctx->~cubeb();
  free(ctx);
}

static void audiounit_stream_destroy(cubeb_stream * stm);

static int
audio_stream_desc_init(AudioStreamBasicDescription * ss,
                       const cubeb_stream_params * stream_params)
{
  switch (stream_params->format) {
  case CUBEB_SAMPLE_S16LE:
    ss->mBitsPerChannel = 16;
    ss->mFormatFlags = kAudioFormatFlagIsSignedInteger;
    break;
  case CUBEB_SAMPLE_S16BE:
    ss->mBitsPerChannel = 16;
    ss->mFormatFlags = kAudioFormatFlagIsSignedInteger |
      kAudioFormatFlagIsBigEndian;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    ss->mBitsPerChannel = 32;
    ss->mFormatFlags = kAudioFormatFlagIsFloat;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    ss->mBitsPerChannel = 32;
    ss->mFormatFlags = kAudioFormatFlagIsFloat |
      kAudioFormatFlagIsBigEndian;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  ss->mFormatID = kAudioFormatLinearPCM;
  ss->mFormatFlags |= kLinearPCMFormatFlagIsPacked;
  ss->mSampleRate = stream_params->rate;
  ss->mChannelsPerFrame = stream_params->channels;

  ss->mBytesPerFrame = (ss->mBitsPerChannel / 8) * ss->mChannelsPerFrame;
  ss->mFramesPerPacket = 1;
  ss->mBytesPerPacket = ss->mBytesPerFrame * ss->mFramesPerPacket;

  ss->mReserved = 0;

  return CUBEB_OK;
}

static int
audiounit_create_unit(AudioUnit * unit,
                      bool is_input,
                      const cubeb_stream_params * /* stream_params */,
                      cubeb_devid device)
{
  AudioComponentDescription desc;
  AudioComponent comp;
  UInt32 enable;
  AudioDeviceID devid;
  OSStatus rv;

  desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
  bool use_default_output = false;
  desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
  // Use the DefaultOutputUnit for output when no device is specified
  // so we retain automatic output device switching when the default
  // changes.  Once we have complete support for device notifications
  // and switching, we can use the AUHAL for everything.
  bool use_default_output = device == NULL && !is_input;
  if (use_default_output) {
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  } else {
    desc.componentSubType = kAudioUnitSubType_HALOutput;
  }
#endif
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;
  comp = AudioComponentFindNext(NULL, &desc);
  if (comp == NULL) {
    LOG("Could not find matching audio hardware.");
    return CUBEB_ERROR;
  }

  rv = AudioComponentInstanceNew(comp, unit);
  if (rv != noErr) {
    PRINT_ERROR_CODE("AudioComponentInstanceNew", rv);
    return CUBEB_ERROR;
  }

  if (!use_default_output) {
    enable = 1;
    rv = AudioUnitSetProperty(*unit, kAudioOutputUnitProperty_EnableIO,
                              is_input ? kAudioUnitScope_Input : kAudioUnitScope_Output,
                              is_input ? AU_IN_BUS : AU_OUT_BUS, &enable, sizeof(UInt32));
    if (rv != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/kAudioOutputUnitProperty_EnableIO", rv);
      return CUBEB_ERROR;
    }

    enable = 0;
    rv = AudioUnitSetProperty(*unit, kAudioOutputUnitProperty_EnableIO,
                              is_input ? kAudioUnitScope_Output : kAudioUnitScope_Input,
                              is_input ? AU_OUT_BUS : AU_IN_BUS, &enable, sizeof(UInt32));
    if (rv != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/kAudioOutputUnitProperty_EnableIO", rv);
      return CUBEB_ERROR;
    }

    if (device == NULL) {
      assert(is_input);
      devid = audiounit_get_default_device_id(CUBEB_DEVICE_TYPE_INPUT);
    } else {
      devid = reinterpret_cast<intptr_t>(device);
    }
    int err = AudioUnitSetProperty(*unit, kAudioOutputUnitProperty_CurrentDevice,
                                   kAudioUnitScope_Global,
                                   is_input ? AU_IN_BUS : AU_OUT_BUS,
                                   &devid, sizeof(AudioDeviceID));
    if (err != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/kAudioOutputUnitProperty_CurrentDevice", rv);
      return CUBEB_ERROR;
    }
  }

  return CUBEB_OK;
}

static int
audiounit_init_input_linear_buffer(cubeb_stream * stream, uint32_t capacity)
{
  if (stream->input_desc.mFormatFlags & kAudioFormatFlagIsSignedInteger) {
    stream->input_linear_buffer = new auto_array_wrapper(
        new auto_array<short>(capacity *
                              stream->input_buffer_frames *
                              stream->input_desc.mChannelsPerFrame) );
  } else {
    stream->input_linear_buffer = new auto_array_wrapper(
        new auto_array<float>(capacity *
                              stream->input_buffer_frames *
                              stream->input_desc.mChannelsPerFrame) );
  }

  if (!stream->input_linear_buffer) {
    return CUBEB_ERROR;
  }

  assert(stream->input_linear_buffer->length() == 0);

  // Pre-buffer silence if needed
  if (capacity != 1) {
    size_t silence_size = stream->input_buffer_frames *
                          stream->input_desc.mChannelsPerFrame;
    stream->input_linear_buffer->push_silence(silence_size);

    assert(stream->input_linear_buffer->length() == silence_size);
  }

  return CUBEB_OK;
}

static void
audiounit_destroy_input_linear_buffer(cubeb_stream * stream)
{
  delete stream->input_linear_buffer;
}

static uint32_t
audiounit_clamp_latency(cubeb_stream * stm,
                              uint32_t latency_frames)
{
  // For the 1st stream set anything within safe min-max
  assert(stm->context->active_streams > 0);
  if (stm->context->active_streams == 1) {
    return std::max(std::min<uint32_t>(latency_frames, SAFE_MAX_LATENCY_FRAMES),
                    SAFE_MIN_LATENCY_FRAMES);
  }

  // If more than one stream operates in parallel
  // allow only lower values of latency
  int r;
  UInt32 output_buffer_size = 0;
  UInt32 size = sizeof(output_buffer_size);
  if (stm->output_unit) {
    r = AudioUnitGetProperty(stm->output_unit,
                            kAudioDevicePropertyBufferFrameSize,
                            kAudioUnitScope_Output,
                            AU_OUT_BUS,
                            &output_buffer_size,
                            &size);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetProperty/output/kAudioDevicePropertyBufferFrameSize", r);
      return 0;
    }

    output_buffer_size = std::max(std::min<uint32_t>(output_buffer_size, SAFE_MAX_LATENCY_FRAMES),
                                  SAFE_MIN_LATENCY_FRAMES);
  }

  UInt32 input_buffer_size = 0;
  if (stm->input_unit) {
    r = AudioUnitGetProperty(stm->input_unit,
                            kAudioDevicePropertyBufferFrameSize,
                            kAudioUnitScope_Input,
                            AU_IN_BUS,
                            &input_buffer_size,
                            &size);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetProperty/input/kAudioDevicePropertyBufferFrameSize", r);
      return 0;
    }

    input_buffer_size = std::max(std::min<uint32_t>(input_buffer_size, SAFE_MAX_LATENCY_FRAMES),
                                 SAFE_MIN_LATENCY_FRAMES);
  }

  // Every following active streams can only set smaller latency
  UInt32 upper_latency_limit = 0;
  if (input_buffer_size != 0 && output_buffer_size != 0) {
    upper_latency_limit = std::min<uint32_t>(input_buffer_size, output_buffer_size);
  } else if (input_buffer_size != 0) {
    upper_latency_limit = input_buffer_size;
  } else if (output_buffer_size != 0) {
    upper_latency_limit = output_buffer_size;
  } else {
    upper_latency_limit = SAFE_MAX_LATENCY_FRAMES;
  }

  return std::max(std::min<uint32_t>(latency_frames, upper_latency_limit),
                  SAFE_MIN_LATENCY_FRAMES);
}

static int
setup_audiounit_stream(cubeb_stream * stm)
{
  stm->mutex.assert_current_thread_owns();

  int r;
  AURenderCallbackStruct aurcbs_in;
  AURenderCallbackStruct aurcbs_out;
  UInt32 size;

  if (has_input(stm)) {
    LOG("(%p) Opening input side, rate: %u", stm, stm->input_stream_params.rate);
    r = audiounit_create_unit(&stm->input_unit, true,
                              &stm->input_stream_params,
                              stm->input_device);
    if (r != CUBEB_OK) {
      LOG("(%p) AudioUnit creation for input failed.", stm);
      return r;
    }
  }

  if (has_output(stm)) {
    LOG("(%p) Opening output side, rate: %u", stm, stm->input_stream_params.rate);
    r = audiounit_create_unit(&stm->output_unit, false,
                              &stm->output_stream_params,
                              stm->output_device);
    if (r != CUBEB_OK) {
      LOG("(%p) AudioUnit creation for output failed.", stm);
      return r;
    }
  }

  /* Setup Input Stream! */
  if (has_input(stm)) {
    /* Get input device sample rate. */
    AudioStreamBasicDescription input_hw_desc;
    size = sizeof(AudioStreamBasicDescription);
    r = AudioUnitGetProperty(stm->input_unit,
                            kAudioUnitProperty_StreamFormat,
                            kAudioUnitScope_Input,
                            AU_IN_BUS,
                            &input_hw_desc,
                            &size);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetProperty/input/kAudioUnitProperty_StreamFormat", r);
      return CUBEB_ERROR;
    }
    stm->input_hw_rate = input_hw_desc.mSampleRate;

    /* Set format description according to the input params. */
    r = audio_stream_desc_init(&stm->input_desc, &stm->input_stream_params);
    if (r != CUBEB_OK) {
      LOG("(%p) Setting format description for input failed.", stm);
      return r;
    }

    // Use latency to set buffer size
    stm->input_buffer_frames = stm->latency_frames;
    LOG("(%p) Input buffer frame count %u.", stm, unsigned(stm->input_buffer_frames));
    r = AudioUnitSetProperty(stm->input_unit,
                             kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Output,
                             AU_IN_BUS,
                             &stm->input_buffer_frames,
                             sizeof(UInt32));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/input/kAudioDevicePropertyBufferFrameSize", r);
      return CUBEB_ERROR;
    }

    AudioStreamBasicDescription src_desc = stm->input_desc;
    /* Input AudioUnit must be configured with device's sample rate.
       we will resample inside input callback. */
    src_desc.mSampleRate = stm->input_hw_rate;

    r = AudioUnitSetProperty(stm->input_unit,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output,
                             AU_IN_BUS,
                             &src_desc,
                             sizeof(AudioStreamBasicDescription));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/input/kAudioUnitProperty_StreamFormat", r);
      return CUBEB_ERROR;
    }

    /* Frames per buffer in the input callback. */
    r = AudioUnitSetProperty(stm->input_unit,
                             kAudioUnitProperty_MaximumFramesPerSlice,
                             kAudioUnitScope_Output,
                             AU_IN_BUS,
                             &stm->input_buffer_frames,
                             sizeof(UInt32));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/input/kAudioUnitProperty_MaximumFramesPerSlice", r);
      return CUBEB_ERROR;
    }

    // Input only capacity
    unsigned int array_capacity = 1;
    if (has_output(stm)) {
      // Full-duplex increase capacity
      array_capacity = 8;
    }
    if (audiounit_init_input_linear_buffer(stm, array_capacity) != CUBEB_OK) {
      return CUBEB_ERROR;
    }

    assert(stm->input_unit != NULL);
    aurcbs_in.inputProc = audiounit_input_callback;
    aurcbs_in.inputProcRefCon = stm;

    r = AudioUnitSetProperty(stm->input_unit,
                             kAudioOutputUnitProperty_SetInputCallback,
                             kAudioUnitScope_Global,
                             AU_OUT_BUS,
                             &aurcbs_in,
                             sizeof(aurcbs_in));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/input/kAudioOutputUnitProperty_SetInputCallback", r);
      return CUBEB_ERROR;
    }
    LOG("(%p) Input audiounit init successfully.", stm);
  }

  /* Setup Output Stream! */
  if (has_output(stm)) {
    r = audio_stream_desc_init(&stm->output_desc, &stm->output_stream_params);
    if (r != CUBEB_OK) {
      LOG("(%p) Could not initialize the audio stream description.", stm);
      return r;
    }

    /* Get output device sample rate. */
    AudioStreamBasicDescription output_hw_desc;
    size = sizeof(AudioStreamBasicDescription);
    memset(&output_hw_desc, 0, size);
    r = AudioUnitGetProperty(stm->output_unit,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output,
                             AU_OUT_BUS,
                             &output_hw_desc,
                             &size);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetProperty/output/tkAudioUnitProperty_StreamFormat", r);
      return CUBEB_ERROR;
    }

    r = AudioUnitSetProperty(stm->output_unit,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input,
                             AU_OUT_BUS,
                             &stm->output_desc,
                             sizeof(AudioStreamBasicDescription));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/output/kAudioUnitProperty_StreamFormat", r);
      return CUBEB_ERROR;
    }

    // Use latency to calculate buffer size
    uint32_t output_buffer_frames = stm->latency_frames;
    LOG("(%p) Output buffer frame count %u.", stm, output_buffer_frames);
    r = AudioUnitSetProperty(stm->output_unit,
                             kAudioDevicePropertyBufferFrameSize,
                             kAudioUnitScope_Input,
                             AU_OUT_BUS,
                             &output_buffer_frames,
                             sizeof(output_buffer_frames));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/output/kAudioDevicePropertyBufferFrameSize", r);
      return CUBEB_ERROR;
    }

    assert(stm->output_unit != NULL);
    aurcbs_out.inputProc = audiounit_output_callback;
    aurcbs_out.inputProcRefCon = stm;
    r = AudioUnitSetProperty(stm->output_unit,
                             kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Global,
                             AU_OUT_BUS,
                             &aurcbs_out,
                             sizeof(aurcbs_out));
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitSetProperty/output/kAudioUnitProperty_SetRenderCallback", r);
      return CUBEB_ERROR;
    }
    LOG("(%p) Output audiounit init successfully.", stm);
  }

  // Setting the latency doesn't work well for USB headsets (eg. plantronics).
  // Keep the default latency for now.
#if 0
  buffer_size = latency;

  /* Get the range of latency this particular device can work with, and clamp
   * the requested latency to this acceptable range. */
#if !TARGET_OS_IPHONE
  if (audiounit_get_acceptable_latency_range(&latency_range) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  if (buffer_size < (unsigned int) latency_range.mMinimum) {
    buffer_size = (unsigned int) latency_range.mMinimum;
  } else if (buffer_size > (unsigned int) latency_range.mMaximum) {
    buffer_size = (unsigned int) latency_range.mMaximum;
  }

  /**
   * Get the default buffer size. If our latency request is below the default,
   * set it. Otherwise, use the default latency.
   **/
  size = sizeof(default_buffer_size);
  if (AudioUnitGetProperty(stm->output_unit, kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Output, 0, &default_buffer_size, &size) != 0) {
    return CUBEB_ERROR;
  }

  if (buffer_size < default_buffer_size) {
    /* Set the maximum number of frame that the render callback will ask for,
     * effectively setting the latency of the stream. This is process-wide. */
    if (AudioUnitSetProperty(stm->output_unit, kAudioDevicePropertyBufferFrameSize,
          kAudioUnitScope_Output, 0, &buffer_size, sizeof(buffer_size)) != 0) {
      return CUBEB_ERROR;
    }
  }
#else  // TARGET_OS_IPHONE
  //TODO: [[AVAudioSession sharedInstance] inputLatency]
  // http://stackoverflow.com/questions/13157523/kaudiodevicepropertybufferframesize-replacement-for-ios
#endif
#endif

  /* We use a resampler because input AudioUnit operates
   * reliable only in the capture device sample rate.
   * Resampler will convert it to the user sample rate
   * and deliver it to the callback. */
  uint32_t target_sample_rate;
  if (has_input(stm)) {
    target_sample_rate = stm->input_stream_params.rate;
  } else {
    assert(has_output(stm));
    target_sample_rate = stm->output_stream_params.rate;
  }

  cubeb_stream_params input_unconverted_params;
  if (has_input(stm)) {
    input_unconverted_params = stm->input_stream_params;
    /* Use the rate of the input device. */
    input_unconverted_params.rate = stm->input_hw_rate;
  }

  /* Create resampler. Output params are unchanged
   * because we do not need conversion on the output. */
  stm->resampler = cubeb_resampler_create(stm,
                                          has_input(stm) ? &input_unconverted_params : NULL,
                                          has_output(stm) ? &stm->output_stream_params : NULL,
                                          target_sample_rate,
                                          stm->data_callback,
                                          stm->user_ptr,
                                          CUBEB_RESAMPLER_QUALITY_DESKTOP);
  if (!stm->resampler) {
    LOG("(%p) Could not create resampler.", stm);
    return CUBEB_ERROR;
  }

  if (stm->input_unit != NULL) {
    r = AudioUnitInitialize(stm->input_unit);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitInitialize/input", r);
      return CUBEB_ERROR;
    }
  }

  if (stm->output_unit != NULL) {
    r = AudioUnitInitialize(stm->output_unit);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitInitialize/output", r);
      return CUBEB_ERROR;
    }
  }
  return CUBEB_OK;
}

static int
audiounit_stream_init(cubeb * context,
                      cubeb_stream ** stream,
                      char const * /* stream_name */,
                      cubeb_devid input_device,
                      cubeb_stream_params * input_stream_params,
                      cubeb_devid output_device,
                      cubeb_stream_params * output_stream_params,
                      unsigned int latency_frames,
                      cubeb_data_callback data_callback,
                      cubeb_state_callback state_callback,
                      void * user_ptr)
{
  cubeb_stream * stm;
  int r;

  assert(context);
  *stream = NULL;

  if (context->limit_streams && context->active_streams >= CUBEB_STREAM_MAX) {
    LOG("Reached the stream limit of %d", CUBEB_STREAM_MAX);
    return CUBEB_ERROR;
  }
  context->active_streams += 1;

  stm = (cubeb_stream *) calloc(1, sizeof(cubeb_stream));
  assert(stm);
  // Placement new to call the ctors of cubeb_stream members.
  new (stm) cubeb_stream();

  /* These could be different in the future if we have both
   * full-duplex stream and different devices for input vs output. */
  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->device_changed_callback = NULL;
  if (input_stream_params) {
    stm->input_stream_params = *input_stream_params;
    stm->input_device = input_device;
  }
  if (output_stream_params) {
    stm->output_stream_params = *output_stream_params;
    stm->output_device = output_device;
  }

  /* Init data members where necessary */
  stm->hw_latency_frames = UINT64_MAX;

  /* Silently clamp the latency down to the platform default, because we
   * synthetize the clock from the callbacks, and we want the clock to update
   * often. */
  stm->latency_frames = audiounit_clamp_latency(stm, latency_frames);
  assert(latency_frames > 0);

  stm->switching_device = false;

  {
    // It's not critical to lock here, because no other thread has been started
    // yet, but it allows to assert that the lock has been taken in
    // `setup_audiounit_stream`.
    auto_lock lock(stm->mutex);
    r = setup_audiounit_stream(stm);
  }

  if (r != CUBEB_OK) {
    LOG("(%p) Could not setup the audiounit stream.", stm);
    audiounit_stream_destroy(stm);
    return r;
  }

  r = audiounit_install_device_changed_callback(stm);
  if (r != CUBEB_OK) {
    LOG("(%p) Could not install the device change callback.", stm);
    return r;
  }

  *stream = stm;
  LOG("Cubeb stream (%p) init successful.", stm);
  return CUBEB_OK;
}

static void
close_audiounit_stream(cubeb_stream * stm)
{
  stm->mutex.assert_current_thread_owns();
  if (stm->input_unit) {
    AudioUnitUninitialize(stm->input_unit);
    AudioComponentInstanceDispose(stm->input_unit);
  }

  audiounit_destroy_input_linear_buffer(stm);

  if (stm->output_unit) {
    AudioUnitUninitialize(stm->output_unit);
    AudioComponentInstanceDispose(stm->output_unit);
  }

  cubeb_resampler_destroy(stm->resampler);
}

static void
audiounit_stream_destroy(cubeb_stream * stm)
{
  stm->shutdown = true;

  audiounit_stream_stop_internal(stm);

  {
    auto_lock lock(stm->mutex);
    close_audiounit_stream(stm);
  }

#if !TARGET_OS_IPHONE
  int r = audiounit_uninstall_device_changed_callback(stm);
  if (r != CUBEB_OK) {
    LOG("(%p) Could not uninstall the device changed callback", stm);
  }
#endif

  assert(stm->context->active_streams >= 1);
  stm->context->active_streams -= 1;

  stm->~cubeb_stream();
  free(stm);
}

void
audiounit_stream_start_internal(cubeb_stream * stm)
{
  OSStatus r;
  if (stm->input_unit != NULL) {
    r = AudioOutputUnitStart(stm->input_unit);
    assert(r == 0);
  }
  if (stm->output_unit != NULL) {
    r = AudioOutputUnitStart(stm->output_unit);
    assert(r == 0);
  }
}

static int
audiounit_stream_start(cubeb_stream * stm)
{
  stm->shutdown = false;
  stm->draining = false;

  audiounit_stream_start_internal(stm);

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  LOG("Cubeb stream (%p) started successfully.", stm);
  return CUBEB_OK;
}

void
audiounit_stream_stop_internal(cubeb_stream * stm)
{
  OSStatus r;
  if (stm->input_unit != NULL) {
    r = AudioOutputUnitStop(stm->input_unit);
    assert(r == 0);
  }
  if (stm->output_unit != NULL) {
    r = AudioOutputUnitStop(stm->output_unit);
    assert(r == 0);
  }
}

static int
audiounit_stream_stop(cubeb_stream * stm)
{
  stm->shutdown = true;

  audiounit_stream_stop_internal(stm);

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);

  LOG("Cubeb stream (%p) stopped successfully.", stm);
  return CUBEB_OK;
}

static int
audiounit_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  auto_lock lock(stm->mutex);

  *position = stm->frames_played;
  return CUBEB_OK;
}

int
audiounit_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
#if TARGET_OS_IPHONE
  //TODO
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  auto_lock lock(stm->mutex);
  if (stm->hw_latency_frames == UINT64_MAX) {
    UInt32 size;
    uint32_t device_latency_frames, device_safety_offset;
    double unit_latency_sec;
    AudioDeviceID output_device_id;
    OSStatus r;
    AudioObjectPropertyAddress latency_address = {
      kAudioDevicePropertyLatency,
      kAudioDevicePropertyScopeOutput,
      kAudioObjectPropertyElementMaster
    };
    AudioObjectPropertyAddress safety_offset_address = {
      kAudioDevicePropertySafetyOffset,
      kAudioDevicePropertyScopeOutput,
      kAudioObjectPropertyElementMaster
    };

    r = audiounit_get_output_device_id(&output_device_id);
    if (r != noErr) {
      return CUBEB_ERROR;
    }

    size = sizeof(unit_latency_sec);
    r = AudioUnitGetProperty(stm->output_unit,
                             kAudioUnitProperty_Latency,
                             kAudioUnitScope_Global,
                             0,
                             &unit_latency_sec,
                             &size);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetProperty/kAudioUnitProperty_Latency", r);
      return CUBEB_ERROR;
    }

    size = sizeof(device_latency_frames);
    r = AudioObjectGetPropertyData(output_device_id,
                                   &latency_address,
                                   0,
                                   NULL,
                                   &size,
                                   &device_latency_frames);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetPropertyData/latency_frames", r);
      return CUBEB_ERROR;
    }

    size = sizeof(device_safety_offset);
    r = AudioObjectGetPropertyData(output_device_id,
                                   &safety_offset_address,
                                   0,
                                   NULL,
                                   &size,
                                   &device_safety_offset);
    if (r != noErr) {
      PRINT_ERROR_CODE("AudioUnitGetPropertyData/safety_offset", r);
      return CUBEB_ERROR;
    }

    /* This part is fixed and depend on the stream parameter and the hardware. */
    stm->hw_latency_frames =
      static_cast<uint32_t>(unit_latency_sec * stm->output_desc.mSampleRate)
      + device_latency_frames
      + device_safety_offset;
  }

  *latency = stm->hw_latency_frames + stm->current_latency_frames;

  return CUBEB_OK;
#endif
}

int audiounit_stream_set_volume(cubeb_stream * stm, float volume)
{
  OSStatus r;

  r = AudioUnitSetParameter(stm->output_unit,
                            kHALOutputParam_Volume,
                            kAudioUnitScope_Global,
                            0, volume, 0);

  if (r != noErr) {
    PRINT_ERROR_CODE("AudioUnitSetParameter/kHALOutputParam_Volume", r);
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

int audiounit_stream_set_panning(cubeb_stream * stm, float panning)
{
  if (stm->output_desc.mChannelsPerFrame > 2) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  stm->panning.store(panning, std::memory_order_relaxed);
  return CUBEB_OK;
}

int audiounit_stream_get_current_device(cubeb_stream * stm,
                                        cubeb_device ** const  device)
{
#if TARGET_OS_IPHONE
  //TODO
  return CUBEB_ERROR_NOT_SUPPORTED;
#else
  OSStatus r;
  UInt32 size;
  UInt32 data;
  char strdata[4];
  AudioDeviceID output_device_id;
  AudioDeviceID input_device_id;

  AudioObjectPropertyAddress datasource_address = {
    kAudioDevicePropertyDataSource,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMaster
  };

  AudioObjectPropertyAddress datasource_address_input = {
    kAudioDevicePropertyDataSource,
    kAudioDevicePropertyScopeInput,
    kAudioObjectPropertyElementMaster
  };

  *device = NULL;

  if (audiounit_get_output_device_id(&output_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  *device = new cubeb_device;
  if (!*device) {
    return CUBEB_ERROR;
  }
  PodZero(*device, 1);

  size = sizeof(UInt32);
  /* This fails with some USB headset, so simply return an empty string. */
  r = AudioObjectGetPropertyData(output_device_id,
                                 &datasource_address,
                                 0, NULL, &size, &data);
  if (r != noErr) {
    size = 0;
    data = 0;
  }

  (*device)->output_name = new char[size + 1];
  if (!(*device)->output_name) {
    return CUBEB_ERROR;
  }

  // Turn the four chars packed into a uint32 into a string
  strdata[0] = (char)(data >> 24);
  strdata[1] = (char)(data >> 16);
  strdata[2] = (char)(data >> 8);
  strdata[3] = (char)(data);

  memcpy((*device)->output_name, strdata, size);
  (*device)->output_name[size] = '\0';

  if (audiounit_get_input_device_id(&input_device_id) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  size = sizeof(UInt32);
  r = AudioObjectGetPropertyData(input_device_id, &datasource_address_input, 0, NULL, &size, &data);
  if (r != noErr) {
    LOG("(%p) Error when getting device !", stm);
    size = 0;
    data = 0;
  }

  (*device)->input_name = new char[size + 1];
  if (!(*device)->input_name) {
    return CUBEB_ERROR;
  }

  // Turn the four chars packed into a uint32 into a string
  strdata[0] = (char)(data >> 24);
  strdata[1] = (char)(data >> 16);
  strdata[2] = (char)(data >> 8);
  strdata[3] = (char)(data);

  memcpy((*device)->input_name, strdata, size);
  (*device)->input_name[size] = '\0';

  return CUBEB_OK;
#endif
}

int audiounit_stream_device_destroy(cubeb_stream * /* stream */,
                                    cubeb_device * device)
{
  delete [] device->output_name;
  delete [] device->input_name;
  delete device;
  return CUBEB_OK;
}

int audiounit_stream_register_device_changed_callback(cubeb_stream * stream,
                                                      cubeb_device_changed_callback device_changed_callback)
{
  /* Note: second register without unregister first causes 'nope' error.
   * Current implementation requires unregister before register a new cb. */
  assert(!stream->device_changed_callback);

  auto_lock lock(stream->mutex);

  stream->device_changed_callback = device_changed_callback;

  return CUBEB_OK;
}

static OSStatus
audiounit_get_devices(AudioObjectID ** devices, uint32_t * count)
{
  OSStatus ret;
  UInt32 size = 0;
  AudioObjectPropertyAddress adr = { kAudioHardwarePropertyDevices,
                                     kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMaster };

  ret = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &adr, 0, NULL, &size);
  if (ret != noErr) {
    return ret;
  }

  *count = static_cast<uint32_t>(size / sizeof(AudioObjectID));
  if (size >= sizeof(AudioObjectID)) {
    if (*devices != NULL) {
      delete [] (*devices);
    }
    *devices = new AudioObjectID[*count];
    PodZero(*devices, *count);

    ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &adr, 0, NULL, &size, (void *)*devices);
    if (ret != noErr) {
      delete [] (*devices);
      *devices = NULL;
    }
  } else {
    *devices = NULL;
    ret = -1;
  }

  return ret;
}

static char *
audiounit_strref_to_cstr_utf8(CFStringRef strref)
{
  CFIndex len, size;
  char * ret;
  if (strref == NULL) {
    return NULL;
  }

  len = CFStringGetLength(strref);
  size = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8);
  ret = static_cast<char *>(malloc(size));

  if (!CFStringGetCString(strref, ret, size, kCFStringEncodingUTF8)) {
    free(ret);
    ret = NULL;
  }

  return ret;
}

static uint32_t
audiounit_get_channel_count(AudioObjectID devid, AudioObjectPropertyScope scope)
{
  AudioObjectPropertyAddress adr = { 0, scope, kAudioObjectPropertyElementMaster };
  UInt32 size = 0;
  uint32_t i, ret = 0;

  adr.mSelector = kAudioDevicePropertyStreamConfiguration;

  if (AudioObjectGetPropertyDataSize(devid, &adr, 0, NULL, &size) == noErr && size > 0) {
    AudioBufferList * list = static_cast<AudioBufferList *>(alloca(size));
    if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, list) == noErr) {
      for (i = 0; i < list->mNumberBuffers; i++)
        ret += list->mBuffers[i].mNumberChannels;
    }
  }

  return ret;
}

static void
audiounit_get_available_samplerate(AudioObjectID devid, AudioObjectPropertyScope scope,
                                   uint32_t * min, uint32_t * max, uint32_t * def)
{
  AudioObjectPropertyAddress adr = { 0, scope, kAudioObjectPropertyElementMaster };

  adr.mSelector = kAudioDevicePropertyNominalSampleRate;
  if (AudioObjectHasProperty(devid, &adr)) {
    UInt32 size = sizeof(Float64);
    Float64 fvalue = 0.0;
    if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &fvalue) == noErr) {
      *def = fvalue;
    }
  }

  adr.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
  UInt32 size = 0;
  AudioValueRange range;
  if (AudioObjectHasProperty(devid, &adr) &&
      AudioObjectGetPropertyDataSize(devid, &adr, 0, NULL, &size) == noErr) {
    uint32_t i, count = size / sizeof(AudioValueRange);
    AudioValueRange * ranges = new AudioValueRange[count];
    range.mMinimum = 9999999999.0;
    range.mMaximum = 0.0;
    if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, ranges) == noErr) {
      for (i = 0; i < count; i++) {
        if (ranges[i].mMaximum > range.mMaximum)
          range.mMaximum = ranges[i].mMaximum;
        if (ranges[i].mMinimum < range.mMinimum)
          range.mMinimum = ranges[i].mMinimum;
      }
    }
    delete [] ranges;
    *max = static_cast<uint32_t>(range.mMaximum);
    *min = static_cast<uint32_t>(range.mMinimum);
  } else {
    *min = *max = 0;
  }

}

static UInt32
audiounit_get_device_presentation_latency(AudioObjectID devid, AudioObjectPropertyScope scope)
{
  AudioObjectPropertyAddress adr = { 0, scope, kAudioObjectPropertyElementMaster };
  UInt32 size, dev, stream = 0, offset;
  AudioStreamID sid[1];

  adr.mSelector = kAudioDevicePropertyLatency;
  size = sizeof(UInt32);
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &dev) != noErr) {
    dev = 0;
  }

  adr.mSelector = kAudioDevicePropertyStreams;
  size = sizeof(sid);
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, sid) == noErr) {
    adr.mSelector = kAudioStreamPropertyLatency;
    size = sizeof(UInt32);
    AudioObjectGetPropertyData(sid[0], &adr, 0, NULL, &size, &stream);
  }

  adr.mSelector = kAudioDevicePropertySafetyOffset;
  size = sizeof(UInt32);
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &offset) != noErr) {
    offset = 0;
  }

  return dev + stream + offset;
}

static cubeb_device_info *
audiounit_create_device_from_hwdev(AudioObjectID devid, cubeb_device_type type)
{
  AudioObjectPropertyAddress adr = { 0, 0, kAudioObjectPropertyElementMaster };
  UInt32 size, ch, latency;
  cubeb_device_info * ret;
  CFStringRef str = NULL;
  AudioValueRange range;

  if (type == CUBEB_DEVICE_TYPE_OUTPUT) {
    adr.mScope = kAudioDevicePropertyScopeOutput;
  } else if (type == CUBEB_DEVICE_TYPE_INPUT) {
    adr.mScope = kAudioDevicePropertyScopeInput;
  } else {
    return NULL;
  }

  ch = audiounit_get_channel_count(devid, adr.mScope);
  if (ch == 0) {
    return NULL;
  }

  ret = new cubeb_device_info;
  PodZero(ret, 1);

  size = sizeof(CFStringRef);
  adr.mSelector = kAudioDevicePropertyDeviceUID;
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &str) == noErr && str != NULL) {
    ret->device_id = audiounit_strref_to_cstr_utf8(str);
    ret->devid = (cubeb_devid)(size_t)devid;
    ret->group_id = strdup(ret->device_id);
    CFRelease(str);
  }

  size = sizeof(CFStringRef);
  adr.mSelector = kAudioObjectPropertyName;
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &str) == noErr && str != NULL) {
    UInt32 ds;
    size = sizeof(UInt32);
    adr.mSelector = kAudioDevicePropertyDataSource;
    if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &ds) == noErr) {
      CFStringRef dsname;
      AudioValueTranslation trl = { &ds, sizeof(ds), &dsname, sizeof(dsname) };
      adr.mSelector = kAudioDevicePropertyDataSourceNameForIDCFString;
      size = sizeof(AudioValueTranslation);
      // If there is a datasource for this device, use it instead of the device
      // name.
      if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &trl) == noErr) {
        CFRelease(str);
        str = dsname;
      }
    }

    ret->friendly_name = audiounit_strref_to_cstr_utf8(str);
    CFRelease(str);
  }

  size = sizeof(CFStringRef);
  adr.mSelector = kAudioObjectPropertyManufacturer;
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &str) == noErr && str != NULL) {
    ret->vendor_name = audiounit_strref_to_cstr_utf8(str);
    CFRelease(str);
  }

  ret->type = type;
  ret->state = CUBEB_DEVICE_STATE_ENABLED;
  ret->preferred = (devid == audiounit_get_default_device_id(type)) ?
    CUBEB_DEVICE_PREF_ALL : CUBEB_DEVICE_PREF_NONE;

  ret->max_channels = ch;
  ret->format = (cubeb_device_fmt)CUBEB_DEVICE_FMT_ALL; /* CoreAudio supports All! */
  /* kAudioFormatFlagsAudioUnitCanonical is deprecated, prefer floating point */
  ret->default_format = CUBEB_DEVICE_FMT_F32NE;
  audiounit_get_available_samplerate(devid, adr.mScope,
      &ret->min_rate, &ret->max_rate, &ret->default_rate);

  latency = audiounit_get_device_presentation_latency(devid, adr.mScope);

  adr.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
  size = sizeof(AudioValueRange);
  if (AudioObjectGetPropertyData(devid, &adr, 0, NULL, &size, &range) == noErr) {
    ret->latency_lo = latency + range.mMinimum;
    ret->latency_hi = latency + range.mMaximum;
  } else {
    ret->latency_lo = 10 * ret->default_rate / 1000;  /* Default to  10ms */
    ret->latency_hi = 100 * ret->default_rate / 1000; /* Default to 100ms */
  }

  return ret;
}

static int
audiounit_enumerate_devices(cubeb * /* context */, cubeb_device_type type,
                            cubeb_device_collection ** collection)
{
  AudioObjectID * hwdevs = NULL;
  uint32_t i, hwdevcount = 0;
  OSStatus err;

  if ((err = audiounit_get_devices(&hwdevs, &hwdevcount)) != noErr) {
    return CUBEB_ERROR;
  }

  *collection = static_cast<cubeb_device_collection *>(malloc(sizeof(cubeb_device_collection) +
      sizeof(cubeb_device_info*) * (hwdevcount > 0 ? hwdevcount - 1 : 0)));
  (*collection)->count = 0;

  if (hwdevcount > 0) {
    cubeb_device_info * cur;

    if (type & CUBEB_DEVICE_TYPE_OUTPUT) {
      for (i = 0; i < hwdevcount; i++) {
        if ((cur = audiounit_create_device_from_hwdev(hwdevs[i], CUBEB_DEVICE_TYPE_OUTPUT)) != NULL)
          (*collection)->device[(*collection)->count++] = cur;
      }
    }

    if (type & CUBEB_DEVICE_TYPE_INPUT) {
      for (i = 0; i < hwdevcount; i++) {
        if ((cur = audiounit_create_device_from_hwdev(hwdevs[i], CUBEB_DEVICE_TYPE_INPUT)) != NULL)
          (*collection)->device[(*collection)->count++] = cur;
      }
    }
  }

  delete [] hwdevs;

  return CUBEB_OK;
}

/* qsort compare method. */
int compare_devid(const void * a, const void * b)
{
  return (*(AudioObjectID*)a - *(AudioObjectID*)b);
}

static uint32_t
audiounit_get_devices_of_type(cubeb_device_type devtype, AudioObjectID ** devid_array)
{
  assert(devid_array == NULL || *devid_array == NULL);

  AudioObjectPropertyAddress adr = { kAudioHardwarePropertyDevices,
                                     kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMaster };
  UInt32 size = 0;
  OSStatus ret = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &adr, 0, NULL, &size);
  if (ret != noErr) {
    return 0;
  }
  /* Total number of input and output devices. */
  uint32_t count = (uint32_t)(size / sizeof(AudioObjectID));

  AudioObjectID devices[count];
  ret = AudioObjectGetPropertyData(kAudioObjectSystemObject, &adr, 0, NULL, &size, &devices);
  if (ret != noErr) {
    return 0;
  }
  /* Expected sorted but did not find anything in the docs. */
  qsort(devices, count, sizeof(AudioObjectID), compare_devid);

  if (devtype == (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) {
    if (devid_array) {
      *devid_array = new AudioObjectID[count];
      assert(*devid_array);
      memcpy(*devid_array, &devices, count * sizeof(AudioObjectID));
    }
    return count;
  }

  AudioObjectPropertyScope scope = (devtype == CUBEB_DEVICE_TYPE_INPUT) ?
                                         kAudioDevicePropertyScopeInput :
                                         kAudioDevicePropertyScopeOutput;

  uint32_t dev_count = 0;
  AudioObjectID devices_in_scope[count];
  for(uint32_t i = 0; i < count; ++i) {
    /* For device in the given scope channel must be > 0. */
    if (audiounit_get_channel_count(devices[i], scope) > 0) {
      devices_in_scope[dev_count] = devices[i];
      ++dev_count;
    }
  }

  if (devid_array && dev_count > 0) {
    *devid_array = new AudioObjectID[dev_count];
    assert(*devid_array);
    memcpy(*devid_array, &devices_in_scope, dev_count * sizeof(AudioObjectID));
  }
  return dev_count;
}

static uint32_t
audiounit_equal_arrays(AudioObjectID * left, AudioObjectID * right, uint32_t size)
{
  /* Expected sorted arrays. */
  for (uint32_t i = 0; i < size; ++i) {
    if (left[i] != right[i]) {
      return 0;
    }
  }
  return 1;
}

static OSStatus
audiounit_collection_changed_callback(AudioObjectID /* inObjectID */,
                                      UInt32 /* inNumberAddresses */,
                                      const AudioObjectPropertyAddress * /* inAddresses */,
                                      void * inClientData)
{
  cubeb * context = static_cast<cubeb *>(inClientData);
  auto_lock lock(context->mutex);

  if (context->collection_changed_callback == NULL) {
    /* Listener removed while waiting in mutex, abort. */
    return noErr;
  }

  /* Differentiate input from output changes. */
  if (context->collection_changed_devtype == CUBEB_DEVICE_TYPE_INPUT ||
      context->collection_changed_devtype == CUBEB_DEVICE_TYPE_OUTPUT) {
    AudioObjectID * devices = NULL;
    uint32_t new_number_of_devices = audiounit_get_devices_of_type(context->collection_changed_devtype, &devices);
    /* When count is the same examine the devid for the case of coalescing. */
    if (context->devtype_device_count == new_number_of_devices &&
        audiounit_equal_arrays(devices, context->devtype_device_array, new_number_of_devices)) {
      /* Device changed for the other scope, ignore. */
      delete [] devices;
      return noErr;
    }
    /* Device on desired scope changed, reset counter and array. */
    context->devtype_device_count = new_number_of_devices;
    /* Free the old array before replace. */
    delete [] context->devtype_device_array;
    context->devtype_device_array = devices;
  }

  context->collection_changed_callback(context, context->collection_changed_user_ptr);
  return noErr;
}

static OSStatus
audiounit_add_device_listener(cubeb * context,
                              cubeb_device_type devtype,
                              cubeb_device_collection_changed_callback collection_changed_callback,
                              void * user_ptr)
{
  /* Note: second register without unregister first causes 'nope' error.
   * Current implementation requires unregister before register a new cb. */
  assert(context->collection_changed_callback == NULL);

  AudioObjectPropertyAddress devAddr;
  devAddr.mSelector = kAudioHardwarePropertyDevices;
  devAddr.mScope = kAudioObjectPropertyScopeGlobal;
  devAddr.mElement = kAudioObjectPropertyElementMaster;

  OSStatus ret = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                                                &devAddr,
                                                audiounit_collection_changed_callback,
                                                context);
  if (ret == noErr) {
    /* Expected zero after unregister. */
    assert(context->devtype_device_count == 0);
    assert(context->devtype_device_array == NULL);
    /* Listener works for input and output.
     * When requested one of them we need to differentiate. */
    if (devtype == CUBEB_DEVICE_TYPE_INPUT ||
        devtype == CUBEB_DEVICE_TYPE_OUTPUT) {
      /* Used to differentiate input from output device changes. */
      context->devtype_device_count = audiounit_get_devices_of_type(devtype, &context->devtype_device_array);
    }
    context->collection_changed_devtype = devtype;
    context->collection_changed_callback = collection_changed_callback;
    context->collection_changed_user_ptr = user_ptr;
  }
  return ret;
}

static OSStatus
audiounit_remove_device_listener(cubeb * context)
{
  AudioObjectPropertyAddress devAddr;
  devAddr.mSelector = kAudioHardwarePropertyDevices;
  devAddr.mScope = kAudioObjectPropertyScopeGlobal;
  devAddr.mElement = kAudioObjectPropertyElementMaster;

  /* Note: unregister a non registered cb is not a problem, not checking. */
  OSStatus ret = AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
                                                   &devAddr,
                                                   audiounit_collection_changed_callback,
                                                   context);
  if (ret == noErr) {
    /* Reset all values. */
    context->collection_changed_devtype = CUBEB_DEVICE_TYPE_UNKNOWN;
    context->collection_changed_callback = NULL;
    context->collection_changed_user_ptr = NULL;
    context->devtype_device_count = 0;
    if (context->devtype_device_array) {
      delete [] context->devtype_device_array;
      context->devtype_device_array = NULL;
    }
  }
  return ret;
}

int audiounit_register_device_collection_changed(cubeb * context,
                                                 cubeb_device_type devtype,
                                                 cubeb_device_collection_changed_callback collection_changed_callback,
                                                 void * user_ptr)
{
  OSStatus ret;
  auto_lock lock(context->mutex);
  if (collection_changed_callback) {
    ret = audiounit_add_device_listener(context, devtype,
                                        collection_changed_callback,
                                        user_ptr);
  } else {
    ret = audiounit_remove_device_listener(context);
  }
  return (ret == noErr) ? CUBEB_OK : CUBEB_ERROR;
}

cubeb_ops const audiounit_ops = {
  /*.init =*/ audiounit_init,
  /*.get_backend_id =*/ audiounit_get_backend_id,
  /*.get_max_channel_count =*/ audiounit_get_max_channel_count,
  /*.get_min_latency =*/ audiounit_get_min_latency,
  /*.get_preferred_sample_rate =*/ audiounit_get_preferred_sample_rate,
  /*.enumerate_devices =*/ audiounit_enumerate_devices,
  /*.destroy =*/ audiounit_destroy,
  /*.stream_init =*/ audiounit_stream_init,
  /*.stream_destroy =*/ audiounit_stream_destroy,
  /*.stream_start =*/ audiounit_stream_start,
  /*.stream_stop =*/ audiounit_stream_stop,
  /*.stream_get_position =*/ audiounit_stream_get_position,
  /*.stream_get_latency =*/ audiounit_stream_get_latency,
  /*.stream_set_volume =*/ audiounit_stream_set_volume,
  /*.stream_set_panning =*/ audiounit_stream_set_panning,
  /*.stream_get_current_device =*/ audiounit_stream_get_current_device,
  /*.stream_device_destroy =*/ audiounit_stream_device_destroy,
  /*.stream_register_device_changed_callback =*/ audiounit_stream_register_device_changed_callback,
  /*.register_device_collection_changed =*/ audiounit_register_device_collection_changed
};
