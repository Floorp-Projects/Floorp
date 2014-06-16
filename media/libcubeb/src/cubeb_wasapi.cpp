/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <assert.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <windef.h>
#include <audioclient.h>
#include <process.h>
#include <avrt.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb/cubeb-stdint.h"
#include "cubeb_resampler.h"
#include <stdio.h>

#if 0
#  define LOG(...) do {         \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n");        \
} while(0);
#else
#  define LOG(...)
#endif

#define ARRAY_LENGTH(array_) \
  (sizeof(array_) / sizeof(array_[0]))

namespace {
uint32_t
ms_to_hns(uint32_t ms)
{
  return ms * 10000;
}

uint32_t
hns_to_ms(uint32_t hns)
{
  return hns / 10000;
}

double
hns_to_s(uint32_t hns)
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

typedef HANDLE (WINAPI *set_mm_thread_characteristics_function)(
                                      const char* TaskName, LPDWORD TaskIndex);
typedef BOOL (WINAPI *revert_mm_thread_characteristics_function)(HANDLE handle);

extern cubeb_ops const wasapi_ops;
}

struct cubeb
{
  cubeb_ops const * ops;
  /* Library dynamically opened to increase the render
   * thread priority, and the two function pointers we need. */
  HMODULE mmcss_module;
  set_mm_thread_characteristics_function set_mm_thread_characteristics;
  revert_mm_thread_characteristics_function revert_mm_thread_characteristics;
};

