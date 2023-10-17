// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_COLOR_ENCODING_INTERNAL_H_
#define LIB_JXL_COLOR_ENCODING_INTERNAL_H_

// Metadata for color space conversions.

#include <jxl/cms_interface.h>
#include <jxl/color_encoding.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <array>
#include <cmath>  // std::abs
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/cms/color_encoding_cms.h"
#include "lib/jxl/field_encodings.h"

namespace jxl {

using IccBytes = ::jxl::cms::IccBytes;
using ColorSpace = ::jxl::cms::ColorSpace;
using WhitePoint = ::jxl::cms::WhitePoint;
using Primaries = ::jxl::cms::Primaries;
using TransferFunction = ::jxl::cms::TransferFunction;
using RenderingIntent = ::jxl::cms::RenderingIntent;
using CIExy = ::jxl::cms::CIExy;
using PrimariesCIExy = ::jxl::cms::PrimariesCIExy;

namespace cms {

static inline const char* EnumName(ColorSpace /*unused*/) {
  return "ColorSpace";
}
static inline constexpr uint64_t EnumBits(ColorSpace /*unused*/) {
  using CS = ColorSpace;
  return MakeBit(CS::kRGB) | MakeBit(CS::kGray) | MakeBit(CS::kXYB) |
         MakeBit(CS::kUnknown);
}

static inline const char* EnumName(WhitePoint /*unused*/) {
  return "WhitePoint";
}
static inline constexpr uint64_t EnumBits(WhitePoint /*unused*/) {
  return MakeBit(WhitePoint::kD65) | MakeBit(WhitePoint::kCustom) |
         MakeBit(WhitePoint::kE) | MakeBit(WhitePoint::kDCI);
}

static inline const char* EnumName(Primaries /*unused*/) { return "Primaries"; }
static inline constexpr uint64_t EnumBits(Primaries /*unused*/) {
  using Pr = Primaries;
  return MakeBit(Pr::kSRGB) | MakeBit(Pr::kCustom) | MakeBit(Pr::k2100) |
         MakeBit(Pr::kP3);
}

static inline const char* EnumName(TransferFunction /*unused*/) {
  return "TransferFunction";
}

static inline constexpr uint64_t EnumBits(TransferFunction /*unused*/) {
  using TF = TransferFunction;
  return MakeBit(TF::k709) | MakeBit(TF::kLinear) | MakeBit(TF::kSRGB) |
         MakeBit(TF::kPQ) | MakeBit(TF::kDCI) | MakeBit(TF::kHLG) |
         MakeBit(TF::kUnknown);
}

static inline const char* EnumName(RenderingIntent /*unused*/) {
  return "RenderingIntent";
}
static inline constexpr uint64_t EnumBits(RenderingIntent /*unused*/) {
  using RI = RenderingIntent;
  return MakeBit(RI::kPerceptual) | MakeBit(RI::kRelative) |
         MakeBit(RI::kSaturation) | MakeBit(RI::kAbsolute);
}

}  // namespace cms

// Serializable form of CIExy.
struct Customxy : public Fields {
  Customxy();
  JXL_FIELDS_NAME(Customxy)

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  CIExy Get() const;
  // Returns false if x or y do not fit in the encoding.
  Status Set(const CIExy& xy);

  bool IsSame(const Customxy& other) const {
    return (storage_.x == other.storage_.x) && (storage_.y == other.storage_.y);
  }

 private:
  ::jxl::cms::Customxy storage_;
};

struct CustomTransferFunction : public Fields {
  CustomTransferFunction();
  JXL_FIELDS_NAME(CustomTransferFunction)

  // Sets fields and returns true if nonserialized_color_space has an implicit
  // transfer function, otherwise leaves fields unchanged and returns false.
  bool SetImplicit();

  // Gamma: only used for PNG inputs
  bool IsGamma() const { return storage_.have_gamma; }
  double GetGamma() const {
    JXL_ASSERT(IsGamma());
    return storage_.gamma * 1E-7;  // (0, 1)
  }
  Status SetGamma(double gamma);

