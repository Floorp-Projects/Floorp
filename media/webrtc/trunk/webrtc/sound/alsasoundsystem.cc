/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/alsasoundsystem.h"

#include <algorithm>
#include "webrtc/sound/sounddevicelocator.h"
#include "webrtc/sound/soundinputstreaminterface.h"
#include "webrtc/sound/soundoutputstreaminterface.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/worker.h"

namespace rtc {

// Lookup table from the rtc format enum in soundsysteminterface.h to
// ALSA's enums.
static const snd_pcm_format_t kCricketFormatToAlsaFormatTable[] = {
  // The order here must match the order in soundsysteminterface.h
  SND_PCM_FORMAT_S16_LE,
};

// Lookup table for the size of a single sample of a given format.
static const size_t kCricketFormatToSampleSizeTable[] = {
  // The order here must match the order in soundsysteminterface.h
  sizeof(int16_t),  // 2
};

// Minimum latency we allow, in microseconds. This is more or less arbitrary,
// but it has to be at least large enough to be able to buffer data during a
// missed context switch, and the typical Linux scheduling quantum is 10ms.
static const int kMinimumLatencyUsecs = 20 * 1000;

// The latency we'll use for kNoLatencyRequirements (chosen arbitrarily).
static const int kDefaultLatencyUsecs = kMinimumLatencyUsecs * 2;

// We translate newlines in ALSA device descriptions to hyphens.
static const char kAlsaDescriptionSearch[] = "\n";
static const char kAlsaDescriptionReplace[] = " - ";

class AlsaDeviceLocator : public SoundDeviceLocator {
 public:
  AlsaDeviceLocator(const std::string &name,
                    const std::string &device_name)
      : SoundDeviceLocator(name, device_name) {
    // The ALSA descriptions have newlines in them, which won't show up in
    // a drop-down box. Replace them with hyphens.
    rtc::replace_substrs(kAlsaDescriptionSearch,
                               sizeof(kAlsaDescriptionSearch) - 1,
                               kAlsaDescriptionReplace,
                               sizeof(kAlsaDescriptionReplace) - 1,
                               &name_);
  }

  virtual SoundDeviceLocator *Copy() const {
    return new AlsaDeviceLocator(*this);
  }
};

// Functionality that is common to both AlsaInputStream and AlsaOutputStream.
class AlsaStream {
 public:
  AlsaStream(AlsaSoundSystem *alsa,
             snd_pcm_t *handle,
             size_t frame_size,
             int wait_timeout_ms,
             int flags,
             int freq)
      : alsa_(alsa),
        handle_(handle),
        frame_size_(frame_size),
        wait_timeout_ms_(wait_timeout_ms),
        flags_(flags),
        freq_(freq) {
  }

  ~AlsaStream() {
    Close();
  }

  // Waits for the stream to be ready to accept/return more data, and returns
  // how much can be written/read, or 0 if we need to Wait() again.
  snd_pcm_uframes_t Wait() {
    snd_pcm_sframes_t frames;
    // Ideally we would not use snd_pcm_wait() and instead hook snd_pcm_poll_*
    // into PhysicalSocketServer, but PhysicalSocketServer is nasty enough
    // already and the current clients of SoundSystemInterface do not run
    // anything else on their worker threads, so snd_pcm_wait() is good enough.
    frames = symbol_table()->snd_pcm_avail_update()(handle_);
    if (frames < 0) {
      LOG(LS_ERROR) << "snd_pcm_avail_update(): " << GetError(frames);
      Recover(frames);
      return 0;
    } else if (frames > 0) {
      // Already ready, so no need to wait.
      return frames;
    }
    // Else no space/data available, so must wait.
    int ready = symbol_table()->snd_pcm_wait()(handle_, wait_timeout_ms_);
    if (ready < 0) {
      LOG(LS_ERROR) << "snd_pcm_wait(): " << GetError(ready);
      Recover(ready);
      return 0;
    } else if (ready == 0) {
      // Timeout, so nothing can be written/read right now.
      // We set the timeout to twice the requested latency, so continuous
      // timeouts are indicative of a problem, so log as a warning.
      LOG(LS_WARNING) << "Timeout while waiting on stream";
      return 0;
    }
    // Else ready > 0 (i.e., 1), so it's ready. Get count.
    frames = symbol_table()->snd_pcm_avail_update()(handle_);
    if (frames < 0) {
      LOG(LS_ERROR) << "snd_pcm_avail_update(): " << GetError(frames);
      Recover(frames);
      return 0;
    } else if (frames == 0) {
      // wait() said we were ready, so this ought to have been positive. Has
      // been observed to happen in practice though.
      LOG(LS_WARNING) << "Spurious wake-up";
    }
    return frames;
  }

