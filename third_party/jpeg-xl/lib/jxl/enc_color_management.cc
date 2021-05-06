// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Defined by build system; this avoids IDE warnings. Must come before
// color_management.h (affects header definitions).
#ifndef JPEGXL_ENABLE_SKCMS
#define JPEGXL_ENABLE_SKCMS 0
#endif

#include "lib/jxl/enc_color_management.h"

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
#include "lib/jxl/base/status.h"
#include "lib/jxl/field_encodings.h"
#include "lib/jxl/linalg.h"
#include "lib/jxl/transfer_functions-inl.h"
#if JPEGXL_ENABLE_SKCMS
#include "skcms.h"
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
#if JPEGXL_ENABLE_SKCMS
struct ColorSpaceTransform::SkcmsICC {
  // Parsed skcms_ICCProfiles retain pointers to the original data.
  PaddedBytes icc_src_, icc_dst_;
  skcms_ICCProfile profile_src_, profile_dst_;
};
#endif  // JPEGXL_ENABLE_SKCMS
}  // namespace jxl

#endif  // LIB_JXL_ENC_COLOR_MANAGEMENT_CC_

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

#if JXL_CMS_VERBOSE >= 2
const size_t kX = 0;  // pixel index, multiplied by 3 for RGB
#endif

// xform_src = UndoGammaCompression(buf_src).
void BeforeTransform(ColorSpaceTransform* t, const float* buf_src,
                     float* xform_src) {
  switch (t->preprocess_) {
    case ExtraTF::kNone:
      JXL_DASSERT(false);  // unreachable
      break;

    case ExtraTF::kPQ: {
      // By default, PQ content has an intensity target of 10000, stored
      // exactly.
      HWY_FULL(float) df;
      const auto multiplier = Set(df, t->intensity_target_ == 10000.f
                                          ? 1.0f
                                          : 10000.f / t->intensity_target_);
      for (size_t i = 0; i < t->buf_src_.xsize(); i += Lanes(df)) {
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
      for (size_t i = 0; i < t->buf_src_.xsize(); ++i) {
        xform_src[i] = static_cast<float>(
            TF_HLG().DisplayFromEncoded(static_cast<double>(buf_src[i])));
      }
#if JXL_CMS_VERBOSE >= 2
      printf("pre in %.4f %.4f %.4f undoHLG %.4f %.4f %.4f\n", buf_src[3 * kX],
             buf_src[3 * kX + 1], buf_src[3 * kX + 2], xform_src[3 * kX],
             xform_src[3 * kX + 1], xform_src[3 * kX + 2]);
#endif
      break;

    case ExtraTF::kSRGB:
      HWY_FULL(float) df;
      for (size_t i = 0; i < t->buf_src_.xsize(); i += Lanes(df)) {
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
}

// Applies gamma compression in-place.
void AfterTransform(ColorSpaceTransform* t, float* JXL_RESTRICT buf_dst) {
  switch (t->postprocess_) {
    case ExtraTF::kNone:
      JXL_DASSERT(false);  // unreachable
      break;
    case ExtraTF::kPQ: {
      HWY_FULL(float) df;
      const auto multiplier = Set(df, t->intensity_target_ == 10000.f
                                          ? 1.0f
                                          : t->intensity_target_ * 1e-4f);
      for (size_t i = 0; i < t->buf_dst_.xsize(); i += Lanes(df)) {
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
      for (size_t i = 0; i < t->buf_dst_.xsize(); ++i) {
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
      for (size_t i = 0; i < t->buf_dst_.xsize(); i += Lanes(df)) {
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
}

void DoColorSpaceTransform(ColorSpaceTransform* t, const size_t thread,
                           const float* buf_src, float* buf_dst) {
  // No lock needed.

  float* xform_src = const_cast<float*>(buf_src);  // Read-only.
  if (t->preprocess_ != ExtraTF::kNone) {
    xform_src = t->buf_src_.Row(thread);  // Writable buffer.
    BeforeTransform(t, buf_src, xform_src);
  }

#if JXL_CMS_VERBOSE >= 2
  // Save inputs for printing before in-place transforms overwrite them.
  const float in0 = xform_src[3 * kX + 0];
  const float in1 = xform_src[3 * kX + 1];
  const float in2 = xform_src[3 * kX + 2];
#endif

  if (t->skip_lcms_) {
    if (buf_dst != xform_src) {
      memcpy(buf_dst, xform_src, t->buf_dst_.xsize() * sizeof(*buf_dst));
    }  // else: in-place, no need to copy
  } else {
#if JPEGXL_ENABLE_SKCMS
    JXL_CHECK(skcms_Transform(
        xform_src, skcms_PixelFormat_RGB_fff, skcms_AlphaFormat_Opaque,
        &t->skcms_icc_->profile_src_, buf_dst, skcms_PixelFormat_RGB_fff,
        skcms_AlphaFormat_Opaque, &t->skcms_icc_->profile_dst_, t->xsize_));
#else   // JPEGXL_ENABLE_SKCMS
    JXL_DASSERT(thread < t->transforms_.size());
    cmsHTRANSFORM xform = t->transforms_[thread];
    cmsDoTransform(xform, xform_src, buf_dst,
                   static_cast<cmsUInt32Number>(t->xsize_));
#endif  // JPEGXL_ENABLE_SKCMS
  }
#if JXL_CMS_VERBOSE >= 2
  printf("xform skip%d: %.4f %.4f %.4f (%p) -> (%p) %.4f %.4f %.4f\n",
         t->skip_lcms_, in0, in1, in2, xform_src, buf_dst, buf_dst[3 * kX],
         buf_dst[3 * kX + 1], buf_dst[3 * kX + 2]);
#endif

  if (t->postprocess_ != ExtraTF::kNone) {
    AfterTransform(t, buf_dst);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

HWY_EXPORT(DoColorSpaceTransform);
void DoColorSpaceTransform(ColorSpaceTransform* t, size_t thread,
                           const float* buf_src, float* buf_dst) {
  return HWY_DYNAMIC_DISPATCH(DoColorSpaceTransform)(t, thread, buf_src,
                                                     buf_dst);
}

namespace {

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
Status DecodeProfile(const PaddedBytes& icc, skcms_ICCProfile* const profile) {
  if (!skcms_Parse(icc.data(), icc.size(), profile)) {
    return JXL_FAILURE("Failed to parse ICC profile with %zu bytes",
                       icc.size());
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
    if (MaybeCreateProfile(*c, &bytes) && DecodeProfile(bytes, &profile_test) &&
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
  if (!DecodeProfile(ICC(), &profile)) return;
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

ColorSpaceTransform::~ColorSpaceTransform() {
#if !JPEGXL_ENABLE_SKCMS
  std::lock_guard<std::mutex> guard(LcmsMutex());
  for (void* p : transforms_) {
    TransformDeleter()(p);
  }
#endif
}

ColorSpaceTransform::ColorSpaceTransform()
#if JPEGXL_ENABLE_SKCMS
    : skcms_icc_(new SkcmsICC())
#endif  // JPEGXL_ENABLE_SKCMS
{
}

Status ColorSpaceTransform::Init(const ColorEncoding& c_src,
                                 const ColorEncoding& c_dst,
                                 float intensity_target, size_t xsize,
                                 const size_t num_threads) {
  std::lock_guard<std::mutex> guard(LcmsMutex());
#if JXL_CMS_VERBOSE
  printf("%s -> %s\n", Description(c_src).c_str(), Description(c_dst).c_str());
#endif

#if JPEGXL_ENABLE_SKCMS
  skcms_icc_->icc_src_ = c_src.ICC();
  skcms_icc_->icc_dst_ = c_dst.ICC();
  JXL_RETURN_IF_ERROR(
      DecodeProfile(skcms_icc_->icc_src_, &skcms_icc_->profile_src_));
  JXL_RETURN_IF_ERROR(
      DecodeProfile(skcms_icc_->icc_dst_, &skcms_icc_->profile_dst_));
#else   // JPEGXL_ENABLE_SKCMS
  const cmsContext context = GetContext();
  Profile profile_src, profile_dst;
  JXL_RETURN_IF_ERROR(DecodeProfile(context, c_src.ICC(), &profile_src));
  JXL_RETURN_IF_ERROR(DecodeProfile(context, c_dst.ICC(), &profile_dst));
#endif  // JPEGXL_ENABLE_SKCMS

  skip_lcms_ = false;
  if (c_src.SameColorEncoding(c_dst)) {
    skip_lcms_ = true;
#if JXL_CMS_VERBOSE
    printf("Skip CMS\n");
#endif
  }

  // Special-case for BT.2100 HLG/PQ and SRGB <=> linear:
  const bool src_linear = c_src.tf.IsLinear();
  const bool dst_linear = c_dst.tf.IsLinear();
  if (((c_src.tf.IsPQ() || c_src.tf.IsHLG()) && dst_linear) ||
      ((c_dst.tf.IsPQ() || c_dst.tf.IsHLG()) && src_linear) ||
      ((c_src.tf.IsPQ() != c_dst.tf.IsPQ()) && intensity_target_ != 10000) ||
      (c_src.tf.IsSRGB() && dst_linear) || (c_dst.tf.IsSRGB() && src_linear)) {
    // Construct new profiles as if the data were already/still linear.
    ColorEncoding c_linear_src = c_src;
    ColorEncoding c_linear_dst = c_dst;
    c_linear_src.tf.SetTransferFunction(TransferFunction::kLinear);
    c_linear_dst.tf.SetTransferFunction(TransferFunction::kLinear);
    PaddedBytes icc_src, icc_dst;
#if JPEGXL_ENABLE_SKCMS
    skcms_ICCProfile new_src, new_dst;
#else  // JPEGXL_ENABLE_SKCMS
    Profile new_src, new_dst;
#endif  // JPEGXL_ENABLE_SKCMS
        // Only enable ExtraTF if profile creation succeeded.
    if (MaybeCreateProfile(c_linear_src, &icc_src) &&
        MaybeCreateProfile(c_linear_dst, &icc_dst) &&
#if JPEGXL_ENABLE_SKCMS
        DecodeProfile(icc_src, &new_src) && DecodeProfile(icc_dst, &new_dst)) {
#else   // JPEGXL_ENABLE_SKCMS
        DecodeProfile(context, icc_src, &new_src) &&
        DecodeProfile(context, icc_dst, &new_dst)) {
#endif  // JPEGXL_ENABLE_SKCMS
      if (c_src.SameColorSpace(c_dst)) {
        skip_lcms_ = true;
      }
#if JXL_CMS_VERBOSE
      printf("Special linear <-> HLG/PQ/sRGB; skip=%d\n", skip_lcms_);
#endif
#if JPEGXL_ENABLE_SKCMS
      skcms_icc_->icc_src_ = PaddedBytes();
      skcms_icc_->profile_src_ = new_src;
      skcms_icc_->icc_dst_ = PaddedBytes();
      skcms_icc_->profile_dst_ = new_dst;
#else   // JPEGXL_ENABLE_SKCMS
      profile_src.swap(new_src);
      profile_dst.swap(new_dst);
#endif  // JPEGXL_ENABLE_SKCMS
      if (!c_src.tf.IsLinear()) {
        preprocess_ = c_src.tf.IsSRGB()
                          ? ExtraTF::kSRGB
                          : (c_src.tf.IsPQ() ? ExtraTF::kPQ : ExtraTF::kHLG);
      }
      if (!c_dst.tf.IsLinear()) {
        postprocess_ = c_dst.tf.IsSRGB()
                           ? ExtraTF::kSRGB
                           : (c_dst.tf.IsPQ() ? ExtraTF::kPQ : ExtraTF::kHLG);
      }
    } else {
      JXL_WARNING("Failed to create extra linear profiles");
    }
  }

#if JPEGXL_ENABLE_SKCMS
  if (!skcms_MakeUsableAsDestination(&skcms_icc_->profile_dst_)) {
    return JXL_FAILURE(
        "Failed to make %s usable as a color transform destination",
        Description(c_dst).c_str());
  }
#endif  // JPEGXL_ENABLE_SKCMS

  // Not including alpha channel (copied separately).
  const size_t channels_src = c_src.Channels();
  const size_t channels_dst = c_dst.Channels();
  JXL_CHECK(channels_src == channels_dst);
#if JXL_CMS_VERBOSE
  printf("Channels: %zu; Threads: %zu\n", channels_src, num_threads);
#endif

#if !JPEGXL_ENABLE_SKCMS
  // Type includes color space (XYZ vs RGB), so can be different.
  const uint32_t type_src = Type32(c_src);
  const uint32_t type_dst = Type32(c_dst);
  transforms_.clear();
  for (size_t i = 0; i < num_threads; ++i) {
    const uint32_t intent = static_cast<uint32_t>(c_dst.rendering_intent);
    const uint32_t flags =
        cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC;
    // NOTE: we're using the current thread's context and assuming all state
    // modified by cmsDoTransform resides in the transform, not the context.
    transforms_.emplace_back(cmsCreateTransformTHR(context, profile_src.get(),
                                                   type_src, profile_dst.get(),
                                                   type_dst, intent, flags));
    if (transforms_.back() == nullptr) {
      return JXL_FAILURE("Failed to create transform");
    }
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
#if JPEGXL_ENABLE_SKCMS
  // SkiaCMS doesn't support grayscale float buffers, so we create space for RGB
  // float buffers anyway.
  buf_src_ = ImageF(xsize * 3, num_threads);
  buf_dst_ = ImageF(xsize * 3, num_threads);
#else
  buf_src_ = ImageF(xsize * channels_src, num_threads);
  buf_dst_ = ImageF(xsize * channels_dst, num_threads);
#endif
  intensity_target_ = intensity_target;
  xsize_ = xsize;
  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
