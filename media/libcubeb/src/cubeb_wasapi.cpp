/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <assert.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <windef.h>
#include <audioclient.h>
#include <math.h>
#include <process.h>
#include <avrt.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb/cubeb-stdint.h"
#include "cubeb-speex-resampler.h"
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

double
hns_to_s(uint32_t hns)
{
  return static_cast<double>(hns) / 10000000;
}

long
frame_count_at_rate(long frame_count, float rate)
{
  return static_cast<long>(ceilf(rate * frame_count) + 1);
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

typedef void (*refill_function2)(cubeb_stream * stm,
                                float * data, long frames_needed);

typedef HANDLE (WINAPI *set_mm_thread_characteristics_function)(
                                      const char* TaskName, LPDWORD TaskIndex);
typedef BOOL (WINAPI *revert_mm_thread_characteristics_function)(HANDLE handle);

extern cubeb_ops const wasapi_ops;
}

struct cubeb
{
  cubeb_ops const * ops;
  IMMDevice * device;
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
  /* Resampler instance. If this is !NULL, resampling should happen. */
  SpeexResamplerState * resampler;
  /* Buffer to resample from, into the upmix buffer or the final buffer. */
  float * resampling_src_buffer;
  /* Pointer to the function used to refill the buffer, depending
   * on the respective samplerate of the stream and the mix. */
  refill_function2 refill_function;
  /* Leftover frames handling, only used when resampling. */
  uint32_t leftover_frame_count;
  uint32_t leftover_frame_size;
  float * leftover_frames_buffer;
  /* upmix buffer of size |buffer_frame_count * bytes_per_frame / 2|. */
  float * upmix_buffer;
  /* Number of bytes per frame. Prefer to use frames_to_bytes_before_upmix. */
  uint8_t bytes_per_frame;
  /* True if the stream is draining. */
  bool draining;
};

