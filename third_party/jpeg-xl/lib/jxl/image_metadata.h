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

// Main codestream header bundles, the metadata that applies to all frames.

#ifndef LIB_JXL_IMAGE_METADATA_H_
#define LIB_JXL_IMAGE_METADATA_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/jpeg/jpeg_data.h"
#include "lib/jxl/opsin_params.h"

namespace jxl {

// EXIF orientation of the image. This field overrides any field present in
// actual EXIF metadata. The value tells which transformation the decoder must
// apply after decoding to display the image with the correct orientation.
enum class Orientation : uint32_t {
  // Values 1..8 match the EXIF definitions.
  kIdentity = 1,
  kFlipHorizontal,
  kRotate180,
  kFlipVertical,
  kTranspose,
  kRotate90,
  kAntiTranspose,
  kRotate270,
};
// Don't need an EnumBits because Orientation is not read via Enum().

enum class ExtraChannel : uint32_t {
  // First two enumerators (most common) are cheaper to encode
  kAlpha,
  kDepth,

  kSpotColor,
  kSelectionMask,
  kBlack,  // for CMYK
  kCFA,    // Bayer channel
  kThermal,
  kReserved0,
  kReserved1,
  kReserved2,
  kReserved3,
  kReserved4,
  kReserved5,
  kReserved6,
  kReserved7,
  kUnknown,  // disambiguated via name string, raise warning if unsupported
  kOptional  // like kUnknown but can silently be ignored
};
static inline const char* EnumName(ExtraChannel /*unused*/) {
  return "ExtraChannel";
}
static inline constexpr uint64_t EnumBits(ExtraChannel /*unused*/) {
  using EC = ExtraChannel;
  return MakeBit(EC::kAlpha) | MakeBit(EC::kDepth) | MakeBit(EC::kSpotColor) |
         MakeBit(EC::kSelectionMask) | MakeBit(EC::kBlack) | MakeBit(EC::kCFA) |
         MakeBit(EC::kUnknown) | MakeBit(EC::kOptional);
}

// Used in ImageMetadata and ExtraChannelInfo.
struct BitDepth : public Fields {
  BitDepth();
  const char* Name() const override { return "BitDepth"; }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  // Whether the original (uncompressed) samples are floating point or
  // unsigned integer.
  bool floating_point_sample;

  // Bit depth of the original (uncompressed) image samples. Must be in the
  // range [1, 32].
  uint32_t bits_per_sample;

  // Floating point exponent bits of the original (uncompressed) image samples,
  // only used if floating_point_sample is true.
  // If used, the samples are floating point with:
  // - 1 sign bit
  // - exponent_bits_per_sample exponent bits
  // - (bits_per_sample - exponent_bits_per_sample - 1) mantissa bits
  // If used, exponent_bits_per_sample must be in the range
  // [2, 8] and amount of mantissa bits must be in the range [2, 23].
  // NOTE: exponent_bits_per_sample is 8 for single precision binary32
  // point, 5 for half precision binary16, 7 for fp24.
  uint32_t exponent_bits_per_sample;
};

// Describes one extra channel.
struct ExtraChannelInfo : public Fields {
  ExtraChannelInfo();
  const char* Name() const override { return "ExtraChannelInfo"; }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  size_t Size(size_t size) const {
    const size_t mask = (1u << dim_shift) - 1;
    return (size + mask) >> dim_shift;
  }

  mutable bool all_default;

  ExtraChannel type;
  BitDepth bit_depth;
  uint32_t dim_shift;  // downsampled by 2^dim_shift on each axis

  std::string name;  // UTF-8

  // Conditional:
  bool alpha_associated;  // i.e. premultiplied
  float spot_color[4];    // spot color in linear RGBA
  uint32_t cfa_channel;
};

struct OpsinInverseMatrix : public Fields {
  OpsinInverseMatrix();
  const char* Name() const override { return "OpsinInverseMatrix"; }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  mutable bool all_default;

  float inverse_matrix[9];
  float opsin_biases[3];
  float quant_biases[4];
};

// Information useful for mapping HDR images to lower dynamic range displays.
struct ToneMapping : public Fields {
  ToneMapping();
  const char* Name() const override { return "ToneMapping"; }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  mutable bool all_default;

  // Upper bound on the intensity level present in the image. For unsigned
  // integer pixel encodings, this is the brightness of the largest
  // representable value. The image does not necessarily contain a pixel
  // actually this bright. An encoder is allowed to set 255 for SDR images
  // without computing a histogram.
  float intensity_target;  // [nits]

