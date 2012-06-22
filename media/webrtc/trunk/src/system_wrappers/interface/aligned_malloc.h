/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ALIGNED_MALLOC_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ALIGNED_MALLOC_H_

#include <stddef.h>

namespace webrtc
{
    void* AlignedMalloc(
        size_t size,
        size_t alignment);
    void AlignedFree(
        void* memBlock);
}

#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_ALIGNED_MALLOC_H_
