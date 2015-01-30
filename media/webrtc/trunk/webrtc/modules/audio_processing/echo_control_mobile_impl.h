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

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {

class AudioBuffer;
class CriticalSectionWrapper;

class EchoControlMobileImpl : public EchoControlMobile,
                              public ProcessingComponent {
 public:
  EchoControlMobileImpl(const AudioProcessing* apm,
                        CriticalSectionWrapper* crit);
  virtual ~EchoControlMobileImpl();

  int ProcessRenderAudio(const AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // EchoControlMobile implementation.
  virtual bool is_enabled() const OVERRIDE;

  // ProcessingComponent implementation.
  virtual int Initialize() OVERRIDE;

 private:
  // EchoControlMobile implementation.
  virtual int Enable(bool enable) OVERRIDE;
  virtual int set_routing_mode(RoutingMode mode) OVERRIDE;
  virtual RoutingMode routing_mode() const OVERRIDE;
  virtual int enable_comfort_noise(bool enable) OVERRIDE;
  virtual bool is_comfort_noise_enabled() const OVERRIDE;
  virtual int SetEchoPath(const void* echo_path, size_t size_bytes) OVERRIDE;
  virtual int GetEchoPath(void* echo_path, size_t size_bytes) const OVERRIDE;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const OVERRIDE;
  virtual int InitializeHandle(void* handle) const OVERRIDE;
  virtual int ConfigureHandle(void* handle) const OVERRIDE;
  virtual void DestroyHandle(void* handle) const OVERRIDE;
  virtual int num_handles_required() const OVERRIDE;
  virtual int GetHandleError(void* handle) const OVERRIDE;

  const AudioProcessing* apm_;
  CriticalSectionWrapper* crit_;
  RoutingMode routing_mode_;
  bool comfort_noise_enabled_;
  unsigned char* external_echo_path_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
