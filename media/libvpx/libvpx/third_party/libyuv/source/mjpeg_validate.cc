/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/mjpeg_decoder.h"

#include <string.h>  // For memchr.

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Enable this to try scasb implementation.
// #define ENABLE_SCASB 1

#ifdef ENABLE_SCASB

// Multiple of 1.
__declspec(naked)
const uint8* ScanRow_ERMS(const uint8* src, uint32 val, int count) {
  __asm {
    mov        edx, edi
    mov        edi, [esp + 4]   // src
    mov        eax, [esp + 8]   // val
    mov        ecx, [esp + 12]  // count
    repne scasb
    jne        sr99
    mov        eax, edi
    sub        eax, 1
    mov        edi, edx
    ret

  sr99:
    mov        eax, 0
    mov        edi, edx
    ret
  }
}
#endif

// Helper function to scan for EOI marker.
static LIBYUV_BOOL ScanEOI(const uint8* sample, size_t sample_size) {
  const uint8* end = sample + sample_size - 1;
  const uint8* it = sample;
  for (;;) {
#ifdef ENABLE_SCASB
    it = ScanRow_ERMS(it, 0xff, end - it);
#else
    it = static_cast<const uint8*>(memchr(it, 0xff, end - it));
#endif
    if (it == NULL) {
      break;
    }
    if (it[1] == 0xd9) {
      return LIBYUV_TRUE;  // Success: Valid jpeg.
    }
    ++it;  // Skip over current 0xff.
  }
  // ERROR: Invalid jpeg end code not found. Size sample_size
  return LIBYUV_FALSE;
}

// Helper function to validate the jpeg appears intact.
LIBYUV_BOOL ValidateJpeg(const uint8* sample, size_t sample_size) {
  const size_t kBackSearchSize = 1024;
  if (sample_size < 64) {
    // ERROR: Invalid jpeg size: sample_size
    return LIBYUV_FALSE;
  }
  if (sample[0] != 0xff || sample[1] != 0xd8) {  // Start Of Image
    // ERROR: Invalid jpeg initial start code
    return LIBYUV_FALSE;
  }
  // Step over SOI marker.
  sample += 2;
  sample_size -= 2;

  // Look for the End Of Image (EOI) marker in the end kilobyte of the buffer.
  if (sample_size > kBackSearchSize) {
    if (ScanEOI(sample + sample_size - kBackSearchSize, kBackSearchSize)) {
      return LIBYUV_TRUE;  // Success: Valid jpeg.
    }
    // Reduce search size for forward search.
    sample_size = sample_size - kBackSearchSize + 1;
  }
  return ScanEOI(sample, sample_size);

}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