  // Lower bound on the intensity level present in the image. This may be
  // loose, i.e. lower than the actual darkest pixel. When tone mapping, a
  // decoder will map [min_nits, intensity_target] to the display range.
  float min_nits;

  bool relative_to_max_display;  // see below
  // The tone mapping will leave unchanged (linear mapping) any pixels whose
  // brightness is strictly below this. The interpretation depends on
  // relative_to_max_display. If true, this is a ratio [0, 1] of the maximum
  // display brightness [nits], otherwise an absolute brightness [nits].
  float linear_below;
};

// Contains weights to customize some trasnforms - in particular, XYB and
// upsampling.
struct CustomTransformData : public Fields {
  CustomTransformData();
  const char* Name() const override { return "CustomTransformData"; }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  // Must be set before calling VisitFields. Must equal xyb_encoded of
  // ImageMetadata, should be set by ImageMetadata during VisitFields.
  bool nonserialized_xyb_encoded = false;

  mutable bool all_default;

  OpsinInverseMatrix opsin_inverse_matrix;

  uint32_t custom_weights_mask;
  float upsampling2_weights[15];
  float upsampling4_weights[55];
  float upsampling8_weights[210];
};

// Properties of the original image bundle. This enables Encode(Decode()) to
// re-create an equivalent image without user input.
struct ImageMetadata : public Fields {
  ImageMetadata();
  const char* Name() const override { return "ImageMetadata"; }

  Status VisitFields(Visitor* JXL_RESTRICT visitor) override;

  // Returns bit depth of the JPEG XL compressed alpha channel, or 0 if no alpha
  // channel present. In the theoretical case that there are multiple alpha
  // channels, returns the bit depht of the first.
  uint32_t GetAlphaBits() const {
    const ExtraChannelInfo* alpha = Find(ExtraChannel::kAlpha);
    if (alpha == nullptr) return 0;
    JXL_ASSERT(alpha->bit_depth.bits_per_sample != 0);
    return alpha->bit_depth.bits_per_sample;
  }

  // Sets bit depth of alpha channel, adding extra channel if needed, or
  // removing all alpha channels if bits is 0.
  // Assumes integer alpha channel and not designed to support multiple
  // alpha channels (it's possible to use those features by manipulating
  // extra_channel_info directly).
  //
  // Callers must insert the actual channel image at the same index before any
  // further modifications to extra_channel_info.
  void SetAlphaBits(uint32_t bits, bool alpha_is_premultiplied = false);

  bool HasAlpha() const { return GetAlphaBits() != 0; }

  // Sets the original bit depth fields to indicate unsigned integer of the
  // given bit depth.
  // TODO(lode): move function to BitDepth
  void SetUintSamples(uint32_t bits) {
    bit_depth.bits_per_sample = bits;
    bit_depth.exponent_bits_per_sample = 0;
    bit_depth.floating_point_sample = false;
  }
  // Sets the original bit depth fields to indicate single precision floating
  // point.
  // TODO(lode): move function to BitDepth
  void SetFloat32Samples() {
    bit_depth.bits_per_sample = 32;
    bit_depth.exponent_bits_per_sample = 8;
    bit_depth.floating_point_sample = true;
  }

  void SetFloat16Samples() {
    bit_depth.bits_per_sample = 16;
    bit_depth.exponent_bits_per_sample = 5;
    bit_depth.floating_point_sample = true;
  }

  void SetIntensityTarget(float intensity_target) {
    tone_mapping.intensity_target = intensity_target;
  }
  float IntensityTarget() const {
    JXL_ASSERT(tone_mapping.intensity_target != 0);
    return tone_mapping.intensity_target;
  }

  // Returns first ExtraChannelInfo of the given type, or nullptr if none.
  const ExtraChannelInfo* Find(ExtraChannel type) const {
    for (const ExtraChannelInfo& eci : extra_channel_info) {
      if (eci.type == type) return &eci;
    }
    return nullptr;
  }

  // Returns first ExtraChannelInfo of the given type, or nullptr if none.
  ExtraChannelInfo* Find(ExtraChannel type) {
    for (ExtraChannelInfo& eci : extra_channel_info) {
      if (eci.type == type) return &eci;
    }
    return nullptr;
  }

  Orientation GetOrientation() const {
    return static_cast<Orientation>(orientation);
  }

  bool ExtraFieldsDefault() const;

  mutable bool all_default;

  BitDepth bit_depth;
  bool modular_16_bit_buffer_sufficient;  // otherwise 32 is.

