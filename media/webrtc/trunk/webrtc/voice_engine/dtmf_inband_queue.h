/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_DTMF_INBAND_QUEUE_H
#define WEBRTC_VOICE_ENGINE_DTMF_INBAND_QUEUE_H

#include "critical_section_wrapper.h"
#include "typedefs.h"
#include "voice_engine_defines.h"


namespace webrtc {

class DtmfInbandQueue
{
public:

    DtmfInbandQueue(const WebRtc_Word32 id);

    virtual ~DtmfInbandQueue();

    int AddDtmf(WebRtc_UWord8 DtmfKey,
                WebRtc_UWord16 len,
                WebRtc_UWord8 level);

    WebRtc_Word8 NextDtmf(WebRtc_UWord16* len, WebRtc_UWord8* level);

    bool PendingDtmf();

    void ResetDtmf();

private:
    enum {kDtmfInbandMax = 20};

    WebRtc_Word32 _id;
    CriticalSectionWrapper& _DtmfCritsect;
    WebRtc_UWord8 _nextEmptyIndex;
    WebRtc_UWord8 _DtmfKey[kDtmfInbandMax];
    WebRtc_UWord16 _DtmfLen[kDtmfInbandMax];
    WebRtc_UWord8 _DtmfLevel[kDtmfInbandMax];
};

}   // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_DTMF_INBAND_QUEUE_H
