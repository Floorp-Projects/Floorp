/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Atomic system independant 32-bit signed integer.
// Mac implementation.
#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_ATOMIC32_MAC_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_ATOMIC32_MAC_H_

#include <stdlib.h>
#include <libkern/OSAtomic.h>

#include "common_types.h"

namespace webrtc {
class Atomic32Impl
{
public:
    inline Atomic32Impl(WebRtc_Word32 initialValue);
    inline ~Atomic32Impl();

    inline WebRtc_Word32 operator++();
    inline WebRtc_Word32 operator--();

    inline Atomic32Impl& operator=(const Atomic32Impl& rhs);
    inline Atomic32Impl& operator=(WebRtc_Word32 rhs);
    inline WebRtc_Word32 operator+=(WebRtc_Word32 rhs);
    inline WebRtc_Word32 operator-=(WebRtc_Word32 rhs);

    inline bool CompareExchange(WebRtc_Word32 newValue,
                                WebRtc_Word32 compareValue);

    inline WebRtc_Word32 Value() const;
private:
    void*        _ptrMemory;
    // Volatile ensures full memory barriers.
    volatile WebRtc_Word32* _value;
};

// TODO (hellner) use aligned_malloc instead of doing it manually.
inline Atomic32Impl::Atomic32Impl(WebRtc_Word32 initialValue)
    :
    _ptrMemory(NULL),
    _value(NULL)
{   // Align the memory associated with _value on a 32-bit boundary. This is a
    // requirement for the used Mac APIs to be atomic.
    // Keep _ptrMemory to be able to reclaim memory.
    _ptrMemory = malloc(sizeof(WebRtc_Word32)*2);
    _value = (WebRtc_Word32*) (((uintptr_t)_ptrMemory+3)&(~0x3));
    *_value = initialValue;
}

inline Atomic32Impl::~Atomic32Impl()
{
    if(_ptrMemory != NULL)
    {
        free(_ptrMemory);
    }
}

inline WebRtc_Word32 Atomic32Impl::operator++()
{
    return OSAtomicIncrement32Barrier(
               reinterpret_cast<volatile int32_t*>(_value));
}

inline WebRtc_Word32 Atomic32Impl::operator--()
{
    return OSAtomicDecrement32Barrier(
               reinterpret_cast<volatile int32_t*>(_value));
}

inline Atomic32Impl& Atomic32Impl::operator=(const Atomic32Impl& rhs)
{
    *_value = *rhs._value;
    return *this;
}

inline Atomic32Impl& Atomic32Impl::operator=(WebRtc_Word32 rhs)
{
    *_value = rhs;
    return *this;
}

inline WebRtc_Word32 Atomic32Impl::operator+=(WebRtc_Word32 rhs)
{
    return OSAtomicAdd32Barrier(rhs,
                                reinterpret_cast<volatile int32_t*>(_value));
}

inline WebRtc_Word32 Atomic32Impl::operator-=(WebRtc_Word32 rhs)
{
    return OSAtomicAdd32Barrier(-rhs,
                                reinterpret_cast<volatile int32_t*>(_value));
}

inline bool Atomic32Impl::CompareExchange(WebRtc_Word32 newValue,
                                          WebRtc_Word32 compareValue)
{
    return OSAtomicCompareAndSwap32Barrier(
               compareValue,
               newValue,
               reinterpret_cast<volatile int32_t*>(_value));
}

inline WebRtc_Word32 Atomic32Impl::Value() const
{
    return *_value;
}
} // namespace webrtc
#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_ATOMIC32_MAC_H_
