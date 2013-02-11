/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_H_

#include <assert.h>

#include "typedefs.h"

#if _WIN32
#include "memory_pool_win.h"
#else
#include "memory_pool_posix.h"
#endif

namespace webrtc {

template<class MemoryType>
class MemoryPool
{
public:
    // Factory method, constructor disabled.
    static WebRtc_Word32 CreateMemoryPool(MemoryPool*& memoryPool,
                                          WebRtc_UWord32 initialPoolSize);

    // Try to delete the memory pool. Fail with return value -1 if there is
    // outstanding memory.
    static WebRtc_Word32 DeleteMemoryPool(
        MemoryPool*& memoryPool);

    // Get/return unused memory.
    WebRtc_Word32 PopMemory(MemoryType*&  memory);
    WebRtc_Word32 PushMemory(MemoryType*& memory);
private:
    MemoryPool(WebRtc_Word32 initialPoolSize);
    ~MemoryPool();

    MemoryPoolImpl<MemoryType>* _ptrImpl;
};

template<class MemoryType>
MemoryPool<MemoryType>::MemoryPool(WebRtc_Word32 initialPoolSize)
{
    _ptrImpl = new MemoryPoolImpl<MemoryType>(initialPoolSize);
}

template<class MemoryType>
MemoryPool<MemoryType>::~MemoryPool()
{
    delete _ptrImpl;
}

template<class MemoryType> WebRtc_Word32
MemoryPool<MemoryType>::CreateMemoryPool(MemoryPool*&   memoryPool,
                                         WebRtc_UWord32 initialPoolSize)
{
    memoryPool = new MemoryPool(initialPoolSize);
    if(memoryPool == NULL)
    {
        return -1;
    }
    if(memoryPool->_ptrImpl == NULL)
    {
        delete memoryPool;
        memoryPool = NULL;
        return -1;
    }
    if(!memoryPool->_ptrImpl->Initialize())
    {
        delete memoryPool;
        memoryPool = NULL;
        return -1;
    }
    return 0;
}

template<class MemoryType>
WebRtc_Word32 MemoryPool<MemoryType>::DeleteMemoryPool(MemoryPool*& memoryPool)
{
    if(memoryPool == NULL)
    {
        return -1;
    }
    if(memoryPool->_ptrImpl == NULL)
    {
        return -1;
    }
    if(memoryPool->_ptrImpl->Terminate() == -1)
    {
        return -1;
    }
    delete memoryPool;
    memoryPool = NULL;
    return 0;
}

template<class MemoryType>
WebRtc_Word32 MemoryPool<MemoryType>::PopMemory(MemoryType*& memory)
{
    return _ptrImpl->PopMemory(memory);
}

template<class MemoryType>
WebRtc_Word32 MemoryPool<MemoryType>::PushMemory(MemoryType*& memory)
{
    if(memory == NULL)
    {
        return -1;
    }
    return _ptrImpl->PushMemory(memory);
}
} // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_H_