struct cubeb_stream
{
  cubeb * context;
  /* Mixer pameters. We need to convert the input
   * stream to this samplerate/channel layout, as WASAPI
   * does not resample nor upmix itself. */
  cubeb_stream_params mix_params;
  cubeb_stream_params stream_params;
  cubeb_state_callback state_callback;
  cubeb_data_callback data_callback;
  void * user_ptr;
  /* Main handle on the WASAPI stream. */
  IAudioClient * client;
  /* Interface pointer to use the event-driven interface. */
  IAudioRenderClient * render_client;
  /* Interface pointer to use the clock facilities. */
  IAudioClock * audio_clock;
  /* This event is set by the stream_stop and stream_destroy
   * function, so the render loop can exit properly. */
  HANDLE shutdown_event;
  /* This is set by WASAPI when we should refill the stream. */
  HANDLE refill_event;
  /* Each cubeb_stream has its own thread. */
  HANDLE thread;
  uint64_t clock_freq;
  /* Maximum number of frames we can be requested in a callback. */
  uint32_t buffer_frame_count;
  /* Resampler instance. Resampling will only happen if necessary. */
  cubeb_resampler * resampler;
  /* Buffer used to downmix or upmix to the number of channels the mixer has.
   * its size is |frames_to_bytes_before_mix(buffer_frame_count)|. */
  float * mix_buffer;
  /* True if the stream is draining. */
  bool draining;
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

/* Upmix function, copies a mono channel in two interleaved
 * stereo channel. |out| has to be twice as long as |in| */
template<typename T>
void
mono_to_stereo(T * in, long insamples, T * out)
{
  int j = 0;
  for (int i = 0; i < insamples; ++i, j += 2) {
    out[j] = out[j + 1] = in[i];
  }
}

template<typename T>
void
upmix(T * in, long inframes, T * out, int32_t in_channels, int32_t out_channels)
{
  assert(out_channels >= in_channels);
  /* If we are playing a mono stream over stereo speakers, copy the data over. */
  if (in_channels == 1 && out_channels == 2) {
    mono_to_stereo(in, inframes, out);
    return;
  }
  /* Otherwise, put silence in other channels. */
  long out_index = 0;
  for (long i = 0; i < inframes * in_channels; i += in_channels) {
    for (int j = 0; j < in_channels; ++j) {
      out[out_index + j] = in[i + j];
    }
    for (int j = in_channels; j < out_channels; ++j) {
      out[out_index + j] = 0.0;
    }
    out_index += out_channels;
  }
}

template<typename T>
void
downmix(T * in, long inframes, T * out, int32_t in_channels, int32_t out_channels)
{
  assert(in_channels >= out_channels);
  /* We could use a downmix matrix here, applying mixing weight based on the
   * channel, but directsound and winmm simply drop the channels that cannot be
   * rendered by the hardware, so we do the same for consistency. */
  long out_index = 0;
  for (long i = 0; i < inframes * in_channels; i += in_channels) {
    for (int j = 0; j < out_channels; ++j) {
      out[out_index + j] = in[i + j];
    }
    out_index += out_channels;
  }
}

/* This returns the size of a frame in the stream,
 * before the eventual upmix occurs. */
static size_t
frames_to_bytes_before_mix(cubeb_stream * stm, size_t frames)
{
  size_t stream_frame_size = stm->stream_params.channels * sizeof(float);
  return stream_frame_size * frames;
}

void
refill(cubeb_stream * stm, float * data, long frames_needed)
{
  /* If we need to upmix after resampling, resample into the mix buffer to
   * avoid a copy. */
  float * dest;
  if (should_upmix(stm) || should_downmix(stm)) {
    dest = stm->mix_buffer;
  } else {
    dest = data;
  }

  long out_frames = cubeb_resampler_fill(stm->resampler, dest, frames_needed);

  /* XXX: Handle this error. */
  if (out_frames < 0) {
    assert(false);
  }

  /* Go in draining mode if we got fewer frames than requested. */
  if (out_frames < frames_needed) {
    LOG("draining.");
    stm->draining = true;
  }

  /* If this is not true, there will be glitches.
   * It is alright to have produced less frames if we are draining, though. */
  assert(out_frames == frames_needed || stm->draining);

  if (should_upmix(stm)) {
    upmix(dest, out_frames, data,
          stm->stream_params.channels, stm->mix_params.channels);
  } else if (should_downmix(stm)) {
    downmix(dest, out_frames, data,
            stm->stream_params.channels, stm->mix_params.channels);
  }
}

static unsigned int __stdcall
wasapi_stream_render_loop(LPVOID stream)
{
  cubeb_stream * stm = static_cast<cubeb_stream *>(stream);

  bool is_playing = true;
  HANDLE wait_array[2] = {stm->shutdown_event, stm->refill_event};
  HANDLE mmcss_handle = NULL;
  HRESULT hr;
  bool first = true;
  DWORD mmcss_task_index = 0;

  /* We could consider using "Pro Audio" here for WebAudio and
   * maybe WebRTC. */
  mmcss_handle =
    stm->context->set_mm_thread_characteristics("Audio", &mmcss_task_index);
  if (!mmcss_handle) {
    /* This is not fatal, but we might glitch under heavy load. */
    LOG("Unable to use mmcss to bump the render thread priority: %x", GetLastError());
  }

  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    LOG("could not initialize COM in render thread: %x", hr);
    return hr;
  }

  while (is_playing) {
    DWORD waitResult = WaitForMultipleObjects(ARRAY_LENGTH(wait_array),
                                              wait_array,
                                              FALSE,
                                              INFINITE);

    switch (waitResult) {
    case WAIT_OBJECT_0: { /* shutdown */
      is_playing = false;
      /* We don't check if the drain is actually finished here, we just want to
       * shutdown. */
      if (stm->draining) {
        stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      }
      continue;
    }
    case WAIT_OBJECT_0 + 1: { /* refill */
      UINT32 padding;

      hr = stm->client->GetCurrentPadding(&padding);
      if (FAILED(hr)) {
        LOG("Failed to get padding");
        is_playing = false;
        continue;
      }
      assert(padding <= stm->buffer_frame_count);

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

      BYTE* data;
      hr = stm->render_client->GetBuffer(available, &data);
      if (SUCCEEDED(hr)) {
        refill(stm, reinterpret_cast<float *>(data), available);

        hr = stm->render_client->ReleaseBuffer(available, 0);
        if (FAILED(hr)) {
          LOG("failed to release buffer.");
          is_playing = false;
        }
      } else {
        LOG("failed to get buffer.");
        is_playing = false;
      }
    }
    break;
    default:
      LOG("case %d not handled in render loop.", waitResult);
      abort();
    }
  }

