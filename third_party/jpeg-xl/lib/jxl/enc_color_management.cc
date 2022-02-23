// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_color_management.h"

#ifndef JPEGXL_ENABLE_SKCMS
#define JPEGXL_ENABLE_SKCMS 0
#endif

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_color_management.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/field_encodings.h"
#include "lib/jxl/linalg.h"
#include "lib/jxl/transfer_functions-inl.h"
#if JPEGXL_ENABLE_SKCMS
#include "lib/jxl/enc_jxl_skcms.h"
#else  // JPEGXL_ENABLE_SKCMS
#include "lcms2.h"
#include "lcms2_plugin.h"
#endif  // JPEGXL_ENABLE_SKCMS

#define JXL_CMS_VERBOSE 0

// Define these only once. We can't use HWY_ONCE here because it is defined as
// 1 only on the last pass.
#ifndef LIB_JXL_ENC_COLOR_MANAGEMENT_CC_
#define LIB_JXL_ENC_COLOR_MANAGEMENT_CC_

namespace jxl {
namespace {
struct JxlCms {
#if JPEGXL_ENABLE_SKCMS
  PaddedBytes icc_src, icc_dst;
  skcms_ICCProfile profile_src, profile_dst;
#else
  void* lcms_transform;
#endif

  // These fields are used when the HLG OOTF or inverse OOTF must be applied.
  bool apply_hlg_ootf;
  size_t hlg_ootf_num_channels;
  // Y component of the primaries.
  std::array<float, 3> hlg_ootf_luminances;

  size_t channels_src;
  size_t channels_dst;
  ImageF buf_src;
  ImageF buf_dst;
  float intensity_target;
  bool skip_lcms = false;
  ExtraTF preprocess = ExtraTF::kNone;
  ExtraTF postprocess = ExtraTF::kNone;
};

Status ApplyHlgOotf(JxlCms* t, float* JXL_RESTRICT buf, size_t xsize,
                    bool forward);
}  // namespace
}  // namespace jxl

#endif  // LIB_JXL_ENC_COLOR_MANAGEMENT_CC_

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

#if JXL_CMS_VERBOSE >= 2
const size_t kX = 0;  // pixel index, multiplied by 3 for RGB
#endif

// xform_src = UndoGammaCompression(buf_src).
Status BeforeTransform(JxlCms* t, const float* buf_src, float* xform_src,
                       size_t buf_size) {
  switch (t->preprocess) {
    case ExtraTF::kNone:
      JXL_DASSERT(false);  // unreachable
      break;

    case ExtraTF::kPQ: {
      // By default, PQ content has an intensity target of 10000, stored
      // exactly.
      HWY_FULL(float) df;
      const auto multiplier = Set(df, t->intensity_target == 10000.f
                                          ? 1.0f
                                          : 10000.f / t->intensity_target);
      for (size_t i = 0; i < buf_size; i += Lanes(df)) {
        const auto val = Load(df, buf_src + i);
        const auto result = multiplier * TF_PQ().DisplayFromEncoded(df, val);
        Store(result, df, xform_src + i);
      }
#if JXL_CMS_VERBOSE >= 2
      printf("pre in %.4f %.4f %.4f undoPQ %.4f %.4f %.4f\n", buf_src[3 * kX],
             buf_src[3 * kX + 1], buf_src[3 * kX + 2], xform_src[3 * kX],
             xform_src[3 * kX + 1], xform_src[3 * kX + 2]);
#endif
      break;
    }

    case ExtraTF::kHLG:
      for (size_t i = 0; i < buf_size; ++i) {
        xform_src[i] = static_cast<float>(
            TF_HLG().DisplayFromEncoded(static_cast<double>(buf_src[i])));
      }
      if (t->apply_hlg_ootf) {
        JXL_RETURN_IF_ERROR(
            ApplyHlgOotf(t, xform_src, buf_size, /*forward=*/true));
      }
#if JXL_CMS_VERBOSE >= 2
      printf("pre in %.4f %.4f %.4f undoHLG %.4f %.4f %.4f\n", buf_src[3 * kX],
             buf_src[3 * kX + 1], buf_src[3 * kX + 2], xform_src[3 * kX],
             xform_src[3 * kX + 1], xform_src[3 * kX + 2]);
#endif
      break;

    case ExtraTF::kSRGB:
      HWY_FULL(float) df;
      for (size_t i = 0; i < buf_size; i += Lanes(df)) {
        const auto val = Load(df, buf_src + i);
        const auto result = TF_SRGB().DisplayFromEncoded(val);
        Store(result, df, xform_src + i);
      }
#if JXL_CMS_VERBOSE >= 2
      printf("pre in %.4f %.4f %.4f undoSRGB %.4f %.4f %.4f\n", buf_src[3 * kX],
             buf_src[3 * kX + 1], buf_src[3 * kX + 2], xform_src[3 * kX],
             xform_src[3 * kX + 1], xform_src[3 * kX + 2]);
#endif
      break;
  }
  return true;
}

