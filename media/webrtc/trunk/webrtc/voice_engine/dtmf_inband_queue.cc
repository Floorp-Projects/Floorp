/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/dtmf_inband_queue.h"

namespace webrtc {

DtmfInbandQueue::DtmfInbandQueue(int32_t id):
    _id(id),
    _DtmfCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
    _nextEmptyIndex(0)
{
    memset(_DtmfKey,0, sizeof(_DtmfKey));
    memset(_DtmfLen,0, sizeof(_DtmfLen));
    memset(_DtmfLevel,0, sizeof(_DtmfLevel));
}

DtmfInbandQueue::~DtmfInbandQueue()
{
    delete &_DtmfCritsect;
}

int
DtmfInbandQueue::AddDtmf(uint8_t key, uint16_t len, uint8_t level)
{
    CriticalSectionScoped lock(&_DtmfCritsect);

    if (_nextEmptyIndex >= kDtmfInbandMax)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_id,-1),
                   "DtmfInbandQueue::AddDtmf() unable to add Dtmf tone");
        return -1;
    }
    int32_t index = _nextEmptyIndex;
    _DtmfKey[index] = key;
    _DtmfLen[index] = len;
    _DtmfLevel[index] = level;
    _nextEmptyIndex++;
    return 0;
}

int8_t
DtmfInbandQueue::NextDtmf(uint16_t* len, uint8_t* level)
{
    CriticalSectionScoped lock(&_DtmfCritsect);

    if(!PendingDtmf())
    {
        return -1;
    }
    int8_t nextDtmf = _DtmfKey[0];
    *len=_DtmfLen[0];
    *level=_DtmfLevel[0];

    memmove(&(_DtmfKey[0]), &(_DtmfKey[1]),
            _nextEmptyIndex*sizeof(uint8_t));
    memmove(&(_DtmfLen[0]), &(_DtmfLen[1]),
            _nextEmptyIndex*sizeof(uint16_t));
    memmove(&(_DtmfLevel[0]), &(_DtmfLevel[1]),
            _nextEmptyIndex*sizeof(uint8_t));

    _nextEmptyIndex--;
    return nextDtmf;
}

bool
DtmfInbandQueue::PendingDtmf()
{
    CriticalSectionScoped lock(&_DtmfCritsect);
    return _nextEmptyIndex > 0;
}

void
DtmfInbandQueue::ResetDtmf()
{
    CriticalSectionScoped lock(&_DtmfCritsect);
    _nextEmptyIndex = 0;
}

}  // namespace webrtc
