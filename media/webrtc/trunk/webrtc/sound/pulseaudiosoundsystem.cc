/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sound/pulseaudiosoundsystem.h"

#ifdef HAVE_LIBPULSE

#include <algorithm>
#include "webrtc/sound/sounddevicelocator.h"
#include "webrtc/sound/soundinputstreaminterface.h"
#include "webrtc/sound/soundoutputstreaminterface.h"
#include "webrtc/base/common.h"
#include "webrtc/base/fileutils.h"  // for GetApplicationName()
#include "webrtc/base/logging.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/worker.h"

namespace rtc {

// First PulseAudio protocol version that supports PA_STREAM_ADJUST_LATENCY.
static const uint32_t kAdjustLatencyProtocolVersion = 13;

// Lookup table from the rtc format enum in soundsysteminterface.h to
// Pulse's enums.
static const pa_sample_format_t kCricketFormatToPulseFormatTable[] = {
  // The order here must match the order in soundsysteminterface.h
  PA_SAMPLE_S16LE,
};

// Some timing constants for optimal operation. See
// https://tango.0pointer.de/pipermail/pulseaudio-discuss/2008-January/001170.html
// for a good explanation of some of the factors that go into this.

// Playback.

// For playback, there is a round-trip delay to fill the server-side playback
// buffer, so setting too low of a latency is a buffer underflow risk. We will
// automatically increase the latency if a buffer underflow does occur, but we
// also enforce a sane minimum at start-up time. Anything lower would be
// virtually guaranteed to underflow at least once, so there's no point in
// allowing lower latencies.
static const int kPlaybackLatencyMinimumMsecs = 20;
// Every time a playback stream underflows, we will reconfigure it with target
// latency that is greater by this amount.
static const int kPlaybackLatencyIncrementMsecs = 20;
// We also need to configure a suitable request size. Too small and we'd burn
// CPU from the overhead of transfering small amounts of data at once. Too large
// and the amount of data remaining in the buffer right before refilling it
// would be a buffer underflow risk. We set it to half of the buffer size.
static const int kPlaybackRequestFactor = 2;

// Capture.

// For capture, low latency is not a buffer overflow risk, but it makes us burn
// CPU from the overhead of transfering small amounts of data at once, so we set
// a recommended value that we use for the kLowLatency constant (but if the user
// explicitly requests something lower then we will honour it).
// 1ms takes about 6-7% CPU. 5ms takes about 5%. 10ms takes about 4.x%.
static const int kLowCaptureLatencyMsecs = 10;
// There is a round-trip delay to ack the data to the server, so the
// server-side buffer needs extra space to prevent buffer overflow. 20ms is
// sufficient, but there is no penalty to making it bigger, so we make it huge.
// (750ms is libpulse's default value for the _total_ buffer size in the
// kNoLatencyRequirements case.)
static const int kCaptureBufferExtraMsecs = 750;

static void FillPlaybackBufferAttr(int latency,
                                   pa_buffer_attr *attr) {
  attr->maxlength = latency;
  attr->tlength = latency;
  attr->minreq = latency / kPlaybackRequestFactor;
  attr->prebuf = attr->tlength - attr->minreq;
  LOG(LS_VERBOSE) << "Configuring latency = " << attr->tlength << ", minreq = "
                  << attr->minreq << ", minfill = " << attr->prebuf;
}

static pa_volume_t CricketVolumeToPulseVolume(int volume) {
  // PA's volume space goes from 0% at PA_VOLUME_MUTED (value 0) to 100% at
  // PA_VOLUME_NORM (value 0x10000). It can also go beyond 100% up to
  // PA_VOLUME_MAX (value UINT32_MAX-1), but using that is probably unwise.
  // We just linearly map the 0-255 scale of SoundSystemInterface onto
  // PA_VOLUME_MUTED-PA_VOLUME_NORM. If the programmer exceeds kMaxVolume then
  // they can access the over-100% features of PA.
  return PA_VOLUME_MUTED + (PA_VOLUME_NORM - PA_VOLUME_MUTED) *
      volume / SoundSystemInterface::kMaxVolume;
}

static int PulseVolumeToCricketVolume(pa_volume_t pa_volume) {
  return SoundSystemInterface::kMinVolume +
      (SoundSystemInterface::kMaxVolume - SoundSystemInterface::kMinVolume) *
      pa_volume / PA_VOLUME_NORM;
}

static pa_volume_t MaxChannelVolume(pa_cvolume *channel_volumes) {
  pa_volume_t pa_volume = PA_VOLUME_MUTED;  // Minimum possible value.
  for (int i = 0; i < channel_volumes->channels; ++i) {
    if (pa_volume < channel_volumes->values[i]) {
      pa_volume = channel_volumes->values[i];
    }
  }
  return pa_volume;
}

class PulseAudioDeviceLocator : public SoundDeviceLocator {
 public:
  PulseAudioDeviceLocator(const std::string &name,
                          const std::string &device_name)
      : SoundDeviceLocator(name, device_name) {
  }

  virtual SoundDeviceLocator *Copy() const {
    return new PulseAudioDeviceLocator(*this);
  }
};

// Functionality that is common to both PulseAudioInputStream and
// PulseAudioOutputStream.
class PulseAudioStream {
 public:
  PulseAudioStream(PulseAudioSoundSystem *pulse, pa_stream *stream, int flags)
      : pulse_(pulse), stream_(stream), flags_(flags) {
  }

  ~PulseAudioStream() {
    // Close() should have been called during the containing class's destructor.
    ASSERT(stream_ == NULL);
  }

  // Must be called with the lock held.
  bool Close() {
    if (!IsClosed()) {
      // Unset this here so that we don't get a TERMINATED callback.
      symbol_table()->pa_stream_set_state_callback()(stream_, NULL, NULL);
      if (symbol_table()->pa_stream_disconnect()(stream_) != 0) {
        LOG(LS_ERROR) << "Can't disconnect stream";
        // Continue and return true anyways.
      }
      symbol_table()->pa_stream_unref()(stream_);
      stream_ = NULL;
    }
    return true;
  }

