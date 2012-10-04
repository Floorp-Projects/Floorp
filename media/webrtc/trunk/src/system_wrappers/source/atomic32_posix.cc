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
#include <inttypes.h>
#include <malloc.h>

#include "common_types.h"

namespace webrtc {

Atomic32::Atomic32(WebRtc_Word32 initialValue) : _value(initialValue)
{
    assert(Is32bitAligned());
}

Atomic32::~Atomic32()
{
}

WebRtc_Word32 Atomic32::operator++()
{
    return __sync_fetch_and_add(&_value, 1) + 1;
}

WebRtc_Word32 Atomic32::operator--()
{
    return __sync_fetch_and_sub(&_value, 1) - 1;
}

WebRtc_Word32 Atomic32::operator+=(WebRtc_Word32 value)
{
    WebRtc_Word32 returnValue = __sync_fetch_and_add(&_value, value);
    returnValue += value;
    return returnValue;
}

WebRtc_Word32 Atomic32::operator-=(WebRtc_Word32 value)
{
    WebRtc_Word32 returnValue = __sync_fetch_and_sub(&_value, value);
    returnValue -= value;
    return returnValue;
}

bool Atomic32::CompareExchange(WebRtc_Word32 newValue,
                               WebRtc_Word32 compareValue)
{
    return __sync_bool_compare_and_swap(&_value, compareValue, newValue);
}

WebRtc_Word32 Atomic32::Value() const
{
    return _value;
}
} // namespace webrtc
