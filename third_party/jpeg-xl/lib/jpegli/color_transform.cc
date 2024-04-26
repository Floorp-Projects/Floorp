// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/color_transform.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jpegli/color_transform.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jpegli/decode_internal.h"
#include "lib/jpegli/encode_internal.h"
#include "lib/jpegli/error.h"
#include "lib/jxl/base/compiler_specific.h"

HWY_BEFORE_NAMESPACE();
namespace jpegli {
namespace HWY_NAMESPACE {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::Sub;

template <int kRed, int kGreen, int kBlue, int kAlpha>
void YCbCrToExtRGB(float* row[kMaxComponents], size_t xsize) {
  const HWY_CAPPED(float, 8) df;
  const float* row_y = row[0];
  const float* row_cb = row[1];
  const float* row_cr = row[2];
  float* row_r = row[kRed];
  float* row_g = row[kGreen];
  float* row_b = row[kBlue];
  float* row_a = row[kAlpha];

  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  const auto crcr = Set(df, 1.402f);
  const auto cgcb = Set(df, -0.114f * 1.772f / 0.587f);
  const auto cgcr = Set(df, -0.299f * 1.402f / 0.587f);
  const auto cbcb = Set(df, 1.772f);
  const auto alpha_opaque = Set(df, 127.0f / 255.0f);

  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    const auto y_vec = Load(df, row_y + x);
    const auto cb_vec = Load(df, row_cb + x);
    const auto cr_vec = Load(df, row_cr + x);
    const auto r_vec = MulAdd(crcr, cr_vec, y_vec);
    const auto g_vec = MulAdd(cgcr, cr_vec, MulAdd(cgcb, cb_vec, y_vec));
    const auto b_vec = MulAdd(cbcb, cb_vec, y_vec);
    Store(r_vec, df, row_r + x);
    Store(g_vec, df, row_g + x);
    Store(b_vec, df, row_b + x);
    if (kAlpha >= 0) {
      Store(alpha_opaque, df, row_a + x);
    }
  }
}

void YCbCrToRGB(float* row[kMaxComponents], size_t xsize) {
  YCbCrToExtRGB<0, 1, 2, -1>(row, xsize);
}

void YCbCrToBGR(float* row[kMaxComponents], size_t xsize) {
  YCbCrToExtRGB<2, 1, 0, -1>(row, xsize);
}

void YCbCrToRGBA(float* row[kMaxComponents], size_t xsize) {
  YCbCrToExtRGB<0, 1, 2, 3>(row, xsize);
}

void YCbCrToBGRA(float* row[kMaxComponents], size_t xsize) {
  YCbCrToExtRGB<2, 1, 0, 3>(row, xsize);
}

void YCbCrToARGB(float* row[kMaxComponents], size_t xsize) {
  YCbCrToExtRGB<1, 2, 3, 0>(row, xsize);
}

void YCbCrToABGR(float* row[kMaxComponents], size_t xsize) {
  YCbCrToExtRGB<3, 2, 1, 0>(row, xsize);
}

void YCCKToCMYK(float* row[kMaxComponents], size_t xsize) {
  const HWY_CAPPED(float, 8) df;
  float* JXL_RESTRICT row0 = row[0];
  float* JXL_RESTRICT row1 = row[1];
  float* JXL_RESTRICT row2 = row[2];
  YCbCrToRGB(row, xsize);
  const auto offset = Set(df, -1.0f / 255.0f);
  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    Store(Sub(offset, Load(df, row0 + x)), df, row0 + x);
    Store(Sub(offset, Load(df, row1 + x)), df, row1 + x);
    Store(Sub(offset, Load(df, row2 + x)), df, row2 + x);
  }
}