  TransferFunction GetTransferFunction() const {
    JXL_ASSERT(!IsGamma());
    return storage_.transfer_function;
  }
  void SetTransferFunction(const TransferFunction tf) {
    storage_.have_gamma = false;
    storage_.transfer_function = tf;
  }

  bool IsUnknown() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::kUnknown);
  }
  bool IsSRGB() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::kSRGB);
  }
  bool IsLinear() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::kLinear);
  }
  bool IsPQ() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::kPQ);
  }
  bool IsHLG() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::kHLG);
  }
  bool Is709() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::k709);
  }
  bool IsDCI() const {
    return !storage_.have_gamma &&
           (storage_.transfer_function == TransferFunction::kDCI);
  }
  bool IsSame(const CustomTransferFunction& other) const {
    if (storage_.have_gamma != other.storage_.have_gamma) {
      return false;
    }
    if (storage_.have_gamma) {
      if (storage_.gamma != other.storage_.gamma) {
        return false;
      }
    } else {
      if (storage_.transfer_function != other.storage_.transfer_function) {
        return false;
      }
    }
    return true;
  }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  // Must be set before calling VisitFields!
  ColorSpace nonserialized_color_space = ColorSpace::kRGB;

 private:
  ::jxl::cms::CustomTransferFunction storage_;
};

// Compact encoding of data required to interpret and translate pixels to a
// known color space. Stored in Metadata. Thread-compatible.
struct ColorEncoding : public Fields {
  ColorEncoding();
  JXL_FIELDS_NAME(ColorEncoding)

  // Returns ready-to-use color encodings (initialized on-demand).
  static const ColorEncoding& SRGB(bool is_gray = false);
  static const ColorEncoding& LinearSRGB(bool is_gray = false);

  // Returns true if an ICC profile was successfully created from fields.
  // Must be called after modifying fields. Defined in color_management.cc.
  Status CreateICC();

  // Returns non-empty and valid ICC profile, unless:
  // - WantICC() == true and SetICC() was not yet called;
  // - after a failed call to SetSRGB(), SetICC(), or CreateICC().
  const IccBytes& ICC() const { return storage_.icc; }

  // Returns true if `icc` is assigned and decoded successfully. If so,
  // subsequent WantICC() will return true until DecideIfWantICC() changes it.
  // Returning false indicates data has been lost.
  Status SetICC(IccBytes&& icc, const JxlCmsInterface* cms) {
    if (icc.empty()) return false;
    storage_.icc = std::move(icc);

    if (cms == nullptr) {
      storage_.want_icc = true;
      storage_.have_fields = false;
      return true;
    }

    if (!SetFieldsFromICC(*cms)) {
      storage_.icc.clear();
      return false;
    }

    storage_.want_icc = true;
    return true;
  }

  // Sets the raw ICC profile bytes, without parsing the ICC, and without
  // updating the direct fields such as whitepoint, primaries and color
  // space. Functions to get and set fields, such as SetWhitePoint, cannot be
  // used anymore after this and functions such as IsSRGB return false no matter
  // what the contents of the icc profile.
  Status SetICCRaw(IccBytes&& icc) {
    if (icc.empty()) return false;
    storage_.icc = std::move(icc);

    storage_.want_icc = true;
    storage_.have_fields = false;
    return true;
  }

  // Returns whether to send the ICC profile in the codestream.
  bool WantICC() const { return storage_.want_icc; }

  // Return whether the direct fields are set, if false but ICC is set, only
  // raw ICC bytes are known.
  bool HaveFields() const { return storage_.have_fields; }

  // Causes WantICC() to return false if ICC() can be reconstructed from fields.
  void DecideIfWantICC(const JxlCmsInterface& cms);

  bool IsGray() const { return storage_.color_space == ColorSpace::kGray; }
  bool IsCMYK() const { return storage_.cmyk; }
  size_t Channels() const { return IsGray() ? 1 : 3; }

  // Returns false if the field is invalid and unusable.
  bool HasPrimaries() const {
    return !IsGray() && storage_.color_space != ColorSpace::kXYB;
  }

