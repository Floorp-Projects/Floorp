/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "libyuv/basic_types.h"
#include "libyuv/cpu_id.h"
#include "row.h"
#include "libyuv/video_common.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Note: to do this with Neon vld4.8 would load ARGB values into 4 registers
// and vst would select which 2 components to write.  The low level would need
// to be ARGBToBG, ARGBToGB, ARGBToRG, ARGBToGR

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_ARGBTOBAYERROW_SSSE3
__declspec(naked)
static void ARGBToBayerRow_SSSE3(const uint8* src_argb,
                                 uint8* dst_bayer, uint32 selector, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_argb
    mov        edx, [esp + 8]    // dst_bayer
    movd       xmm5, [esp + 12]  // selector
    mov        ecx, [esp + 16]   // pix
    pshufd     xmm5, xmm5, 0

  wloop:
    movdqa     xmm0, [eax]
    lea        eax, [eax + 16]
    pshufb     xmm0, xmm5
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    sub        ecx, 4
    ja         wloop
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)

#define HAS_ARGBTOBAYERROW_SSSE3
static void ARGBToBayerRow_SSSE3(const uint8* src_argb, uint8* dst_bayer,
                                 uint32 selector, int pix) {
  asm volatile (
    "movd   %3,%%xmm5                          \n"
    "pshufd $0x0,%%xmm5,%%xmm5                 \n"
"1:                                            \n"
    "movdqa (%0),%%xmm0                        \n"
    "lea    0x10(%0),%0                        \n"
    "pshufb %%xmm5,%%xmm0                      \n"
    "movd   %%xmm0,(%1)                        \n"
    "lea    0x4(%1),%1                         \n"
    "sub    $0x4,%2                            \n"
    "ja     1b                                 \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_bayer), // %1
    "+r"(pix)        // %2
  : "g"(selector)    // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif

);
}
#endif

static void ARGBToBayerRow_C(const uint8* src_argb,
                             uint8* dst_bayer, uint32 selector, int pix) {
  int index0 = selector & 0xff;
  int index1 = (selector >> 8) & 0xff;
  // Copy a row of Bayer.
  for (int x = 0; x < pix - 1; x += 2) {
    dst_bayer[0] = src_argb[index0];
    dst_bayer[1] = src_argb[index1];
    src_argb += 8;
    dst_bayer += 2;
  }
  if (pix & 1) {
    dst_bayer[0] = src_argb[index0];
  }
}

// generate a selector mask useful for pshufb
static uint32 GenerateSelector(int select0, int select1) {
  return static_cast<uint32>(select0) |
         static_cast<uint32>((select1 + 4) << 8) |
         static_cast<uint32>((select0 + 8) << 16) |
         static_cast<uint32>((select1 + 12) << 24);
}

// Converts 32 bit ARGB to any Bayer RGB format.
int ARGBToBayerRGB(const uint8* src_rgb, int src_stride_rgb,
                   uint8* dst_bayer, int dst_stride_bayer,
                   uint32 dst_fourcc_bayer,
                   int width, int height) {
  if (height < 0) {
    height = -height;
    src_rgb = src_rgb + (height - 1) * src_stride_rgb;
    src_stride_rgb = -src_stride_rgb;
  }
  void (*ARGBToBayerRow)(const uint8* src_argb,
                         uint8* dst_bayer, uint32 selector, int pix);
#if defined(HAS_ARGBTOBAYERROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 4) &&
      IS_ALIGNED(src_rgb, 16) && IS_ALIGNED(src_stride_rgb, 16) &&
      IS_ALIGNED(dst_bayer, 4) && IS_ALIGNED(dst_stride_bayer, 4)) {
    ARGBToBayerRow = ARGBToBayerRow_SSSE3;
  } else
