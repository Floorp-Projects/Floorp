/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dtmf_inband_queue.h"
#include "trace.h"

namespace webrtc {

DtmfInbandQueue::DtmfInbandQueue(const WebRtc_Word32 id):
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
DtmfInbandQueue::AddDtmf(WebRtc_UWord8 key,
                         WebRtc_UWord16 len,
                         WebRtc_UWord8 level)
{
    CriticalSectionScoped lock(&_DtmfCritsect);

    if (_nextEmptyIndex >= kDtmfInbandMax)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_id,-1),
                   "DtmfInbandQueue::AddDtmf() unable to add Dtmf tone");
        return -1;
    }
    WebRtc_Word32 index = _nextEmptyIndex;
    _DtmfKey[index] = key;
    _DtmfLen[index] = len;
    _DtmfLevel[index] = level;
    _nextEmptyIndex++;
    return 0;
}

WebRtc_Word8
DtmfInbandQueue::NextDtmf(WebRtc_UWord16* len, WebRtc_UWord8* level)
{
    CriticalSectionScoped lock(&_DtmfCritsect);

    if(!PendingDtmf())
    {
        return -1;
    }
    WebRtc_Word8 nextDtmf = _DtmfKey[0];
    *len=_DtmfLen[0];
    *level=_DtmfLevel[0];

    memmove(&(_DtmfKey[0]), &(_DtmfKey[1]),
            _nextEmptyIndex*sizeof(WebRtc_UWord8));
    memmove(&(_DtmfLen[0]), &(_DtmfLen[1]),
            _nextEmptyIndex*sizeof(WebRtc_UWord16));
    memmove(&(_DtmfLevel[0]), &(_DtmfLevel[1]),
            _nextEmptyIndex*sizeof(WebRtc_UWord8));

    _nextEmptyIndex--;
    return nextDtmf;
}

bool 
DtmfInbandQueue::PendingDtmf()
{
    return(_nextEmptyIndex>0);        
}

void 
DtmfInbandQueue::ResetDtmf()
{
    _nextEmptyIndex = 0;
}

}  // namespace webrtc
