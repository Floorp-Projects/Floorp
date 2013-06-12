/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_TRANSFORM_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_TRANSFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "webrtc/modules/audio_coding/codecs/isac/fix/source/settings.h"
#include "webrtc/typedefs.h"

/* Cosine table 1 in Q14 */
extern const WebRtc_Word16 kCosTab1[FRAMESAMPLES/2];

/* Sine table 1 in Q14 */
extern const WebRtc_Word16 kSinTab1[FRAMESAMPLES/2];

/* Cosine table 2 in Q14 */
extern const WebRtc_Word16 kCosTab2[FRAMESAMPLES/4];

/* Sine table 2 in Q14 */
extern const WebRtc_Word16 kSinTab2[FRAMESAMPLES/4];

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_TRANSFORM_H_ */
