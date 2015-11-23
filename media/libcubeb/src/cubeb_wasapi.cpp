/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <initguid.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <windef.h>
#include <audioclient.h>
#include <devicetopology.h>
#include <process.h>
#include <avrt.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb/cubeb-stdint.h"
#include "cubeb_resampler.h"
#include <stdio.h>
#include <stdlib.h>
#include <cmath>

/* devicetopology.h missing in MinGW. */
#ifndef __devicetopology_h__
#include "cubeb_devicetopology.h"
#endif

/* Taken from winbase.h, Not in MinGW. */
#ifndef STACK_SIZE_PARAM_IS_A_RESERVATION
#define STACK_SIZE_PARAM_IS_A_RESERVATION   0x00010000    // Threads only
#endif

#ifndef PKEY_Device_FriendlyName
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName,    0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING
#endif
#ifndef PKEY_Device_InstanceId
DEFINE_PROPERTYKEY(PKEY_Device_InstanceId,      0x78c34fc8, 0x104a, 0x4aca, 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57, 0x00000100); //    VT_LPWSTR
#endif

// #define LOGGING_ENABLED

#ifdef LOGGING_ENABLED
#define LOG(...) do {                           \
    fprintf(stderr, __VA_ARGS__);               \
  } while(0)
#else
#define LOG(...)
#endif

#define ARRAY_LENGTH(array_)                    \
  (sizeof(array_) / sizeof(array_[0]))

namespace {
uint32_t
ms_to_hns(uint32_t ms)
{
  return ms * 10000;
}

uint32_t
hns_to_ms(REFERENCE_TIME hns)
{
  return hns / 10000;
}

double
hns_to_s(REFERENCE_TIME hns)
{
  return static_cast<double>(hns) / 10000000;
}

void
SafeRelease(HANDLE handle)
{
  if (handle) {
    CloseHandle(handle);
  }
}

template <typename T>
void SafeRelease(T * ptr)
{
  if (ptr) {
    ptr->Release();
  }
}

/* This wraps a critical section to track the owner in debug mode, adapted from
   NSPR and http://blogs.msdn.com/b/oldnewthing/archive/2013/07/12/10433554.aspx */
class owned_critical_section
{
public:
  owned_critical_section()
#ifdef DEBUG
    : owner(0)
#endif
  {
    InitializeCriticalSection(&critical_section);
  }

  ~owned_critical_section()
  {
    DeleteCriticalSection(&critical_section);
  }

  void enter()
  {
    EnterCriticalSection(&critical_section);
#ifdef DEBUG
    XASSERT(owner != GetCurrentThreadId() && "recursive locking");
    owner = GetCurrentThreadId();
#endif
  }

  void leave()
  {
#ifdef DEBUG
    /* GetCurrentThreadId cannot return 0: it is not a the valid thread id */
    owner = 0;
#endif
    LeaveCriticalSection(&critical_section);
  }

  /* This is guaranteed to have the good behaviour if it succeeds. The behaviour
     is undefined otherwise. */
  void assert_current_thread_owns()
  {
#ifdef DEBUG
    /* This implies owner != 0, because GetCurrentThreadId cannot return 0. */
    XASSERT(owner == GetCurrentThreadId());
#endif
  }

private:
  CRITICAL_SECTION critical_section;
#ifdef DEBUG
  DWORD owner;
#endif
};

struct auto_lock {
  auto_lock(owned_critical_section * lock)
    : lock(lock)
  {
    lock->enter();
  }
  ~auto_lock()
  {
    lock->leave();
  }
private:
  owned_critical_section * lock;
};

struct auto_com {
  auto_com() {
    result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  }
  ~auto_com() {
    if (result == RPC_E_CHANGED_MODE) {
      // This is not an error, COM was not initialized by this function, so it is
      // not necessary to uninit it.
      LOG("COM was already initialized in STA.\n");
    } else if (result == S_FALSE) {
      // This is not an error. We are allowed to call CoInitializeEx more than
      // once, as long as it is matches by an CoUninitialize call.
      // We do that in the dtor which is guaranteed to be called.
      LOG("COM was already initialized in MTA\n");
    }
    if (SUCCEEDED(result)) {
      CoUninitialize();
    }
  }
  bool ok() {
    return result == RPC_E_CHANGED_MODE || SUCCEEDED(result);
  }
private:
  HRESULT result;
};

typedef HANDLE (WINAPI *set_mm_thread_characteristics_function)(
                                      const char * TaskName, LPDWORD TaskIndex);
typedef BOOL (WINAPI *revert_mm_thread_characteristics_function)(HANDLE handle);

extern cubeb_ops const wasapi_ops;

int wasapi_stream_stop(cubeb_stream * stm);
int wasapi_stream_start(cubeb_stream * stm);
void close_wasapi_stream(cubeb_stream * stm);
int setup_wasapi_stream(cubeb_stream * stm);

}

struct cubeb
{
  cubeb_ops const * ops;
  /* Library dynamically opened to increase the render thread priority, and
     the two function pointers we need. */
  HMODULE mmcss_module;
  set_mm_thread_characteristics_function set_mm_thread_characteristics;
  revert_mm_thread_characteristics_function revert_mm_thread_characteristics;
};

class wasapi_endpoint_notification_client;

struct cubeb_stream
{
  cubeb * context;
  /* Mixer pameters. We need to convert the input stream to this
     samplerate/channel layout, as WASAPI * does not resample nor upmix
     itself. */
  cubeb_stream_params mix_params;
  cubeb_stream_params stream_params;
  /* The latency initially requested for this stream. */
  unsigned latency;
  cubeb_state_callback state_callback;
  cubeb_data_callback data_callback;
  void * user_ptr;

  /* Lifetime considerations:
     - client, render_client, audio_clock and audio_stream_volume are interface
       pointer to the IAudioClient.
     - The lifetime for device_enumerator and notification_client, resampler,
       mix_buffer are the same as the cubeb_stream instance. */

