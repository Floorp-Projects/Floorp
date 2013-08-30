/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This utility will portably force the volume of the default microphone to max.

#include <cstdio>

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

int main(int argc, char** argv) {
  webrtc::VoiceEngine* voe = webrtc::VoiceEngine::Create();
  if (voe == NULL) {
    fprintf(stderr, "Failed to initialize voice engine.\n");
    return 1;
  }

  webrtc::VoEBase* base = webrtc::VoEBase::GetInterface(voe);
  webrtc::VoEVolumeControl* volume_control =
      webrtc::VoEVolumeControl::GetInterface(voe);

  if (base->Init() != 0) {
    fprintf(stderr, "Failed to initialize voice engine base.\n");
    return 1;
  }
  // Set to 0 first in case the mic is above 100%.
  if (volume_control->SetMicVolume(0) != 0) {
    fprintf(stderr, "Failed set volume to 0.\n");
    return 1;
  }
  if (volume_control->SetMicVolume(255) != 0) {
    fprintf(stderr, "Failed set volume to 255.\n");
    return 1;
  }

  return 0;
}