template <int kRed, int kGreen, int kBlue>
void ExtRGBToYCbCr(float* row[kMaxComponents], size_t xsize) {
  const HWY_CAPPED(float, 8) df;
  const float* row_r = row[kRed];
  const float* row_g = row[kGreen];
  const float* row_b = row[kBlue];
  float* row_y = row[0];
  float* row_cb = row[1];
  float* row_cr = row[2];
  // Full-range BT.601 as defined by JFIF Clause 7:
  // https://www.itu.int/rec/T-REC-T.871-201105-I/en
  const auto c128 = Set(df, 128.0f);
  const auto kR = Set(df, 0.299f);  // NTSC luma
  const auto kG = Set(df, 0.587f);
  const auto kB = Set(df, 0.114f);
  const auto kAmpR = Set(df, 0.701f);
  const auto kAmpB = Set(df, 0.886f);
  const auto kDiffR = Add(kAmpR, kR);
  const auto kDiffB = Add(kAmpB, kB);
  const auto kNormR = Div(Set(df, 1.0f), (Add(kAmpR, Add(kG, kB))));
  const auto kNormB = Div(Set(df, 1.0f), (Add(kR, Add(kG, kAmpB))));

  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    const auto r = Load(df, row_r + x);
    const auto g = Load(df, row_g + x);
    const auto b = Load(df, row_b + x);
    const auto r_base = Mul(r, kR);
    const auto r_diff = Mul(r, kDiffR);
    const auto g_base = Mul(g, kG);
    const auto b_base = Mul(b, kB);
    const auto b_diff = Mul(b, kDiffB);
    const auto y_base = Add(r_base, Add(g_base, b_base));
    const auto cb_vec = MulAdd(Sub(b_diff, y_base), kNormB, c128);
    const auto cr_vec = MulAdd(Sub(r_diff, y_base), kNormR, c128);
    Store(y_base, df, row_y + x);
    Store(cb_vec, df, row_cb + x);
    Store(cr_vec, df, row_cr + x);
  }
}

void RGBToYCbCr(float* row[kMaxComponents], size_t xsize) {
  ExtRGBToYCbCr<0, 1, 2>(row, xsize);
}

void BGRToYCbCr(float* row[kMaxComponents], size_t xsize) {
  ExtRGBToYCbCr<2, 1, 0>(row, xsize);
}

void ARGBToYCbCr(float* row[kMaxComponents], size_t xsize) {
  ExtRGBToYCbCr<1, 2, 3>(row, xsize);
}

void ABGRToYCbCr(float* row[kMaxComponents], size_t xsize) {
  ExtRGBToYCbCr<3, 2, 1>(row, xsize);
}

