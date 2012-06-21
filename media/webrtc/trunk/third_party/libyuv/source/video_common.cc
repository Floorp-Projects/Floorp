/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "libyuv/video_common.h"

#include <sstream>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#define ARRAY_SIZE(x) (static_cast<int>((sizeof(x)/sizeof(x[0]))))

struct FourCCAliasEntry {
  uint32 alias;
  uint32 canonical;
};

static const FourCCAliasEntry kFourCCAliases[] = {
  {FOURCC_IYUV, FOURCC_I420},
  {FOURCC_YU12, FOURCC_I420},
  {FOURCC_YU16, FOURCC_I422},
  {FOURCC_YU24, FOURCC_I444},
  {FOURCC_YUYV, FOURCC_YUY2},
  {FOURCC_YUVS, FOURCC_YUY2},
  {FOURCC_HDYC, FOURCC_UYVY},
  {FOURCC_2VUY, FOURCC_UYVY},
  {FOURCC_BA81, FOURCC_BGGR},
  {FOURCC_JPEG, FOURCC_MJPG},  // Note: JPEG has DHT while MJPG does not.
  {FOURCC_RGB3, FOURCC_RAW},
  {FOURCC_BGR3, FOURCC_24BG},
};

uint32 CanonicalFourCC(uint32 fourcc) {
  for (int i = 0; i < ARRAY_SIZE(kFourCCAliases); ++i) {
    if (kFourCCAliases[i].alias == fourcc) {
      return kFourCCAliases[i].canonical;
    }
  }
  // Not an alias, so return it as-is.
  return fourcc;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