  // Must be called with the lock held.
  int LatencyUsecs() {
    if (!(flags_ & SoundSystemInterface::FLAG_REPORT_LATENCY)) {
      return 0;
    }

    pa_usec_t latency;
    int negative;
    Lock();
    int re = symbol_table()->pa_stream_get_latency()(stream_, &latency,
        &negative);
    Unlock();
    if (re != 0) {
      LOG(LS_ERROR) << "Can't query latency";
      // We'd rather continue playout/capture with an incorrect delay than stop
      // it altogether, so return a valid value.
      return 0;
    }
    if (negative) {
      // The delay can be negative for monitoring streams if the captured
      // samples haven't been played yet. In such a case, "latency" contains the
      // magnitude, so we must negate it to get the real value.
      return -latency;
    } else {
      return latency;
    }
  }

  PulseAudioSoundSystem *pulse() {
    return pulse_;
  }

  PulseAudioSymbolTable *symbol_table() {
    return &pulse()->symbol_table_;
  }

  pa_stream *stream() {
    ASSERT(stream_ != NULL);
    return stream_;
  }

  bool IsClosed() {
    return stream_ == NULL;
  }

  void Lock() {
    pulse()->Lock();
  }

  void Unlock() {
    pulse()->Unlock();
  }

 private:
  PulseAudioSoundSystem *pulse_;
  pa_stream *stream_;
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioStream);
};