#endif
  {
    ARGBToBayerRow = ARGBToBayerRow_C;
  }

  int blue_index = 0;
  int green_index = 1;
  int red_index = 2;

  // Now build a lookup table containing the indices for the four pixels in each
  // 2x2 Bayer grid.
  uint32 index_map[2];
  switch (dst_fourcc_bayer) {
    default:
      assert(false);
    case FOURCC_RGGB:
      index_map[0] = GenerateSelector(red_index, green_index);
      index_map[1] = GenerateSelector(green_index, blue_index);
      break;
    case FOURCC_BGGR:
      index_map[0] = GenerateSelector(blue_index, green_index);
      index_map[1] = GenerateSelector(green_index, red_index);
      break;
    case FOURCC_GRBG:
      index_map[0] = GenerateSelector(green_index, red_index);
      index_map[1] = GenerateSelector(blue_index, green_index);
      break;
    case FOURCC_GBRG:
      index_map[0] = GenerateSelector(green_index, blue_index);
      index_map[1] = GenerateSelector(red_index, green_index);
      break;
  }

  // Now convert.
  for (int y = 0; y < height; ++y) {
    ARGBToBayerRow(src_rgb, dst_bayer, index_map[y & 1], width);
    src_rgb += src_stride_rgb;
    dst_bayer += dst_stride_bayer;
  }
  return 0;
}

#define AVG(a,b) (((a) + (b)) >> 1)

static void BayerRowBG(const uint8* src_bayer0, int src_stride_bayer,
                       uint8* dst_rgb, int pix) {
  const uint8* src_bayer1 = src_bayer0 + src_stride_bayer;
  uint8 g = src_bayer0[1];
  uint8 r = src_bayer1[1];
  for (int x = 0; x < pix - 2; x += 2) {
    dst_rgb[0] = src_bayer0[0];
    dst_rgb[1] = AVG(g, src_bayer0[1]);
    dst_rgb[2] = AVG(r, src_bayer1[1]);
    dst_rgb[3] = 255U;
    dst_rgb[4] = AVG(src_bayer0[0], src_bayer0[2]);
    dst_rgb[5] = src_bayer0[1];
    dst_rgb[6] = src_bayer1[1];
    dst_rgb[7] = 255U;
    g = src_bayer0[1];
    r = src_bayer1[1];
    src_bayer0 += 2;
    src_bayer1 += 2;
    dst_rgb += 8;
  }
  dst_rgb[0] = src_bayer0[0];
  dst_rgb[1] = AVG(g, src_bayer0[1]);
  dst_rgb[2] = AVG(r, src_bayer1[1]);
  dst_rgb[3] = 255U;
  if (!(pix & 1)) {
    dst_rgb[4] = src_bayer0[0];
    dst_rgb[5] = src_bayer0[1];
    dst_rgb[6] = src_bayer1[1];
    dst_rgb[7] = 255U;
  }
}

static void BayerRowRG(const uint8* src_bayer0, int src_stride_bayer,
                       uint8* dst_rgb, int pix) {
  const uint8* src_bayer1 = src_bayer0 + src_stride_bayer;
  uint8 g = src_bayer0[1];
  uint8 b = src_bayer1[1];
  for (int x = 0; x < pix - 2; x += 2) {
    dst_rgb[0] = AVG(b, src_bayer1[1]);
    dst_rgb[1] = AVG(g, src_bayer0[1]);
    dst_rgb[2] = src_bayer0[0];
    dst_rgb[3] = 255U;
    dst_rgb[4] = src_bayer1[1];
    dst_rgb[5] = src_bayer0[1];
    dst_rgb[6] = AVG(src_bayer0[0], src_bayer0[2]);
    dst_rgb[7] = 255U;
    g = src_bayer0[1];
    b = src_bayer1[1];
    src_bayer0 += 2;
    src_bayer1 += 2;
    dst_rgb += 8;
  }
  dst_rgb[0] = AVG(b, src_bayer1[1]);
  dst_rgb[1] = AVG(g, src_bayer0[1]);
  dst_rgb[2] = src_bayer0[0];
  dst_rgb[3] = 255U;
  if (!(pix & 1)) {
    dst_rgb[4] = src_bayer1[1];
    dst_rgb[5] = src_bayer0[1];
    dst_rgb[6] = src_bayer0[0];
    dst_rgb[7] = 255U;
  }
}