// Applies gamma compression in-place.
Status AfterTransform(JxlCms* t, float* JXL_RESTRICT buf_dst, size_t buf_size) {
  switch (t->postprocess) {
    case ExtraTF::kNone:
      JXL_DASSERT(false);  // unreachable
      break;
    case ExtraTF::kPQ: {
      HWY_FULL(float) df;
      const auto multiplier =
          Set(df, t->intensity_target == 10000.f ? 1.0f
                                                 : t->intensity_target * 1e-4f);
      for (size_t i = 0; i < buf_size; i += Lanes(df)) {
        const auto val = Load(df, buf_dst + i);
        const auto result = TF_PQ().EncodedFromDisplay(df, multiplier * val);
        Store(result, df, buf_dst + i);
      }
#if JXL_CMS_VERBOSE >= 2
      printf("after PQ enc %.4f %.4f %.4f\n", buf_dst[3 * kX],
             buf_dst[3 * kX + 1], buf_dst[3 * kX + 2]);
#endif
      break;
    }
    case ExtraTF::kHLG:
      if (t->apply_hlg_ootf) {
        JXL_RETURN_IF_ERROR(
            ApplyHlgOotf(t, buf_dst, buf_size, /*forward=*/false));
      }
      for (size_t i = 0; i < buf_size; ++i) {
        buf_dst[i] = static_cast<float>(
            TF_HLG().EncodedFromDisplay(static_cast<double>(buf_dst[i])));
      }
#if JXL_CMS_VERBOSE >= 2
      printf("after HLG enc %.4f %.4f %.4f\n", buf_dst[3 * kX],
             buf_dst[3 * kX + 1], buf_dst[3 * kX + 2]);
#endif
      break;
    case ExtraTF::kSRGB:
      HWY_FULL(float) df;
      for (size_t i = 0; i < buf_size; i += Lanes(df)) {
        const auto val = Load(df, buf_dst + i);
        const auto result =
            TF_SRGB().EncodedFromDisplay(HWY_FULL(float)(), val);
        Store(result, df, buf_dst + i);
      }
#if JXL_CMS_VERBOSE >= 2
      printf("after SRGB enc %.4f %.4f %.4f\n", buf_dst[3 * kX],
             buf_dst[3 * kX + 1], buf_dst[3 * kX + 2]);
#endif
      break;
  }
  return true;
}

