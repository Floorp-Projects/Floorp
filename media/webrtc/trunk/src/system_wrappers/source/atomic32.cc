/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "atomic32_wrapper.h"

#if defined(_WIN32)
    #include "atomic32_win.h"
#elif defined(WEBRTC_LINUX)
    #include "atomic32_linux.h"
#elif defined(WEBRTC_MAC)
    #include "atomic32_mac.h"
#else
    #error unsupported os!
#endif

namespace webrtc {
Atomic32Wrapper::Atomic32Wrapper(WebRtc_Word32 initialValue)
    : _impl(*new Atomic32Impl(initialValue))
{
}

Atomic32Wrapper::~Atomic32Wrapper()
{
    delete &_impl;
}

WebRtc_Word32 Atomic32Wrapper::operator++()
{
    return ++_impl;
}

WebRtc_Word32 Atomic32Wrapper::operator--()
{
    return --_impl;
}

// Read and write to properly aligned variables are atomic operations.
// Ex reference (for Windows): http://msdn.microsoft.com/en-us/library/ms684122(v=VS.85).aspx
// TODO (hellner) operator= and Atomic32Wrapper::Value() can be fully
// implemented here.
Atomic32Wrapper& Atomic32Wrapper::operator=(const Atomic32Wrapper& rhs)
{
    if(this == &rhs)
    {
        return *this;
    }
    _impl = rhs._impl;
    return *this;
}

Atomic32Wrapper& Atomic32Wrapper::operator=(WebRtc_Word32 rhs)
{
    _impl = rhs;
    return *this;
}

WebRtc_Word32 Atomic32Wrapper::operator+=(WebRtc_Word32 rhs)
{
    return _impl += rhs;
}

WebRtc_Word32 Atomic32Wrapper::operator-=(WebRtc_Word32 rhs)
{
    return _impl -= rhs;
}

bool Atomic32Wrapper::CompareExchange(WebRtc_Word32 newValue,
                                      WebRtc_Word32 compareValue)
{
    return _impl.CompareExchange(newValue,compareValue);
}

WebRtc_Word32 Atomic32Wrapper::Value() const
{
    return _impl.Value();
}
} // namespace webrtc
