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

int32_t
DTMFqueue::AddDTMF(uint8_t key, uint16_t len, uint8_t level)
{
    CriticalSectionScoped lock(_DTMFCritsect);

    if(_nextEmptyIndex >= DTMF_OUTBAND_MAX)
    {
        return -1;
    }
    int32_t index = _nextEmptyIndex;
    _DTMFKey[index] = key;
    _DTMFLen[index] = len;
    _DTMFLevel[index] = level;
    _nextEmptyIndex++;
    return 0;
}

int8_t
DTMFqueue::NextDTMF(uint8_t* DTMFKey, uint16_t* len, uint8_t* level)
{
    CriticalSectionScoped lock(_DTMFCritsect);

    if(!PendingDTMF())
    {
        return -1;
    }
    *DTMFKey=_DTMFKey[0];
    *len=_DTMFLen[0];
    *level=_DTMFLevel[0];

    memmove(&(_DTMFKey[0]), &(_DTMFKey[1]), _nextEmptyIndex*sizeof(uint8_t));
    memmove(&(_DTMFLen[0]), &(_DTMFLen[1]), _nextEmptyIndex*sizeof(uint16_t));
    memmove(&(_DTMFLevel[0]), &(_DTMFLevel[1]), _nextEmptyIndex*sizeof(uint8_t));

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