  /* Main handle on the WASAPI stream. */
  IAudioClient * client;
  /* Interface pointer to use the event-driven interface. */
  IAudioRenderClient * render_client;
  /* Interface pointer to use the volume facilities. */
  IAudioStreamVolume * audio_stream_volume;
  /* Interface pointer to use the stream audio clock. */
  IAudioClock * audio_clock;
  /* Frames written to the stream since it was opened. Reset on device
     change. Uses mix_params.rate. */
  UINT64 frames_written;
  /* Frames written to the (logical) stream since it was first
     created. Updated on device change. Uses stream_params.rate. */
  UINT64 total_frames_written;
  /* Last valid reported stream position.  Used to ensure the position
     reported by stream_get_position increases monotonically. */
  UINT64 prev_position;
  /* Device enumerator to be able to be notified when the default
     device change. */
  IMMDeviceEnumerator * device_enumerator;
  /* Device notification client, to be able to be notified when the default
     audio device changes and route the audio to the new default audio output
     device */
  wasapi_endpoint_notification_client * notification_client;
  /* This event is set by the stream_stop and stream_destroy
     function, so the render loop can exit properly. */
  HANDLE shutdown_event;
  /* Set by OnDefaultDeviceChanged when a stream reconfiguration is required.
     The reconfiguration is handled by the render loop thread. */
  HANDLE reconfigure_event;
  /* This is set by WASAPI when we should refill the stream. */
  HANDLE refill_event;
  /* Each cubeb_stream has its own thread. */
  HANDLE thread;
  /* The lock protects all members that are touched by the render thread or
     change during a device reset, including: audio_clock, audio_stream_volume,
     client, frames_written, mix_params, total_frames_written, prev_position. */
  owned_critical_section * stream_reset_lock;
  /* Maximum number of frames we can be requested in a callback. */
  uint32_t buffer_frame_count;
  /* Resampler instance. Resampling will only happen if necessary. */
  cubeb_resampler * resampler;
  /* Buffer used to downmix or upmix to the number of channels the mixer has.
     its size is |frames_to_bytes_before_mix(buffer_frame_count)|. */
  float * mix_buffer;
  /* Stream volume.  Set via stream_set_volume and used to reset volume on
     device changes. */
  float volume;
  /* True if the stream is draining. */
  bool draining;
};


class wasapi_endpoint_notification_client : public IMMNotificationClient
{
public:
  /* The implementation of MSCOM was copied from MSDN. */
  ULONG STDMETHODCALLTYPE
  AddRef()
  {
    return InterlockedIncrement(&ref_count);
  }

  ULONG STDMETHODCALLTYPE
  Release()
  {
    ULONG ulRef = InterlockedDecrement(&ref_count);
    if (0 == ulRef) {
      delete this;
    }
    return ulRef;
  }

  HRESULT STDMETHODCALLTYPE
  QueryInterface(REFIID riid, VOID **ppvInterface)
  {
    if (__uuidof(IUnknown) == riid) {
      AddRef();
      *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvInterface = (IMMNotificationClient*)this;
    } else {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  wasapi_endpoint_notification_client(HANDLE event)
    : ref_count(1)
    , reconfigure_event(event)
  { }

  virtual ~wasapi_endpoint_notification_client()
  { }

  HRESULT STDMETHODCALLTYPE
  OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id)
  {
    LOG("Audio device default changed.\n");

    /* we only support a single stream type for now. */
    if (flow != eRender && role != eMultimedia) {
      return S_OK;
    }

    BOOL ok = SetEvent(reconfigure_event);
    if (!ok) {
      LOG("SetEvent on reconfigure_event failed: %x\n", GetLastError());
    }

    return S_OK;
  }

  /* The remaining methods are not implemented, they simply log when called (if
     log is enabled), for debugging. */
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR device_id)
  {
    LOG("Audio device added.\n");
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR device_id)
  {
    LOG("Audio device removed.\n");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state)
  {
    LOG("Audio device state changed.\n");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key)
  {
    LOG("Audio device property value changed.\n");
    return S_OK;
  }
private:
  /* refcount for this instance, necessary to implement MSCOM semantics. */
  LONG ref_count;
  HANDLE reconfigure_event;
};

