/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_INCLUDE_MOCK_FAKE_VOE_EXTERNAL_MEDIA_H_
#define WEBRTC_VOICE_ENGINE_INCLUDE_MOCK_FAKE_VOE_EXTERNAL_MEDIA_H_

#include <map>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/test/fake_common.h"
#include "webrtc/voice_engine/include/voe_external_media.h"

namespace webrtc {

class FakeVoEExternalMedia : public VoEExternalMedia {
 public:
  FakeVoEExternalMedia() {}
  virtual ~FakeVoEExternalMedia() {}

  WEBRTC_STUB(Release, ());
  WEBRTC_FUNC(RegisterExternalMediaProcessing,
      (int channel, ProcessingTypes type, VoEMediaProcess& processObject)) {
    callback_map_[type] = &processObject;
    return 0;
  }
  WEBRTC_FUNC(DeRegisterExternalMediaProcessing,
      (int channel, ProcessingTypes type)) {
    callback_map_.erase(type);
    return 0;
  }
  WEBRTC_STUB(SetExternalRecordingStatus, (bool enable));
  WEBRTC_STUB(SetExternalPlayoutStatus, (bool enable));
  WEBRTC_STUB(ExternalRecordingInsertData,
      (const int16_t speechData10ms[], int lengthSamples,
       int samplingFreqHz, int current_delay_ms));
  WEBRTC_STUB(ExternalPlayoutGetData,
      (int16_t speechData10ms[], int samplingFreqHz,
       int current_delay_ms, int& lengthSamples));
  WEBRTC_STUB(GetAudioFrame, (int channel, int desired_sample_rate_hz,
                              AudioFrame* frame));
  WEBRTC_STUB(SetExternalMixing, (int channel, bool enable));

  // Use this to trigger the Process() callback to a registered media processor.
  // If |audio| is NULL, a zero array of the correct length will be forwarded.
  void CallProcess(ProcessingTypes type, int16_t* audio,
                   int samples_per_channel, int sample_rate_hz,
                   int num_channels) {
    const int length = samples_per_channel * num_channels;
    rtc::scoped_ptr<int16_t[]> data;
    if (!audio) {
      data.reset(new int16_t[length]);
      memset(data.get(), 0, length * sizeof(data[0]));
      audio = data.get();
    }

    std::map<ProcessingTypes, VoEMediaProcess*>::const_iterator it =
        callback_map_.find(type);
    if (it != callback_map_.end()) {
      it->second->Process(0, type, audio, samples_per_channel, sample_rate_hz,
                          num_channels == 2 ? true : false);
    }
  }

 private:
  std::map<ProcessingTypes, VoEMediaProcess*> callback_map_;
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_INCLUDE_MOCK_FAKE_VOE_EXTERNAL_MEDIA_H_