  int CurrentDelayUsecs() {
    if (!(flags_ & SoundSystemInterface::FLAG_REPORT_LATENCY)) {
      return 0;
    }

    snd_pcm_sframes_t delay;
    int err = symbol_table()->snd_pcm_delay()(handle_, &delay);
    if (err != 0) {
      LOG(LS_ERROR) << "snd_pcm_delay(): " << GetError(err);
      Recover(err);
      // We'd rather continue playout/capture with an incorrect delay than stop
      // it altogether, so return a valid value.
      return 0;
    }
    // The delay is in frames. Convert to microseconds.
    return delay * rtc::kNumMicrosecsPerSec / freq_;
  }

  // Used to recover from certain recoverable errors, principally buffer overrun
  // or underrun (identified as EPIPE). Without calling this the stream stays
  // in the error state forever.
  bool Recover(int error) {
    int err;
    err = symbol_table()->snd_pcm_recover()(
        handle_,
        error,
        // Silent; i.e., no logging on stderr.
        1);
    if (err != 0) {
      // Docs say snd_pcm_recover returns the original error if it is not one
      // of the recoverable ones, so this log message will probably contain the
      // same error twice.
      LOG(LS_ERROR) << "Unable to recover from \"" << GetError(error) << "\": "
                    << GetError(err);
      return false;
    }
    if (error == -EPIPE &&  // Buffer underrun/overrun.
        symbol_table()->snd_pcm_stream()(handle_) == SND_PCM_STREAM_CAPTURE) {
      // For capture streams we also have to repeat the explicit start() to get
      // data flowing again.
      err = symbol_table()->snd_pcm_start()(handle_);
      if (err != 0) {
        LOG(LS_ERROR) << "snd_pcm_start(): " << GetError(err);
        return false;
      }
    }
    return true;
  }

  bool Close() {
    if (handle_) {
      int err;
      err = symbol_table()->snd_pcm_drop()(handle_);
      if (err != 0) {
        LOG(LS_ERROR) << "snd_pcm_drop(): " << GetError(err);
        // Continue anyways.
      }
      err = symbol_table()->snd_pcm_close()(handle_);
      if (err != 0) {
        LOG(LS_ERROR) << "snd_pcm_close(): " << GetError(err);
        // Continue anyways.
      }
      handle_ = NULL;
    }
    return true;
  }

  AlsaSymbolTable *symbol_table() {
    return &alsa_->symbol_table_;
  }

  snd_pcm_t *handle() {
    return handle_;
  }

  const char *GetError(int err) {
    return alsa_->GetError(err);
  }

  size_t frame_size() {
    return frame_size_;
  }

 private:
  AlsaSoundSystem *alsa_;
  snd_pcm_t *handle_;
  size_t frame_size_;
  int wait_timeout_ms_;
  int flags_;
  int freq_;

