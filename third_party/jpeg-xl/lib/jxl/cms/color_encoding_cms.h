// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_CMS_COLOR_ENCODING_CMS_H_
#define LIB_JXL_CMS_COLOR_ENCODING_CMS_H_

#include <cstdint>
#include <vector>

namespace jxl {
namespace cms {

using IccBytes = std::vector<uint8_t>;

// (All CIE units are for the standard 1931 2 degree observer)

// Color space the color pixel data is encoded in. The color pixel data is
// 3-channel in all cases except in case of kGray, where it uses only 1 channel.
// This also determines the amount of channels used in modular encoding.
enum class ColorSpace : uint32_t {
  // Trichromatic color data. This also includes CMYK if a kBlack
  // ExtraChannelInfo is present. This implies, if there is an ICC profile, that
  // the ICC profile uses a 3-channel color space if no kBlack extra channel is
  // present, or uses color space 'CMYK' if a kBlack extra channel is present.
  kRGB,
  // Single-channel data. This implies, if there is an ICC profile, that the ICC
  // profile also represents single-channel data and has the appropriate color
  // space ('GRAY').
  kGray,
  // Like kRGB, but implies fixed values for primaries etc.
  kXYB,
  // For non-RGB/gray data, e.g. from non-electro-optical sensors. Otherwise
  // the same conditions as kRGB apply.
  kUnknown
  // NB: don't forget to update EnumBits!
};

// Values from CICP ColourPrimaries.
enum class WhitePoint : uint32_t {
  kD65 = 1,     // sRGB/BT.709/Display P3/BT.2020
  kCustom = 2,  // Actual values encoded in separate fields
  kE = 10,      // XYZ
  kDCI = 11,    // DCI-P3
  // NB: don't forget to update EnumBits!
};

// Values from CICP ColourPrimaries
enum class Primaries : uint32_t {
  kSRGB = 1,    // Same as BT.709
  kCustom = 2,  // Actual values encoded in separate fields
  k2100 = 9,    // Same as BT.2020
  kP3 = 11,
  // NB: don't forget to update EnumBits!
};

// Values from CICP TransferCharacteristics
enum class TransferFunction : uint32_t {
  k709 = 1,
  kUnknown = 2,
  kLinear = 8,
  kSRGB = 13,
  kPQ = 16,   // from BT.2100
  kDCI = 17,  // from SMPTE RP 431-2 reference projector
  kHLG = 18,  // from BT.2100
  // NB: don't forget to update EnumBits!
};

enum class RenderingIntent : uint32_t {
  // Values match ICC sRGB encodings.
  kPerceptual = 0,  // good for photos, requires a profile with LUT.
  kRelative,        // good for logos.
  kSaturation,      // perhaps useful for CG with fully saturated colors.
  kAbsolute,        // leaves white point unchanged; good for proofing.
  // NB: don't forget to update EnumBits!
};

// Chromaticity (Y is omitted because it is 1 for white points and implicit for
// primaries)
struct CIExy {
  double x = 0.0;
  double y = 0.0;
};

struct PrimariesCIExy {
  CIExy r;
  CIExy g;
  CIExy b;
};

// Serializable form of CIExy.
struct Customxy {
  int32_t x;
  int32_t y;
};

struct CustomTransferFunction {
  // Highest reasonable value for the gamma of a transfer curve.
  static constexpr uint32_t kMaxGamma = 8192;
  static constexpr uint32_t kGammaMul = 10000000;

  bool have_gamma;

  // OETF exponent to go from linear to gamma-compressed.
  uint32_t gamma;  // Only used if have_gamma_.

  // Can be kUnknown.
  TransferFunction transfer_function;  // Only used if !have_gamma_.
};

// Compact encoding of data required to interpret and translate pixels to a
// known color space. Stored in Metadata. Thread-compatible.
struct ColorEncoding {
  // Only valid if HaveFields()
  WhitePoint white_point;
  Primaries primaries;  // Only valid if HasPrimaries()
  RenderingIntent rendering_intent;

  // If true, the codestream contains an ICC profile and we do not serialize
  // fields. Otherwise, fields are serialized and we create an ICC profile.
  bool want_icc;

  // When false, fields such as white_point and tf are invalid and must not be
  // used. This occurs after setting a raw bytes-only ICC profile, only the
  // ICC bytes may be used. The color_space_ field is still valid.
  bool have_fields = true;

  IccBytes icc;  // Valid ICC profile

  ColorSpace color_space;  // Can be kUnknown
  bool cmyk = false;

  // "late sync" fields
  CustomTransferFunction tf;
  Customxy white;  // Only used if white_point == kCustom
  Customxy red;    // Only used if primaries == kCustom
  Customxy green;  // Only used if primaries == kCustom
  Customxy blue;   // Only used if primaries == kCustom
};

}  // namespace cms
}  // namespace jxl

#endif  // LIB_JXL_CMS_COLOR_ENCODING_CMS_H_