// Implementation of an input stream. See soundinputstreaminterface.h regarding
// thread-safety.
class PulseAudioInputStream :
    public SoundInputStreamInterface,
    private rtc::Worker {

  struct GetVolumeCallbackData {
    PulseAudioInputStream *instance;
    pa_cvolume *channel_volumes;
  };

  struct GetSourceChannelCountCallbackData {
    PulseAudioInputStream *instance;
    uint8_t *channels;
  };

 public:
  PulseAudioInputStream(PulseAudioSoundSystem *pulse,
                        pa_stream *stream,
                        int flags)
      : stream_(pulse, stream, flags),
        temp_sample_data_(NULL),
        temp_sample_data_size_(0) {
    // This callback seems to never be issued, but let's set it anyways.
    symbol_table()->pa_stream_set_overflow_callback()(stream, &OverflowCallback,
        NULL);
  }

  virtual ~PulseAudioInputStream() {
    bool success = Close();
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
    bool ret = false;

    Lock();

    // Unlike output streams, input streams have no concept of a stream volume,
    // only a device volume. So we have to retrieve the volume of the device
    // itself.

    pa_cvolume channel_volumes;

    GetVolumeCallbackData data;
    data.instance = this;
    data.channel_volumes = &channel_volumes;

    pa_operation *op = symbol_table()->pa_context_get_source_info_by_index()(
            stream_.pulse()->context_,
            symbol_table()->pa_stream_get_device_index()(stream_.stream()),
            &GetVolumeCallbackThunk,
            &data);
    if (!stream_.pulse()->FinishOperation(op)) {
      goto done;
    }

    if (data.channel_volumes) {
      // This pointer was never unset by the callback, so we must have received
      // an empty list of infos. This probably never happens, but we code for it
      // anyway.
      LOG(LS_ERROR) << "Did not receive GetVolumeCallback";
      goto done;
    }

    // We now have the volume for each channel. Each channel could have a
    // different volume if, e.g., the user went and changed the volumes in the
    // PA UI. To get a single volume for SoundSystemInterface we just take the
    // maximum. Ideally we'd do so with pa_cvolume_max, but it doesn't exist in
    // Hardy, so we do it manually.
    pa_volume_t pa_volume;
    pa_volume = MaxChannelVolume(&channel_volumes);
    // Now map onto the SoundSystemInterface range.
    *volume = PulseVolumeToCricketVolume(pa_volume);

    ret = true;
   done:
    Unlock();
    return ret;
  }

  virtual bool SetVolume(int volume) {
    bool ret = false;
    pa_volume_t pa_volume = CricketVolumeToPulseVolume(volume);

    Lock();

    // Unlike output streams, input streams have no concept of a stream volume,
    // only a device volume. So we have to change the volume of the device
    // itself.

    // The device may have a different number of channels than the stream and
    // their mapping may be different, so we don't want to use the channel count
    // from our sample spec. We could use PA_CHANNELS_MAX to cover our bases,
    // and the server allows that even if the device's channel count is lower,
    // but some buggy PA clients don't like that (the pavucontrol on Hardy dies
    // in an assert if the channel count is different). So instead we look up
    // the actual number of channels that the device has.

    uint8_t channels;

    GetSourceChannelCountCallbackData data;
    data.instance = this;
    data.channels = &channels;

    uint32_t device_index = symbol_table()->pa_stream_get_device_index()(
        stream_.stream());

    pa_operation *op = symbol_table()->pa_context_get_source_info_by_index()(
        stream_.pulse()->context_,
        device_index,
        &GetSourceChannelCountCallbackThunk,
        &data);
    if (!stream_.pulse()->FinishOperation(op)) {
      goto done;
    }

    if (data.channels) {
      // This pointer was never unset by the callback, so we must have received
      // an empty list of infos. This probably never happens, but we code for it
      // anyway.
      LOG(LS_ERROR) << "Did not receive GetSourceChannelCountCallback";
      goto done;
    }

    pa_cvolume channel_volumes;
    symbol_table()->pa_cvolume_set()(&channel_volumes, channels, pa_volume);

    op = symbol_table()->pa_context_set_source_volume_by_index()(
        stream_.pulse()->context_,
        device_index,
        &channel_volumes,
        // This callback merely logs errors.
        &SetVolumeCallback,
        NULL);
    if (!op) {
      LOG(LS_ERROR) << "pa_context_set_source_volume_by_index()";
      goto done;
    }
    // Don't need to wait for this to complete.
    symbol_table()->pa_operation_unref()(op);

    ret = true;
   done:
    Unlock();
    return ret;
  }

  virtual bool Close() {
    if (!StopReading()) {
      return false;
    }
    bool ret = true;
    if (!stream_.IsClosed()) {
      Lock();
      ret = stream_.Close();
      Unlock();
    }
    return ret;
  }

  virtual int LatencyUsecs() {
    return stream_.LatencyUsecs();
  }

 private:
  void Lock() {
    stream_.Lock();
  }

  void Unlock() {
    stream_.Unlock();
  }

  PulseAudioSymbolTable *symbol_table() {
    return stream_.symbol_table();
  }

  void EnableReadCallback() {
    symbol_table()->pa_stream_set_read_callback()(
         stream_.stream(),
         &ReadCallbackThunk,
         this);
  }

  void DisableReadCallback() {
    symbol_table()->pa_stream_set_read_callback()(
         stream_.stream(),
         NULL,
         NULL);
  }

  static void ReadCallbackThunk(pa_stream *unused1,
                                size_t unused2,
                                void *userdata) {
    PulseAudioInputStream *instance =
        static_cast<PulseAudioInputStream *>(userdata);
    instance->OnReadCallback();
  }

  void OnReadCallback() {
    // We get the data pointer and size now in order to save one Lock/Unlock
    // on OnMessage.
    if (symbol_table()->pa_stream_peek()(stream_.stream(),
                                         &temp_sample_data_,
                                         &temp_sample_data_size_) != 0) {
      LOG(LS_ERROR) << "Can't read data!";
      return;
    }
    // Since we consume the data asynchronously on a different thread, we have
    // to temporarily disable the read callback or else Pulse will call it
    // continuously until we consume the data. We re-enable it below.
    DisableReadCallback();
    HaveWork();
  }

  // Inherited from Worker.
  virtual void OnStart() {
    Lock();
    EnableReadCallback();
    Unlock();
  }

  // Inherited from Worker.
  virtual void OnHaveWork() {
    ASSERT(temp_sample_data_ && temp_sample_data_size_);
    SignalSamplesRead(temp_sample_data_,
                      temp_sample_data_size_,
                      this);
    temp_sample_data_ = NULL;
    temp_sample_data_size_ = 0;

    Lock();
    for (;;) {
      // Ack the last thing we read.
      if (symbol_table()->pa_stream_drop()(stream_.stream()) != 0) {
        LOG(LS_ERROR) << "Can't ack read data";
      }

      if (symbol_table()->pa_stream_readable_size()(stream_.stream()) <= 0) {
        // Then that was all the data.
        break;
      }

      // Else more data.
      const void *sample_data;
      size_t sample_data_size;
      if (symbol_table()->pa_stream_peek()(stream_.stream(),
                                           &sample_data,
                                           &sample_data_size) != 0) {
        LOG(LS_ERROR) << "Can't read data!";
        break;
      }

      // Drop lock for sigslot dispatch, which could take a while.
      Unlock();
      SignalSamplesRead(sample_data, sample_data_size, this);
      Lock();

      // Return to top of loop for the ack and the check for more data.
    }
    EnableReadCallback();
    Unlock();
  }

  // Inherited from Worker.
  virtual void OnStop() {
    Lock();
    DisableReadCallback();
    Unlock();
  }

  static void OverflowCallback(pa_stream *stream,
                               void *userdata) {
    LOG(LS_WARNING) << "Buffer overflow on capture stream " << stream;
  }

  static void GetVolumeCallbackThunk(pa_context *unused,
                                     const pa_source_info *info,
                                     int eol,
                                     void *userdata) {
    GetVolumeCallbackData *data =
        static_cast<GetVolumeCallbackData *>(userdata);
    data->instance->OnGetVolumeCallback(info, eol, &data->channel_volumes);
  }

  void OnGetVolumeCallback(const pa_source_info *info,
                           int eol,
                           pa_cvolume **channel_volumes) {
    if (eol) {
      // List is over. Wake GetVolume().
      stream_.pulse()->Signal();
      return;
    }

    if (*channel_volumes) {
      **channel_volumes = info->volume;
      // Unset the pointer so that we know that we have have already copied the
      // volume.
      *channel_volumes = NULL;
    } else {
      // We have received an additional callback after the first one, which
      // doesn't make sense for a single source. This probably never happens,
      // but we code for it anyway.
      LOG(LS_WARNING) << "Ignoring extra GetVolumeCallback";
    }
  }

  static void GetSourceChannelCountCallbackThunk(pa_context *unused,
                                                 const pa_source_info *info,
                                                 int eol,
                                                 void *userdata) {
    GetSourceChannelCountCallbackData *data =
        static_cast<GetSourceChannelCountCallbackData *>(userdata);
    data->instance->OnGetSourceChannelCountCallback(info, eol, &data->channels);
  }

  void OnGetSourceChannelCountCallback(const pa_source_info *info,
                                       int eol,
                                       uint8_t **channels) {
    if (eol) {
      // List is over. Wake SetVolume().
      stream_.pulse()->Signal();
      return;
    }

    if (*channels) {
      **channels = info->channel_map.channels;
      // Unset the pointer so that we know that we have have already copied the
      // channel count.
      *channels = NULL;
    } else {
      // We have received an additional callback after the first one, which
      // doesn't make sense for a single source. This probably never happens,
      // but we code for it anyway.
      LOG(LS_WARNING) << "Ignoring extra GetSourceChannelCountCallback";
    }
  }

  static void SetVolumeCallback(pa_context *unused1,
                                int success,
                                void *unused2) {
    if (!success) {
      LOG(LS_ERROR) << "Failed to change capture volume";
    }
  }

  PulseAudioStream stream_;
  // Temporary storage for passing data between threads.
  const void *temp_sample_data_;
  size_t temp_sample_data_size_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioInputStream);
};

