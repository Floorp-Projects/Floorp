/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Common definitions for video, including fourcc and VideoFormat.

#ifndef INCLUDE_LIBYUV_VIDEO_COMMON_H_  // NOLINT
#define INCLUDE_LIBYUV_VIDEO_COMMON_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// Definition of FourCC codes
//////////////////////////////////////////////////////////////////////////////

// Convert four characters to a FourCC code.
// Needs to be a macro otherwise the OS X compiler complains when the kFormat*
// constants are used in a switch.
#define FOURCC(a, b, c, d) ( \
    (static_cast<uint32>(a)) | (static_cast<uint32>(b) << 8) | \
    (static_cast<uint32>(c) << 16) | (static_cast<uint32>(d) << 24))

// Some pages discussing FourCC codes:
//   http://www.fourcc.org/yuv.php
//   http://v4l2spec.bytesex.org/spec/book1.htm
//   http://developer.apple.com/quicktime/icefloe/dispatch020.html
//   http://msdn.microsoft.com/library/windows/desktop/dd206750.aspx#nv12
//   http://people.xiph.org/~xiphmont/containers/nut/nut4cc.txt

enum FourCC {
  // Canonical fourcc codes used in our code.
  FOURCC_I420 = FOURCC('I', '4', '2', '0'),
  FOURCC_I422 = FOURCC('I', '4', '2', '2'),
  FOURCC_I444 = FOURCC('I', '4', '4', '4'),
  FOURCC_I411 = FOURCC('I', '4', '1', '1'),
  FOURCC_I400 = FOURCC('I', '4', '0', '0'),
  FOURCC_YV12 = FOURCC('Y', 'V', '1', '2'),
  FOURCC_YV16 = FOURCC('Y', 'V', '1', '6'),
  FOURCC_YV24 = FOURCC('Y', 'V', '2', '4'),
  FOURCC_YUY2 = FOURCC('Y', 'U', 'Y', '2'),
  FOURCC_UYVY = FOURCC('U', 'Y', 'V', 'Y'),
  FOURCC_M420 = FOURCC('M', '4', '2', '0'),
  FOURCC_Q420 = FOURCC('Q', '4', '2', '0'),
  FOURCC_V210 = FOURCC('V', '2', '1', '0'),
  FOURCC_24BG = FOURCC('2', '4', 'B', 'G'),
  FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B'),
  FOURCC_BGRA = FOURCC('B', 'G', 'R', 'A'),
  FOURCC_ABGR = FOURCC('A', 'B', 'G', 'R'),
  FOURCC_RGBA = FOURCC('R', 'G', 'B', 'A'),
  FOURCC_RGBP = FOURCC('R', 'G', 'B', 'P'),  // bgr565.
  FOURCC_RGBO = FOURCC('R', 'G', 'B', 'O'),  // abgr1555.
  FOURCC_R444 = FOURCC('R', '4', '4', '4'),  // argb4444.
  FOURCC_RAW  = FOURCC('r', 'a', 'w', ' '),
  FOURCC_NV21 = FOURCC('N', 'V', '2', '1'),
  FOURCC_NV12 = FOURCC('N', 'V', '1', '2'),
  FOURCC_MJPG = FOURCC('M', 'J', 'P', 'G'),
  FOURCC_H264 = FOURCC('H', '2', '6', '4'),
  // Next four are Bayer RGB formats. The four characters define the order of
  // the colours in each 2x2 pixel grid, going left-to-right and top-to-bottom.
  FOURCC_RGGB = FOURCC('R', 'G', 'G', 'B'),
  FOURCC_BGGR = FOURCC('B', 'G', 'G', 'R'),
  FOURCC_GRBG = FOURCC('G', 'R', 'B', 'G'),
  FOURCC_GBRG = FOURCC('G', 'B', 'R', 'G'),