namespace {
bool should_upmix(cubeb_stream * stream)
{
  return stream->mix_params.channels > stream->stream_params.channels;
}

bool should_downmix(cubeb_stream * stream)
{
  return stream->mix_params.channels < stream->stream_params.channels;
}

double stream_to_mix_samplerate_ratio(cubeb_stream * stream)
{
  stream->stream_reset_lock->assert_current_thread_owns();
  return double(stream->stream_params.rate) / stream->mix_params.rate;
}

/* Upmix function, copies a mono channel into L and R */
template<typename T>
void
mono_to_stereo(T * in, long insamples, T * out, int32_t out_channels)
{
  for (int i = 0, j = 0; i < insamples; ++i, j += out_channels) {
    out[j] = out[j + 1] = in[i];
  }
}

template<typename T>
void
upmix(T * in, long inframes, T * out, int32_t in_channels, int32_t out_channels)
{
  XASSERT(out_channels >= in_channels && in_channels > 0);

  /* Either way, if we have 2 or more channels, the first two are L and R. */
  /* If we are playing a mono stream over stereo speakers, copy the data over. */
  if (in_channels == 1 && out_channels >= 2) {
    mono_to_stereo(in, inframes, out, out_channels);
  } else {
    /* Copy through. */
    for (int i = 0, o = 0; i < inframes * in_channels;
        i += in_channels, o += out_channels) {
      for (int j = 0; j < in_channels; ++j) {
        out[o + j] = in[i + j];
      }
    }
  }

  /* Check if more channels. */
  if (out_channels <= 2) {
    return;
  }

  /* Put silence in remaining channels. */
  for (long i = 0, o = 0; i < inframes; ++i, o += out_channels) {
    for (int j = 2; j < out_channels; ++j) {
      out[o + j] = 0.0;
    }
  }
}

template<typename T>
void
downmix(T * in, long inframes, T * out, int32_t in_channels, int32_t out_channels)
{
  XASSERT(in_channels >= out_channels);
  /* We could use a downmix matrix here, applying mixing weight based on the
     channel, but directsound and winmm simply drop the channels that cannot be
     rendered by the hardware, so we do the same for consistency. */
  long out_index = 0;
  for (long i = 0; i < inframes * in_channels; i += in_channels) {
    for (int j = 0; j < out_channels; ++j) {
      out[out_index + j] = in[i + j];
    }
    out_index += out_channels;
  }
}

/* This returns the size of a frame in the stream, before the eventual upmix
   occurs. */
static size_t
frames_to_bytes_before_mix(cubeb_stream * stm, size_t frames)
{
  size_t stream_frame_size = stm->stream_params.channels * sizeof(float);
  return stream_frame_size * frames;
}

long
refill(cubeb_stream * stm, float * data, long frames_needed)
{
  /* If we need to upmix after resampling, resample into the mix buffer to
     avoid a copy. */
  float * dest;
  if (should_upmix(stm) || should_downmix(stm)) {
    dest = stm->mix_buffer;
  } else {
    dest = data;
  }

  long out_frames = cubeb_resampler_fill(stm->resampler, dest, frames_needed);
  /* TODO: Report out_frames < 0 as an error via the API. */
  XASSERT(out_frames >= 0);

  {
    auto_lock lock(stm->stream_reset_lock);
    stm->frames_written += out_frames;
  }

  /* Go in draining mode if we got fewer frames than requested. */
  if (out_frames < frames_needed) {
    LOG("start draining.\n");
    stm->draining = true;
  }

  /* If this is not true, there will be glitches.
     It is alright to have produced less frames if we are draining, though. */
  XASSERT(out_frames == frames_needed || stm->draining);

  if (should_upmix(stm)) {
    upmix(dest, out_frames, data,
          stm->stream_params.channels, stm->mix_params.channels);
  } else if (should_downmix(stm)) {
    downmix(dest, out_frames, data,
            stm->stream_params.channels, stm->mix_params.channels);
  }

  return out_frames;
}

static unsigned int __stdcall
wasapi_stream_render_loop(LPVOID stream)
{
  cubeb_stream * stm = static_cast<cubeb_stream *>(stream);

  bool is_playing = true;
  HANDLE wait_array[3] = {stm->shutdown_event, stm->reconfigure_event, stm->refill_event};
  HANDLE mmcss_handle = NULL;
  HRESULT hr = 0;
  DWORD mmcss_task_index = 0;
  auto_com com;
  if (!com.ok()) {
    LOG("COM initialization failed on render_loop thread.\n");
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return 0;
  }

  /* We could consider using "Pro Audio" here for WebAudio and
     maybe WebRTC. */
  mmcss_handle =
    stm->context->set_mm_thread_characteristics("Audio", &mmcss_task_index);
  if (!mmcss_handle) {
    /* This is not fatal, but we might glitch under heavy load. */
    LOG("Unable to use mmcss to bump the render thread priority: %x\n", GetLastError());
  }


  /* WaitForMultipleObjects timeout can trigger in cases where we don't want to
     treat it as a timeout, such as across a system sleep/wake cycle.  Trigger
     the timeout error handling only when the timeout_limit is reached, which is
     reset on each successful loop. */
  unsigned timeout_count = 0;
  const unsigned timeout_limit = 5;
  while (is_playing) {
    DWORD waitResult = WaitForMultipleObjects(ARRAY_LENGTH(wait_array),
                                              wait_array,
                                              FALSE,
                                              1000);
    if (waitResult != WAIT_TIMEOUT) {
      timeout_count = 0;
    }
    switch (waitResult) {
    case WAIT_OBJECT_0: { /* shutdown */
      is_playing = false;
      /* We don't check if the drain is actually finished here, we just want to
         shutdown. */
      if (stm->draining) {
        stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      }
      continue;
    }
    case WAIT_OBJECT_0 + 1: { /* reconfigure */
      /* Close the stream */
      stm->client->Stop();
      {
        auto_lock lock(stm->stream_reset_lock);
        close_wasapi_stream(stm);
        /* Reopen a stream and start it immediately. This will automatically pick the
           new default device for this role. */
        int r = setup_wasapi_stream(stm);
        if (r != CUBEB_OK) {
          /* Don't destroy the stream here, since we expect the caller to do
             so after the error has propagated via the state callback. */
          is_playing = false;
          hr = E_FAIL;
          continue;
        }
      }
      stm->client->Start();
      break;
    }
    case WAIT_OBJECT_0 + 2: { /* refill */
      UINT32 padding;

      hr = stm->client->GetCurrentPadding(&padding);
      if (FAILED(hr)) {
        LOG("Failed to get padding: %x\n", hr);
        is_playing = false;
        continue;
      }
      XASSERT(padding <= stm->buffer_frame_count);

      if (stm->draining) {
        if (padding == 0) {
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
          is_playing = false;
        }
        continue;
      }

      long available = stm->buffer_frame_count - padding;

      if (available == 0) {
        continue;
      }

      BYTE * data;
      hr = stm->render_client->GetBuffer(available, &data);
      if (SUCCEEDED(hr)) {
        long wrote = refill(stm, reinterpret_cast<float *>(data), available);
        XASSERT(wrote == available || stm->draining);

        hr = stm->render_client->ReleaseBuffer(wrote, 0);
        if (FAILED(hr)) {
          LOG("failed to release buffer: %x\n", hr);
          is_playing = false;
        }
      } else {
        LOG("failed to get buffer: %x\n", hr);
        is_playing = false;
      }
    }
      break;
    case WAIT_TIMEOUT:
      XASSERT(stm->shutdown_event == wait_array[0]);
      if (++timeout_count >= timeout_limit) {
        is_playing = false;
        hr = E_FAIL;
      }
      break;
    default:
      LOG("case %d not handled in render loop.", waitResult);
      abort();
    }
  }

  if (FAILED(hr)) {
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
  }

  stm->context->revert_mm_thread_characteristics(mmcss_handle);

  return 0;
}

void wasapi_destroy(cubeb * context);

HANDLE WINAPI set_mm_thread_characteristics_noop(const char *, LPDWORD mmcss_task_index)
{
  return (HANDLE)1;
}

BOOL WINAPI revert_mm_thread_characteristics_noop(HANDLE mmcss_handle)
{
  return true;
}

HRESULT register_notification_client(cubeb_stream * stm)
{
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&stm->device_enumerator));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %x\n", hr);
    return hr;
  }

  stm->notification_client = new wasapi_endpoint_notification_client(stm->reconfigure_event);

  hr = stm->device_enumerator->RegisterEndpointNotificationCallback(stm->notification_client);
  if (FAILED(hr)) {
    LOG("Could not register endpoint notification callback: %x\n", hr);
    return hr;
  }

  return hr;
}