// Implementation of an output stream. See soundoutputstreaminterface.h
// regarding thread-safety.
class PulseAudioOutputStream :
    public SoundOutputStreamInterface,
    private rtc::Worker {

  struct GetVolumeCallbackData {
    PulseAudioOutputStream *instance;
    pa_cvolume *channel_volumes;
  };

 public:
  PulseAudioOutputStream(PulseAudioSoundSystem *pulse,
                         pa_stream *stream,
                         int flags,
                         int latency)
      : stream_(pulse, stream, flags),
        configured_latency_(latency),
        temp_buffer_space_(0) {
    symbol_table()->pa_stream_set_underflow_callback()(stream,
                                                       &UnderflowCallbackThunk,
                                                       this);
  }

  virtual ~PulseAudioOutputStream() {
    bool success = Close();
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
    bool ret = true;
    Lock();
    if (symbol_table()->pa_stream_write()(stream_.stream(),
                                          sample_data,
                                          size,
                                          NULL,
                                          0,
                                          PA_SEEK_RELATIVE) != 0) {
      LOG(LS_ERROR) << "Unable to write";
      ret = false;
    }
    Unlock();
    return ret;
  }

  virtual bool GetVolume(int *volume) {
    bool ret = false;

    Lock();

    pa_cvolume channel_volumes;

    GetVolumeCallbackData data;
    data.instance = this;
    data.channel_volumes = &channel_volumes;

    pa_operation *op = symbol_table()->pa_context_get_sink_input_info()(
            stream_.pulse()->context_,
            symbol_table()->pa_stream_get_index()(stream_.stream()),
            &GetVolumeCallbackThunk,
            &data);
    if (!stream_.pulse()->FinishOperation(op)) {
      goto done;
    }

    if (data.channel_volumes) {
      // This pointer was never unset by the callback, so we must have received
      // an empty list of infos. This probably never happens, but we code for it
      // anyway.
      LOG(LS_ERROR) << "Did not receive GetVolumeCallback";
      goto done;
    }

    // We now have the volume for each channel. Each channel could have a
    // different volume if, e.g., the user went and changed the volumes in the
    // PA UI. To get a single volume for SoundSystemInterface we just take the
    // maximum. Ideally we'd do so with pa_cvolume_max, but it doesn't exist in
    // Hardy, so we do it manually.
    pa_volume_t pa_volume;
    pa_volume = MaxChannelVolume(&channel_volumes);
    // Now map onto the SoundSystemInterface range.
    *volume = PulseVolumeToCricketVolume(pa_volume);

    ret = true;
   done:
    Unlock();
    return ret;
  }

  virtual bool SetVolume(int volume) {
    bool ret = false;
    pa_volume_t pa_volume = CricketVolumeToPulseVolume(volume);

    Lock();

    const pa_sample_spec *spec = symbol_table()->pa_stream_get_sample_spec()(
        stream_.stream());
    if (!spec) {
      LOG(LS_ERROR) << "pa_stream_get_sample_spec()";
      goto done;
    }

    pa_cvolume channel_volumes;
    symbol_table()->pa_cvolume_set()(&channel_volumes, spec->channels,
        pa_volume);

    pa_operation *op;
    op = symbol_table()->pa_context_set_sink_input_volume()(
        stream_.pulse()->context_,
        symbol_table()->pa_stream_get_index()(stream_.stream()),
        &channel_volumes,
        // This callback merely logs errors.
        &SetVolumeCallback,
        NULL);
    if (!op) {
      LOG(LS_ERROR) << "pa_context_set_sink_input_volume()";
      goto done;
    }
    // Don't need to wait for this to complete.
    symbol_table()->pa_operation_unref()(op);

    ret = true;
   done:
    Unlock();
    return ret;
  }

  virtual bool Close() {
    if (!DisableBufferMonitoring()) {
      return false;
    }
    bool ret = true;
    if (!stream_.IsClosed()) {
      Lock();
      symbol_table()->pa_stream_set_underflow_callback()(stream_.stream(),
                                                         NULL,
                                                         NULL);
      ret = stream_.Close();
      Unlock();
    }
    return ret;
  }

  virtual int LatencyUsecs() {
    return stream_.LatencyUsecs();
  }

#if 0
  // TODO: Versions 0.9.16 and later of Pulse have a new API for
  // zero-copy writes, but Hardy is not new enough to have that so we can't
  // rely on it. Perhaps auto-detect if it's present or not and use it if we
  // can?

  virtual bool GetWriteBuffer(void **buffer, size_t *size) {
    bool ret = true;
    Lock();
    if (symbol_table()->pa_stream_begin_write()(stream_.stream(), buffer, size)
            != 0) {
      LOG(LS_ERROR) << "Can't get write buffer";
      ret = false;
    }
    Unlock();
    return ret;
  }

  // Releases the caller's hold on the write buffer. "written" must be the
  // amount of data that was written.
  virtual bool ReleaseWriteBuffer(void *buffer, size_t written) {
    bool ret = true;
    Lock();
    if (written == 0) {
      if (symbol_table()->pa_stream_cancel_write()(stream_.stream()) != 0) {
        LOG(LS_ERROR) << "Can't cancel write";
        ret = false;
      }
    } else {
      if (symbol_table()->pa_stream_write()(stream_.stream(),
                                            buffer,
                                            written,
                                            NULL,
                                            0,
                                            PA_SEEK_RELATIVE) != 0) {
        LOG(LS_ERROR) << "Unable to write";
        ret = false;
      }
    }
    Unlock();
    return ret;
  }
#endif

 private:
  void Lock() {
    stream_.Lock();
  }

  void Unlock() {
    stream_.Unlock();
  }

  PulseAudioSymbolTable *symbol_table() {
    return stream_.symbol_table();
  }

  void EnableWriteCallback() {
    pa_stream_state_t state = symbol_table()->pa_stream_get_state()(
        stream_.stream());
    if (state == PA_STREAM_READY) {
      // May already have available space. Must check.
      temp_buffer_space_ = symbol_table()->pa_stream_writable_size()(
          stream_.stream());
      if (temp_buffer_space_ > 0) {
        // Yup, there is already space available, so if we register a write
        // callback then it will not receive any event. So dispatch one ourself
        // instead.
        HaveWork();
        return;
      }
    }
    symbol_table()->pa_stream_set_write_callback()(
         stream_.stream(),
         &WriteCallbackThunk,
         this);
  }

  void DisableWriteCallback() {
    symbol_table()->pa_stream_set_write_callback()(
         stream_.stream(),
         NULL,
         NULL);
  }

  static void WriteCallbackThunk(pa_stream *unused,
                                 size_t buffer_space,
                                 void *userdata) {
    PulseAudioOutputStream *instance =
        static_cast<PulseAudioOutputStream *>(userdata);
    instance->OnWriteCallback(buffer_space);
  }

  void OnWriteCallback(size_t buffer_space) {
    temp_buffer_space_ = buffer_space;
    // Since we write the data asynchronously on a different thread, we have
    // to temporarily disable the write callback or else Pulse will call it
    // continuously until we write the data. We re-enable it below.
    DisableWriteCallback();
    HaveWork();
  }

  // Inherited from Worker.
  virtual void OnStart() {
    Lock();
    EnableWriteCallback();
    Unlock();
  }

  // Inherited from Worker.
  virtual void OnHaveWork() {
    ASSERT(temp_buffer_space_ > 0);

    SignalBufferSpace(temp_buffer_space_, this);

    temp_buffer_space_ = 0;
    Lock();
    EnableWriteCallback();
    Unlock();
  }

  // Inherited from Worker.
  virtual void OnStop() {
    Lock();
    DisableWriteCallback();
    Unlock();
  }

  static void UnderflowCallbackThunk(pa_stream *unused,
                                     void *userdata) {
    PulseAudioOutputStream *instance =
        static_cast<PulseAudioOutputStream *>(userdata);
    instance->OnUnderflowCallback();
  }

  void OnUnderflowCallback() {
    LOG(LS_WARNING) << "Buffer underflow on playback stream "
                    << stream_.stream();

    if (configured_latency_ == SoundSystemInterface::kNoLatencyRequirements) {
      // We didn't configure a pa_buffer_attr before, so switching to one now
      // would be questionable.
      return;
    }

    // Otherwise reconfigure the stream with a higher target latency.

    const pa_sample_spec *spec = symbol_table()->pa_stream_get_sample_spec()(
        stream_.stream());
    if (!spec) {
      LOG(LS_ERROR) << "pa_stream_get_sample_spec()";
      return;
    }

    size_t bytes_per_sec = symbol_table()->pa_bytes_per_second()(spec);

    int new_latency = configured_latency_ +
        bytes_per_sec * kPlaybackLatencyIncrementMsecs /
        rtc::kNumMicrosecsPerSec;

    pa_buffer_attr new_attr = {0};
    FillPlaybackBufferAttr(new_latency, &new_attr);

    pa_operation *op = symbol_table()->pa_stream_set_buffer_attr()(
        stream_.stream(),
        &new_attr,
        // No callback.
        NULL,
        NULL);
    if (!op) {
      LOG(LS_ERROR) << "pa_stream_set_buffer_attr()";
      return;
    }
    // Don't need to wait for this to complete.
    symbol_table()->pa_operation_unref()(op);

    // Save the new latency in case we underflow again.
    configured_latency_ = new_latency;
  }

  static void GetVolumeCallbackThunk(pa_context *unused,
                                     const pa_sink_input_info *info,
                                     int eol,
                                     void *userdata) {
    GetVolumeCallbackData *data =
        static_cast<GetVolumeCallbackData *>(userdata);
    data->instance->OnGetVolumeCallback(info, eol, &data->channel_volumes);
  }

  void OnGetVolumeCallback(const pa_sink_input_info *info,
                           int eol,
                           pa_cvolume **channel_volumes) {
    if (eol) {
      // List is over. Wake GetVolume().
      stream_.pulse()->Signal();
      return;
    }

    if (*channel_volumes) {
      **channel_volumes = info->volume;
      // Unset the pointer so that we know that we have have already copied the
      // volume.
      *channel_volumes = NULL;
    } else {
      // We have received an additional callback after the first one, which
      // doesn't make sense for a single sink input. This probably never
      // happens, but we code for it anyway.
      LOG(LS_WARNING) << "Ignoring extra GetVolumeCallback";
    }
  }

  static void SetVolumeCallback(pa_context *unused1,
                                int success,
                                void *unused2) {
    if (!success) {
      LOG(LS_ERROR) << "Failed to change playback volume";
    }
  }

  PulseAudioStream stream_;
  int configured_latency_;
  // Temporary storage for passing data between threads.
  size_t temp_buffer_space_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioOutputStream);
};

