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

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/voice_engine_defines.h"


namespace webrtc {

class DtmfInbandQueue
{
public:

    DtmfInbandQueue(int32_t id);

    virtual ~DtmfInbandQueue();

    int AddDtmf(uint8_t DtmfKey, uint16_t len, uint8_t level);

    int8_t NextDtmf(uint16_t* len, uint8_t* level);

    bool PendingDtmf();

    void ResetDtmf();

private:
    enum {kDtmfInbandMax = 20};

    int32_t _id;
    CriticalSectionWrapper& _DtmfCritsect;
    uint8_t _nextEmptyIndex;
    uint8_t _DtmfKey[kDtmfInbandMax];
    uint16_t _DtmfLen[kDtmfInbandMax];
    uint8_t _DtmfLevel[kDtmfInbandMax];
};

}   // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_DTMF_INBAND_QUEUE_H