  DISALLOW_COPY_AND_ASSIGN(AlsaStream);
};

// Implementation of an input stream. See soundinputstreaminterface.h regarding
// thread-safety.
class AlsaInputStream :
    public SoundInputStreamInterface,
    private rtc::Worker {
 public:
  AlsaInputStream(AlsaSoundSystem *alsa,
                  snd_pcm_t *handle,
                  size_t frame_size,
                  int wait_timeout_ms,
                  int flags,
                  int freq)
      : stream_(alsa, handle, frame_size, wait_timeout_ms, flags, freq),
        buffer_size_(0) {
  }

  virtual ~AlsaInputStream() {
    bool success = StopReading();
    // We need that to live.
    VERIFY(success);
  }

  virtual bool StartReading() {
    return StartWork();
  }

  virtual bool StopReading() {
    return StopWork();
  }

  virtual bool GetVolume(int *volume) {
    // TODO: Implement this.
    return false;
  }

  virtual bool SetVolume(int volume) {
    // TODO: Implement this.
    return false;
  }

  virtual bool Close() {
    return StopReading() && stream_.Close();
  }

  virtual int LatencyUsecs() {
    return stream_.CurrentDelayUsecs();
  }

 private:
  // Inherited from Worker.
  virtual void OnStart() {
    HaveWork();
  }

  // Inherited from Worker.
  virtual void OnHaveWork() {
    // Block waiting for data.
    snd_pcm_uframes_t avail = stream_.Wait();
    if (avail > 0) {
      // Data is available.
      size_t size = avail * stream_.frame_size();
      if (size > buffer_size_) {
        // Must increase buffer size.
        buffer_.reset(new char[size]);
        buffer_size_ = size;
      }
      // Read all the data.
      snd_pcm_sframes_t read = stream_.symbol_table()->snd_pcm_readi()(
          stream_.handle(),
          buffer_.get(),
          avail);
      if (read < 0) {
        LOG(LS_ERROR) << "snd_pcm_readi(): " << GetError(read);
        stream_.Recover(read);
      } else if (read == 0) {
        // Docs say this shouldn't happen.
        ASSERT(false);
        LOG(LS_ERROR) << "No data?";
      } else {
        // Got data. Pass it off to the app.
        SignalSamplesRead(buffer_.get(),
                          read * stream_.frame_size(),
                          this);
      }
    }
    // Check for more data with no delay, after any pending messages are
    // dispatched.
    HaveWork();
  }

  // Inherited from Worker.
  virtual void OnStop() {
    // Nothing to do.
  }

  const char *GetError(int err) {
    return stream_.GetError(err);
  }

  AlsaStream stream_;
  rtc::scoped_ptr<char[]> buffer_;
  size_t buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(AlsaInputStream);
};

// Implementation of an output stream. See soundoutputstreaminterface.h
// regarding thread-safety.
class AlsaOutputStream :
    public SoundOutputStreamInterface,
    private rtc::Worker {
 public:
  AlsaOutputStream(AlsaSoundSystem *alsa,
                   snd_pcm_t *handle,
                   size_t frame_size,
                   int wait_timeout_ms,
                   int flags,
                   int freq)
      : stream_(alsa, handle, frame_size, wait_timeout_ms, flags, freq) {
  }

  virtual ~AlsaOutputStream() {
    bool success = DisableBufferMonitoring();
    // We need that to live.
    VERIFY(success);
  }

  virtual bool EnableBufferMonitoring() {
    return StartWork();
  }

  virtual bool DisableBufferMonitoring() {
    return StopWork();
  }

  virtual bool WriteSamples(const void *sample_data,
                            size_t size) {
    if (size % stream_.frame_size() != 0) {
      // No client of SoundSystemInterface does this, so let's not support it.
      // (If we wanted to support it, we'd basically just buffer the fractional
      // frame until we get more data.)
      ASSERT(false);
      LOG(LS_ERROR) << "Writes with fractional frames are not supported";
      return false;
    }
    snd_pcm_uframes_t frames = size / stream_.frame_size();
    snd_pcm_sframes_t written = stream_.symbol_table()->snd_pcm_writei()(
        stream_.handle(),
        sample_data,
        frames);
    if (written < 0) {
      LOG(LS_ERROR) << "snd_pcm_writei(): " << GetError(written);
      stream_.Recover(written);
      return false;
    } else if (static_cast<snd_pcm_uframes_t>(written) < frames) {
      // Shouldn't happen. Drop the rest of the data.
      LOG(LS_ERROR) << "Stream wrote only " << written << " of " << frames
                    << " frames!";
      return false;
    }
    return true;
  }

  virtual bool GetVolume(int *volume) {
    // TODO: Implement this.
    return false;
  }

  virtual bool SetVolume(int volume) {
    // TODO: Implement this.
    return false;
  }

  virtual bool Close() {
    return DisableBufferMonitoring() && stream_.Close();
  }

  virtual int LatencyUsecs() {
    return stream_.CurrentDelayUsecs();
  }

 private:
  // Inherited from Worker.
  virtual void OnStart() {
    HaveWork();
  }

  // Inherited from Worker.
  virtual void OnHaveWork() {
    snd_pcm_uframes_t avail = stream_.Wait();
    if (avail > 0) {
      size_t space = avail * stream_.frame_size();
      SignalBufferSpace(space, this);
    }
    HaveWork();
  }

  // Inherited from Worker.
  virtual void OnStop() {
    // Nothing to do.
  }

  const char *GetError(int err) {
    return stream_.GetError(err);
  }

  AlsaStream stream_;

  DISALLOW_COPY_AND_ASSIGN(AlsaOutputStream);
};

AlsaSoundSystem::AlsaSoundSystem() : initialized_(false) {}

AlsaSoundSystem::~AlsaSoundSystem() {
  // Not really necessary, because Terminate() doesn't really do anything.
  Terminate();
}

bool AlsaSoundSystem::Init() {
  if (IsInitialized()) {
    return true;
  }

  // Load libasound.
  if (!symbol_table_.Load()) {
    // Very odd for a Linux machine to not have a working libasound ...
    LOG(LS_ERROR) << "Failed to load symbol table";
    return false;
  }

  initialized_ = true;

  return true;
}

void AlsaSoundSystem::Terminate() {
  if (!IsInitialized()) {
    return;
  }

  initialized_ = false;

  // We do not unload the symbol table because we may need it again soon if
  // Init() is called again.
}

bool AlsaSoundSystem::EnumeratePlaybackDevices(
    SoundDeviceLocatorList *devices) {
  return EnumerateDevices(devices, false);
}

bool AlsaSoundSystem::EnumerateCaptureDevices(
    SoundDeviceLocatorList *devices) {
  return EnumerateDevices(devices, true);
}

bool AlsaSoundSystem::GetDefaultPlaybackDevice(SoundDeviceLocator **device) {
  return GetDefaultDevice(device);
}

bool AlsaSoundSystem::GetDefaultCaptureDevice(SoundDeviceLocator **device) {
  return GetDefaultDevice(device);
}

SoundOutputStreamInterface *AlsaSoundSystem::OpenPlaybackDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params) {
  return OpenDevice<SoundOutputStreamInterface>(
      device,
      params,
      SND_PCM_STREAM_PLAYBACK,
      &AlsaSoundSystem::StartOutputStream);
}

