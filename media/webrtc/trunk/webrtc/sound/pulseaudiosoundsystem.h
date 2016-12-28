/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_PULSEAUDIOSOUNDSYSTEM_H_
#define WEBRTC_SOUND_PULSEAUDIOSOUNDSYSTEM_H_

#ifdef HAVE_LIBPULSE

#include "webrtc/sound/pulseaudiosymboltable.h"
#include "webrtc/sound/soundsysteminterface.h"
#include "webrtc/base/constructormagic.h"

namespace rtc {

class PulseAudioInputStream;
class PulseAudioOutputStream;
class PulseAudioStream;

// Sound system implementation for PulseAudio, a cross-platform sound server
// (but commonly used only on Linux, which is the only platform we support
// it on).
// Init(), Terminate(), and the destructor should never be invoked concurrently,
// but all other methods are thread-safe.
class PulseAudioSoundSystem : public SoundSystemInterface {
  friend class PulseAudioInputStream;
  friend class PulseAudioOutputStream;
  friend class PulseAudioStream;
 public:
  static SoundSystemInterface *Create() {
    return new PulseAudioSoundSystem();
  }

  PulseAudioSoundSystem();

  virtual ~PulseAudioSoundSystem();

  virtual bool Init();
  virtual void Terminate();

  virtual bool EnumeratePlaybackDevices(SoundDeviceLocatorList *devices);
  virtual bool EnumerateCaptureDevices(SoundDeviceLocatorList *devices);

  virtual bool GetDefaultPlaybackDevice(SoundDeviceLocator **device);
  virtual bool GetDefaultCaptureDevice(SoundDeviceLocator **device);

  virtual SoundOutputStreamInterface *OpenPlaybackDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);
  virtual SoundInputStreamInterface *OpenCaptureDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params);

  virtual const char *GetName() const;

 private:
  bool IsInitialized();

  static void ConnectToPulseCallbackThunk(pa_context *context, void *userdata);

  void OnConnectToPulseCallback(pa_context *context, bool *connect_done);

  bool ConnectToPulse(pa_context *context);

  pa_context *CreateNewConnection();

  template <typename InfoStruct>
  bool EnumerateDevices(SoundDeviceLocatorList *devices,
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
                            void *userdata));

  static void EnumeratePlaybackDevicesCallbackThunk(pa_context *unused,
                                                    const pa_sink_info *info,
                                                    int eol,
                                                    void *userdata);

  static void EnumerateCaptureDevicesCallbackThunk(pa_context *unused,
                                                   const pa_source_info *info,
                                                   int eol,
                                                   void *userdata);

  void OnEnumeratePlaybackDevicesCallback(
      SoundDeviceLocatorList *devices,
      const pa_sink_info *info,
      int eol);

  void OnEnumerateCaptureDevicesCallback(
      SoundDeviceLocatorList *devices,
      const pa_source_info *info,
      int eol);

  template <const char *(pa_server_info::*field)>
  static void GetDefaultDeviceCallbackThunk(
      pa_context *unused,
      const pa_server_info *info,
      void *userdata);

  template <const char *(pa_server_info::*field)>
  void OnGetDefaultDeviceCallback(
      const pa_server_info *info,
      SoundDeviceLocator **device);

  template <const char *(pa_server_info::*field)>
  bool GetDefaultDevice(SoundDeviceLocator **device);

  static void StreamStateChangedCallbackThunk(pa_stream *stream,
                                              void *userdata);

  void OnStreamStateChangedCallback(pa_stream *stream);

  template <typename StreamInterface>
  StreamInterface *OpenDevice(
      const SoundDeviceLocator *device,
      const OpenParams &params,
      const char *stream_name,
      StreamInterface *(PulseAudioSoundSystem::*connect_fn)(
          pa_stream *stream,
          const char *dev,
          int flags,
          pa_stream_flags_t pa_flags,
          int latency,
          const pa_sample_spec &spec));

  SoundOutputStreamInterface *ConnectOutputStream(
      pa_stream *stream,
      const char *dev,
      int flags,
      pa_stream_flags_t pa_flags,
      int latency,
      const pa_sample_spec &spec);

  SoundInputStreamInterface *ConnectInputStream(
      pa_stream *stream,
      const char *dev,
      int flags,
      pa_stream_flags_t pa_flags,
      int latency,
      const pa_sample_spec &spec);

  bool FinishOperation(pa_operation *op);

  void Lock();
  void Unlock();
  void Wait();
  void Signal();

  const char *LastError();

  pa_threaded_mainloop *mainloop_;
  pa_context *context_;
  PulseAudioSymbolTable symbol_table_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PulseAudioSoundSystem);
};

}  // namespace rtc

#endif  // HAVE_LIBPULSE

#endif  // WEBRTC_SOUND_PULSEAUDIOSOUNDSYSTEM_H_