  // Returns true after setting the field to a value defined by color_space,
  // otherwise false and leaves the field unchanged.
  bool ImplicitWhitePoint() {
    if (storage_.color_space == ColorSpace::kXYB) {
      storage_.white_point = WhitePoint::kD65;
      return true;
    }
    return false;
  }

  // Returns whether the color space is known to be sRGB. If a raw unparsed ICC
  // profile is set without the fields being set, this returns false, even if
  // the content of the ICC profile would match sRGB.
  bool IsSRGB() const {
    if (!storage_.have_fields) return false;
    if (!IsGray() && storage_.color_space != ColorSpace::kRGB) return false;
    if (storage_.white_point != WhitePoint::kD65) return false;
    if (storage_.primaries != Primaries::kSRGB) return false;
    if (!tf.IsSRGB()) return false;
    return true;
  }

  // Returns whether the color space is known to be linear sRGB. If a raw
  // unparsed ICC profile is set without the fields being set, this returns
  // false, even if the content of the ICC profile would match linear sRGB.
  bool IsLinearSRGB() const {
    if (!storage_.have_fields) return false;
    if (!IsGray() && storage_.color_space != ColorSpace::kRGB) return false;
    if (storage_.white_point != WhitePoint::kD65) return false;
    if (storage_.primaries != Primaries::kSRGB) return false;
    if (!tf.IsLinear()) return false;
    return true;
  }

  Status SetSRGB(const ColorSpace cs,
                 const RenderingIntent ri = RenderingIntent::kRelative) {
    storage_.icc.clear();
    JXL_ASSERT(cs == ColorSpace::kGray || cs == ColorSpace::kRGB);
    storage_.color_space = cs;
    storage_.white_point = WhitePoint::kD65;
    storage_.primaries = Primaries::kSRGB;
    tf.SetTransferFunction(TransferFunction::kSRGB);
    storage_.rendering_intent = ri;
    return CreateICC();
  }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  // Accessors ensure tf.nonserialized_color_space is updated at the same time.
  ColorSpace GetColorSpace() const { return storage_.color_space; }
  void SetColorSpace(const ColorSpace cs) {
    storage_.color_space = cs;
    tf.nonserialized_color_space = cs;
  }

  CIExy GetWhitePoint() const;
  Status SetWhitePoint(const CIExy& xy);

  WhitePoint GetWhitePointType() const { return storage_.white_point; }
  Status SetWhitePointType(const WhitePoint& wp);

  PrimariesCIExy GetPrimaries() const;
  Status SetPrimaries(const PrimariesCIExy& xy);

  Primaries GetPrimariesType() const { return storage_.primaries; }
  Status SetPrimariesType(const Primaries& p);

  RenderingIntent GetRenderingIntent() const {
    return storage_.rendering_intent;
  }
  Status SetRenderingIntent(const RenderingIntent& ri);

  // Checks if the color spaces (including white point / primaries) are the
  // same, but ignores the transfer function, rendering intent and ICC bytes.
  bool SameColorSpace(const ColorEncoding& other) const {
    if (storage_.color_space != other.storage_.color_space) return false;

    if (storage_.white_point != other.storage_.white_point) return false;
    if (storage_.white_point == WhitePoint::kCustom) {
      if (!white_.IsSame(other.white_)) {
        return false;
      }
    }

    if (HasPrimaries() != other.HasPrimaries()) return false;
    if (HasPrimaries()) {
      if (storage_.primaries != other.storage_.primaries) return false;
      if (storage_.primaries == Primaries::kCustom) {
        if (!red_.IsSame(other.red_)) return false;
        if (!green_.IsSame(other.green_)) return false;
        if (!blue_.IsSame(other.blue_)) return false;
      }
    }
    return true;
  }

  // Checks if the color space and transfer function are the same, ignoring
  // rendering intent and ICC bytes
  bool SameColorEncoding(const ColorEncoding& other) const {
    return SameColorSpace(other) && tf.IsSame(other.tf);
  }

  mutable bool all_default;

  // Only valid if HaveFields()
  CustomTransferFunction tf;