HRESULT unregister_notification_client(cubeb_stream * stm)
{
  XASSERT(stm);

  if (!stm->device_enumerator) {
    return S_OK;
  }

  stm->device_enumerator->UnregisterEndpointNotificationCallback(stm->notification_client);

  SafeRelease(stm->notification_client);
  SafeRelease(stm->device_enumerator);

  return S_OK;
}

HRESULT get_default_endpoint(IMMDevice ** device)
{
  IMMDeviceEnumerator * enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %x\n", hr);
    return hr;
  }
  /* eMultimedia is okay for now ("Music, movies, narration, [...]").
     We will need to change this when we distinguish streams by use-case, other
     possible values being eConsole ("Games, system notification sounds [...]")
     and eCommunication ("Voice communication"). */
  hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device);
  if (FAILED(hr)) {
    LOG("Could not get default audio endpoint: %x\n", hr);
    SafeRelease(enumerator);
    return hr;
  }

  SafeRelease(enumerator);

  return ERROR_SUCCESS;
}

double
current_stream_delay(cubeb_stream * stm)
{
  stm->stream_reset_lock->assert_current_thread_owns();

  /* If the default audio endpoint went away during playback and we weren't
     able to configure a new one, it's possible the caller may call this
     before the error callback has propogated back. */
  if (!stm->audio_clock) {
    return 0;
  }

  UINT64 freq;
  HRESULT hr = stm->audio_clock->GetFrequency(&freq);
  if (FAILED(hr)) {
    LOG("GetFrequency failed: %x\n", hr);
    return 0;
  }

  UINT64 pos;
  hr = stm->audio_clock->GetPosition(&pos, NULL);
  if (FAILED(hr)) {
    LOG("GetPosition failed: %x\n", hr);
    return 0;
  }

  double cur_pos = static_cast<double>(pos) / freq;
  double max_pos = static_cast<double>(stm->frames_written)  / stm->mix_params.rate;
  double delay = max_pos - cur_pos;
  XASSERT(delay >= 0);

  return delay;
}

int
stream_set_volume(cubeb_stream * stm, float volume)
{
  stm->stream_reset_lock->assert_current_thread_owns();

  if (!stm->audio_stream_volume) {
    return CUBEB_ERROR;
  }

  uint32_t channels;
  HRESULT hr = stm->audio_stream_volume->GetChannelCount(&channels);
  if (hr != S_OK) {
    LOG("could not get the channel count: %x\n", hr);
    return CUBEB_ERROR;
  }

  /* up to 9.1 for now */
  if (channels > 10) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  float volumes[10];
  for (uint32_t i = 0; i < channels; i++) {
    volumes[i] = volume;
  }

  hr = stm->audio_stream_volume->SetAllVolumes(channels,  volumes);
  if (hr != S_OK) {
    LOG("could not set the channels volume: %x\n", hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}
} // namespace anonymous

extern "C" {
int wasapi_init(cubeb ** context, char const * context_name)
{
  HRESULT hr;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  /* We don't use the device yet, but need to make sure we can initialize one
     so that this backend is not incorrectly enabled on platforms that don't
     support WASAPI. */
  IMMDevice * device;
  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    LOG("Could not get device: %x\n", hr);
    return CUBEB_ERROR;
  }
  SafeRelease(device);

  cubeb * ctx = (cubeb *)calloc(1, sizeof(cubeb));
  if (!ctx) {
    return CUBEB_ERROR;
  }

  ctx->ops = &wasapi_ops;

  ctx->mmcss_module = LoadLibraryA("Avrt.dll");

  if (ctx->mmcss_module) {
    ctx->set_mm_thread_characteristics =
      (set_mm_thread_characteristics_function) GetProcAddress(
          ctx->mmcss_module, "AvSetMmThreadCharacteristicsA");
    ctx->revert_mm_thread_characteristics =
      (revert_mm_thread_characteristics_function) GetProcAddress(
          ctx->mmcss_module, "AvRevertMmThreadCharacteristics");
    if (!(ctx->set_mm_thread_characteristics && ctx->revert_mm_thread_characteristics)) {
      LOG("Could not load AvSetMmThreadCharacteristics or AvRevertMmThreadCharacteristics: %x\n", GetLastError());
      FreeLibrary(ctx->mmcss_module);
    }
  } else {
    // This is not a fatal error, but we might end up glitching when
    // the system is under high load.
    LOG("Could not load Avrt.dll\n");
    ctx->set_mm_thread_characteristics = &set_mm_thread_characteristics_noop;
    ctx->revert_mm_thread_characteristics = &revert_mm_thread_characteristics_noop;
  }

  *context = ctx;

  return CUBEB_OK;
}
}

namespace {
void stop_and_join_render_thread(cubeb_stream * stm)
{
  if (!stm->thread) {
    return;
  }

  BOOL ok = SetEvent(stm->shutdown_event);
  if (!ok) {
    LOG("Destroy SetEvent failed: %d\n", GetLastError());
  }

  DWORD r = WaitForSingleObject(stm->thread, INFINITE);
  if (r == WAIT_FAILED) {
    LOG("Destroy WaitForSingleObject on thread failed: %d\n", GetLastError());
  }

  CloseHandle(stm->thread);
  stm->thread = NULL;

  CloseHandle(stm->shutdown_event);
  stm->shutdown_event = 0;
}

void wasapi_destroy(cubeb * context)
{
  if (context->mmcss_module) {
    FreeLibrary(context->mmcss_module);
  }
  free(context);
}

char const * wasapi_get_backend_id(cubeb * context)
{
  return "wasapi";
}

int
wasapi_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  HRESULT hr;
  IAudioClient * client;
  WAVEFORMATEX * mix_format;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  XASSERT(ctx && max_channels);

  IMMDevice * device;
  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, (void **)&client);
  SafeRelease(device);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  hr = client->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    SafeRelease(client);
    return CUBEB_ERROR;
  }

  *max_channels = mix_format->nChannels;

  CoTaskMemFree(mix_format);
  SafeRelease(client);

  return CUBEB_OK;
}