  if (FAILED(hr)) {
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  }

  stm->context->revert_mm_thread_characteristics(mmcss_handle);

  CoUninitialize();
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

HRESULT get_default_endpoint(IMMDevice ** device)
{
  IMMDeviceEnumerator * enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator.");
    return hr;
  }
  /* eMultimedia is okay for now ("Music, movies, narration, [...]").
   * We will need to change this when we distinguish streams by use-case, other
   * possible values being eConsole ("Games, system notification sounds [...]")
   * and eCommunication ("Voice communication"). */
  hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device);
  if (FAILED(hr)) {
    LOG("Could not get default audio endpoint.");
    SafeRelease(enumerator);
    return hr;
  }

  SafeRelease(enumerator);

  return ERROR_SUCCESS;
}
} // namespace anonymous

extern "C" {
int wasapi_init(cubeb ** context, char const * context_name)
{
  HRESULT hr;

  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    LOG("Could not init COM.");
    return CUBEB_ERROR;
  }

  /* We don't use the device yet, but need to make sure we can initialize one
     so that this backend is not incorrectly enabled on platforms that don't
     support WASAPI. */
  IMMDevice * device;
  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    LOG("Could not get device.");
    return CUBEB_ERROR;
  }
  SafeRelease(device);

  cubeb * ctx = (cubeb *)calloc(1, sizeof(cubeb));

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
      LOG("Could not load AvSetMmThreadCharacteristics or AvRevertMmThreadCharacteristics: %x", GetLastError());
      FreeLibrary(ctx->mmcss_module);
    }
  } else {
    // This is not a fatal error, but we might end up glitching when
    // the system is under high load.
    LOG("Could not load Avrt.dll");
    ctx->set_mm_thread_characteristics = &set_mm_thread_characteristics_noop;
    ctx->revert_mm_thread_characteristics = &revert_mm_thread_characteristics_noop;
  }

  *context = ctx;

  return CUBEB_OK;
}
}

namespace {

void wasapi_destroy(cubeb * context)
{
  if (context->mmcss_module) {
    FreeLibrary(context->mmcss_module);
  }
  free(context);
}

char const* wasapi_get_backend_id(cubeb * context)
{
  return "wasapi";
}

int
wasapi_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  IAudioClient * client;
  WAVEFORMATEX * mix_format;

  assert(ctx && max_channels);

  IMMDevice * device;
  HRESULT hr = get_default_endpoint(&device);
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

  /* The second parameter is for exclusive mode, that we don't use. */
  hr = client->GetDevicePeriod(&default_period, NULL);
  if (FAILED(hr)) {
    SafeRelease(client);
    return CUBEB_ERROR;
  }

  /* According to the docs, the best latency we can achieve is by synchronizing
   * the stream and the engine.
   * http://msdn.microsoft.com/en-us/library/windows/desktop/dd370871%28v=vs.85%29.aspx */
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

/* Based on the mix format and the stream format, try to find a way to play what
 * the user requested. */
static void
handle_channel_layout(cubeb_stream * stm,  WAVEFORMATEX ** mix_format, const cubeb_stream_params * stream_params)
{
  /* Common case: the hardware is stereo. Up-mixing and down-mixing will be
   * handled in the callback. */
  if ((*mix_format)->nChannels <= 2) {
    return;
  }

  /* Otherwise, the hardware supports more than two channels. */
  WAVEFORMATEX hw_mixformat = **mix_format;

  /* The docs say that GetMixFormat is always of type WAVEFORMATEXTENSIBLE [1],
   * so the reinterpret_cast below should be safe. In practice, this is not
   * true, and we just want to bail out and let the rest of the code find a good
   * conversion path instead of trying to make WASAPI do it by itself.
   * [1]: http://msdn.microsoft.com/en-us/library/windows/desktop/dd370811%28v=vs.85%29.aspx*/
  if ((*mix_format)->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
    return;
  }

  /* The hardware is in surround mode, we want to only use front left and front
   * right. Try that, and check if it works. */
  WAVEFORMATEXTENSIBLE * format_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>((*mix_format));
  switch (stream_params->channels) {
    case 1: /* Mono */
      format_pcm->dwChannelMask = KSAUDIO_SPEAKER_MONO;
      break;
    case 2: /* Stereo */
      format_pcm->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
      break;
    default:
      assert(false && "Channel layout not supported.");
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
     * eventual upmix/downmix ourselves */
    LOG("Using WASAPI suggested format: channels: %d", closest->nChannels);
    WAVEFORMATEXTENSIBLE * closest_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(closest);
    assert(closest_pcm->SubFormat == format_pcm->SubFormat);
    CoTaskMemFree(*mix_format);
    *mix_format = closest;
  } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
    /* Not supported, no suggestion. This should not happen, but it does in the
     * field with some sound cards. We restore the mix format, and let the rest
     * of the code figure out the right conversion path. */
    **mix_format = hw_mixformat;
  } else if (hr == S_OK) {
    LOG("Requested format accepted by WASAPI.");
  }
}

