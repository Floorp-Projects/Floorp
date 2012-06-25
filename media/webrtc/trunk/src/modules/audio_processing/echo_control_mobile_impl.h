/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_

#include "audio_processing.h"
#include "processing_component.h"

namespace webrtc {
class AudioProcessingImpl;
class AudioBuffer;

class EchoControlMobileImpl : public EchoControlMobile,
                              public ProcessingComponent {
 public:
  explicit EchoControlMobileImpl(const AudioProcessingImpl* apm);
  virtual ~EchoControlMobileImpl();

  int ProcessRenderAudio(const AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // EchoControlMobile implementation.
  virtual bool is_enabled() const;

  // ProcessingComponent implementation.
  virtual int Initialize();

 private:
  // EchoControlMobile implementation.
  virtual int Enable(bool enable);
  virtual int set_routing_mode(RoutingMode mode);
  virtual RoutingMode routing_mode() const;
  virtual int enable_comfort_noise(bool enable);
  virtual bool is_comfort_noise_enabled() const;
  virtual int SetEchoPath(const void* echo_path, size_t size_bytes);
  virtual int GetEchoPath(void* echo_path, size_t size_bytes) const;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
  RoutingMode routing_mode_;
  bool comfort_noise_enabled_;
  unsigned char* external_echo_path_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