SoundInputStreamInterface *AlsaSoundSystem::OpenCaptureDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params) {
  return OpenDevice<SoundInputStreamInterface>(
      device,
      params,
      SND_PCM_STREAM_CAPTURE,
      &AlsaSoundSystem::StartInputStream);
}

const char *AlsaSoundSystem::GetName() const {
  return "ALSA";
}

bool AlsaSoundSystem::EnumerateDevices(
    SoundDeviceLocatorList *devices,
    bool capture_not_playback) {
  ClearSoundDeviceLocatorList(devices);

  if (!IsInitialized()) {
    return false;
  }

  const char *type = capture_not_playback ? "Input" : "Output";
  // dmix and dsnoop are only for playback and capture, respectively, but ALSA
  // stupidly includes them in both lists.
  const char *ignore_prefix = capture_not_playback ? "dmix:" : "dsnoop:";
  // (ALSA lists many more "devices" of questionable interest, but we show them
  // just in case the weird devices may actually be desirable for some
  // users/systems.)
  const char *ignore_default = "default";
  const char *ignore_null = "null";
  const char *ignore_pulse = "pulse";
  // The 'pulse' entry has a habit of mysteriously disappearing when you query
  // a second time. Remove it from our list. (GIPS lib did the same thing.)
  int err;

  void **hints;
  err = symbol_table_.snd_device_name_hint()(-1,     // All cards
                                             "pcm",  // Only PCM devices
                                             &hints);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_device_name_hint(): " << GetError(err);
    return false;
  }

  for (void **list = hints; *list != NULL; ++list) {
    char *actual_type = symbol_table_.snd_device_name_get_hint()(*list, "IOID");
    if (actual_type) {  // NULL means it's both.
      bool wrong_type = (strcmp(actual_type, type) != 0);
      free(actual_type);
      if (wrong_type) {
        // Wrong type of device (i.e., input vs. output).
        continue;
      }
    }

    char *name = symbol_table_.snd_device_name_get_hint()(*list, "NAME");
    if (!name) {
      LOG(LS_ERROR) << "Device has no name???";
      // Skip it.
      continue;
    }

    // Now check if we actually want to show this device.
    if (strcmp(name, ignore_default) != 0 &&
        strcmp(name, ignore_null) != 0 &&
        strcmp(name, ignore_pulse) != 0 &&
        !rtc::starts_with(name, ignore_prefix)) {

      // Yes, we do.
      char *desc = symbol_table_.snd_device_name_get_hint()(*list, "DESC");
      if (!desc) {
        // Virtual devices don't necessarily have descriptions. Use their names
        // instead (not pretty!).
        desc = name;
      }

      AlsaDeviceLocator *device = new AlsaDeviceLocator(desc, name);

      devices->push_back(device);

      if (desc != name) {
        free(desc);
      }
    }

    free(name);
  }

  err = symbol_table_.snd_device_name_free_hint()(hints);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_device_name_free_hint(): " << GetError(err);
    // Continue and return true anyways, since we did get the whole list.
  }

  return true;
}