PulseAudioSoundSystem::PulseAudioSoundSystem()
    : mainloop_(NULL), context_(NULL) {
}

PulseAudioSoundSystem::~PulseAudioSoundSystem() {
  Terminate();
}

bool PulseAudioSoundSystem::Init() {
  if (IsInitialized()) {
    return true;
  }

  // Load libpulse.
  if (!symbol_table_.Load()) {
    // Most likely the Pulse library and sound server are not installed on
    // this system.
    LOG(LS_WARNING) << "Failed to load symbol table";
    return false;
  }

  // Now create and start the Pulse event thread.
  mainloop_ = symbol_table_.pa_threaded_mainloop_new()();
  if (!mainloop_) {
    LOG(LS_ERROR) << "Can't create mainloop";
    goto fail0;
  }

  if (symbol_table_.pa_threaded_mainloop_start()(mainloop_) != 0) {
    LOG(LS_ERROR) << "Can't start mainloop";
    goto fail1;
  }

  Lock();
  context_ = CreateNewConnection();
  Unlock();

  if (!context_) {
    goto fail2;
  }

  // Otherwise we're now ready!
  return true;

 fail2:
  symbol_table_.pa_threaded_mainloop_stop()(mainloop_);
 fail1:
  symbol_table_.pa_threaded_mainloop_free()(mainloop_);
  mainloop_ = NULL;
 fail0:
  return false;
}

