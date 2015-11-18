/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/ns/include/noise_suppression_x.h"

#include <stdlib.h>

#include "webrtc/common_audio/signal_processing/include/real_fft.h"
#include "webrtc/modules/audio_processing/ns/nsx_core.h"
#include "webrtc/modules/audio_processing/ns/nsx_defines.h"

int WebRtcNsx_Create(NsxHandle** nsxInst) {
  NoiseSuppressionFixedC* self = malloc(sizeof(NoiseSuppressionFixedC));
  *nsxInst = (NsxHandle*)self;

  if (self != NULL) {
    WebRtcSpl_Init();
    self->real_fft = NULL;
    self->initFlag = 0;
    return 0;
  } else {
    return -1;
  }

}

int WebRtcNsx_Free(NsxHandle* nsxInst) {
  WebRtcSpl_FreeRealFFT(((NoiseSuppressionFixedC*)nsxInst)->real_fft);
  free(nsxInst);
  return 0;
}

int WebRtcNsx_Init(NsxHandle* nsxInst, uint32_t fs) {
  return WebRtcNsx_InitCore((NoiseSuppressionFixedC*)nsxInst, fs);
}

int WebRtcNsx_set_policy(NsxHandle* nsxInst, int mode) {
  return WebRtcNsx_set_policy_core((NoiseSuppressionFixedC*)nsxInst, mode);
}

void WebRtcNsx_Process(NsxHandle* nsxInst,
                      const short* const* speechFrame,
                      int num_bands,
                      short* const* outFrame) {
  WebRtcNsx_ProcessCore((NoiseSuppressionFixedC*)nsxInst, speechFrame,
                        num_bands, outFrame);
}