int
wasapi_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_ms)
{
  HRESULT hr;
  IAudioClient * client;
  REFERENCE_TIME default_period;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  IMMDevice * device;
  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    LOG("Could not get default endpoint: %x\n", hr);
    return CUBEB_ERROR;
  }

  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, (void **)&client);
  SafeRelease(device);
  if (FAILED(hr)) {
    LOG("Could not activate device for latency: %x\n", hr);
    return CUBEB_ERROR;
  }

  /* The second parameter is for exclusive mode, that we don't use. */
  hr = client->GetDevicePeriod(&default_period, NULL);
  if (FAILED(hr)) {
    SafeRelease(client);
    LOG("Could not get device period: %x\n", hr);
    return CUBEB_ERROR;
  }

  LOG("default device period: %ld\n", default_period);

  /* According to the docs, the best latency we can achieve is by synchronizing
     the stream and the engine.
     http://msdn.microsoft.com/en-us/library/windows/desktop/dd370871%28v=vs.85%29.aspx */
  *latency_ms = hns_to_ms(default_period);

  SafeRelease(client);

  return CUBEB_OK;
}

int
wasapi_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  HRESULT hr;
  IAudioClient * client;
  WAVEFORMATEX * mix_format;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  IMMDevice * device;
  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, (void **)&client);
  SafeRelease(device);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  hr = client->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    SafeRelease(client);
    return CUBEB_ERROR;
  }

  *rate = mix_format->nSamplesPerSec;

  CoTaskMemFree(mix_format);
  SafeRelease(client);

  return CUBEB_OK;
}

void wasapi_stream_destroy(cubeb_stream * stm);

/* Based on the mix format and the stream format, try to find a way to play
   what the user requested. */
static void
handle_channel_layout(cubeb_stream * stm,  WAVEFORMATEX ** mix_format, const cubeb_stream_params * stream_params)
{
  /* Common case: the hardware is stereo. Up-mixing and down-mixing will be
     handled in the callback. */
  if ((*mix_format)->nChannels <= 2) {
    return;
  }

  /* The docs say that GetMixFormat is always of type WAVEFORMATEXTENSIBLE [1],
     so the reinterpret_cast below should be safe. In practice, this is not
     true, and we just want to bail out and let the rest of the code find a good
     conversion path instead of trying to make WASAPI do it by itself.
     [1]: http://msdn.microsoft.com/en-us/library/windows/desktop/dd370811%28v=vs.85%29.aspx*/
  if ((*mix_format)->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
    return;
  }

  WAVEFORMATEXTENSIBLE * format_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(*mix_format);

  /* Stash a copy of the original mix format in case we need to restore it later. */
  WAVEFORMATEXTENSIBLE hw_mix_format = *format_pcm;

  /* The hardware is in surround mode, we want to only use front left and front
     right. Try that, and check if it works. */
  switch (stream_params->channels) {
    case 1: /* Mono */
      format_pcm->dwChannelMask = KSAUDIO_SPEAKER_MONO;
      break;
    case 2: /* Stereo */
      format_pcm->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
      break;
    default:
      XASSERT(false && "Channel layout not supported.");
      break;
  }
  (*mix_format)->nChannels = stream_params->channels;
  (*mix_format)->nBlockAlign = ((*mix_format)->wBitsPerSample * (*mix_format)->nChannels) / 8;
  (*mix_format)->nAvgBytesPerSec = (*mix_format)->nSamplesPerSec * (*mix_format)->nBlockAlign;
  format_pcm->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  (*mix_format)->wBitsPerSample = 32;
  format_pcm->Samples.wValidBitsPerSample = (*mix_format)->wBitsPerSample;

  /* Check if wasapi will accept our channel layout request. */
  WAVEFORMATEX * closest;
  HRESULT hr = stm->client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                              *mix_format,
                                              &closest);

  if (hr == S_FALSE) {
    /* Not supported, but WASAPI gives us a suggestion. Use it, and handle the
       eventual upmix/downmix ourselves */
    LOG("Using WASAPI suggested format: channels: %d\n", closest->nChannels);
    WAVEFORMATEXTENSIBLE * closest_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(closest);
    XASSERT(closest_pcm->SubFormat == format_pcm->SubFormat);
    CoTaskMemFree(*mix_format);
    *mix_format = closest;
  } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
    /* Not supported, no suggestion. This should not happen, but it does in the
       field with some sound cards. We restore the mix format, and let the rest
       of the code figure out the right conversion path. */
    *reinterpret_cast<WAVEFORMATEXTENSIBLE *>(*mix_format) = hw_mix_format;
  } else if (hr == S_OK) {
    LOG("Requested format accepted by WASAPI.\n");
  } else {
    LOG("IsFormatSupported unhandled error: %x\n", hr);
  }
}