Status DoColorSpaceTransform(void* cms_data, const size_t thread,
                             const float* buf_src, float* buf_dst,
                             size_t xsize) {
  // No lock needed.
  JxlCms* t = reinterpret_cast<JxlCms*>(cms_data);

  const float* xform_src = buf_src;  // Read-only.
  if (t->preprocess != ExtraTF::kNone) {
    float* mutable_xform_src = t->buf_src.Row(thread);  // Writable buffer.
    JXL_RETURN_IF_ERROR(BeforeTransform(t, buf_src, mutable_xform_src,
                                        xsize * t->channels_src));
    xform_src = mutable_xform_src;
  }

#if JPEGXL_ENABLE_SKCMS
  if (t->channels_src == 1 && !t->skip_lcms) {
    // Expand from 1 to 3 channels, starting from the end in case
    // xform_src == t->buf_src.Row(thread).
    float* mutable_xform_src = t->buf_src.Row(thread);
    for (size_t i = 0; i < xsize; ++i) {
      const size_t x = xsize - i - 1;
      mutable_xform_src[x * 3] = mutable_xform_src[x * 3 + 1] =
          mutable_xform_src[x * 3 + 2] = xform_src[x];
    }
    xform_src = mutable_xform_src;
  }
#endif

#if JXL_CMS_VERBOSE >= 2
  // Save inputs for printing before in-place transforms overwrite them.
  const float in0 = xform_src[3 * kX + 0];
  const float in1 = xform_src[3 * kX + 1];
  const float in2 = xform_src[3 * kX + 2];
#endif

  if (t->skip_lcms) {
    if (buf_dst != xform_src) {
      memcpy(buf_dst, xform_src, xsize * t->channels_src * sizeof(*buf_dst));
    }  // else: in-place, no need to copy
  } else {
#if JPEGXL_ENABLE_SKCMS
    JXL_CHECK(skcms_Transform(
        xform_src, skcms_PixelFormat_RGB_fff, skcms_AlphaFormat_Opaque,
        &t->profile_src, buf_dst, skcms_PixelFormat_RGB_fff,
        skcms_AlphaFormat_Opaque, &t->profile_dst, xsize));
#else   // JPEGXL_ENABLE_SKCMS
    cmsDoTransform(t->lcms_transform, xform_src, buf_dst,
                   static_cast<cmsUInt32Number>(xsize));
#endif  // JPEGXL_ENABLE_SKCMS
  }
#if JXL_CMS_VERBOSE >= 2
  printf("xform skip%d: %.4f %.4f %.4f (%p) -> (%p) %.4f %.4f %.4f\n",
         t->skip_lcms, in0, in1, in2, xform_src, buf_dst, buf_dst[3 * kX],
         buf_dst[3 * kX + 1], buf_dst[3 * kX + 2]);
#endif

#if JPEGXL_ENABLE_SKCMS
  if (t->channels_dst == 1 && !t->skip_lcms) {
    // Contract back from 3 to 1 channel, this time forward.
    float* grayscale_buf_dst = t->buf_dst.Row(thread);
    for (size_t x = 0; x < xsize; ++x) {
      grayscale_buf_dst[x] = buf_dst[x * 3];
    }
    buf_dst = grayscale_buf_dst;
  }
#endif

  if (t->postprocess != ExtraTF::kNone) {
    JXL_RETURN_IF_ERROR(AfterTransform(t, buf_dst, xsize * t->channels_dst));
  }
  return true;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
namespace {

HWY_EXPORT(DoColorSpaceTransform);
int DoColorSpaceTransform(void* t, size_t thread, const float* buf_src,
                          float* buf_dst, size_t xsize) {
  return HWY_DYNAMIC_DISPATCH(DoColorSpaceTransform)(t, thread, buf_src,
                                                     buf_dst, xsize);
}

// Define to 1 on OS X as a workaround for older LCMS lacking MD5.
#define JXL_CMS_OLD_VERSION 0

// cms functions (even *THR) are not thread-safe, except cmsDoTransform.
// To ensure all functions are covered without frequent lock-taking nor risk of
// recursive lock, we lock in the top-level APIs.
static std::mutex& LcmsMutex() {
  static std::mutex m;
  return m;
}

#if JPEGXL_ENABLE_SKCMS

JXL_MUST_USE_RESULT CIExy CIExyFromXYZ(const float XYZ[3]) {
  const float factor = 1.f / (XYZ[0] + XYZ[1] + XYZ[2]);
  CIExy xy;
  xy.x = XYZ[0] * factor;
  xy.y = XYZ[1] * factor;
  return xy;
}

#else  // JPEGXL_ENABLE_SKCMS
// (LCMS interface requires xyY but we omit the Y for white points/primaries.)

JXL_MUST_USE_RESULT CIExy CIExyFromxyY(const cmsCIExyY& xyY) {
  CIExy xy;
  xy.x = xyY.x;
  xy.y = xyY.y;
  return xy;
}

JXL_MUST_USE_RESULT CIExy CIExyFromXYZ(const cmsCIEXYZ& XYZ) {
  cmsCIExyY xyY;
  cmsXYZ2xyY(/*Dest=*/&xyY, /*Source=*/&XYZ);
  return CIExyFromxyY(xyY);
}

JXL_MUST_USE_RESULT cmsCIEXYZ D50_XYZ() {
  // Quantized D50 as stored in ICC profiles.
  return {0.96420288, 1.0, 0.82490540};
}

JXL_MUST_USE_RESULT cmsCIExyY xyYFromCIExy(const CIExy& xy) {
  const cmsCIExyY xyY = {xy.x, xy.y, 1.0};
  return xyY;
}

// RAII

struct ProfileDeleter {
  void operator()(void* p) { cmsCloseProfile(p); }
};
using Profile = std::unique_ptr<void, ProfileDeleter>;

struct TransformDeleter {
  void operator()(void* p) { cmsDeleteTransform(p); }
};
using Transform = std::unique_ptr<void, TransformDeleter>;

struct CurveDeleter {
  void operator()(cmsToneCurve* p) { cmsFreeToneCurve(p); }
};
using Curve = std::unique_ptr<cmsToneCurve, CurveDeleter>;

Status CreateProfileXYZ(const cmsContext context,
                        Profile* JXL_RESTRICT profile) {
  profile->reset(cmsCreateXYZProfileTHR(context));
  if (profile->get() == nullptr) return JXL_FAILURE("Failed to create XYZ");
  return true;
}

#endif  // !JPEGXL_ENABLE_SKCMS

#if JPEGXL_ENABLE_SKCMS
// IMPORTANT: icc must outlive profile.
Status DecodeProfile(const uint8_t* icc, size_t size,
                     skcms_ICCProfile* const profile) {
  if (!skcms_Parse(icc, size, profile)) {
    return JXL_FAILURE("Failed to parse ICC profile with %" PRIuS " bytes",
                       size);
  }
  return true;
}
#else  // JPEGXL_ENABLE_SKCMS
Status DecodeProfile(const cmsContext context, const PaddedBytes& icc,
                     Profile* profile) {
  profile->reset(cmsOpenProfileFromMemTHR(context, icc.data(), icc.size()));
  if (profile->get() == nullptr) {
    return JXL_FAILURE("Failed to decode profile");
  }

  // WARNING: due to the LCMS MD5 issue mentioned above, many existing
  // profiles have incorrect MD5, so do not even bother checking them nor
  // generating warning clutter.

  return true;
}
#endif  // JPEGXL_ENABLE_SKCMS

#if JPEGXL_ENABLE_SKCMS

ColorSpace ColorSpaceFromProfile(const skcms_ICCProfile& profile) {
  switch (profile.data_color_space) {
    case skcms_Signature_RGB:
      return ColorSpace::kRGB;
    case skcms_Signature_Gray:
      return ColorSpace::kGray;
    default:
      return ColorSpace::kUnknown;
  }
}

// "profile1" is pre-decoded to save time in DetectTransferFunction.
Status ProfileEquivalentToICC(const skcms_ICCProfile& profile1,
                              const PaddedBytes& icc) {
  skcms_ICCProfile profile2;
  JXL_RETURN_IF_ERROR(skcms_Parse(icc.data(), icc.size(), &profile2));
  return skcms_ApproximatelyEqualProfiles(&profile1, &profile2);
}

// vector_out := matmul(matrix, vector_in)
void MatrixProduct(const skcms_Matrix3x3& matrix, const float vector_in[3],
                   float vector_out[3]) {
  for (int i = 0; i < 3; ++i) {
    vector_out[i] = 0;
    for (int j = 0; j < 3; ++j) {
      vector_out[i] += matrix.vals[i][j] * vector_in[j];
    }
  }
}

// Returns white point that was specified when creating the profile.
JXL_MUST_USE_RESULT Status UnadaptedWhitePoint(const skcms_ICCProfile& profile,
                                               CIExy* out) {
  float media_white_point_XYZ[3];
  if (!skcms_GetWTPT(&profile, media_white_point_XYZ)) {
    return JXL_FAILURE("ICC profile does not contain WhitePoint tag");
  }
  skcms_Matrix3x3 CHAD;
  if (!skcms_GetCHAD(&profile, &CHAD)) {
    // If there is no chromatic adaptation matrix, it means that the white point
    // is already unadapted.
    *out = CIExyFromXYZ(media_white_point_XYZ);
    return true;
  }
  // Otherwise, it has been adapted to the PCS white point using said matrix,
  // and the adaptation needs to be undone.
  skcms_Matrix3x3 inverse_CHAD;
  if (!skcms_Matrix3x3_invert(&CHAD, &inverse_CHAD)) {
    return JXL_FAILURE("Non-invertible ChromaticAdaptation matrix");
  }
  float unadapted_white_point_XYZ[3];
  MatrixProduct(inverse_CHAD, media_white_point_XYZ, unadapted_white_point_XYZ);
  *out = CIExyFromXYZ(unadapted_white_point_XYZ);
  return true;
}

Status IdentifyPrimaries(const skcms_ICCProfile& profile,
                         const CIExy& wp_unadapted, ColorEncoding* c) {
  if (!c->HasPrimaries()) return true;

  skcms_Matrix3x3 CHAD, inverse_CHAD;
  if (skcms_GetCHAD(&profile, &CHAD)) {
    JXL_RETURN_IF_ERROR(skcms_Matrix3x3_invert(&CHAD, &inverse_CHAD));
  } else {
    static constexpr skcms_Matrix3x3 kLMSFromXYZ = {
        {{0.8951, 0.2664, -0.1614},
         {-0.7502, 1.7135, 0.0367},
         {0.0389, -0.0685, 1.0296}}};
    static constexpr skcms_Matrix3x3 kXYZFromLMS = {
        {{0.9869929, -0.1470543, 0.1599627},
         {0.4323053, 0.5183603, 0.0492912},
         {-0.0085287, 0.0400428, 0.9684867}}};
    static constexpr float kWpD50XYZ[3] = {0.96420288, 1.0, 0.82490540};
    float wp_unadapted_XYZ[3];
    JXL_RETURN_IF_ERROR(CIEXYZFromWhiteCIExy(wp_unadapted, wp_unadapted_XYZ));
    float wp_D50_LMS[3], wp_unadapted_LMS[3];
    MatrixProduct(kLMSFromXYZ, kWpD50XYZ, wp_D50_LMS);
    MatrixProduct(kLMSFromXYZ, wp_unadapted_XYZ, wp_unadapted_LMS);
    inverse_CHAD = {{{wp_unadapted_LMS[0] / wp_D50_LMS[0], 0, 0},
                     {0, wp_unadapted_LMS[1] / wp_D50_LMS[1], 0},
                     {0, 0, wp_unadapted_LMS[2] / wp_D50_LMS[2]}}};
    inverse_CHAD = skcms_Matrix3x3_concat(&kXYZFromLMS, &inverse_CHAD);
    inverse_CHAD = skcms_Matrix3x3_concat(&inverse_CHAD, &kLMSFromXYZ);
  }

  float XYZ[3];
  PrimariesCIExy primaries;
  CIExy* const chromaticities[] = {&primaries.r, &primaries.g, &primaries.b};
  for (int i = 0; i < 3; ++i) {
    float RGB[3] = {};
    RGB[i] = 1;
    skcms_Transform(RGB, skcms_PixelFormat_RGB_fff, skcms_AlphaFormat_Opaque,
                    &profile, XYZ, skcms_PixelFormat_RGB_fff,
                    skcms_AlphaFormat_Opaque, skcms_XYZD50_profile(), 1);
    float unadapted_XYZ[3];
    MatrixProduct(inverse_CHAD, XYZ, unadapted_XYZ);
    *chromaticities[i] = CIExyFromXYZ(unadapted_XYZ);
  }
  return c->SetPrimaries(primaries);
}

void DetectTransferFunction(const skcms_ICCProfile& profile,
                            ColorEncoding* JXL_RESTRICT c) {
  if (c->tf.SetImplicit()) return;

  for (TransferFunction tf : Values<TransferFunction>()) {
    // Can only create profile from known transfer function.
    if (tf == TransferFunction::kUnknown) continue;

    c->tf.SetTransferFunction(tf);

    skcms_ICCProfile profile_test;
    PaddedBytes bytes;
    if (MaybeCreateProfile(*c, &bytes) &&
        DecodeProfile(bytes.data(), bytes.size(), &profile_test) &&
        skcms_ApproximatelyEqualProfiles(&profile, &profile_test)) {
      return;
    }
  }

  c->tf.SetTransferFunction(TransferFunction::kUnknown);
}

#else  // JPEGXL_ENABLE_SKCMS

uint32_t Type32(const ColorEncoding& c) {
  if (c.IsGray()) return TYPE_GRAY_FLT;
  return TYPE_RGB_FLT;
}

uint32_t Type64(const ColorEncoding& c) {
  if (c.IsGray()) return TYPE_GRAY_DBL;
  return TYPE_RGB_DBL;
}

ColorSpace ColorSpaceFromProfile(const Profile& profile) {
  switch (cmsGetColorSpace(profile.get())) {
    case cmsSigRgbData:
      return ColorSpace::kRGB;
    case cmsSigGrayData:
      return ColorSpace::kGray;
    default:
      return ColorSpace::kUnknown;
  }
}

// "profile1" is pre-decoded to save time in DetectTransferFunction.
Status ProfileEquivalentToICC(const cmsContext context, const Profile& profile1,
                              const PaddedBytes& icc, const ColorEncoding& c) {
  const uint32_t type_src = Type64(c);

  Profile profile2;
  JXL_RETURN_IF_ERROR(DecodeProfile(context, icc, &profile2));

  Profile profile_xyz;
  JXL_RETURN_IF_ERROR(CreateProfileXYZ(context, &profile_xyz));

  const uint32_t intent = INTENT_RELATIVE_COLORIMETRIC;
  const uint32_t flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_BLACKPOINTCOMPENSATION |
                         cmsFLAGS_HIGHRESPRECALC;
  Transform xform1(cmsCreateTransformTHR(context, profile1.get(), type_src,
                                         profile_xyz.get(), TYPE_XYZ_DBL,
                                         intent, flags));
  Transform xform2(cmsCreateTransformTHR(context, profile2.get(), type_src,
                                         profile_xyz.get(), TYPE_XYZ_DBL,
                                         intent, flags));
  if (xform1 == nullptr || xform2 == nullptr) {
    return JXL_FAILURE("Failed to create transform");
  }

  double in[3];
  double out1[3];
  double out2[3];

  // Uniformly spaced samples from very dark to almost fully bright.
  const double init = 1E-3;
  const double step = 0.2;

  if (c.IsGray()) {
    // Finer sampling and replicate each component.
    for (in[0] = init; in[0] < 1.0; in[0] += step / 8) {
      cmsDoTransform(xform1.get(), in, out1, 1);
      cmsDoTransform(xform2.get(), in, out2, 1);
      if (!ApproxEq(out1[0], out2[0], 2E-4)) {
        return false;
      }
    }
  } else {
    for (in[0] = init; in[0] < 1.0; in[0] += step) {
      for (in[1] = init; in[1] < 1.0; in[1] += step) {
        for (in[2] = init; in[2] < 1.0; in[2] += step) {
          cmsDoTransform(xform1.get(), in, out1, 1);
          cmsDoTransform(xform2.get(), in, out2, 1);
          for (size_t i = 0; i < 3; ++i) {
            if (!ApproxEq(out1[i], out2[i], 2E-4)) {
              return false;
            }
          }
        }
      }
    }
  }

  return true;
}

// Returns white point that was specified when creating the profile.
// NOTE: we can't just use cmsSigMediaWhitePointTag because its interpretation
// differs between ICC versions.
JXL_MUST_USE_RESULT cmsCIEXYZ UnadaptedWhitePoint(const cmsContext context,
                                                  const Profile& profile,
                                                  const ColorEncoding& c) {
  cmsCIEXYZ XYZ = {1.0, 1.0, 1.0};

  Profile profile_xyz;
  if (!CreateProfileXYZ(context, &profile_xyz)) return XYZ;
  // Array arguments are one per profile.
  cmsHPROFILE profiles[2] = {profile.get(), profile_xyz.get()};
  // Leave white point unchanged - that is what we're trying to extract.
  cmsUInt32Number intents[2] = {INTENT_ABSOLUTE_COLORIMETRIC,
                                INTENT_ABSOLUTE_COLORIMETRIC};
  cmsBool black_compensation[2] = {0, 0};
  cmsFloat64Number adaption[2] = {0.0, 0.0};
  // Only transforming a single pixel, so skip expensive optimizations.
  cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_HIGHRESPRECALC;
  Transform xform(cmsCreateExtendedTransform(
      context, 2, profiles, black_compensation, intents, adaption, nullptr, 0,
      Type64(c), TYPE_XYZ_DBL, flags));
  if (!xform) return XYZ;  // TODO(lode): return error

  // xy are relative, so magnitude does not matter if we ignore output Y.
  const cmsFloat64Number in[3] = {1.0, 1.0, 1.0};
  cmsDoTransform(xform.get(), in, &XYZ.X, 1);
  return XYZ;
}

Status IdentifyPrimaries(const Profile& profile, const cmsCIEXYZ& wp_unadapted,
                         ColorEncoding* c) {
  if (!c->HasPrimaries()) return true;
  if (ColorSpaceFromProfile(profile) == ColorSpace::kUnknown) return true;

  // These were adapted to the profile illuminant before storing in the profile.
  const cmsCIEXYZ* adapted_r = static_cast<const cmsCIEXYZ*>(
      cmsReadTag(profile.get(), cmsSigRedColorantTag));
  const cmsCIEXYZ* adapted_g = static_cast<const cmsCIEXYZ*>(
      cmsReadTag(profile.get(), cmsSigGreenColorantTag));
  const cmsCIEXYZ* adapted_b = static_cast<const cmsCIEXYZ*>(
      cmsReadTag(profile.get(), cmsSigBlueColorantTag));
  if (adapted_r == nullptr || adapted_g == nullptr || adapted_b == nullptr) {
    return JXL_FAILURE("Failed to retrieve colorants");
  }

  // TODO(janwas): no longer assume Bradford and D50.
  // Undo the chromatic adaptation.
  const cmsCIEXYZ d50 = D50_XYZ();

  cmsCIEXYZ r, g, b;
  cmsAdaptToIlluminant(&r, &d50, &wp_unadapted, adapted_r);
  cmsAdaptToIlluminant(&g, &d50, &wp_unadapted, adapted_g);
  cmsAdaptToIlluminant(&b, &d50, &wp_unadapted, adapted_b);

  const PrimariesCIExy rgb = {CIExyFromXYZ(r), CIExyFromXYZ(g),
                              CIExyFromXYZ(b)};
  return c->SetPrimaries(rgb);
}

void DetectTransferFunction(const cmsContext context, const Profile& profile,
                            ColorEncoding* JXL_RESTRICT c) {
  if (c->tf.SetImplicit()) return;

  for (TransferFunction tf : Values<TransferFunction>()) {
    // Can only create profile from known transfer function.
    if (tf == TransferFunction::kUnknown) continue;

    c->tf.SetTransferFunction(tf);

    PaddedBytes icc_test;
    if (MaybeCreateProfile(*c, &icc_test) &&
        ProfileEquivalentToICC(context, profile, icc_test, *c)) {
      return;
    }
  }

  c->tf.SetTransferFunction(TransferFunction::kUnknown);
}

void ErrorHandler(cmsContext context, cmsUInt32Number code, const char* text) {
  JXL_WARNING("LCMS error %u: %s", code, text);
}

// Returns a context for the current thread, creating it if necessary.
cmsContext GetContext() {
  static thread_local void* context_;
  if (context_ == nullptr) {
    context_ = cmsCreateContext(nullptr, nullptr);
    JXL_ASSERT(context_ != nullptr);

    cmsSetLogErrorHandlerTHR(static_cast<cmsContext>(context_), &ErrorHandler);
  }
  return static_cast<cmsContext>(context_);
}

#endif  // JPEGXL_ENABLE_SKCMS

Status GetPrimariesLuminances(const ColorEncoding& encoding,
                              float luminances[3]) {
  // Explanation:
  // We know that the three primaries must sum to white:
  //
  // [Xr, Xg, Xb;     [1;     [Xw;
  //  Yr, Yg, Yb;  ×   1;  =   Yw;
  //  Zr, Zg, Zb]      1]      Zw]
  //
  // By noting that X = x·(X+Y+Z), Y = y·(X+Y+Z) and Z = z·(X+Y+Z) (note the
  // lower case indicating chromaticity), and factoring the totals (X+Y+Z) out
  // of the left matrix and into the all-ones vector, we get:
  //
  // [xr, xg, xb;     [Xr + Yr + Zr;     [Xw;
  //  yr, yg, yb;  ×   Xg + Yg + Zg;  =   Yw;
  //  zr, zg, zb]      Xb + Yb + Zb]      Zw]
  //
  // Which makes it apparent that we can compute those totals as:
  //
  //                  [Xr + Yr + Zr;     inv([xr, xg, xb;      [Xw;
  //                   Xg + Yg + Zg;  =       yr, yg, yb;   ×   Yw;
  //                   Xb + Yb + Zb]          zr, zg, zb])      Zw]
  //
  // From there, by multiplying each total by its corresponding y, we get Y for
  // that primary.

  float white_XYZ[3];
  JXL_RETURN_IF_ERROR(
      CIEXYZFromWhiteCIExy(encoding.GetWhitePoint(), white_XYZ));

  const PrimariesCIExy primaries = encoding.GetPrimaries();
  double chromaticities[3][3] = {
      {primaries.r.x, primaries.g.x, primaries.b.x},
      {primaries.r.y, primaries.g.y, primaries.b.y},
      {1 - primaries.r.x - primaries.r.y, 1 - primaries.g.x - primaries.g.y,
       1 - primaries.b.x - primaries.b.y}};
  JXL_RETURN_IF_ERROR(Inv3x3Matrix(&chromaticities[0][0]));
  const double ys[3] = {primaries.r.y, primaries.g.y, primaries.b.y};
  for (size_t i = 0; i < 3; ++i) {
    luminances[i] = ys[i] * (chromaticities[i][0] * white_XYZ[0] +
                             chromaticities[i][1] * white_XYZ[1] +
                             chromaticities[i][2] * white_XYZ[2]);
  }
  return true;
}

Status ApplyHlgOotf(JxlCms* t, float* JXL_RESTRICT buf, size_t xsize,
                    bool forward) {
  if (295 <= t->intensity_target && t->intensity_target <= 305) {
    // The gamma is approximately 1 so this can essentially be skipped.
    return true;
  }
  float gamma = 1.2f * std::pow(1.111f, std::log2(t->intensity_target * 1e-3f));
  if (!forward) gamma = 1.f / gamma;

  switch (t->hlg_ootf_num_channels) {
    case 1:
      for (size_t x = 0; x < xsize; ++x) {
        buf[x] = std::pow(buf[x], gamma);
      }
      break;

    case 3:
      for (size_t x = 0; x < xsize; x += 3) {
        const float luminance = buf[x] * t->hlg_ootf_luminances[0] +
                                buf[x + 1] * t->hlg_ootf_luminances[1] +
                                buf[x + 2] * t->hlg_ootf_luminances[2];
        const float ratio = std::pow(luminance, gamma - 1);
        if (std::isfinite(ratio)) {
          buf[x] *= ratio;
          buf[x + 1] *= ratio;
          buf[x + 2] *= ratio;
          if (forward && gamma < 1) {
            // If gamma < 1, the ratio above will be > 1 which can push bright
            // saturated highlights out of gamut. There are several possible
            // ways to bring them back in-gamut; this one preserves hue and
            // saturation at the slight expense of luminance. If !forward, the
            // previously-applied forward OOTF with gamma > 1 already pushed
            // those highlights down and we are simply putting them back where
            // they were so this is not necessary.
            const float maximum =
                std::max(buf[x], std::max(buf[x + 1], buf[x + 2]));
            if (maximum > 1) {
              const float normalizer = 1.f / maximum;
              buf[x] *= normalizer;
              buf[x + 1] *= normalizer;
              buf[x + 2] *= normalizer;
            }
          }
        }
      }
      break;

    default:
      return JXL_FAILURE("HLG OOTF not implemented for %" PRIuS " channels",
                         t->hlg_ootf_num_channels);
  }
  return true;
}

}  // namespace

// All functions that call lcms directly (except ColorSpaceTransform::Run) must
// lock LcmsMutex().

Status ColorEncoding::SetFieldsFromICC() {
  // In case parsing fails, mark the ColorEncoding as invalid.
  SetColorSpace(ColorSpace::kUnknown);
  tf.SetTransferFunction(TransferFunction::kUnknown);

  if (icc_.empty()) return JXL_FAILURE("Empty ICC profile");

#if JPEGXL_ENABLE_SKCMS
  if (icc_.size() < 128) {
    return JXL_FAILURE("ICC file too small");
  }

  skcms_ICCProfile profile;
  JXL_RETURN_IF_ERROR(skcms_Parse(icc_.data(), icc_.size(), &profile));

  // skcms does not return the rendering intent, so get it from the file. It
  // is encoded as big-endian 32-bit integer in bytes 60..63.
  uint32_t rendering_intent32 = icc_[67];
  if (rendering_intent32 > 3 || icc_[64] != 0 || icc_[65] != 0 ||
      icc_[66] != 0) {
    return JXL_FAILURE("Invalid rendering intent %u\n", rendering_intent32);
  }

  SetColorSpace(ColorSpaceFromProfile(profile));

  CIExy wp_unadapted;
  JXL_RETURN_IF_ERROR(UnadaptedWhitePoint(profile, &wp_unadapted));
  JXL_RETURN_IF_ERROR(SetWhitePoint(wp_unadapted));

  // Relies on color_space.
  JXL_RETURN_IF_ERROR(IdentifyPrimaries(profile, wp_unadapted, this));

  // Relies on color_space/white point/primaries being set already.
  DetectTransferFunction(profile, this);
  // ICC and RenderingIntent have the same values (0..3).
  rendering_intent = static_cast<RenderingIntent>(rendering_intent32);
#else   // JPEGXL_ENABLE_SKCMS

  std::lock_guard<std::mutex> guard(LcmsMutex());
  const cmsContext context = GetContext();

  Profile profile;
  JXL_RETURN_IF_ERROR(DecodeProfile(context, icc_, &profile));

  const cmsUInt32Number rendering_intent32 =
      cmsGetHeaderRenderingIntent(profile.get());
  if (rendering_intent32 > 3) {
    return JXL_FAILURE("Invalid rendering intent %u\n", rendering_intent32);
  }

  SetColorSpace(ColorSpaceFromProfile(profile));

  const cmsCIEXYZ wp_unadapted = UnadaptedWhitePoint(context, profile, *this);
  JXL_RETURN_IF_ERROR(SetWhitePoint(CIExyFromXYZ(wp_unadapted)));

  // Relies on color_space.
  JXL_RETURN_IF_ERROR(IdentifyPrimaries(profile, wp_unadapted, this));

  // Relies on color_space/white point/primaries being set already.
  DetectTransferFunction(context, profile, this);

  // ICC and RenderingIntent have the same values (0..3).
  rendering_intent = static_cast<RenderingIntent>(rendering_intent32);
#endif  // JPEGXL_ENABLE_SKCMS

  return true;
}

void ColorEncoding::DecideIfWantICC() {
  PaddedBytes icc_new;
  bool equivalent;
#if JPEGXL_ENABLE_SKCMS
  skcms_ICCProfile profile;
  if (!DecodeProfile(ICC().data(), ICC().size(), &profile)) return;
  if (!MaybeCreateProfile(*this, &icc_new)) return;
  equivalent = ProfileEquivalentToICC(profile, icc_new);
#else   // JPEGXL_ENABLE_SKCMS
  const cmsContext context = GetContext();
  Profile profile;
  if (!DecodeProfile(context, ICC(), &profile)) return;
  if (!MaybeCreateProfile(*this, &icc_new)) return;
  equivalent = ProfileEquivalentToICC(context, profile, icc_new, *this);
#endif  // JPEGXL_ENABLE_SKCMS

  // Successfully created a profile => reconstruction should be equivalent.
  JXL_ASSERT(equivalent);
  want_icc_ = false;
}

namespace {

void JxlCmsDestroy(void* cms_data) {
  if (cms_data == nullptr) return;
  JxlCms* t = reinterpret_cast<JxlCms*>(cms_data);
#if !JPEGXL_ENABLE_SKCMS
  std::lock_guard<std::mutex> guard(LcmsMutex());
  TransformDeleter()(t->lcms_transform);
#endif
  delete t;
}

void* JxlCmsInit(void* init_data, size_t num_threads, size_t xsize,
                 const JxlColorProfile* input, const JxlColorProfile* output,
                 float intensity_target) {
  auto t = jxl::make_unique<JxlCms>();
  PaddedBytes icc_src, icc_dst;
  icc_src.assign(input->icc.data, input->icc.data + input->icc.size);
  ColorEncoding c_src;
  if (!c_src.SetICC(std::move(icc_src))) {
    JXL_NOTIFY_ERROR("JxlCmsInit: failed to parse input ICC");
    return nullptr;
  }
  icc_dst.assign(output->icc.data, output->icc.data + output->icc.size);
  ColorEncoding c_dst;
  if (!c_dst.SetICC(std::move(icc_dst))) {
    JXL_NOTIFY_ERROR("JxlCmsInit: failed to parse output ICC");
    return nullptr;
  }
#if JXL_CMS_VERBOSE
  printf("%s -> %s\n", Description(c_src).c_str(), Description(c_dst).c_str());
#endif

#if JPEGXL_ENABLE_SKCMS
  if (!DecodeProfile(input->icc.data, input->icc.size, &t->profile_src)) {
    JXL_NOTIFY_ERROR("JxlCmsInit: skcms failed to parse input ICC");
    return nullptr;
  }
  if (!DecodeProfile(output->icc.data, output->icc.size, &t->profile_dst)) {
    JXL_NOTIFY_ERROR("JxlCmsInit: skcms failed to parse output ICC");
    return nullptr;
  }
#else   // JPEGXL_ENABLE_SKCMS
  std::lock_guard<std::mutex> guard(LcmsMutex());
  const cmsContext context = GetContext();
  Profile profile_src, profile_dst;
  if (!DecodeProfile(context, c_src.ICC(), &profile_src)) {
    JXL_NOTIFY_ERROR("JxlCmsInit: lcms failed to parse input ICC");
    return nullptr;
  }
  if (!DecodeProfile(context, c_dst.ICC(), &profile_dst)) {
    JXL_NOTIFY_ERROR("JxlCmsInit: lcms failed to parse output ICC");
    return nullptr;
  }
#endif  // JPEGXL_ENABLE_SKCMS

  t->skip_lcms = false;
  if (c_src.SameColorEncoding(c_dst)) {
    t->skip_lcms = true;
#if JXL_CMS_VERBOSE
    printf("Skip CMS\n");
#endif
  }

  t->apply_hlg_ootf = c_src.tf.IsHLG() != c_dst.tf.IsHLG();
  if (t->apply_hlg_ootf) {
    const ColorEncoding* c_hlg = c_src.tf.IsHLG() ? &c_src : &c_dst;
    t->hlg_ootf_num_channels = c_hlg->Channels();
    if (t->hlg_ootf_num_channels == 3 &&
        !GetPrimariesLuminances(*c_hlg, t->hlg_ootf_luminances.data())) {
      JXL_NOTIFY_ERROR(
          "JxlCmsInit: failed to compute the luminances of primaries");
      return nullptr;
    }
  }

  // Special-case SRGB <=> linear if the primaries / white point are the same,
  // or any conversion where PQ or HLG is involved:
  bool src_linear = c_src.tf.IsLinear();
  const bool dst_linear = c_dst.tf.IsLinear();

  if (c_src.tf.IsPQ() || c_src.tf.IsHLG() ||
      (c_src.tf.IsSRGB() && dst_linear && c_src.SameColorSpace(c_dst))) {
    // Construct new profile as if the data were already/still linear.
    ColorEncoding c_linear_src = c_src;
    c_linear_src.tf.SetTransferFunction(TransferFunction::kLinear);
#if JPEGXL_ENABLE_SKCMS
    skcms_ICCProfile new_src;
#else  // JPEGXL_ENABLE_SKCMS
    Profile new_src;
#endif  // JPEGXL_ENABLE_SKCMS
        // Only enable ExtraTF if profile creation succeeded.
    if (MaybeCreateProfile(c_linear_src, &icc_src) &&
#if JPEGXL_ENABLE_SKCMS
        DecodeProfile(icc_src.data(), icc_src.size(), &new_src)) {
#else   // JPEGXL_ENABLE_SKCMS
        DecodeProfile(context, icc_src, &new_src)) {
#endif  // JPEGXL_ENABLE_SKCMS
#if JXL_CMS_VERBOSE
      printf("Special HLG/PQ/sRGB -> linear\n");
#endif
#if JPEGXL_ENABLE_SKCMS
      t->icc_src = std::move(icc_src);
      t->profile_src = new_src;
#else   // JPEGXL_ENABLE_SKCMS
      profile_src.swap(new_src);
#endif  // JPEGXL_ENABLE_SKCMS
      t->preprocess = c_src.tf.IsSRGB()
                          ? ExtraTF::kSRGB
                          : (c_src.tf.IsPQ() ? ExtraTF::kPQ : ExtraTF::kHLG);
      c_src = c_linear_src;
      src_linear = true;
    } else {
      if (t->apply_hlg_ootf) {
        JXL_NOTIFY_ERROR(
            "Failed to create extra linear source profile, and HLG OOTF "
            "required");
        return nullptr;
      }
      JXL_WARNING("Failed to create extra linear destination profile");
    }
  }

  if (c_dst.tf.IsPQ() || c_dst.tf.IsHLG() ||
      (c_dst.tf.IsSRGB() && src_linear && c_src.SameColorSpace(c_dst))) {
    ColorEncoding c_linear_dst = c_dst;
    c_linear_dst.tf.SetTransferFunction(TransferFunction::kLinear);
#if JPEGXL_ENABLE_SKCMS
    skcms_ICCProfile new_dst;
#else   // JPEGXL_ENABLE_SKCMS
    Profile new_dst;
#endif  // JPEGXL_ENABLE_SKCMS
    // Only enable ExtraTF if profile creation succeeded.
    if (MaybeCreateProfile(c_linear_dst, &icc_dst) &&
#if JPEGXL_ENABLE_SKCMS
        DecodeProfile(icc_dst.data(), icc_dst.size(), &new_dst)) {
#else   // JPEGXL_ENABLE_SKCMS
        DecodeProfile(context, icc_dst, &new_dst)) {
#endif  // JPEGXL_ENABLE_SKCMS
#if JXL_CMS_VERBOSE
      printf("Special linear -> HLG/PQ/sRGB\n");
#endif
#if JPEGXL_ENABLE_SKCMS
      t->icc_dst = std::move(icc_dst);
      t->profile_dst = new_dst;
#else   // JPEGXL_ENABLE_SKCMS
      profile_dst.swap(new_dst);
#endif  // JPEGXL_ENABLE_SKCMS
      t->postprocess = c_dst.tf.IsSRGB()
                           ? ExtraTF::kSRGB
                           : (c_dst.tf.IsPQ() ? ExtraTF::kPQ : ExtraTF::kHLG);
      c_dst = c_linear_dst;
    } else {
      if (t->apply_hlg_ootf) {
        JXL_NOTIFY_ERROR(
            "Failed to create extra linear destination profile, and inverse "
            "HLG OOTF required");
        return nullptr;
      }
      JXL_WARNING("Failed to create extra linear destination profile");
    }
  }

  if (c_src.SameColorEncoding(c_dst)) {
#if JXL_CMS_VERBOSE
    printf("Same intermediary linear profiles, skipping CMS\n");
#endif
    t->skip_lcms = true;
  }

#if JPEGXL_ENABLE_SKCMS
  if (!skcms_MakeUsableAsDestination(&t->profile_dst)) {
    JXL_NOTIFY_ERROR(
        "Failed to make %s usable as a color transform destination",
        Description(c_dst).c_str());
    return nullptr;
  }
#endif  // JPEGXL_ENABLE_SKCMS

  // Not including alpha channel (copied separately).
  const size_t channels_src = c_src.Channels();
  const size_t channels_dst = c_dst.Channels();
  JXL_CHECK(channels_src == channels_dst);
#if JXL_CMS_VERBOSE
  printf("Channels: %" PRIuS "; Threads: %" PRIuS "\n", channels_src,
         num_threads);
#endif

#if !JPEGXL_ENABLE_SKCMS
  // Type includes color space (XYZ vs RGB), so can be different.
  const uint32_t type_src = Type32(c_src);
  const uint32_t type_dst = Type32(c_dst);
  const uint32_t intent = static_cast<uint32_t>(c_dst.rendering_intent);
  // Use cmsFLAGS_NOCACHE to disable the 1-pixel cache and make calling
  // cmsDoTransform() thread-safe.
  const uint32_t flags = cmsFLAGS_NOCACHE | cmsFLAGS_BLACKPOINTCOMPENSATION |
                         cmsFLAGS_HIGHRESPRECALC;
  t->lcms_transform =
      cmsCreateTransformTHR(context, profile_src.get(), type_src,
                            profile_dst.get(), type_dst, intent, flags);
  if (t->lcms_transform == nullptr) {
    JXL_NOTIFY_ERROR("Failed to create transform");
    return nullptr;
  }
#endif  // !JPEGXL_ENABLE_SKCMS

  // Ideally LCMS would convert directly from External to Image3. However,
  // cmsDoTransformLineStride only accepts 32-bit BytesPerPlaneIn, whereas our
  // planes can be more than 4 GiB apart. Hence, transform inputs/outputs must
  // be interleaved. Calling cmsDoTransform for each pixel is expensive
  // (indirect call). We therefore transform rows, which requires per-thread
  // buffers. To avoid separate allocations, we use the rows of an image.
  // Because LCMS apparently also cannot handle <= 16 bit inputs and 32-bit
  // outputs (or vice versa), we use floating point input/output.
  t->channels_src = channels_src;
  t->channels_dst = channels_dst;
#if JPEGXL_ENABLE_SKCMS
  // SkiaCMS doesn't support grayscale float buffers, so we create space for RGB
  // float buffers anyway.
  t->buf_src = ImageF(xsize * 3, num_threads);
  t->buf_dst = ImageF(xsize * 3, num_threads);
#else
  t->buf_src = ImageF(xsize * channels_src, num_threads);
  t->buf_dst = ImageF(xsize * channels_dst, num_threads);
#endif
  t->intensity_target = intensity_target;
  return t.release();
}

float* JxlCmsGetSrcBuf(void* cms_data, size_t thread) {
  JxlCms* t = reinterpret_cast<JxlCms*>(cms_data);
  return t->buf_src.Row(thread);
}

float* JxlCmsGetDstBuf(void* cms_data, size_t thread) {
  JxlCms* t = reinterpret_cast<JxlCms*>(cms_data);
  return t->buf_dst.Row(thread);
}

}  // namespace

const JxlCmsInterface& GetJxlCms() {
  static constexpr JxlCmsInterface kInterface = {
      /*init_data=*/nullptr,
      /*init=*/&JxlCmsInit,
      /*get_src_buf=*/&JxlCmsGetSrcBuf,
      /*get_dst_buf=*/&JxlCmsGetDstBuf,
      /*run=*/&DoColorSpaceTransform,
      /*destroy=*/&JxlCmsDestroy};
  return kInterface;
}

}  // namespace jxl
#endif  // HWY_ONCE
