/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/differ_block.h"

#include <string.h>

#include "webrtc/typedefs.h"
#include "webrtc/modules/desktop_capture/differ_block_sse2.h"
#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"

namespace webrtc {

bool BlockDifference_C(const uint8_t* image1,
                       const uint8_t* image2,
                       int stride) {
  int width_bytes = kBlockSize * kBytesPerPixel;

  for (int y = 0; y < kBlockSize; y++) {
    if (memcmp(image1, image2, width_bytes) != 0)
      return true;
    image1 += stride;
    image2 += stride;
  }
  return false;
}

bool BlockDifference(const uint8_t* image1,
                     const uint8_t* image2,
                     int stride) {
  static bool (*diff_proc)(const uint8_t*, const uint8_t*, int) = NULL;

  if (!diff_proc) {
#if !defined(WEBRTC_ARCH_X86_FAMILY)
    // For ARM and MIPS processors, always use C version.
    // TODO(hclam): Implement a NEON version.
    diff_proc = &BlockDifference_C;
#else
    bool have_sse2 = WebRtc_GetCPUInfo(kSSE2) != 0;
    // For x86 processors, check if SSE2 is supported.
    if (have_sse2 && kBlockSize == 32) {
      diff_proc = &BlockDifference_SSE2_W32;
    } else if (have_sse2 && kBlockSize == 16) {
      diff_proc = &BlockDifference_SSE2_W16;
    } else {
      diff_proc = &BlockDifference_C;
    }
#endif
  }

  return diff_proc(image1, image2, stride);
}

}  // namespace webrtc