static void BayerRowGB(const uint8* src_bayer0, int src_stride_bayer,
                       uint8* dst_rgb, int pix) {
  const uint8* src_bayer1 = src_bayer0 + src_stride_bayer;
  uint8 b = src_bayer0[1];
  for (int x = 0; x < pix - 2; x += 2) {
    dst_rgb[0] = AVG(b, src_bayer0[1]);
    dst_rgb[1] = src_bayer0[0];
    dst_rgb[2] = src_bayer1[0];
    dst_rgb[3] = 255U;
    dst_rgb[4] = src_bayer0[1];
    dst_rgb[5] = AVG(src_bayer0[0], src_bayer0[2]);
    dst_rgb[6] = AVG(src_bayer1[0], src_bayer1[2]);
    dst_rgb[7] = 255U;
    b = src_bayer0[1];
    src_bayer0 += 2;
    src_bayer1 += 2;
    dst_rgb += 8;
  }
  dst_rgb[0] = AVG(b, src_bayer0[1]);
  dst_rgb[1] = src_bayer0[0];
  dst_rgb[2] = src_bayer1[0];
  dst_rgb[3] = 255U;
  if (!(pix & 1)) {
    dst_rgb[4] = src_bayer0[1];
    dst_rgb[5] = src_bayer0[0];
    dst_rgb[6] = src_bayer1[0];
    dst_rgb[7] = 255U;
  }
}

static void BayerRowGR(const uint8* src_bayer0, int src_stride_bayer,
                       uint8* dst_rgb, int pix) {
  const uint8* src_bayer1 = src_bayer0 + src_stride_bayer;
  uint8 r = src_bayer0[1];
  for (int x = 0; x < pix - 2; x += 2) {
    dst_rgb[0] = src_bayer1[0];
    dst_rgb[1] = src_bayer0[0];
    dst_rgb[2] = AVG(r, src_bayer0[1]);
    dst_rgb[3] = 255U;
    dst_rgb[4] = AVG(src_bayer1[0], src_bayer1[2]);
    dst_rgb[5] = AVG(src_bayer0[0], src_bayer0[2]);
    dst_rgb[6] = src_bayer0[1];
    dst_rgb[7] = 255U;
    r = src_bayer0[1];
    src_bayer0 += 2;
    src_bayer1 += 2;
    dst_rgb += 8;
  }
  dst_rgb[0] = src_bayer1[0];
  dst_rgb[1] = src_bayer0[0];
  dst_rgb[2] = AVG(r, src_bayer0[1]);
  dst_rgb[3] = 255U;
  if (!(pix & 1)) {
    dst_rgb[4] = src_bayer1[0];
    dst_rgb[5] = src_bayer0[0];
    dst_rgb[6] = src_bayer0[1];
    dst_rgb[7] = 255U;
  }
}

// Converts any Bayer RGB format to ARGB.
int BayerRGBToARGB(const uint8* src_bayer, int src_stride_bayer,
                   uint32 src_fourcc_bayer,
                   uint8* dst_rgb, int dst_stride_rgb,
                   int width, int height) {
  if (height < 0) {
    height = -height;
    dst_rgb = dst_rgb + (height - 1) * dst_stride_rgb;
    dst_stride_rgb = -dst_stride_rgb;
  }
  void (*BayerRow0)(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_rgb, int pix);
  void (*BayerRow1)(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_rgb, int pix);

  switch (src_fourcc_bayer) {
    default:
      assert(false);
    case FOURCC_RGGB:
      BayerRow0 = BayerRowRG;
      BayerRow1 = BayerRowGB;
      break;
    case FOURCC_BGGR:
      BayerRow0 = BayerRowBG;
      BayerRow1 = BayerRowGR;
      break;
    case FOURCC_GRBG:
      BayerRow0 = BayerRowGR;
      BayerRow1 = BayerRowBG;
      break;
    case FOURCC_GBRG:
      BayerRow0 = BayerRowGB;
      BayerRow1 = BayerRowRG;
      break;
  }

  for (int y = 0; y < height - 1; y += 2) {
    BayerRow0(src_bayer, src_stride_bayer, dst_rgb, width);
    BayerRow1(src_bayer + src_stride_bayer, -src_stride_bayer,
        dst_rgb + dst_stride_rgb, width);
    src_bayer += src_stride_bayer * 2;
    dst_rgb += dst_stride_rgb * 2;
  }
  if (height & 1) {
    BayerRow0(src_bayer, -src_stride_bayer, dst_rgb, width);
  }
  return 0;
}

