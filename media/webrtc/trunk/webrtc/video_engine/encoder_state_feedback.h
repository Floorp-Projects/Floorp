/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(mflodman) ViEEncoder has a time check to not send key frames too often,
// move the logic to this class.

#ifndef WEBRTC_VIDEO_ENGINE_ENCODER_STATE_FEEDBACK_H_
#define WEBRTC_VIDEO_ENGINE_ENCODER_STATE_FEEDBACK_H_

#include <map>

#include "webrtc/base/constructormagic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class EncoderStateFeedbackObserver;
class RtcpIntraFrameObserver;
class ViEEncoder;

class EncoderStateFeedback {
 public:
  friend class EncoderStateFeedbackObserver;

  EncoderStateFeedback();
  ~EncoderStateFeedback();

  // Adds an encoder to receive feedback for a unique ssrc.
  bool AddEncoder(uint32_t ssrc, ViEEncoder* encoder);

  // Removes a registered ViEEncoder.
  void RemoveEncoder(const ViEEncoder* encoder);

  // Returns an observer to register at the requesting class. The observer has
  // the same lifetime as the EncoderStateFeedback instance.
  RtcpIntraFrameObserver* GetRtcpIntraFrameObserver();

 protected:
  // Called by EncoderStateFeedbackObserver when a new key frame is requested.
  void OnReceivedIntraFrameRequest(uint32_t ssrc);
  void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id);
  void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id);
  void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc);

 private:
  typedef std::map<uint32_t,  ViEEncoder*> SsrcEncoderMap;

  scoped_ptr<CriticalSectionWrapper> crit_;

  // Instance registered at the class requesting new key frames.
  scoped_ptr<EncoderStateFeedbackObserver> observer_;

  // Maps a unique ssrc to the given encoder.
  SsrcEncoderMap encoders_;

  DISALLOW_COPY_AND_ASSIGN(EncoderStateFeedback);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_ENCODER_STATE_FEEDBACK_H_
