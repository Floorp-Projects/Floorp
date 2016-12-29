/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_DTMF_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_DTMF_IMPL_H

#include "webrtc/voice_engine/include/voe_dtmf.h"
#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEDtmfImpl : public VoEDtmf {
 public:
  int SendTelephoneEvent(int channel,
                         int eventCode,
                         bool outOfBand = true,
                         int lengthMs = 160,
                         int attenuationDb = 10) override;

  int SetSendTelephoneEventPayloadType(int channel,
                                       unsigned char type) override;

  int GetSendTelephoneEventPayloadType(int channel,
                                       unsigned char& type) override;

  int SetDtmfFeedbackStatus(bool enable, bool directFeedback = false) override;

  int GetDtmfFeedbackStatus(bool& enabled, bool& directFeedback) override;

  int PlayDtmfTone(int eventCode,
                   int lengthMs = 200,
                   int attenuationDb = 10) override;

 protected:
  VoEDtmfImpl(voe::SharedData* shared);
  ~VoEDtmfImpl() override;

 private:
  bool _dtmfFeedback;
  bool _dtmfDirectFeedback;
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_DTMF_IMPL_H