  // Whether the colors values of the pixels of frames are encoded in the
  // codestream using the absolute XYB color space, or the using values that
  // follow the color space defined by the ColorEncoding or ICC profile. This
  // determines when or whether a CMS (Color Management System) is needed to get
  // the pixels in a desired color space. In one case, the pixels have one known
  // color space and a CMS is needed to convert them to the original image's
  // color space, in the other case the pixels have the color space of the
  // original image and a CMS is required if a different display space, or a
  // single known consistent color space for multiple decoded images, is
  // desired. In all cases, the color space of all frames from a single image is
  // the same, both VarDCT and modular frames.
  //
  // If true: then frames can be decoded to XYB (which can also be converted to
  // linear and non-linear sRGB with the built in conversion without CMS). The
  // attached ColorEncoding or ICC profile has no effect on the meaning of the
  // pixel's color values, but instead indicates what the color profile of the
  // original image was, and what color profile one should convert to when
  // decoding to integers to prevent clipping and precision loss. To do that
  // conversion requires a CMS.
  //
  // If false: then the color values of decoded frames are in the space defined
  // by the attached ColorEncoding or ICC profile. To instead get the pixels in
  // a chosen known color space, such as sRGB, requires a CMS, since the
  // attached ColorEncoding or ICC profile could be any arbitrary color space.
  // This mode is typically used for lossless images encoded as integers.
  // Frames can also use YCbCr encoding, some frames may and some may not, but
  // this is not a different color space but a certain encoding of the RGB
  // values.
  //
  // Note: if !xyb_encoded, but the attached color profile indicates XYB (which
  // can happen either if it's a ColorEncoding with color_space_ ==
  // ColorSpace::kXYB, or if it's an ICC Profile that has been crafted to
  // represent XYB), then the frames still may not use ColorEncoding kXYB, they
  // must still use kNone (or kYCbCr, which would mean applying the YCbCr
  // transform to the 3-channel XYB data), since with !xyb_encoded, the 3
  // channels are stored as-is, no matter what meaning the color profile assigns
  // to them. To use ColorEncoding::kXYB, xyb_encoded must be true.
  //
  // This value is defined in image metadata because this is the global
  // codestream header. This value does not affect the image itself, so is not
  // image metadata per se, it only affects the encoding, and what color space
  // the decoder can receive the pixels in without needing a CMS.
  bool xyb_encoded;

  ColorEncoding color_encoding;

  // These values are initialized to defaults such that the 'extra_fields'
  // condition in VisitFields uses correctly initialized values.
  uint32_t orientation = 1;
  bool have_preview = false;
  bool have_animation = false;
  bool have_intrinsic_size = false;

  // If present, the stored image has the dimensions of the first SizeHeader,
  // but decoders are advised to resample or display per `intrinsic_size`.
  SizeHeader intrinsic_size;  // only if have_intrinsic_size

  ToneMapping tone_mapping;

  // When reading: deserialized. When writing: automatically set from vector.
  uint32_t num_extra_channels;
  std::vector<ExtraChannelInfo> extra_channel_info;

  CustomTransformData transform_data;  // often default

  // Only present if m.have_preview.
  PreviewHeader preview_size;
  // Only present if m.have_animation.
  AnimationHeader animation;

  uint64_t extensions;

  // Option to stop parsing after basic info, and treat as if the later
  // fields do not participate. Use to parse only basic image information
  // excluding the final larger or variable sized data.
  bool nonserialized_only_parse_basic_info = false;
};

Status ReadImageMetadata(BitReader* JXL_RESTRICT reader,
                         ImageMetadata* JXL_RESTRICT metadata);

Status WriteImageMetadata(const ImageMetadata& metadata,
                          BitWriter* JXL_RESTRICT writer, size_t layer,
                          AuxOut* aux_out);

// All metadata applicable to the entire codestream (dimensions, extra channels,
// ...)
struct CodecMetadata {
  // TODO(lode): use the preview and animation fields too, in place of the
  // nonserialized_ ones in ImageMetadata.
  ImageMetadata m;
  // The size of the codestream: this is the nominal size applicable to all
  // frames, although some frames can have a different effective size through
  // crop, dc_level or representing a the preview.
  SizeHeader size;
  // Often default.
  CustomTransformData transform_data;

  size_t xsize() const { return size.xsize(); }
  size_t ysize() const { return size.ysize(); }
};

}  // namespace jxl

#endif  // LIB_JXL_IMAGE_METADATA_H_