int setup_wasapi_stream(cubeb_stream * stm)
{
  HRESULT hr;
  IMMDevice * device;
  WAVEFORMATEX * mix_format;

  stm->stream_reset_lock->assert_current_thread_owns();

  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  XASSERT(!stm->client && "WASAPI stream already setup, close it first.");

  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    LOG("Could not get default endpoint, error: %x\n", hr);
    return CUBEB_ERROR;
  }

  /* Get a client. We will get all other interfaces we need from
     this pointer. */
  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, (void **)&stm->client);
  SafeRelease(device);
  if (FAILED(hr)) {
    LOG("Could not activate the device to get an audio client: error: %x\n", hr);
    return CUBEB_ERROR;
  }

  /* We have to distinguish between the format the mixer uses,
     and the format the stream we want to play uses. */
  hr = stm->client->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    LOG("Could not fetch current mix format from the audio client: error: %x\n", hr);
    return CUBEB_ERROR;
  }

  handle_channel_layout(stm, &mix_format, &stm->stream_params);

  /* Shared mode WASAPI always supports float32 sample format, so this
     is safe. */
  stm->mix_params.format = CUBEB_SAMPLE_FLOAT32NE;
  stm->mix_params.rate = mix_format->nSamplesPerSec;
  stm->mix_params.channels = mix_format->nChannels;

  hr = stm->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                               AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                               AUDCLNT_STREAMFLAGS_NOPERSIST,
                               ms_to_hns(stm->latency),
                               0,
                               mix_format,
                               NULL);
  CoTaskMemFree(mix_format);
  if (FAILED(hr)) {
    LOG("Unable to initialize audio client: %x\n", hr);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetBufferSize(&stm->buffer_frame_count);
  if (FAILED(hr)) {
    LOG("Could not get the buffer size from the client: %x\n", hr);
    return CUBEB_ERROR;
  }

  if (should_upmix(stm) || should_downmix(stm)) {
    stm->mix_buffer = (float *) malloc(frames_to_bytes_before_mix(stm, stm->buffer_frame_count));
  }

  hr = stm->client->SetEventHandle(stm->refill_event);
  if (FAILED(hr)) {
    LOG("Could set the event handle for the client: %x\n", hr);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetService(__uuidof(IAudioRenderClient),
                               (void **)&stm->render_client);
  if (FAILED(hr)) {
    LOG("Could not get the render client: %x\n", hr);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetService(__uuidof(IAudioStreamVolume),
                               (void **)&stm->audio_stream_volume);
  if (FAILED(hr)) {
    LOG("Could not get the IAudioStreamVolume: %x\n", hr);
    return CUBEB_ERROR;
  }

  XASSERT(stm->frames_written == 0);
  hr = stm->client->GetService(__uuidof(IAudioClock),
                               (void **)&stm->audio_clock);
  if (FAILED(hr)) {
    LOG("Could not get the IAudioClock: %x\n", hr);
    return CUBEB_ERROR;
  }

  /* Restore the stream volume over a device change. */
  if (stream_set_volume(stm, stm->volume) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  /* If we are playing a mono stream, we only resample one channel,
     and copy it over, so we are always resampling the number
     of channels of the stream, not the number of channels
     that WASAPI wants. */
  stm->resampler = cubeb_resampler_create(stm, stm->stream_params,
                                          stm->mix_params.rate,
                                          stm->data_callback,
                                          stm->buffer_frame_count,
                                          stm->user_ptr,
                                          CUBEB_RESAMPLER_QUALITY_DESKTOP);
  if (!stm->resampler) {
    LOG("Could not get a resampler\n");
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

int
wasapi_stream_init(cubeb * context, cubeb_stream ** stream,
                   char const * stream_name, cubeb_stream_params stream_params,
                   unsigned int latency, cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  HRESULT hr;
  int rv;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  XASSERT(context && stream);

  if (stream_params.format != CUBEB_SAMPLE_FLOAT32NE) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  cubeb_stream * stm = (cubeb_stream *)calloc(1, sizeof(cubeb_stream));

  XASSERT(stm);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->stream_params = stream_params;
  stm->draining = false;
  stm->latency = latency;
  stm->volume = 1.0;

  stm->stream_reset_lock = new owned_critical_section();

  stm->reconfigure_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->reconfigure_event) {
    LOG("Can't create the reconfigure event, error: %x\n", GetLastError());
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  stm->refill_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->refill_event) {
    LOG("Can't create the refill event, error: %x\n", GetLastError());
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  {
    /* Locking here is not strictly necessary, because we don't have a
       notification client that can reset the stream yet, but it lets us
       assert that the lock is held in the function. */
    auto_lock lock(stm->stream_reset_lock);
    rv = setup_wasapi_stream(stm);
  }
  if (rv != CUBEB_OK) {
    wasapi_stream_destroy(stm);
    return rv;
  }

  hr = register_notification_client(stm);
  if (FAILED(hr)) {
    /* this is not fatal, we can still play audio, but we won't be able
       to keep using the default audio endpoint if it changes. */
    LOG("failed to register notification client, %x\n", hr);
  }

  *stream = stm;

  return CUBEB_OK;
}

void close_wasapi_stream(cubeb_stream * stm)
{
  XASSERT(stm);

  stm->stream_reset_lock->assert_current_thread_owns();

  SafeRelease(stm->client);
  stm->client = NULL;

  SafeRelease(stm->render_client);
  stm->render_client = NULL;

  SafeRelease(stm->audio_stream_volume);
  stm->audio_stream_volume = NULL;

  SafeRelease(stm->audio_clock);
  stm->audio_clock = NULL;
  stm->total_frames_written += round(stm->frames_written * stream_to_mix_samplerate_ratio(stm));
  stm->frames_written = 0;

  if (stm->resampler) {
    cubeb_resampler_destroy(stm->resampler);
    stm->resampler = NULL;
  }

  free(stm->mix_buffer);
  stm->mix_buffer = NULL;
}

void wasapi_stream_destroy(cubeb_stream * stm)
{
  XASSERT(stm);

  unregister_notification_client(stm);

  stop_and_join_render_thread(stm);

  SafeRelease(stm->reconfigure_event);
  SafeRelease(stm->refill_event);

  {
    auto_lock lock(stm->stream_reset_lock);
    close_wasapi_stream(stm);
  }

  delete stm->stream_reset_lock;

  free(stm);
}

int wasapi_stream_start(cubeb_stream * stm)
{
  auto_lock lock(stm->stream_reset_lock);

  XASSERT(stm && !stm->thread && !stm->shutdown_event);

  if (!stm->client) {
    return CUBEB_ERROR;
  }

  HRESULT hr = stm->client->Start();
  if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
    LOG("audioclient invalid device, reconfiguring\n", hr);

    BOOL ok = ResetEvent(stm->reconfigure_event);
    if (!ok) {
      LOG("resetting reconfig event failed: %x\n", GetLastError());
    }

    close_wasapi_stream(stm);
    int r = setup_wasapi_stream(stm);
    if (r != CUBEB_OK) {
      LOG("reconfigure failed\n");
      return r;
    }

    HRESULT hr = stm->client->Start();
    if (FAILED(hr)) {
      LOG("could not start the stream after reconfig: %x\n", hr);
      return CUBEB_ERROR;
    }
  } else if (FAILED(hr)) {
    LOG("could not start the stream.\n");
    return CUBEB_ERROR;
  }

  stm->shutdown_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->shutdown_event) {
    LOG("Can't create the shutdown event, error: %x\n", GetLastError());
    return CUBEB_ERROR;
  }

  stm->thread = (HANDLE) _beginthreadex(NULL, 256 * 1024, wasapi_stream_render_loop, stm, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
  if (stm->thread == NULL) {
    LOG("could not create WASAPI render thread.\n");
    return CUBEB_ERROR;
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

int wasapi_stream_stop(cubeb_stream * stm)
{
  XASSERT(stm);

  {
    auto_lock lock(stm->stream_reset_lock);

    if (stm->client) {
      HRESULT hr = stm->client->Stop();
      if (FAILED(hr)) {
        LOG("could not stop AudioClient\n");
        return CUBEB_ERROR;
      }
    }

    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  }

  stop_and_join_render_thread(stm);

  return CUBEB_OK;
}

int wasapi_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  XASSERT(stm && position);
  auto_lock lock(stm->stream_reset_lock);

  /* Calculate how far behind the current stream head the playback cursor is. */
  uint64_t stream_delay = current_stream_delay(stm) * stm->stream_params.rate;

  /* Calculate the logical stream head in frames at the stream sample rate. */
  uint64_t max_pos = stm->total_frames_written +
                     round(stm->frames_written * stream_to_mix_samplerate_ratio(stm));

  *position = max_pos;
  if (stream_delay <= *position) {
    *position -= stream_delay;
  }

  if (*position < stm->prev_position) {
    *position = stm->prev_position;
  }
  stm->prev_position = *position;

  return CUBEB_OK;
}

int wasapi_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  XASSERT(stm && latency);

  auto_lock lock(stm->stream_reset_lock);

  /* The GetStreamLatency method only works if the
     AudioClient has been initialized. */
  if (!stm->client) {
    return CUBEB_ERROR;
  }

  REFERENCE_TIME latency_hns;
  HRESULT hr = stm->client->GetStreamLatency(&latency_hns);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  double latency_s = hns_to_s(latency_hns);
  *latency = static_cast<uint32_t>(latency_s * stm->stream_params.rate);

  return CUBEB_OK;
}

int wasapi_stream_set_volume(cubeb_stream * stm, float volume)
{
  auto_lock lock(stm->stream_reset_lock);

  if (stream_set_volume(stm, volume) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  stm->volume = volume;

  return CUBEB_OK;
}

static char *
wstr_to_utf8(LPCWSTR str)
{
  char * ret = NULL;
  int size;

  size = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, ret, 0, NULL, NULL);
  if (size > 0) {
    ret = (char *) malloc(size);
    ::WideCharToMultiByte(CP_UTF8, 0, str, -1, ret, size, NULL, NULL);
  }

  return ret;
}

static IMMDevice *
wasapi_get_device_node(IMMDeviceEnumerator * enumerator, IMMDevice * dev)
{
  IMMDevice * ret = NULL;
  IDeviceTopology * devtopo = NULL;
  IConnector * connector = NULL;

  if (SUCCEEDED(dev->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, (void**)&devtopo)) &&
      SUCCEEDED(devtopo->GetConnector(0, &connector))) {
    LPWSTR filterid;
    if (SUCCEEDED(connector->GetDeviceIdConnectedTo(&filterid))) {
      if (FAILED(enumerator->GetDevice(filterid, &ret)))
        ret = NULL;
      CoTaskMemFree(filterid);
    }
  }

  SafeRelease(connector);
  SafeRelease(devtopo);
  return ret;
}

static BOOL
wasapi_is_default_device(EDataFlow flow, ERole role, LPCWSTR device_id,
    IMMDeviceEnumerator * enumerator)
{
  BOOL ret = FALSE;
  IMMDevice * dev;
  HRESULT hr;

  hr = enumerator->GetDefaultAudioEndpoint(flow, role, &dev);
  if (SUCCEEDED(hr)) {
    LPWSTR defdevid = NULL;
    if (SUCCEEDED(dev->GetId(&defdevid)))
      ret = (wcscmp(defdevid, device_id) == 0);
    if (defdevid != NULL)
      CoTaskMemFree(defdevid);
    SafeRelease(dev);
  }

  return ret;
}

static cubeb_device_info *
wasapi_create_device(IMMDeviceEnumerator * enumerator, IMMDevice * dev)
{
  IMMEndpoint * endpoint = NULL;
  IMMDevice * devnode;
  IAudioClient * client = NULL;
  cubeb_device_info * ret = NULL;
  EDataFlow flow;
  LPWSTR device_id = NULL;
  DWORD state = DEVICE_STATE_NOTPRESENT;
  IPropertyStore * propstore = NULL;
  PROPVARIANT propvar;
  REFERENCE_TIME def_period, min_period;
  HRESULT hr;

  PropVariantInit(&propvar);

  hr = dev->QueryInterface(IID_PPV_ARGS(&endpoint));
  if (FAILED(hr)) goto done;

  hr = endpoint->GetDataFlow(&flow);
  if (FAILED(hr)) goto done;

  hr = dev->GetId(&device_id);
  if (FAILED(hr)) goto done;

  hr = dev->OpenPropertyStore(STGM_READ, &propstore);
  if (FAILED(hr)) goto done;

  hr = dev->GetState(&state);
  if (FAILED(hr)) goto done;

  ret = (cubeb_device_info *)calloc(1, sizeof(cubeb_device_info));

  ret->devid = ret->device_id = wstr_to_utf8(device_id);
  hr = propstore->GetValue(PKEY_Device_FriendlyName, &propvar);
  if (SUCCEEDED(hr))
    ret->friendly_name = wstr_to_utf8(propvar.pwszVal);

  devnode = wasapi_get_device_node(enumerator, dev);
  if (devnode != NULL) {
    IPropertyStore * ps = NULL;
    hr = devnode->OpenPropertyStore(STGM_READ, &ps);
    if (FAILED(hr)) goto done;

    PropVariantClear(&propvar);
    hr = ps->GetValue(PKEY_Device_InstanceId, &propvar);
    if (SUCCEEDED(hr)) {
      ret->group_id = wstr_to_utf8(propvar.pwszVal);
    }
    SafeRelease(ps);
  }

  ret->preferred = CUBEB_DEVICE_PREF_NONE;
  if (wasapi_is_default_device(flow, eMultimedia, device_id, enumerator))
    ret->preferred = (cubeb_device_pref)(ret->preferred | CUBEB_DEVICE_PREF_MULTIMEDIA);
  if (wasapi_is_default_device(flow, eCommunications, device_id, enumerator))
    ret->preferred = (cubeb_device_pref)(ret->preferred | CUBEB_DEVICE_PREF_VOICE);
  if (wasapi_is_default_device(flow, eConsole, device_id, enumerator))
    ret->preferred = (cubeb_device_pref)(ret->preferred | CUBEB_DEVICE_PREF_NOTIFICATION);

  if (flow == eRender) ret->type = CUBEB_DEVICE_TYPE_OUTPUT;
  else if (flow == eCapture) ret->type = CUBEB_DEVICE_TYPE_INPUT;
  switch (state) {
    case DEVICE_STATE_ACTIVE:
      ret->state = CUBEB_DEVICE_STATE_ENABLED;
      break;
    case DEVICE_STATE_UNPLUGGED:
      ret->state = CUBEB_DEVICE_STATE_UNPLUGGED;
      break;
    default:
      ret->state = CUBEB_DEVICE_STATE_DISABLED;
      break;
  };

  ret->format = CUBEB_DEVICE_FMT_F32NE; /* cubeb only supports 32bit float at the moment */
  ret->default_format = CUBEB_DEVICE_FMT_F32NE;
  PropVariantClear(&propvar);
  hr = propstore->GetValue(PKEY_AudioEngine_DeviceFormat, &propvar);
  if (SUCCEEDED(hr) && propvar.vt == VT_BLOB) {
    if (propvar.blob.cbSize == sizeof(PCMWAVEFORMAT)) {
      const PCMWAVEFORMAT * pcm = reinterpret_cast<const PCMWAVEFORMAT *>(propvar.blob.pBlobData);

      ret->max_rate = ret->min_rate = ret->default_rate = pcm->wf.nSamplesPerSec;
      ret->max_channels = pcm->wf.nChannels;
    } else if (propvar.blob.cbSize >= sizeof(WAVEFORMATEX)) {
      WAVEFORMATEX* wfx = reinterpret_cast<WAVEFORMATEX*>(propvar.blob.pBlobData);

      if (propvar.blob.cbSize >= sizeof(WAVEFORMATEX) + wfx->cbSize ||
          wfx->wFormatTag == WAVE_FORMAT_PCM) {
        ret->max_rate = ret->min_rate = ret->default_rate = wfx->nSamplesPerSec;
        ret->max_channels = wfx->nChannels;
      }
    }
  }

  if (SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, (void**)&client)) &&
      SUCCEEDED(client->GetDevicePeriod(&def_period, &min_period))) {
    ret->latency_lo_ms = hns_to_ms(min_period);
    ret->latency_hi_ms = hns_to_ms(def_period);
  } else {
    ret->latency_lo_ms = 0;
    ret->latency_hi_ms = 0;
  }
  SafeRelease(client);

done:
  SafeRelease(devnode);
  SafeRelease(endpoint);
  SafeRelease(propstore);
  if (device_id != NULL)
    CoTaskMemFree(device_id);
  PropVariantClear(&propvar);
  return ret;
}

static int
wasapi_enumerate_devices(cubeb * context, cubeb_device_type type,
                         cubeb_device_collection ** out)
{
  auto_com com;
  IMMDeviceEnumerator * enumerator;
  IMMDeviceCollection * collection;
  IMMDevice * dev;
  cubeb_device_info * cur;
  HRESULT hr;
  UINT cc, i;
  EDataFlow flow;

  *out = NULL;

  if (!com.ok())
    return CUBEB_ERROR;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %x\n", hr);
    return CUBEB_ERROR;
  }

  if (type == CUBEB_DEVICE_TYPE_OUTPUT) flow = eRender;
  else if (type == CUBEB_DEVICE_TYPE_INPUT)  flow = eCapture;
  else if (type & (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_INPUT)) flow = eAll;
  else return CUBEB_ERROR;

  hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATEMASK_ALL, &collection);
  if (FAILED(hr)) {
    LOG("Could not enumerate audio endpoints: %x\n", hr);
    return CUBEB_ERROR;
  }

  hr = collection->GetCount(&cc);
  if (FAILED(hr)) {
    LOG("IMMDeviceCollection::GetCount() failed: %x\n", hr);
    return CUBEB_ERROR;
  }
  *out = (cubeb_device_collection *) malloc(sizeof(cubeb_device_collection) +
      sizeof(cubeb_device_info*) * (cc > 0 ? cc - 1 : 0));
  (*out)->count = 0;
  for (i = 0; i < cc; i++) {
    hr = collection->Item(i, &dev);
    if (FAILED(hr)) {
      LOG("IMMDeviceCollection::Item(%u) failed: %x\n", i-1, hr);
    } else if ((cur = wasapi_create_device(enumerator, dev)) != NULL) {
      (*out)->device[(*out)->count++] = cur;
    }
  }

  SafeRelease(collection);
  SafeRelease(enumerator);
  return CUBEB_OK;
}

cubeb_ops const wasapi_ops = {
  /*.init =*/ wasapi_init,
  /*.get_backend_id =*/ wasapi_get_backend_id,
  /*.get_max_channel_count =*/ wasapi_get_max_channel_count,
  /*.get_min_latency =*/ wasapi_get_min_latency,
  /*.get_preferred_sample_rate =*/ wasapi_get_preferred_sample_rate,
  /*.enumerate_devices =*/ wasapi_enumerate_devices,
  /*.destroy =*/ wasapi_destroy,
  /*.stream_init =*/ wasapi_stream_init,
  /*.stream_destroy =*/ wasapi_stream_destroy,
  /*.stream_start =*/ wasapi_stream_start,
  /*.stream_stop =*/ wasapi_stream_stop,
  /*.stream_get_position =*/ wasapi_stream_get_position,
  /*.stream_get_latency =*/ wasapi_stream_get_latency,
  /*.stream_set_volume =*/ wasapi_stream_set_volume,
  /*.stream_set_panning =*/ NULL,
  /*.stream_get_current_device =*/ NULL,
  /*.stream_device_destroy =*/ NULL,
  /*.stream_register_device_changed_callback =*/ NULL
};
} // namespace anonymous