void PulseAudioSoundSystem::Terminate() {
  if (!IsInitialized()) {
    return;
  }

  Lock();
  symbol_table_.pa_context_disconnect()(context_);
  symbol_table_.pa_context_unref()(context_);
  Unlock();
  context_ = NULL;
  symbol_table_.pa_threaded_mainloop_stop()(mainloop_);
  symbol_table_.pa_threaded_mainloop_free()(mainloop_);
  mainloop_ = NULL;

  // We do not unload the symbol table because we may need it again soon if
  // Init() is called again.
}

bool PulseAudioSoundSystem::EnumeratePlaybackDevices(
    SoundDeviceLocatorList *devices) {
  return EnumerateDevices<pa_sink_info>(
      devices,
      symbol_table_.pa_context_get_sink_info_list(),
      &EnumeratePlaybackDevicesCallbackThunk);
}

bool PulseAudioSoundSystem::EnumerateCaptureDevices(
    SoundDeviceLocatorList *devices) {
  return EnumerateDevices<pa_source_info>(
      devices,
      symbol_table_.pa_context_get_source_info_list(),
      &EnumerateCaptureDevicesCallbackThunk);
}

bool PulseAudioSoundSystem::GetDefaultPlaybackDevice(
    SoundDeviceLocator **device) {
  return GetDefaultDevice<&pa_server_info::default_sink_name>(device);
}

bool PulseAudioSoundSystem::GetDefaultCaptureDevice(
    SoundDeviceLocator **device) {
  return GetDefaultDevice<&pa_server_info::default_source_name>(device);
}

SoundOutputStreamInterface *PulseAudioSoundSystem::OpenPlaybackDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params) {
  return OpenDevice<SoundOutputStreamInterface>(
      device,
      params,
      "Playback",
      &PulseAudioSoundSystem::ConnectOutputStream);
}

SoundInputStreamInterface *PulseAudioSoundSystem::OpenCaptureDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params) {
  return OpenDevice<SoundInputStreamInterface>(
      device,
      params,
      "Capture",
      &PulseAudioSoundSystem::ConnectInputStream);
}

const char *PulseAudioSoundSystem::GetName() const {
  return "PulseAudio";
}

inline bool PulseAudioSoundSystem::IsInitialized() {
  return mainloop_ != NULL;
}

struct ConnectToPulseCallbackData {
  PulseAudioSoundSystem *instance;
  bool connect_done;
};

void PulseAudioSoundSystem::ConnectToPulseCallbackThunk(
    pa_context *context, void *userdata) {
  ConnectToPulseCallbackData *data =
      static_cast<ConnectToPulseCallbackData *>(userdata);
  data->instance->OnConnectToPulseCallback(context, &data->connect_done);
}

void PulseAudioSoundSystem::OnConnectToPulseCallback(
    pa_context *context, bool *connect_done) {
  pa_context_state_t state = symbol_table_.pa_context_get_state()(context);
  if (state == PA_CONTEXT_READY ||
      state == PA_CONTEXT_FAILED ||
      state == PA_CONTEXT_TERMINATED) {
    // Connection process has reached a terminal state. Wake ConnectToPulse().
    *connect_done = true;
    Signal();
  }
}

// Must be called with the lock held.
bool PulseAudioSoundSystem::ConnectToPulse(pa_context *context) {
  bool ret = true;
  ConnectToPulseCallbackData data;
  // Have to put this up here to satisfy the compiler.
  pa_context_state_t state;

  data.instance = this;
  data.connect_done = false;

  symbol_table_.pa_context_set_state_callback()(context,
                                                &ConnectToPulseCallbackThunk,
                                                &data);

  // Connect to PulseAudio sound server.
  if (symbol_table_.pa_context_connect()(
          context,
          NULL,          // Default server
          PA_CONTEXT_NOAUTOSPAWN,
          NULL) != 0) {  // No special fork handling needed
    LOG(LS_ERROR) << "Can't start connection to PulseAudio sound server";
    ret = false;
    goto done;
  }

  // Wait for the connection state machine to reach a terminal state.
  do {
    Wait();
  } while (!data.connect_done);

  // Now check to see what final state we reached.
  state = symbol_table_.pa_context_get_state()(context);

  if (state != PA_CONTEXT_READY) {
    if (state == PA_CONTEXT_FAILED) {
      LOG(LS_ERROR) << "Failed to connect to PulseAudio sound server";
    } else if (state == PA_CONTEXT_TERMINATED) {
      LOG(LS_ERROR) << "PulseAudio connection terminated early";
    } else {
      // Shouldn't happen, because we only signal on one of those three states.
      LOG(LS_ERROR) << "Unknown problem connecting to PulseAudio";
    }
    ret = false;
  }

 done:
  // We unset our callback for safety just in case the state might somehow
  // change later, because the pointer to "data" will be invalid after return
  // from this function.
  symbol_table_.pa_context_set_state_callback()(context, NULL, NULL);
  return ret;
}

// Must be called with the lock held.
pa_context *PulseAudioSoundSystem::CreateNewConnection() {
  // Create connection context.
  std::string app_name;
  // TODO: Pulse etiquette says this name should be localized. Do
  // we care?
  rtc::Filesystem::GetApplicationName(&app_name);
  pa_context *context = symbol_table_.pa_context_new()(
      symbol_table_.pa_threaded_mainloop_get_api()(mainloop_),
      app_name.c_str());
  if (!context) {
    LOG(LS_ERROR) << "Can't create context";
    goto fail0;
  }

  // Now connect.
  if (!ConnectToPulse(context)) {
    goto fail1;
  }

  // Otherwise the connection succeeded and is ready.
  return context;

 fail1:
  symbol_table_.pa_context_unref()(context);
 fail0:
  return NULL;
}

