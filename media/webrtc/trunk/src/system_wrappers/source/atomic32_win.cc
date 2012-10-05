/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "atomic32.h"

#include <assert.h>
#include <windows.h>

#include "common_types.h"
#include "compile_assert.h"

namespace webrtc {

Atomic32::Atomic32(WebRtc_Word32 initialValue) : _value(initialValue)
{
    // Make sure that the counter variable we're using is of the same size
    // as what the API expects.
    COMPILE_ASSERT(sizeof(_value) == sizeof(LONG));
    assert(Is32bitAligned());
}

Atomic32::~Atomic32()
{
}

WebRtc_Word32 Atomic32::operator++()
{
    return static_cast<WebRtc_Word32>(InterlockedIncrement(
        reinterpret_cast<volatile LONG*>(&_value)));
}

WebRtc_Word32 Atomic32::operator--()
{
    return static_cast<WebRtc_Word32>(InterlockedDecrement(
        reinterpret_cast<volatile LONG*>(&_value)));
}

WebRtc_Word32 Atomic32::operator+=(WebRtc_Word32 value)
{
    return InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(&_value),
                                  value);
}

WebRtc_Word32 Atomic32::operator-=(WebRtc_Word32 value)
{
    return InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(&_value),
                                  -value);
}

bool Atomic32::CompareExchange(WebRtc_Word32 newValue,
                               WebRtc_Word32 compareValue)
{
    const LONG oldValue = InterlockedCompareExchange(
          reinterpret_cast<volatile LONG*>(&_value),
          newValue,
          compareValue);
    // If the old value and the compare value is the same an exchange happened.
    return (oldValue == compareValue);
}

WebRtc_Word32 Atomic32::Value() const
{
    return _value;
}
}  // namespace webrtc
