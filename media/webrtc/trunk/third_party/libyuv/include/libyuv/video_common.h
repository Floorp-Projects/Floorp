/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
* Common definitions for video, including fourcc and VideoFormat
*/


#ifndef LIBYUV_SOURCE_VIDEO_COMMON_H_
#define LIBYUV_SOURCE_VIDEO_COMMON_H_

#include <string>

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// Definition of fourcc.
//////////////////////////////////////////////////////////////////////////////
// Convert four characters to a fourcc code.
// Needs to be a macro otherwise the OS X compiler complains when the kFormat*
// constants are used in a switch.
#define FOURCC(a, b, c, d) (\
    (static_cast<uint32>(a)) | (static_cast<uint32>(b) << 8) | \
    (static_cast<uint32>(c) << 16) | (static_cast<uint32>(d) << 24))

// Some good pages discussing FourCC codes:
//   http://developer.apple.com/quicktime/icefloe/dispatch020.html
//   http://www.fourcc.org/yuv.php
enum FourCC {
  // Canonical fourcc codes used in our code.
  FOURCC_I420 = FOURCC('I', '4', '2', '0'),
  FOURCC_I422 = FOURCC('I', '4', '2', '2'),
  FOURCC_I444 = FOURCC('I', '4', '4', '4'),
  FOURCC_I400 = FOURCC('I', '4', '0', '0'),
  FOURCC_YV12 = FOURCC('Y', 'V', '1', '2'),
  FOURCC_YV16 = FOURCC('Y', 'V', '1', '6'),
  FOURCC_YV24 = FOURCC('Y', 'V', '2', '4'),
  FOURCC_YUY2 = FOURCC('Y', 'U', 'Y', '2'),
  FOURCC_UYVY = FOURCC('U', 'Y', 'V', 'Y'),
  FOURCC_M420 = FOURCC('M', '4', '2', '0'),
  FOURCC_Q420 = FOURCC('Q', '4', '2', '0'),
  FOURCC_24BG = FOURCC('2', '4', 'B', 'G'),
  FOURCC_ABGR = FOURCC('A', 'B', 'G', 'R'),
  FOURCC_BGRA = FOURCC('B', 'G', 'R', 'A'),
  FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B'),
  FOURCC_MJPG = FOURCC('M', 'J', 'P', 'G'),
  FOURCC_RAW  = FOURCC('r', 'a', 'w', ' '),
  FOURCC_NV21 = FOURCC('N', 'V', '2', '1'),
  FOURCC_NV12 = FOURCC('N', 'V', '1', '2'),
  // Next four are Bayer RGB formats. The four characters define the order of
  // the colours in each 2x2 pixel grid, going left-to-right and top-to-bottom.
  FOURCC_RGGB = FOURCC('R', 'G', 'G', 'B'),
  FOURCC_BGGR = FOURCC('B', 'G', 'G', 'R'),
  FOURCC_GRBG = FOURCC('G', 'R', 'B', 'G'),
  FOURCC_GBRG = FOURCC('G', 'B', 'R', 'G'),

  // Aliases for canonical fourcc codes, replaced with their canonical
  // equivalents by CanonicalFourCC().
  FOURCC_IYUV = FOURCC('I', 'Y', 'U', 'V'),  // Alias for I420
  FOURCC_YU12 = FOURCC('Y', 'U', '1', '2'),  // Alias for I420
  FOURCC_YU16 = FOURCC('Y', 'U', '1', '6'),  // Alias for I422
  FOURCC_YU24 = FOURCC('Y', 'U', '2', '4'),  // Alias for I444
  FOURCC_YUYV = FOURCC('Y', 'U', 'Y', 'V'),  // Alias for YUY2
  FOURCC_YUVS = FOURCC('y', 'u', 'v', 's'),  // Alias for YUY2 on Mac
  FOURCC_HDYC = FOURCC('H', 'D', 'Y', 'C'),  // Alias for UYVY
  FOURCC_2VUY = FOURCC('2', 'v', 'u', 'y'),  // Alias for UYVY
  FOURCC_JPEG = FOURCC('J', 'P', 'E', 'G'),  // Alias for MJPG
  FOURCC_BA81 = FOURCC('B', 'A', '8', '1'),  // Alias for BGGR
  FOURCC_RGB3 = FOURCC('R', 'G', 'B', '3'),  // Alias for RAW
  FOURCC_BGR3 = FOURCC('B', 'G', 'R', '3'),  // Alias for 24BG

  // Match any fourcc.
  FOURCC_ANY  = 0xFFFFFFFF,
};

// Converts fourcc aliases into canonical ones.
uint32 CanonicalFourCC(uint32 fourcc);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // LIBYUV_SOURCE_VIDEO_COMMON_H_