struct EnumerateDevicesCallbackData {
  PulseAudioSoundSystem *instance;
  SoundSystemInterface::SoundDeviceLocatorList *devices;
};

void PulseAudioSoundSystem::EnumeratePlaybackDevicesCallbackThunk(
    pa_context *unused,
    const pa_sink_info *info,
    int eol,
    void *userdata) {
  EnumerateDevicesCallbackData *data =
      static_cast<EnumerateDevicesCallbackData *>(userdata);
  data->instance->OnEnumeratePlaybackDevicesCallback(data->devices, info, eol);
}

void PulseAudioSoundSystem::EnumerateCaptureDevicesCallbackThunk(
    pa_context *unused,
    const pa_source_info *info,
    int eol,
    void *userdata) {
  EnumerateDevicesCallbackData *data =
      static_cast<EnumerateDevicesCallbackData *>(userdata);
  data->instance->OnEnumerateCaptureDevicesCallback(data->devices, info, eol);
}

void PulseAudioSoundSystem::OnEnumeratePlaybackDevicesCallback(
    SoundDeviceLocatorList *devices,
    const pa_sink_info *info,
    int eol) {
  if (eol) {
    // List is over. Wake EnumerateDevices().
    Signal();
    return;
  }

  // Else this is the next device.
  devices->push_back(
      new PulseAudioDeviceLocator(info->description, info->name));
}

void PulseAudioSoundSystem::OnEnumerateCaptureDevicesCallback(
    SoundDeviceLocatorList *devices,
    const pa_source_info *info,
    int eol) {
  if (eol) {
    // List is over. Wake EnumerateDevices().
    Signal();
    return;
  }

  if (info->monitor_of_sink != PA_INVALID_INDEX) {
    // We don't want to list monitor sources, since they are almost certainly
    // not what the user wants for voice conferencing.
    return;
  }

  // Else this is the next device.
  devices->push_back(
      new PulseAudioDeviceLocator(info->description, info->name));
}

template <typename InfoStruct>
bool PulseAudioSoundSystem::EnumerateDevices(
    SoundDeviceLocatorList *devices,
    pa_operation *(*enumerate_fn)(
        pa_context *c,
        void (*callback_fn)(
            pa_context *c,
            const InfoStruct *i,
            int eol,
            void *userdata),
        void *userdata),
    void (*callback_fn)(
        pa_context *c,
        const InfoStruct *i,
        int eol,
        void *userdata)) {
  ClearSoundDeviceLocatorList(devices);
  if (!IsInitialized()) {
    return false;
  }

  EnumerateDevicesCallbackData data;
  data.instance = this;
  data.devices = devices;

  Lock();
  pa_operation *op = (*enumerate_fn)(
      context_,
      callback_fn,
      &data);
  bool ret = FinishOperation(op);
  Unlock();
  return ret;
}

struct GetDefaultDeviceCallbackData {
  PulseAudioSoundSystem *instance;
  SoundDeviceLocator **device;
};

template <const char *(pa_server_info::*field)>
void PulseAudioSoundSystem::GetDefaultDeviceCallbackThunk(
    pa_context *unused,
    const pa_server_info *info,
    void *userdata) {
  GetDefaultDeviceCallbackData *data =
      static_cast<GetDefaultDeviceCallbackData *>(userdata);
  data->instance->OnGetDefaultDeviceCallback<field>(info, data->device);
}

template <const char *(pa_server_info::*field)>
void PulseAudioSoundSystem::OnGetDefaultDeviceCallback(
    const pa_server_info *info,
    SoundDeviceLocator **device) {
  if (info) {
    const char *dev = info->*field;
    if (dev) {
      *device = new PulseAudioDeviceLocator("Default device", dev);
    }
  }
  Signal();
}

template <const char *(pa_server_info::*field)>
bool PulseAudioSoundSystem::GetDefaultDevice(SoundDeviceLocator **device) {
  if (!IsInitialized()) {
    return false;
  }
  bool ret;
  *device = NULL;
  GetDefaultDeviceCallbackData data;
  data.instance = this;
  data.device = device;
  Lock();
  pa_operation *op = symbol_table_.pa_context_get_server_info()(
      context_,
      &GetDefaultDeviceCallbackThunk<field>,
      &data);
  ret = FinishOperation(op);
  Unlock();
  return ret && (*device != NULL);
}

void PulseAudioSoundSystem::StreamStateChangedCallbackThunk(
    pa_stream *stream,
    void *userdata) {
  PulseAudioSoundSystem *instance =
      static_cast<PulseAudioSoundSystem *>(userdata);
  instance->OnStreamStateChangedCallback(stream);
}

void PulseAudioSoundSystem::OnStreamStateChangedCallback(pa_stream *stream) {
  pa_stream_state_t state = symbol_table_.pa_stream_get_state()(stream);
  if (state == PA_STREAM_READY) {
    LOG(LS_INFO) << "Pulse stream " << stream << " ready";
  } else if (state == PA_STREAM_FAILED ||
             state == PA_STREAM_TERMINATED ||
             state == PA_STREAM_UNCONNECTED) {
    LOG(LS_ERROR) << "Pulse stream " << stream << " failed to connect: "
                  << LastError();
  }
}