void CMYKToYCCK(float* row[kMaxComponents], size_t xsize) {
  const HWY_CAPPED(float, 8) df;
  float* JXL_RESTRICT row0 = row[0];
  float* JXL_RESTRICT row1 = row[1];
  float* JXL_RESTRICT row2 = row[2];
  const auto unity = Set(df, 255.0f);
  for (size_t x = 0; x < xsize; x += Lanes(df)) {
    Store(Sub(unity, Load(df, row0 + x)), df, row0 + x);
    Store(Sub(unity, Load(df, row1 + x)), df, row1 + x);
    Store(Sub(unity, Load(df, row2 + x)), df, row2 + x);
  }
  RGBToYCbCr(row, xsize);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jpegli
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegli {

HWY_EXPORT(CMYKToYCCK);
HWY_EXPORT(YCCKToCMYK);
HWY_EXPORT(YCbCrToRGB);
HWY_EXPORT(YCbCrToBGR);
HWY_EXPORT(YCbCrToRGBA);
HWY_EXPORT(YCbCrToBGRA);
HWY_EXPORT(YCbCrToARGB);
HWY_EXPORT(YCbCrToABGR);
HWY_EXPORT(RGBToYCbCr);
HWY_EXPORT(BGRToYCbCr);
HWY_EXPORT(ARGBToYCbCr);
HWY_EXPORT(ABGRToYCbCr);

bool CheckColorSpaceComponents(int num_components, J_COLOR_SPACE colorspace) {
  switch (colorspace) {
    case JCS_GRAYSCALE:
      return num_components == 1;
    case JCS_RGB:
    case JCS_YCbCr:
#ifdef JCS_EXTENSIONS
    case JCS_EXT_RGB:
    case JCS_EXT_BGR:
#endif
      return num_components == 3;
    case JCS_CMYK:
    case JCS_YCCK:
#ifdef JCS_EXTENSIONS
    case JCS_EXT_RGBX:
    case JCS_EXT_BGRX:
    case JCS_EXT_XBGR:
    case JCS_EXT_XRGB:
#endif
#ifdef JCS_ALPHA_EXTENSIONS
    case JCS_EXT_RGBA:
    case JCS_EXT_BGRA:
    case JCS_EXT_ABGR:
    case JCS_EXT_ARGB:
#endif
      return num_components == 4;
    default:
      // Unrecognized colorspaces can have any number of channels, since no
      // color transform will be performed on them.
      return true;
  }
}

void NullTransform(float* row[kMaxComponents], size_t len) {}

void FillAlpha(float* row, size_t len) {
  static const float kAlpha = 127.0f / 255.0f;
  for (size_t i = 0; i < len; ++i) {
    row[i] = kAlpha;
  }
}

// Works for BGR as well.
void GrayscaleToRGB(float* row[kMaxComponents], size_t len) {
  memcpy(row[1], row[0], len * sizeof(row[1][0]));
  memcpy(row[2], row[0], len * sizeof(row[2][0]));
}

// Works for BGRA as well.
void GrayscaleToRGBA(float* row[kMaxComponents], size_t len) {
  memcpy(row[1], row[0], len * sizeof(row[1][0]));
  memcpy(row[2], row[0], len * sizeof(row[2][0]));
  FillAlpha(row[3], len);
}

// Works for ABGR as well.
void GrayscaleToARGB(float* row[kMaxComponents], size_t len) {
  memcpy(row[1], row[0], len * sizeof(row[1][0]));
  memcpy(row[2], row[0], len * sizeof(row[2][0]));
  memcpy(row[3], row[0], len * sizeof(row[1][0]));
  FillAlpha(row[0], len);
}

void GrayscaleToYCbCr(float* row[kMaxComponents], size_t len) {
  memset(row[1], 0, len * sizeof(row[1][0]));
  memset(row[2], 0, len * sizeof(row[2][0]));
}

void RGBToBGR(float* row[kMaxComponents], size_t len) {
  for (size_t i = 0; i < len; ++i) {
    std::swap(row[0][i], row[2][i]);
  }
}

void RGBToRGBA(float* row[kMaxComponents], size_t len) {
  FillAlpha(row[3], len);
}

void RGBToBGRA(float* row[kMaxComponents], size_t len) {
  static const float kAlpha = 127.0f / 255.0f;
  for (size_t i = 0; i < len; ++i) {
    std::swap(row[0][i], row[2][i]);
    row[3][i] = kAlpha;
  }
}

void RGBToARGB(float* row[kMaxComponents], size_t len) {
  memcpy(row[3], row[2], len * sizeof(row[1][0]));
  memcpy(row[2], row[1], len * sizeof(row[2][0]));
  memcpy(row[1], row[0], len * sizeof(row[1][0]));
  FillAlpha(row[0], len);
}

void RGBToABGR(float* row[kMaxComponents], size_t len) {
  static const float kAlpha = 127.0f / 255.0f;
  for (size_t i = 0; i < len; ++i) {
    std::swap(row[1][i], row[2][i]);
    row[3][i] = row[0][i];
    row[0][i] = kAlpha;
  }
}

void ChooseColorTransform(j_compress_ptr cinfo) {
  jpeg_comp_master* m = cinfo->master;
  if (!CheckColorSpaceComponents(cinfo->input_components,
                                 cinfo->in_color_space)) {
    JPEGLI_ERROR("Invalid number of input components %d for colorspace %d",
                 cinfo->input_components, cinfo->in_color_space);
  }
  if (!CheckColorSpaceComponents(cinfo->num_components,
                                 cinfo->jpeg_color_space)) {
    JPEGLI_ERROR("Invalid number of components %d for colorspace %d",
                 cinfo->num_components, cinfo->jpeg_color_space);
  }
  if (cinfo->jpeg_color_space == cinfo->in_color_space) {
    if (cinfo->num_components != cinfo->input_components) {
      JPEGLI_ERROR("Input/output components mismatch:  %d vs %d",
                   cinfo->input_components, cinfo->num_components);
    }
    // No color transform requested.
    m->color_transform = NullTransform;
    return;
  }

  if (cinfo->in_color_space == JCS_RGB && m->xyb_mode) {
    JPEGLI_ERROR("Color transform on XYB colorspace is not supported.");
  }

  m->color_transform = nullptr;
  if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
    if (cinfo->in_color_space == JCS_RGB) {
      m->color_transform = HWY_DYNAMIC_DISPATCH(RGBToYCbCr);
    } else if (cinfo->in_color_space == JCS_YCbCr ||
               cinfo->in_color_space == JCS_YCCK) {
      // Since the first luminance channel is the grayscale version of the
      // image, nothing to do here
      m->color_transform = NullTransform;
    }
  } else if (cinfo->jpeg_color_space == JCS_RGB) {
    if (cinfo->in_color_space == JCS_GRAYSCALE) {
      m->color_transform = GrayscaleToRGB;
    }
  } else if (cinfo->jpeg_color_space == JCS_YCbCr) {
    if (cinfo->in_color_space == JCS_RGB) {
      m->color_transform = HWY_DYNAMIC_DISPATCH(RGBToYCbCr);
    } else if (cinfo->in_color_space == JCS_GRAYSCALE) {
      m->color_transform = GrayscaleToYCbCr;
    }
  } else if (cinfo->jpeg_color_space == JCS_YCCK) {
    if (cinfo->in_color_space == JCS_CMYK) {
      m->color_transform = HWY_DYNAMIC_DISPATCH(CMYKToYCCK);
    }
  }

  if (cinfo->jpeg_color_space == JCS_GRAYSCALE ||
      cinfo->jpeg_color_space == JCS_YCbCr) {
    switch (cinfo->in_color_space) {
#ifdef JCS_EXTENSIONS
      case JCS_EXT_RGB:
      case JCS_EXT_RGBX:
        m->color_transform = HWY_DYNAMIC_DISPATCH(RGBToYCbCr);
        break;
      case JCS_EXT_BGR:
      case JCS_EXT_BGRX:
        m->color_transform = HWY_DYNAMIC_DISPATCH(BGRToYCbCr);
        break;
      case JCS_EXT_XRGB:
        m->color_transform = HWY_DYNAMIC_DISPATCH(ARGBToYCbCr);
        break;
      case JCS_EXT_XBGR:
        m->color_transform = HWY_DYNAMIC_DISPATCH(ABGRToYCbCr);
        break;
#endif
#ifdef JCS_ALPHA_EXTENSIONS
      case JCS_EXT_RGBA:
        m->color_transform = HWY_DYNAMIC_DISPATCH(RGBToYCbCr);
        break;
      case JCS_EXT_BGRA:
        m->color_transform = HWY_DYNAMIC_DISPATCH(BGRToYCbCr);
        break;
      case JCS_EXT_ARGB:
        m->color_transform = HWY_DYNAMIC_DISPATCH(ARGBToYCbCr);
        break;
      case JCS_EXT_ABGR:
        m->color_transform = HWY_DYNAMIC_DISPATCH(ABGRToYCbCr);
        break;
#endif
      default:;  // Nothing to do.
    }
  }

  if (m->color_transform == nullptr) {
    // TODO(szabadka) Support more color transforms.
    JPEGLI_ERROR("Unsupported color transform %d -> %d", cinfo->in_color_space,
                 cinfo->jpeg_color_space);
  }
}

void ChooseColorTransform(j_decompress_ptr cinfo) {
  jpeg_decomp_master* m = cinfo->master;
  if (!CheckColorSpaceComponents(cinfo->out_color_components,
                                 cinfo->out_color_space)) {
    JPEGLI_ERROR("Invalid number of output components %d for colorspace %d",
                 cinfo->out_color_components, cinfo->out_color_space);
  }
  if (!CheckColorSpaceComponents(cinfo->num_components,
                                 cinfo->jpeg_color_space)) {
    JPEGLI_ERROR("Invalid number of components %d for colorspace %d",
                 cinfo->num_components, cinfo->jpeg_color_space);
  }
  if (cinfo->jpeg_color_space == cinfo->out_color_space) {
    if (cinfo->num_components != cinfo->out_color_components) {
      JPEGLI_ERROR("Input/output components mismatch:  %d vs %d",
                   cinfo->num_components, cinfo->out_color_components);
    }
    // No color transform requested.
    m->color_transform = NullTransform;
    return;
  }

  m->color_transform = nullptr;
  if (cinfo->jpeg_color_space == JCS_GRAYSCALE) {
    switch (cinfo->out_color_space) {
      case JCS_RGB:
        m->color_transform = GrayscaleToRGB;
        break;
#ifdef JCS_EXTENSIONS
      case JCS_EXT_RGB:
      case JCS_EXT_BGR:
        m->color_transform = GrayscaleToRGB;
        break;
      case JCS_EXT_RGBX:
      case JCS_EXT_BGRX:
        m->color_transform = GrayscaleToRGBA;
        break;
      case JCS_EXT_XRGB:
      case JCS_EXT_XBGR:
        m->color_transform = GrayscaleToARGB;
        break;
#endif
#ifdef JCS_ALPHA_EXTENSIONS
      case JCS_EXT_RGBA:
      case JCS_EXT_BGRA:
        m->color_transform = GrayscaleToRGBA;
        break;
      case JCS_EXT_ARGB:
      case JCS_EXT_ABGR:
        m->color_transform = GrayscaleToARGB;
        break;
#endif
      default:
        m->color_transform = nullptr;
    }
  } else if (cinfo->jpeg_color_space == JCS_RGB) {
    switch (cinfo->out_color_space) {
      case JCS_GRAYSCALE:
        m->color_transform = HWY_DYNAMIC_DISPATCH(RGBToYCbCr);
        break;
#ifdef JCS_EXTENSIONS
      case JCS_EXT_RGB:
        m->color_transform = NullTransform;
        break;
      case JCS_EXT_BGR:
        m->color_transform = RGBToBGR;
        break;
      case JCS_EXT_RGBX:
        m->color_transform = RGBToRGBA;
        break;
      case JCS_EXT_BGRX:
        m->color_transform = RGBToBGRA;
        break;
      case JCS_EXT_XRGB:
        m->color_transform = RGBToARGB;
        break;
      case JCS_EXT_XBGR:
        m->color_transform = RGBToABGR;
        break;
#endif
#ifdef JCS_ALPHA_EXTENSIONS
      case JCS_EXT_RGBA:
        m->color_transform = RGBToRGBA;
        break;
      case JCS_EXT_BGRA:
        m->color_transform = RGBToBGRA;
        break;
      case JCS_EXT_ARGB:
        m->color_transform = RGBToARGB;
        break;
      case JCS_EXT_ABGR:
        m->color_transform = RGBToABGR;
        break;
#endif
      default:
        m->color_transform = nullptr;
    }
  } else if (cinfo->jpeg_color_space == JCS_YCbCr) {
    switch (cinfo->out_color_space) {
      case JCS_GRAYSCALE:
        m->color_transform = NullTransform;
        break;
      case JCS_RGB:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToRGB);
        break;
#ifdef JCS_EXTENSIONS
      case JCS_EXT_RGB:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToRGB);
        break;
      case JCS_EXT_BGR:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToBGR);
        break;
      case JCS_EXT_RGBX:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToRGBA);
        break;
      case JCS_EXT_BGRX:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToBGRA);
        break;
      case JCS_EXT_XRGB:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToARGB);
        break;
      case JCS_EXT_XBGR:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToABGR);
        break;
#endif
#ifdef JCS_ALPHA_EXTENSIONS
      case JCS_EXT_RGBA:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToRGBA);
        break;
      case JCS_EXT_BGRA:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToBGRA);
        break;
      case JCS_EXT_ARGB:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToARGB);
        break;
      case JCS_EXT_ABGR:
        m->color_transform = HWY_DYNAMIC_DISPATCH(YCbCrToABGR);
        break;
#endif
      default:
        m->color_transform = nullptr;
    }
  } else if (cinfo->jpeg_color_space == JCS_YCCK) {
    if (cinfo->out_color_space == JCS_CMYK) {
      m->color_transform = HWY_DYNAMIC_DISPATCH(YCCKToCMYK);
    }
  }

  if (m->color_transform == nullptr) {
    // TODO(szabadka) Support more color transforms.
    JPEGLI_ERROR("Unsupported color transform %d -> %d",
                 cinfo->jpeg_color_space, cinfo->out_color_space);
  }
}

}  // namespace jpegli
#endif  // HWY_ONCE
