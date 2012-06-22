/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/convert.h"

#include "libyuv/basic_types.h"
#include "libyuv/format_conversion.h"
#include "libyuv/planar_functions.h"
#include "libyuv/video_common.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Convert I420 to specified format.
// TODO(fbarchard): sample_size should be used to ensure the low levels do
// not read outside the buffer provided.  It is measured in bytes and is the
// size of the frame.  With MJPEG it is the compressed size of the frame.
// TODO(fbarchard): add I400, I444 and RAW support.
int ConvertFromI420(const uint8* y, int y_stride,
                    const uint8* u, int u_stride,
                    const uint8* v, int v_stride,
                    uint8* dst_sample, size_t dst_sample_size,
                    int width, int height,
                    uint32 format) {
  if (y == NULL || u == NULL || v == NULL || dst_sample == NULL) {
    return -1;
  }
  switch (format) {
    // Single plane formats
    case FOURCC_YUY2:
      I420ToYUY2(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width * 2,
                 width, height);
      break;
    case FOURCC_UYVY:
      I420ToUYVY(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width * 2,
                 width, height);
      break;
    case FOURCC_24BG:
      I420ToRGB24(y, y_stride,
                  u, u_stride,
                  v, v_stride,
                  dst_sample, width * 3,
                  width, height);
      break;
    case FOURCC_RAW:
      I420ToRAW(y, y_stride,
                u, u_stride,
                v, v_stride,
                dst_sample, width * 3,
                width, height);
      break;
    case FOURCC_ARGB:
      I420ToARGB(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width * 4,
                 width, height);
      break;
    case FOURCC_BGRA:
      I420ToBGRA(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width * 4,
                 width, height);
      break;
    case FOURCC_ABGR:
      I420ToABGR(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width * 4,
                 width, height);
      break;
#ifdef HAVEI420TOBAYER
    case FOURCC_BGGR:
    case FOURCC_RGGB:
    case FOURCC_GRBG:
    case FOURCC_GBRG:
      I420ToBayerRGB(y, y_stride,
                     u, u_stride,
                     v, v_stride,
                     dst_sample, width, format,
                     width, height);
      break;
#endif
    case FOURCC_I400:
      I400Copy(y, y_stride,
               dst_sample, width,
               width, height);
      break;
    // Triplanar formats
    case FOURCC_I420:
    case FOURCC_YV12: {
      int halfwidth = (width + 1) / 2;
      int halfheight = (height + 1) / 2;
      uint8* dst_u;
      uint8* dst_v;
      if (format == FOURCC_I420) {
        dst_u = dst_sample + width * height;
        dst_v = dst_u + halfwidth * halfheight;
      } else {
        dst_v = dst_sample + width * height;
        dst_u = dst_v + halfwidth * halfheight;
      }
      I420Copy(y, y_stride,
               u, u_stride,
               v, v_stride,
               dst_sample, width,
               dst_u, halfwidth,
               dst_v, halfwidth,
               width, height);
      break;
    }
    case FOURCC_I422:
    case FOURCC_YV16: {
      int halfwidth = (width + 1) / 2;
      uint8* dst_u;
      uint8* dst_v;
      if (format == FOURCC_I422) {
        dst_u = dst_sample + width * height;
        dst_v = dst_u + halfwidth * height;
      } else {
        dst_v = dst_sample + width * height;
        dst_u = dst_v + halfwidth * height;
      }
      I420ToI422(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width,
                 dst_u, halfwidth,
                 dst_v, halfwidth,
                 width, height);
      break;
    }
    case FOURCC_I444:
    case FOURCC_YV24: {
      uint8* dst_u;
      uint8* dst_v;
      if (format == FOURCC_I444) {
        dst_u = dst_sample + width * height;
        dst_v = dst_u + width * height;
      } else {
        dst_v = dst_sample + width * height;
        dst_u = dst_v + width * height;
      }
      I420ToI444(y, y_stride,
                 u, u_stride,
                 v, v_stride,
                 dst_sample, width,
                 dst_u, width,
                 dst_v, width,
                 width, height);
      break;
    }

    // Formats not supported - MJPG, biplanar, some rgb formats.
    default:
      return -1;  // unknown fourcc - return failure code.
  }
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