  // Aliases for canonical fourcc codes, replaced with their canonical
  // equivalents by CanonicalFourCC().
  FOURCC_IYUV = FOURCC('I', 'Y', 'U', 'V'),  // Alias for I420.
  FOURCC_YU12 = FOURCC('Y', 'U', '1', '2'),  // Alias for I420.
  FOURCC_YU16 = FOURCC('Y', 'U', '1', '6'),  // Alias for I422.
  FOURCC_YU24 = FOURCC('Y', 'U', '2', '4'),  // Alias for I444.
  FOURCC_YUYV = FOURCC('Y', 'U', 'Y', 'V'),  // Alias for YUY2.
  FOURCC_YUVS = FOURCC('y', 'u', 'v', 's'),  // Alias for YUY2 on Mac.
  FOURCC_HDYC = FOURCC('H', 'D', 'Y', 'C'),  // Alias for UYVY.
  FOURCC_2VUY = FOURCC('2', 'v', 'u', 'y'),  // Alias for UYVY.
  FOURCC_JPEG = FOURCC('J', 'P', 'E', 'G'),  // Alias for MJPG.
  FOURCC_DMB1 = FOURCC('d', 'm', 'b', '1'),  // Alias for MJPG on Mac.
  FOURCC_BA81 = FOURCC('B', 'A', '8', '1'),  // Alias for BGGR.
  FOURCC_RGB3 = FOURCC('R', 'G', 'B', '3'),  // Alias for RAW.
  FOURCC_BGR3 = FOURCC('B', 'G', 'R', '3'),  // Alias for 24BG.

  // Match any fourcc.
  FOURCC_ANY  = 0xFFFFFFFF,
};

enum FourCCBpp {
  // Canonical fourcc codes used in our code.
  FOURCC_BPP_I420 = 12,
  FOURCC_BPP_I422 = 16,
  FOURCC_BPP_I444 = 24,
  FOURCC_BPP_I411 = 12,
  FOURCC_BPP_I400 = 8,
  FOURCC_BPP_YV12 = 12,
  FOURCC_BPP_YV16 = 16,
  FOURCC_BPP_YV24 = 24,
  FOURCC_BPP_YUY2 = 16,
  FOURCC_BPP_UYVY = 16,
  FOURCC_BPP_M420 = 12,
  FOURCC_BPP_Q420 = 12,
  FOURCC_BPP_V210 = 22,  // 128 / 6 actually.
  FOURCC_BPP_24BG = 24,
  FOURCC_BPP_ARGB = 32,
  FOURCC_BPP_BGRA = 32,
  FOURCC_BPP_ABGR = 32,
  FOURCC_BPP_RGBA = 32,
  FOURCC_BPP_RGBP = 16,
  FOURCC_BPP_RGBO = 16,
  FOURCC_BPP_R444 = 16,
  FOURCC_BPP_RAW  = 24,
  FOURCC_BPP_NV21 = 12,
  FOURCC_BPP_NV12 = 12,
  FOURCC_BPP_MJPG = 0,  // 0 means unknown.
  FOURCC_BPP_H264 = 0,
  // Next four are Bayer RGB formats. The four characters define the order of
  // the colours in each 2x2 pixel grid, going left-to-right and top-to-bottom.
  FOURCC_BPP_RGGB = 8,
  FOURCC_BPP_BGGR = 8,
  FOURCC_BPP_GRBG = 8,
  FOURCC_BPP_GBRG = 8,

  // Aliases for canonical fourcc codes, replaced with their canonical
  // equivalents by CanonicalFourCC().
  FOURCC_BPP_IYUV = 12,
  FOURCC_BPP_YU12 = 12,
  FOURCC_BPP_YU16 = 16,
  FOURCC_BPP_YU24 = 24,
  FOURCC_BPP_YUYV = 16,
  FOURCC_BPP_YUVS = 16,
  FOURCC_BPP_HDYC = 16,
  FOURCC_BPP_2VUY = 16,
  FOURCC_BPP_JPEG = 1,
  FOURCC_BPP_DMB1 = 1,
  FOURCC_BPP_BA81 = 8,
  FOURCC_BPP_RGB3 = 24,
  FOURCC_BPP_BGR3 = 24,

  // Match any fourcc.
  FOURCC_BPP_ANY  = 0,  // 0 means unknown.
};

// Converts fourcc aliases into canonical ones.
LIBYUV_API uint32 CanonicalFourCC(uint32 fourcc);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_VIDEO_COMMON_H_  NOLINT