namespace {
bool should_upmix(cubeb_stream * stream)
{
  return stream->upmix_buffer;
}

/* Upmix function, copies a mono channel in two interleaved
 * stereo channel. |out| has to be twice as long as |in| */
template<typename T>
void
mono_to_stereo(T * in, long insamples, T * out)
{
  int j = 0;
  for (int i = 0; i < insamples; i++, j+=2) {
    out[j] = out[j + 1] = in[i];
  }
}

template<typename T>
void
upmix(T * in, long inframes, T * out, int32_t in_channels, int32_t out_channels)
{
  /* If we are playing a mono stream over stereo speakers, copy the data over. */
  if (in_channels == 1 && out_channels == 2) {
    mono_to_stereo(in, inframes, out);
    return;
  }
  /* Otherwise, put silence in other channels. */
  long out_index = 0;
  for (long i = 0; i < inframes * in_channels; i+=in_channels) {
    for (int j = 0; j < in_channels; j++) {
      out[out_index + j] = in[i + j];
    }
    for (int j = in_channels; j < out_channels; j++) {
      out[out_index + j] = 0.0;
    }
    out_index += out_channels;
  }
}

/* This returns the size of a frame in the stream,
 * before the eventual upmix occurs. */
static size_t
frame_to_bytes_before_upmix(cubeb_stream * stm, size_t frames)
{
  return stm->bytes_per_frame / (should_upmix(stm) ? 2 : 1) * frames;
}

void
refill_with_resampling(cubeb_stream * stm, float * data, long frames_needed)
{
  /* Use more input frames that strictly necessary, so in the worst case,
   * we have leftover unresampled frames at the end, that we can use
   * during the next iteration. */
  float rate =
    static_cast<float>(stm->stream_params.rate) / stm->mix_params.rate;

  long before_resampling = frame_count_at_rate(frames_needed, rate);

  long frame_requested = before_resampling - stm->leftover_frame_count;

  size_t leftover_bytes =
    frame_to_bytes_before_upmix(stm, stm->leftover_frame_count);

  /* Copy the previous leftover frames to the front of the buffer. */
  memcpy(stm->resampling_src_buffer, stm->leftover_frames_buffer, leftover_bytes);
  uint8_t * buffer_start = reinterpret_cast<uint8_t *>(
                                  stm->resampling_src_buffer) + leftover_bytes;

  long got = stm->data_callback(stm, stm->user_ptr, buffer_start, frame_requested);

  if (got != frame_requested) {
    stm->draining = true;
  }

  uint32_t in_frames = before_resampling;
  uint32_t out_frames = frames_needed;

  /* if we need to upmix after resampling, resample into
   * the upmix buffer to avoid a copy */
  float * resample_dest;
  if (should_upmix(stm)) {
    resample_dest = stm->upmix_buffer;
  } else {
    resample_dest = data;
  }

  speex_resampler_process_interleaved_float(stm->resampler,
                                            stm->resampling_src_buffer,
                                            &in_frames,
                                            resample_dest,
                                            &out_frames);

  /* Copy the leftover frames to buffer for the next time. */
  stm->leftover_frame_count = before_resampling - in_frames;
  size_t unresampled_bytes =
    frame_to_bytes_before_upmix(stm, stm->leftover_frame_count);

  uint8_t * leftover_frames_start =
    reinterpret_cast<uint8_t *>(stm->resampling_src_buffer);
  leftover_frames_start += frame_to_bytes_before_upmix(stm, in_frames);

  assert(stm->leftover_frame_count <= stm->leftover_frame_size);
  memcpy(stm->leftover_frames_buffer, leftover_frames_start, unresampled_bytes);

  /* If this is not true, there will be glitches.
   * It is alright to have produced less frames if we are draining, though. */
  assert(out_frames == frames_needed || stm->draining);

  if (should_upmix(stm)) {
    upmix(resample_dest, out_frames, data,
          stm->stream_params.channels, stm->mix_params.channels);
  }
}

void
refill(cubeb_stream * stm, float * data, long frames_needed)
{
  /* If we need to upmix after resampling, get the data into
   * the upmix buffer to avoid a copy. */
  float * dest;
  if (should_upmix(stm)) {
    dest = stm->upmix_buffer;
  } else {
    dest = data;
  }

  long got = stm->data_callback(stm, stm->user_ptr, dest, frames_needed);

  if (got != frames_needed) {
    LOG("draining.");
    stm->draining = true;
  }

  if (should_upmix(stm)) {
    upmix(dest, got, data,
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
    LOG("Unable to use mmcss to bump the render thread priority: %d",
        GetLastError());
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
        stm->refill_function(stm, reinterpret_cast<float *>(data), available);

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
    };
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
} // namespace anonymous

extern "C" {
int wasapi_init(cubeb ** context, char const * context_name)
{
  HRESULT hr;
  IMMDeviceEnumerator * enumerator = NULL;

  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    LOG("Could not init COM.");
    return CUBEB_ERROR;
  }

  cubeb * ctx = (cubeb *)calloc(1, sizeof(cubeb));

  ctx->ops = &wasapi_ops;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                        NULL, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator.");
    wasapi_destroy(ctx);
    return CUBEB_ERROR;
  }

  /* eMultimedia is okay for now ("Music, movies, narration, [...]").
   * We will need to change this when we distinguish streams by use-case, other
   * possible values being eConsole ("Games, system notification sounds [...]")
   * and eCommunication ("Voice communication"). */
  hr = enumerator->GetDefaultAudioEndpoint(eRender,
                                           eMultimedia,
                                           &ctx->device);
  if (FAILED(hr)) {
    LOG("Could not get default audio endpoint.");
    SafeRelease(enumerator);
    wasapi_destroy(ctx);
    return CUBEB_ERROR;
  }

  ctx->mmcss_module = LoadLibraryA("Avrt.dll");

  if (ctx->mmcss_module) {
    ctx->set_mm_thread_characteristics =
      (set_mm_thread_characteristics_function) GetProcAddress(
          ctx->mmcss_module, "AvSetMmThreadCharacteristicsA");
    ctx->revert_mm_thread_characteristics =
      (revert_mm_thread_characteristics_function) GetProcAddress(
          ctx->mmcss_module, "AvRevertMmThreadCharacteristics");
    if (!(ctx->set_mm_thread_characteristics && ctx->revert_mm_thread_characteristics)) {
      LOG("Could not load AvSetMmThreadCharacteristics or AvRevertMmThreadCharacteristics: %d", GetLastError());
      FreeLibrary(ctx->mmcss_module);
    }
  } else {
    // This is not a fatal error, but we might end up glitching when
    // the system is under high load.
    LOG("Could not load Avrt.dll");
    ctx->set_mm_thread_characteristics = &set_mm_thread_characteristics_noop;
    ctx->revert_mm_thread_characteristics = &revert_mm_thread_characteristics_noop;
  }
  
  SafeRelease(enumerator);

  *context = ctx;

  return CUBEB_OK;
}
}

