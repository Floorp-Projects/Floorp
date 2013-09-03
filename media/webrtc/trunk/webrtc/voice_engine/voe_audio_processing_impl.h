/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_IMPL_H

#include "webrtc/voice_engine/include/voe_audio_processing.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEAudioProcessingImpl : public VoEAudioProcessing {
 public:
  virtual int SetNsStatus(bool enable, NsModes mode = kNsUnchanged);

  virtual int GetNsStatus(bool& enabled, NsModes& mode);

  virtual int SetAgcStatus(bool enable, AgcModes mode = kAgcUnchanged);

  virtual int GetAgcStatus(bool& enabled, AgcModes& mode);

  virtual int SetAgcConfig(AgcConfig config);

  virtual int GetAgcConfig(AgcConfig& config);

  virtual int SetRxNsStatus(int channel,
                            bool enable,
                            NsModes mode = kNsUnchanged);

  virtual int GetRxNsStatus(int channel, bool& enabled, NsModes& mode);

  virtual int SetRxAgcStatus(int channel,
                             bool enable,
                             AgcModes mode = kAgcUnchanged);

  virtual int GetRxAgcStatus(int channel, bool& enabled, AgcModes& mode);

  virtual int SetRxAgcConfig(int channel, AgcConfig config);

  virtual int GetRxAgcConfig(int channel, AgcConfig& config);

  virtual int SetEcStatus(bool enable, EcModes mode = kEcUnchanged);
  virtual int GetEcStatus(bool& enabled, EcModes& mode);
  virtual int EnableDriftCompensation(bool enable);
  virtual bool DriftCompensationEnabled();

  virtual void SetDelayOffsetMs(int offset);
  virtual int DelayOffsetMs();

  virtual int SetAecmMode(AecmModes mode = kAecmSpeakerphone,
                          bool enableCNG = true);

  virtual int GetAecmMode(AecmModes& mode, bool& enabledCNG);

  virtual int EnableHighPassFilter(bool enable);
  virtual bool IsHighPassFilterEnabled();

  virtual int RegisterRxVadObserver(int channel,
                                    VoERxVadCallback& observer);

  virtual int DeRegisterRxVadObserver(int channel);

  virtual int VoiceActivityIndicator(int channel);

  virtual int SetEcMetricsStatus(bool enable);

  virtual int GetEcMetricsStatus(bool& enabled);

  virtual int GetEchoMetrics(int& ERL, int& ERLE, int& RERL, int& A_NLP);

  virtual int GetEcDelayMetrics(int& delay_median, int& delay_std);

  virtual int StartDebugRecording(const char* fileNameUTF8);

  virtual int StopDebugRecording();

  virtual int SetTypingDetectionStatus(bool enable);

  virtual int GetTypingDetectionStatus(bool& enabled);

  virtual int TimeSinceLastTyping(int &seconds);

  // TODO(niklase) Remove default argument as soon as libJingle is updated!
  virtual int SetTypingDetectionParameters(int timeWindow,
                                           int costPerTyping,
                                           int reportingThreshold,
                                           int penaltyDecay,
                                           int typeEventDelay = 0);

  virtual void EnableStereoChannelSwapping(bool enable);
  virtual bool IsStereoChannelSwappingEnabled();

 protected:
  VoEAudioProcessingImpl(voe::SharedData* shared);
  virtual ~VoEAudioProcessingImpl();

 private:
  bool _isAecMode;
  voe::SharedData* _shared;
};

}  //  namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_AUDIO_PROCESSING_IMPL_H