bool AlsaSoundSystem::GetDefaultDevice(SoundDeviceLocator **device) {
  if (!IsInitialized()) {
    return false;
  }
  *device = new AlsaDeviceLocator("Default device", "default");
  return true;
}

inline size_t AlsaSoundSystem::FrameSize(const OpenParams &params) {
  ASSERT(static_cast<int>(params.format) <
         ARRAY_SIZE(kCricketFormatToSampleSizeTable));
  return kCricketFormatToSampleSizeTable[params.format] * params.channels;
}

template <typename StreamInterface>
StreamInterface *AlsaSoundSystem::OpenDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params,
    snd_pcm_stream_t type,
    StreamInterface *(AlsaSoundSystem::*start_fn)(
        snd_pcm_t *handle,
        size_t frame_size,
        int wait_timeout_ms,
        int flags,
        int freq)) {

  if (!IsInitialized()) {
    return NULL;
  }

  StreamInterface *stream;
  int err;

  const char *dev = static_cast<const AlsaDeviceLocator *>(device)->
      device_name().c_str();

  snd_pcm_t *handle = NULL;
  err = symbol_table_.snd_pcm_open()(
      &handle,
      dev,
      type,
      // No flags.
      0);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_pcm_open(" << dev << "): " << GetError(err);
    return NULL;
  }
  LOG(LS_VERBOSE) << "Opening " << dev;
  ASSERT(handle);  // If open succeeded, handle ought to be valid

  // Compute requested latency in microseconds.
  int latency;
  if (params.latency == kNoLatencyRequirements) {
    latency = kDefaultLatencyUsecs;
  } else {
    // kLowLatency is 0, so we treat it the same as a request for zero latency.
    // Compute what the user asked for.
    latency = rtc::kNumMicrosecsPerSec *
        params.latency /
        params.freq /
        FrameSize(params);
    // And this is what we'll actually use.
    latency = std::max(latency, kMinimumLatencyUsecs);
  }

  ASSERT(static_cast<int>(params.format) <
         ARRAY_SIZE(kCricketFormatToAlsaFormatTable));

  err = symbol_table_.snd_pcm_set_params()(
      handle,
      kCricketFormatToAlsaFormatTable[params.format],
      // SoundSystemInterface only supports interleaved audio.
      SND_PCM_ACCESS_RW_INTERLEAVED,
      params.channels,
      params.freq,
      1,  // Allow ALSA to resample.
      latency);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_pcm_set_params(): " << GetError(err);
    goto fail;
  }

  err = symbol_table_.snd_pcm_prepare()(handle);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_pcm_prepare(): " << GetError(err);
    goto fail;
  }

  stream = (this->*start_fn)(
      handle,
      FrameSize(params),
      // We set the wait time to twice the requested latency, so that wait
      // timeouts should be rare.
      2 * latency / rtc::kNumMicrosecsPerMillisec,
      params.flags,
      params.freq);
  if (stream) {
    return stream;
  }
  // Else fall through.

 fail:
  err = symbol_table_.snd_pcm_close()(handle);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_pcm_close(): " << GetError(err);
  }
  return NULL;
}

SoundOutputStreamInterface *AlsaSoundSystem::StartOutputStream(
    snd_pcm_t *handle,
    size_t frame_size,
    int wait_timeout_ms,
    int flags,
    int freq) {
  // Nothing to do here but instantiate the stream.
  return new AlsaOutputStream(
      this, handle, frame_size, wait_timeout_ms, flags, freq);
}

SoundInputStreamInterface *AlsaSoundSystem::StartInputStream(
    snd_pcm_t *handle,
    size_t frame_size,
    int wait_timeout_ms,
    int flags,
    int freq) {
  // Output streams start automatically once enough data has been written, but
  // input streams must be started manually or else snd_pcm_wait() will never
  // return true.
  int err;
  err = symbol_table_.snd_pcm_start()(handle);
  if (err != 0) {
    LOG(LS_ERROR) << "snd_pcm_start(): " << GetError(err);
    return NULL;
  }
  return new AlsaInputStream(
      this, handle, frame_size, wait_timeout_ms, flags, freq);
}

inline const char *AlsaSoundSystem::GetError(int err) {
  return symbol_table_.snd_strerror()(err);
}

}  // namespace rtc
