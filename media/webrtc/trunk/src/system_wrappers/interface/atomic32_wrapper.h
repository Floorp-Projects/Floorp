/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Atomic system independant 32-bit integer.
// Note: uses full memory barriers.
// Note: assumes 32-bit (or higher) system
#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ATOMIC32_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ATOMIC32_WRAPPER_H_

#include "common_types.h"

namespace webrtc {
class Atomic32Impl;
class Atomic32Wrapper
{
public:
    Atomic32Wrapper(WebRtc_Word32 initialValue = 0);
    ~Atomic32Wrapper();

    // Prefix operator!
    WebRtc_Word32 operator++();
    WebRtc_Word32 operator--();

    Atomic32Wrapper& operator=(const Atomic32Wrapper& rhs);
    Atomic32Wrapper& operator=(WebRtc_Word32 rhs);

    WebRtc_Word32 operator+=(WebRtc_Word32 rhs);
    WebRtc_Word32 operator-=(WebRtc_Word32 rhs);

    // Sets the value atomically to newValue if the value equals compare value.
    // The function returns true if the exchange happened.
    bool CompareExchange(WebRtc_Word32 newValue, WebRtc_Word32 compareValue);
    WebRtc_Word32 Value() const;
private:
    // Disable the + and - operator since it's unclear what these operations
    // should do.
    Atomic32Wrapper operator+(const Atomic32Wrapper& rhs);
    Atomic32Wrapper operator-(const Atomic32Wrapper& rhs);

    WebRtc_Word32& operator++(int);
    WebRtc_Word32& operator--(int);

    // Cheshire cat to hide the implementation (faster than
    // using virtual functions)
    Atomic32Impl& _impl;
};
} // namespace webrtc
#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ATOMIC32_WRAPPER_H_
