/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_COMPILE_ASSERT_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_COMPILE_ASSERT_H_

template <bool>
struct CompileAssert {};

// Use this macro to verify at compile time that certain restrictions are met.
// The first argument is the expression to evaluate, the second is a unique
// identifier for the assert.
// Example:
//   COMPILE_ASSERT(sizeof(foo) < 128, foo_too_large);
#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg) \
    typedef CompileAssert<static_cast<bool>(expr)> \
        msg[static_cast<bool>(expr) ? 1 : -1]

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_COMPILE_ASSERT_H_