  void ToExternal(JxlColorEncoding* external) const;
  Status FromExternal(const JxlColorEncoding& external);
  std::string Description() const;

 private:
  // Returns true if all fields have been initialized (possibly to kUnknown).
  // Returns false if the ICC profile is invalid or decoding it fails.
  Status SetFieldsFromICC(const JxlCmsInterface& cms);

  static std::array<ColorEncoding, 2> CreateC2(Primaries pr,
                                               TransferFunction tf);

  ::jxl::cms::ColorEncoding storage_;
  // Only used if white_point == kCustom.
  Customxy white_;

  // Only used if primaries == kCustom.
  Customxy red_;
  Customxy green_;
  Customxy blue_;
};

// Returns whether the two inputs are approximately equal.
static inline bool ApproxEq(const double a, const double b,
                            double max_l1 = 1E-3) {
  // Threshold should be sufficient for ICC's 15-bit fixed-point numbers.
  // We have seen differences of 7.1E-5 with lcms2 and 1E-3 with skcms.
  return std::abs(a - b) <= max_l1;
}

// Returns a representation of the ColorEncoding fields (not icc).
// Example description: "RGB_D65_SRG_Rel_Lin"
std::string Description(const ColorEncoding& c);
static inline std::ostream& operator<<(std::ostream& os,
                                       const ColorEncoding& c) {
  return os << Description(c);
}

Status PrimariesToXYZ(float rx, float ry, float gx, float gy, float bx,
                      float by, float wx, float wy, float matrix[9]);
Status PrimariesToXYZD50(float rx, float ry, float gx, float gy, float bx,
                         float by, float wx, float wy, float matrix[9]);
Status AdaptToXYZD50(float wx, float wy, float matrix[9]);

class ColorSpaceTransform {
 public:
  explicit ColorSpaceTransform(const JxlCmsInterface& cms) : cms_(cms) {}
  ~ColorSpaceTransform() {
    if (cms_data_ != nullptr) {
      cms_.destroy(cms_data_);
    }
  }

  // Cannot copy.
  ColorSpaceTransform(const ColorSpaceTransform&) = delete;
  ColorSpaceTransform& operator=(const ColorSpaceTransform&) = delete;

  Status Init(const ColorEncoding& c_src, const ColorEncoding& c_dst,
              float intensity_target, size_t xsize, size_t num_threads) {
    xsize_ = xsize;
    JxlColorProfile input_profile;
    icc_src_ = c_src.ICC();
    input_profile.icc.data = icc_src_.data();
    input_profile.icc.size = icc_src_.size();
    c_src.ToExternal(&input_profile.color_encoding);
    input_profile.num_channels = c_src.IsCMYK() ? 4 : c_src.Channels();
    JxlColorProfile output_profile;
    icc_dst_ = c_dst.ICC();
    output_profile.icc.data = icc_dst_.data();
    output_profile.icc.size = icc_dst_.size();
    c_dst.ToExternal(&output_profile.color_encoding);
    if (c_dst.IsCMYK())
      return JXL_FAILURE("Conversion to CMYK is not supported");
    output_profile.num_channels = c_dst.Channels();
    cms_data_ = cms_.init(cms_.init_data, num_threads, xsize, &input_profile,
                          &output_profile, intensity_target);
    JXL_RETURN_IF_ERROR(cms_data_ != nullptr);
    return true;
  }

  float* BufSrc(const size_t thread) const {
    return cms_.get_src_buf(cms_data_, thread);
  }

  float* BufDst(const size_t thread) const {
    return cms_.get_dst_buf(cms_data_, thread);
  }

  Status Run(const size_t thread, const float* buf_src, float* buf_dst) {
    return cms_.run(cms_data_, thread, buf_src, buf_dst, xsize_);
  }

 private:
  JxlCmsInterface cms_;
  void* cms_data_ = nullptr;
  // The interface may retain pointers into these.
  IccBytes icc_src_;
  IccBytes icc_dst_;
  size_t xsize_;
};

}  // namespace jxl

#endif  // LIB_JXL_COLOR_ENCODING_INTERNAL_H_