template <typename StreamInterface>
StreamInterface *PulseAudioSoundSystem::OpenDevice(
    const SoundDeviceLocator *device,
    const OpenParams &params,
    const char *stream_name,
    StreamInterface *(PulseAudioSoundSystem::*connect_fn)(
        pa_stream *stream,
        const char *dev,
        int flags,
        pa_stream_flags_t pa_flags,
        int latency,
        const pa_sample_spec &spec)) {
  if (!IsInitialized()) {
    return NULL;
  }

  const char *dev = static_cast<const PulseAudioDeviceLocator *>(device)->
      device_name().c_str();

  StreamInterface *stream_interface = NULL;

  ASSERT(params.format < ARRAY_SIZE(kCricketFormatToPulseFormatTable));

  pa_sample_spec spec;
  spec.format = kCricketFormatToPulseFormatTable[params.format];
  spec.rate = params.freq;
  spec.channels = params.channels;

  int pa_flags = 0;
  if (params.flags & FLAG_REPORT_LATENCY) {
    pa_flags |= PA_STREAM_INTERPOLATE_TIMING |
                PA_STREAM_AUTO_TIMING_UPDATE;
  }

  if (params.latency != kNoLatencyRequirements) {
    // If configuring a specific latency then we want to specify
    // PA_STREAM_ADJUST_LATENCY to make the server adjust parameters
    // automatically to reach that target latency. However, that flag doesn't
    // exist in Ubuntu 8.04 and many people still use that, so we have to check
    // the protocol version of libpulse.
    if (symbol_table_.pa_context_get_protocol_version()(context_) >=
        kAdjustLatencyProtocolVersion) {
      pa_flags |= PA_STREAM_ADJUST_LATENCY;
    }
  }

  Lock();

  pa_stream *stream = symbol_table_.pa_stream_new()(context_, stream_name,
      &spec, NULL);
  if (!stream) {
    LOG(LS_ERROR) << "Can't create pa_stream";
    goto done;
  }

  // Set a state callback to log errors.
  symbol_table_.pa_stream_set_state_callback()(stream,
                                               &StreamStateChangedCallbackThunk,
                                               this);

  stream_interface = (this->*connect_fn)(
      stream,
      dev,
      params.flags,
      static_cast<pa_stream_flags_t>(pa_flags),
      params.latency,
      spec);
  if (!stream_interface) {
    LOG(LS_ERROR) << "Can't connect stream to " << dev;
    symbol_table_.pa_stream_unref()(stream);
  }

 done:
  Unlock();
  return stream_interface;
}

// Must be called with the lock held.
SoundOutputStreamInterface *PulseAudioSoundSystem::ConnectOutputStream(
    pa_stream *stream,
    const char *dev,
    int flags,
    pa_stream_flags_t pa_flags,
    int latency,
    const pa_sample_spec &spec) {
  pa_buffer_attr attr = {0};
  pa_buffer_attr *pattr = NULL;
  if (latency != kNoLatencyRequirements) {
    // kLowLatency is 0, so we treat it the same as a request for zero latency.
    ssize_t bytes_per_sec = symbol_table_.pa_bytes_per_second()(&spec);
    latency = std::max(
        latency, static_cast<int>(bytes_per_sec * kPlaybackLatencyMinimumMsecs /
                                  rtc::kNumMicrosecsPerSec));
    FillPlaybackBufferAttr(latency, &attr);
    pattr = &attr;
  }
  if (symbol_table_.pa_stream_connect_playback()(
          stream,
          dev,
          pattr,
          pa_flags,
          // Let server choose volume
          NULL,
          // Not synchronized to any other playout
          NULL) != 0) {
    return NULL;
  }
  return new PulseAudioOutputStream(this, stream, flags, latency);
}

// Must be called with the lock held.
SoundInputStreamInterface *PulseAudioSoundSystem::ConnectInputStream(
    pa_stream *stream,
    const char *dev,
    int flags,
    pa_stream_flags_t pa_flags,
    int latency,
    const pa_sample_spec &spec) {
  pa_buffer_attr attr = {0};
  pa_buffer_attr *pattr = NULL;
  if (latency != kNoLatencyRequirements) {
    size_t bytes_per_sec = symbol_table_.pa_bytes_per_second()(&spec);
    if (latency == kLowLatency) {
      latency = bytes_per_sec * kLowCaptureLatencyMsecs /
          rtc::kNumMicrosecsPerSec;
    }
    // Note: fragsize specifies a maximum transfer size, not a minimum, so it is
    // not possible to force a high latency setting, only a low one.
    attr.fragsize = latency;
    attr.maxlength = latency + bytes_per_sec * kCaptureBufferExtraMsecs /
        rtc::kNumMicrosecsPerSec;
    LOG(LS_VERBOSE) << "Configuring latency = " << attr.fragsize
                    << ", maxlength = " << attr.maxlength;
    pattr = &attr;
  }
  if (symbol_table_.pa_stream_connect_record()(stream,
                                               dev,
                                               pattr,
                                               pa_flags) != 0) {
    return NULL;
  }
  return new PulseAudioInputStream(this, stream, flags);
}

// Must be called with the lock held.
bool PulseAudioSoundSystem::FinishOperation(pa_operation *op) {
  if (!op) {
    LOG(LS_ERROR) << "Failed to start operation";
    return false;
  }

  do {
    Wait();
  } while (symbol_table_.pa_operation_get_state()(op) == PA_OPERATION_RUNNING);

  symbol_table_.pa_operation_unref()(op);

  return true;
}

inline void PulseAudioSoundSystem::Lock() {
  symbol_table_.pa_threaded_mainloop_lock()(mainloop_);
}

inline void PulseAudioSoundSystem::Unlock() {
  symbol_table_.pa_threaded_mainloop_unlock()(mainloop_);
}

// Must be called with the lock held.
inline void PulseAudioSoundSystem::Wait() {
  symbol_table_.pa_threaded_mainloop_wait()(mainloop_);
}

// Must be called with the lock held.
inline void PulseAudioSoundSystem::Signal() {
  symbol_table_.pa_threaded_mainloop_signal()(mainloop_, 0);
}

// Must be called with the lock held.
const char *PulseAudioSoundSystem::LastError() {
  return symbol_table_.pa_strerror()(symbol_table_.pa_context_errno()(
      context_));
}

}  // namespace rtc

#endif  // HAVE_LIBPULSE