int
wasapi_stream_init(cubeb * context, cubeb_stream ** stream,
                   char const * stream_name, cubeb_stream_params stream_params,
                   unsigned int latency, cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  HRESULT hr;
  WAVEFORMATEX * mix_format;

  assert(context && stream);

  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    LOG("Could not initialize COM.");
    return CUBEB_ERROR;
  }

  cubeb_stream * stm = (cubeb_stream *)calloc(1, sizeof(cubeb_stream));

  assert(stm);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->stream_params = stream_params;
  stm->draining = false;

  stm->shutdown_event = CreateEvent(NULL, 0, 0, NULL);
  stm->refill_event = CreateEvent(NULL, 0, 0, NULL);

  if (!stm->shutdown_event) {
    LOG("Can't create the shutdown event, error: %x.", GetLastError());
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  if (!stm->refill_event) {
    SafeRelease(stm->shutdown_event);
    LOG("Can't create the refill event, error: %x.", GetLastError());
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  IMMDevice * device;
  hr = get_default_endpoint(&device);
  if (FAILED(hr)) {
    LOG("Could not get default endpoint, error: %x", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  /* Get a client. We will get all other interfaces we need from
   * this pointer. */
  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, (void **)&stm->client);
  SafeRelease(device);
  if (FAILED(hr)) {
    LOG("Could not activate the device to get an audio client: error: %x", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  /* We have to distinguish between the format the mixer uses,
  * and the format the stream we want to play uses. */
  hr = stm->client->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    LOG("Could not fetch current mix format from the audio client: error: %x", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  handle_channel_layout(stm, &mix_format, &stream_params);

  /* Shared mode WASAPI always supports float32 sample format, so this
   * is safe. */
  stm->mix_params.format = CUBEB_SAMPLE_FLOAT32NE;
  stm->mix_params.rate = mix_format->nSamplesPerSec;
  stm->mix_params.channels = mix_format->nChannels;

  hr = stm->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                               AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                               AUDCLNT_STREAMFLAGS_NOPERSIST,
                               ms_to_hns(latency),
                               0,
                               mix_format,
                               NULL);

  CoTaskMemFree(mix_format);

  if (FAILED(hr)) {
    LOG("Unable to initialize audio client: %x.", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetBufferSize(&stm->buffer_frame_count);
  if (FAILED(hr)) {
    LOG("Could not get the buffer size from the client %x.", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  if (should_upmix(stm) || should_downmix(stm)) {
    stm->mix_buffer = (float *) malloc(frames_to_bytes_before_mix(stm, stm->buffer_frame_count));
  }

  hr = stm->client->SetEventHandle(stm->refill_event);
  if (FAILED(hr)) {
    LOG("Could set the event handle for the client %x.", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetService(__uuidof(IAudioRenderClient),
                               (void **)&stm->render_client);
  if (FAILED(hr)) {
    LOG("Could not get the render client %x.", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetService(__uuidof(IAudioClock),
                               (void **)&stm->audio_clock);
  if (FAILED(hr)) {
    LOG("Could not get the IAudioClock, %x", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->audio_clock->GetFrequency(&stm->clock_freq);
  if (FAILED(hr)) {
    LOG("failed to get audio clock frequency, %x", hr);
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  /* If we are playing a mono stream, we only resample one channel,
   * and copy it over, so we are always resampling the number
   * of channels of the stream, not the number of channels
   * that WASAPI wants. */
  stm->resampler = cubeb_resampler_create(stm, stream_params,
                                          stm->mix_params.rate,
                                          data_callback,
                                          stm->buffer_frame_count,
                                          user_ptr,
                                          CUBEB_RESAMPLER_QUALITY_DESKTOP);
  if (!stm->resampler) {
    LOG("Could not get a resampler");
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

void wasapi_stream_destroy(cubeb_stream * stm)
{
  assert(stm);

  if (stm->thread) {
    SetEvent(stm->shutdown_event);
    WaitForSingleObject(stm->thread, INFINITE);
    CloseHandle(stm->thread);
    stm->thread = 0;
  }

  SafeRelease(stm->shutdown_event);
  SafeRelease(stm->refill_event);

  SafeRelease(stm->client);
  SafeRelease(stm->render_client);
  SafeRelease(stm->audio_clock);

  cubeb_resampler_destroy(stm->resampler);

  free(stm->mix_buffer);
  free(stm);
  CoUninitialize();
}

int wasapi_stream_start(cubeb_stream * stm)
{
  HRESULT hr;

  assert(stm);

  stm->thread = (HANDLE) _beginthreadex(NULL, 64 * 1024, wasapi_stream_render_loop, stm, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
  if (stm->thread == NULL) {
    LOG("could not create WASAPI render thread.");
    return CUBEB_ERROR;
  }

  hr = stm->client->Start();
  if (FAILED(hr)) {
    LOG("could not start the stream.");
    return CUBEB_ERROR;
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

int wasapi_stream_stop(cubeb_stream * stm)
{
  assert(stm && stm->shutdown_event);

  SetEvent(stm->shutdown_event);

  HRESULT hr = stm->client->Stop();
  if (FAILED(hr)) {
    LOG("could not stop AudioClient");
  }

  if (stm->thread) {
    WaitForSingleObject(stm->thread, INFINITE);
    CloseHandle(stm->thread);
    stm->thread = NULL;
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);

  return CUBEB_OK;
}

int wasapi_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  assert(stm && position);

  UINT64 pos;
  HRESULT hr;

  hr = stm->audio_clock->GetPosition(&pos, NULL);
  if (FAILED(hr)) {
    LOG("Could not get accurate position: %x\n", hr);
    return CUBEB_ERROR;
  }

  *position = static_cast<uint64_t>(static_cast<double>(pos) / stm->clock_freq * stm->stream_params.rate);

  return CUBEB_OK;
}

int wasapi_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  assert(stm && latency);

  /* The GetStreamLatency method only works if the
   * AudioClient has been initialized. */
  if (!stm->client) {
    return CUBEB_ERROR;
  }

  REFERENCE_TIME latency_hns;
  stm->client->GetStreamLatency(&latency_hns);
  double latency_s = hns_to_s(latency_hns);
  *latency = static_cast<uint32_t>(latency_s * stm->stream_params.rate);

  return CUBEB_OK;
}

cubeb_ops const wasapi_ops = {
  /*.init =*/ wasapi_init,
  /*.get_backend_id =*/ wasapi_get_backend_id,
  /*.get_max_channel_count =*/ wasapi_get_max_channel_count,
  /*.get_min_latency =*/ wasapi_get_min_latency,
  /*.get_preferred_sample_rate =*/ wasapi_get_preferred_sample_rate,
  /*.destroy =*/ wasapi_destroy,
  /*.stream_init =*/ wasapi_stream_init,
  /*.stream_destroy =*/ wasapi_stream_destroy,
  /*.stream_start =*/ wasapi_stream_start,
  /*.stream_stop =*/ wasapi_stream_stop,
  /*.stream_get_position =*/ wasapi_stream_get_position,
  /*.stream_get_latency =*/ wasapi_stream_get_latency
 };
} // namespace anonymous