// Converts any Bayer RGB format to ARGB.
int BayerRGBToI420(const uint8* src_bayer, int src_stride_bayer,
                   uint32 src_fourcc_bayer,
                   uint8* dst_y, int dst_stride_y,
                   uint8* dst_u, int dst_stride_u,
                   uint8* dst_v, int dst_stride_v,
                   int width, int height) {
  if (width * 4 > kMaxStride) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    int halfheight = (height + 1) >> 1;
    dst_y = dst_y + (height - 1) * dst_stride_y;
    dst_u = dst_u + (halfheight - 1) * dst_stride_u;
    dst_v = dst_v + (halfheight - 1) * dst_stride_v;
    dst_stride_y = -dst_stride_y;
    dst_stride_u = -dst_stride_u;
    dst_stride_v = -dst_stride_v;
  }
  void (*BayerRow0)(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_rgb, int pix);
  void (*BayerRow1)(const uint8* src_bayer, int src_stride_bayer,
                    uint8* dst_rgb, int pix);
  void (*ARGBToYRow)(const uint8* src_argb, uint8* dst_y, int pix);
  void (*ARGBToUVRow)(const uint8* src_argb0, int src_stride_argb,
                      uint8* dst_u, uint8* dst_v, int width);
  SIMD_ALIGNED(uint8 row[kMaxStride * 2]);

#if defined(HAS_ARGBTOYROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(row, 16) && IS_ALIGNED(kMaxStride, 16) &&
      IS_ALIGNED(dst_y, 16) && IS_ALIGNED(dst_stride_y, 16)) {
    ARGBToYRow = ARGBToYRow_SSSE3;
  } else
#endif
  {
    ARGBToYRow = ARGBToYRow_C;
  }
#if defined(HAS_ARGBTOUVROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(row, 16) && IS_ALIGNED(kMaxStride, 16) &&
      IS_ALIGNED(dst_u, 8) && IS_ALIGNED(dst_stride_u, 8) &&
      IS_ALIGNED(dst_v, 8) && IS_ALIGNED(dst_stride_v, 8)) {
    ARGBToUVRow = ARGBToUVRow_SSSE3;
  } else
#endif
  {
    ARGBToUVRow = ARGBToUVRow_C;
  }

  switch (src_fourcc_bayer) {
    default:
      assert(false);
    case FOURCC_RGGB:
      BayerRow0 = BayerRowRG;
      BayerRow1 = BayerRowGB;
      break;
    case FOURCC_BGGR:
      BayerRow0 = BayerRowBG;
      BayerRow1 = BayerRowGR;
      break;
    case FOURCC_GRBG:
      BayerRow0 = BayerRowGR;
      BayerRow1 = BayerRowBG;
      break;
    case FOURCC_GBRG:
      BayerRow0 = BayerRowGB;
      BayerRow1 = BayerRowRG;
      break;
  }

  for (int y = 0; y < height - 1; y += 2) {
    BayerRow0(src_bayer, src_stride_bayer, row, width);
    BayerRow1(src_bayer + src_stride_bayer, -src_stride_bayer,
              row + kMaxStride, width);
    ARGBToUVRow(row, kMaxStride, dst_u, dst_v, width);
    ARGBToYRow(row, dst_y, width);
    ARGBToYRow(row + kMaxStride, dst_y + dst_stride_y, width);
    src_bayer += src_stride_bayer * 2;
    dst_y += dst_stride_y * 2;
    dst_u += dst_stride_u;
    dst_v += dst_stride_v;
  }
  // TODO(fbarchard): Make sure this filters properly
  if (height & 1) {
    BayerRow0(src_bayer, src_stride_bayer, row, width);
    ARGBToUVRow(row, 0, dst_u, dst_v, width);
    ARGBToYRow(row, dst_y, width);
  }
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
