/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dtmf_queue.h"

#include <string.h> //memset

namespace webrtc {
DTMFqueue::DTMFqueue():
    _DTMFCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _nextEmptyIndex(0)
{
    memset(_DTMFKey,0, sizeof(_DTMFKey));
    memset(_DTMFLen,0, sizeof(_DTMFLen));
    memset(_DTMFLevel,0, sizeof(_DTMFLevel));
}

DTMFqueue::~DTMFqueue()
{
    delete _DTMFCritsect;
}

WebRtc_Word32
DTMFqueue::AddDTMF(WebRtc_UWord8 key, WebRtc_UWord16 len, WebRtc_UWord8 level)
{
    CriticalSectionScoped lock(_DTMFCritsect);

    if(_nextEmptyIndex >= DTMF_OUTBAND_MAX)
    {
        return -1;
    }
    WebRtc_Word32 index = _nextEmptyIndex;
    _DTMFKey[index] = key;
    _DTMFLen[index] = len;
    _DTMFLevel[index] = level;
    _nextEmptyIndex++;
    return 0;
}

WebRtc_Word8
DTMFqueue::NextDTMF(WebRtc_UWord8* DTMFKey, WebRtc_UWord16* len, WebRtc_UWord8* level)
{
    CriticalSectionScoped lock(_DTMFCritsect);

    if(!PendingDTMF())
    {
        return -1;
    }
    *DTMFKey=_DTMFKey[0];
    *len=_DTMFLen[0];
    *level=_DTMFLevel[0];

    memmove(&(_DTMFKey[0]), &(_DTMFKey[1]), _nextEmptyIndex*sizeof(WebRtc_UWord8));
    memmove(&(_DTMFLen[0]), &(_DTMFLen[1]), _nextEmptyIndex*sizeof(WebRtc_UWord16));
    memmove(&(_DTMFLevel[0]), &(_DTMFLevel[1]), _nextEmptyIndex*sizeof(WebRtc_UWord8));

    _nextEmptyIndex--;
    return 0;
}

bool
DTMFqueue::PendingDTMF()
{
    return(_nextEmptyIndex>0);
}

void
DTMFqueue::ResetDTMF()
{
    _nextEmptyIndex = 0;
}
} // namespace webrtc