namespace {

void wasapi_destroy(cubeb * context)
{
  SafeRelease(context->device);
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
  HRESULT hr;
  IAudioClient * client;
  WAVEFORMATEX * mix_format;

  assert(ctx && max_channels);

  hr = ctx->device->Activate(__uuidof(IAudioClient),
                             CLSCTX_INPROC_SERVER,
                             NULL, (void **)&client);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  hr = client->GetMixFormat(&mix_format);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  *max_channels = mix_format->nChannels;

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
  assert((*mix_format)->wFormatTag == WAVE_FORMAT_EXTENSIBLE);

  /* Common case: the hardware supports stereo, and the stream is mono or
   * stereo. Easy. */
  if ((*mix_format)->nChannels == 2 &&
      stream_params->channels <= 2) {
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
     * eventual upmix ourselve */
    LOG("Using WASAPI suggested format: channels: %d", closest->nChannels);
    CoTaskMemFree(*mix_format);
    *mix_format = closest;
  } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
    /* Not supported, no suggestion, there is a bug somewhere. */
    assert(false && "Format not supported, and no suggestion from WASAPI.");
  } else if (hr == S_OK) {
    LOG("Requested format accepted by WASAPI.");
  } else {
    assert(false && "Not reached.");
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

  /* 30ms in shared mode is the minimum we can get when using WASAPI */
  if (latency < 30) {
    LOG("Latency too low: got %u (30ms minimum)", latency);
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  /* we don't support more that two channels for now. */
  if (stream_params.channels > 2) {
    return CUBEB_ERROR_INVALID_FORMAT;
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
    LOG("Can't create the shutdown event, error: %d.", GetLastError());
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }


  if (!stm->refill_event) {
    SafeRelease(stm->shutdown_event);
    LOG("Can't create the refill event, error: %d.", GetLastError());
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  /* Get a client. We will get all other interfaces we need from
   * this pointer. */
  hr = context->device->Activate(__uuidof(IAudioClient),
                                 CLSCTX_INPROC_SERVER,
                                 NULL, (void **)&stm->client);
  if (FAILED(hr)) {
    LOG("Could not activate the device to get an audio client.");
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  /* We have to distinguish between the format the mixer uses,
  * and the format the stream we want to play uses. */
  stm->client->GetMixFormat(&mix_format);

  /* this is the number of bytes per frame after the eventual upmix. */
  stm->bytes_per_frame = static_cast<uint8_t>(mix_format->nBlockAlign);

  handle_channel_layout(stm, &mix_format, &stream_params);

  /* Shared mode WASAPI always supports float32 sample format, so this
   * is safe. */
  stm->mix_params.format = CUBEB_SAMPLE_FLOAT32NE;

  stm->mix_params.rate = mix_format->nSamplesPerSec;
  stm->mix_params.channels = mix_format->nChannels;

  float resampling_rate = static_cast<float>(stm->stream_params.rate) /
                          stm->mix_params.rate;


  if (resampling_rate != 1.0) {
    /* If we are playing a mono stream, we only resample one channel,
     * and copy it over, so we are always resampling the number
     * of channels of the stream, not the number of channels
     * that WASAPI wants. */
    stm->resampler = speex_resampler_init(stm->stream_params.channels,
                                          stm->stream_params.rate,
                                          stm->mix_params.rate,
                                          SPEEX_RESAMPLER_QUALITY_DESKTOP,
                                          NULL);
    if (!stm->resampler) {
      CoTaskMemFree(mix_format);
      wasapi_stream_destroy(stm);
      return CUBEB_ERROR;
    }

    /* Get a little buffer so we can store the leftover frames,
     * that is, the samples not consumed by the resampler that we will end up
     * using next time the render callback is called. */
    stm->leftover_frame_size = static_cast<uint32_t>(ceilf(1 / resampling_rate * 2) + 1);
    stm->leftover_frames_buffer = (float *)malloc(frame_to_bytes_before_upmix(stm, stm->leftover_frame_size));

    stm->refill_function = &refill_with_resampling;
  } else {
    stm->refill_function = &refill;
  }

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
    LOG("Could not get the buffer size from the client.");
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  assert(stm->mix_params.channels >= 2);

  if (stm->mix_params.channels != stm->stream_params.channels) {
    stm->upmix_buffer = (float *) malloc(frame_to_bytes_before_upmix(stm, stm->buffer_frame_count));
  }

  /* If we are going to resample, we will end up needing a buffer
   * to resample from, because speex's resampler does not do
   * in-place processing. Of course we need to take the resampling
   * factor and the channel layout into account. */
  if (stm->resampler) {
    size_t frames_needed = static_cast<size_t>(frame_count_at_rate(stm->buffer_frame_count, resampling_rate));
    stm->resampling_src_buffer = (float *)malloc(frame_to_bytes_before_upmix(stm, frames_needed));
  }

  hr = stm->client->SetEventHandle(stm->refill_event);
  if (FAILED(hr)) {
    LOG("Could set the event handle for the client.");
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetService(__uuidof(IAudioRenderClient),
                                     (void **)&stm->render_client);
  if (FAILED(hr)) {
    LOG("Could not get the render client.");
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->client->GetService(__uuidof(IAudioClock),
                                     (void **)&stm->audio_clock);
  if (FAILED(hr)) {
    LOG("Could not get the IAudioClock.");
    wasapi_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  hr = stm->audio_clock->GetFrequency(&stm->clock_freq);
  if (FAILED(hr)) {
    LOG("failed to get audio clock frequency.");
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

  if (stm->resampler) {
    speex_resampler_destroy(stm->resampler);
  }

  free(stm->leftover_frames_buffer);
  free(stm->resampling_src_buffer);
  free(stm->upmix_buffer);
  free(stm);
  CoUninitialize();
}

int wasapi_stream_start(cubeb_stream * stm)
{
  HRESULT hr;

  assert(stm);

  stm->thread = (HANDLE) _beginthreadex(NULL, 64 * 1024, wasapi_stream_render_loop, stm, 0, NULL);
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
  /*.get_max_channel_count*/ wasapi_get_max_channel_count,
  /*.destroy =*/ wasapi_destroy,
  /*.stream_init =*/ wasapi_stream_init,
  /*.stream_destroy =*/ wasapi_stream_destroy,
  /*.stream_start =*/ wasapi_stream_start,
  /*.stream_stop =*/ wasapi_stream_stop,
  /*.stream_get_position =*/ wasapi_stream_get_position
  ///*.stream_get_latency =*/ wasapi_stream_get_latency
 };
} // namespace anonymous

