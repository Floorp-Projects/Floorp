// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/test_utils.h"

#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/memory_manager.h>
#include <jxl/types.h>

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lib/extras/metrics.h"
#include "lib/extras/packed_image_convert.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/float.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_aux_out.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/enc_fields.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/enc_icc_codec.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/icc_codec.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/padded_bytes.h"
#include "lib/jxl/test_memory_manager.h"

#if !defined(TEST_DATA_PATH)
#include "tools/cpp/runfiles/runfiles.h"
#endif

namespace jxl {
namespace test {

#if defined(TEST_DATA_PATH)
std::string GetTestDataPath(const std::string& filename) {
  return std::string(TEST_DATA_PATH "/") + filename;
}
#else
using bazel::tools::cpp::runfiles::Runfiles;
const std::unique_ptr<Runfiles> kRunfiles(Runfiles::Create(""));
std::string GetTestDataPath(const std::string& filename) {
  std::string root(JPEGXL_ROOT_PACKAGE "/testdata/");
  return kRunfiles->Rlocation(root + filename);
}
#endif

jxl::IccBytes GetIccTestProfile() {
  const uint8_t* profile = reinterpret_cast<const uint8_t*>(
      "\0\0\3\200lcms\0040\0\0mntrRGB XYZ "
      "\a\344\0\a\0\27\0\21\0$"
      "\0\37acspAPPL\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\366"
      "\326\0\1\0\0\0\0\323-lcms\372c\207\36\227\200{"
      "\2\232s\255\327\340\0\n\26\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
      "\0\0\0\0\0\0\0\0\rdesc\0\0\1 "
      "\0\0\0Bcprt\0\0\1d\0\0\1\0wtpt\0\0\2d\0\0\0\24chad\0\0\2x\0\0\0,"
      "bXYZ\0\0\2\244\0\0\0\24gXYZ\0\0\2\270\0\0\0\24rXYZ\0\0\2\314\0\0\0\24rTR"
      "C\0\0\2\340\0\0\0 gTRC\0\0\2\340\0\0\0 bTRC\0\0\2\340\0\0\0 "
      "chrm\0\0\3\0\0\0\0$dmnd\0\0\3$\0\0\0("
      "dmdd\0\0\3L\0\0\0002mluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0&"
      "\0\0\0\34\0R\0G\0B\0_\0D\0006\0005\0_\0S\0R\0G\0_\0R\0e\0l\0_"
      "\0L\0i\0n\0\0mluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0\344\0\0\0\34\0C\0o\0"
      "p\0y\0r\0i\0g\0h\0t\0 \0002\0000\0001\08\0 \0G\0o\0o\0g\0l\0e\0 "
      "\0L\0L\0C\0,\0 \0C\0C\0-\0B\0Y\0-\0S\0A\0 \0003\0.\0000\0 "
      "\0U\0n\0p\0o\0r\0t\0e\0d\0 "
      "\0l\0i\0c\0e\0n\0s\0e\0(\0h\0t\0t\0p\0s\0:\0/\0/"
      "\0c\0r\0e\0a\0t\0i\0v\0e\0c\0o\0m\0m\0o\0n\0s\0.\0o\0r\0g\0/"
      "\0l\0i\0c\0e\0n\0s\0e\0s\0/\0b\0y\0-\0s\0a\0/\0003\0.\0000\0/"
      "\0l\0e\0g\0a\0l\0c\0o\0d\0e\0)XYZ "
      "\0\0\0\0\0\0\366\326\0\1\0\0\0\0\323-"
      "sf32\0\0\0\0\0\1\fB\0\0\5\336\377\377\363%"
      "\0\0\a\223\0\0\375\220\377\377\373\241\377\377\375\242\0\0\3\334\0\0\300"
      "nXYZ \0\0\0\0\0\0o\240\0\08\365\0\0\3\220XYZ "
      "\0\0\0\0\0\0$\237\0\0\17\204\0\0\266\304XYZ "
      "\0\0\0\0\0\0b\227\0\0\267\207\0\0\30\331para\0\0\0\0\0\3\0\0\0\1\0\0\0\1"
      "\0\0\0\0\0\0\0\1\0\0\0\0\0\0chrm\0\0\0\0\0\3\0\0\0\0\243\327\0\0T|"
      "\0\0L\315\0\0\231\232\0\0&"
      "g\0\0\17\\mluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0\f\0\0\0\34\0G\0o\0o\0g"
      "\0l\0emluc\0\0\0\0\0\0\0\1\0\0\0\fenUS\0\0\0\26\0\0\0\34\0I\0m\0a\0g\0e"
      "\0 \0c\0o\0d\0e\0c\0\0");
  size_t profile_size = 896;
  jxl::IccBytes icc_profile;
  icc_profile.assign(profile, profile + profile_size);
  return icc_profile;
}

std::vector<uint8_t> GetCompressedIccTestProfile() {
  const uint8_t* raw_icc_data = reinterpret_cast<const uint8_t*>(
      "\x1f\x8b\x01\x33\x38\x18\x00\x30\x20\x8c"
      "\xe6\x81\x59\x00\x64\x69\x2c\x50\x80\xfc\xbc\x8e\xd6\xf7\x84\x66"
      "\x0c\x46\x68\x8e\xc9\x1e\x35\xb7\xe6\x79\x0a\x38\x0f\x2d\x0b\x15"
      "\x94\x56\x90\x28\x39\x09\x48\x27\x1f\xc3\x2a\x85\xb3\x82\x01\x46"
      "\x86\x28\x19\xe4\x64\x24\x3d\x69\x74\xa4\x9e\x24\x3e\x4a\x6d\x31"
      "\xa4\x54\x2a\x35\xc5\xf0\x9e\x34\xa0\x27\x8d\x8a\x04\xb0\xec\x8e"
      "\xdb\xee\xcc\x40\x5e\x71\x96\xcc\x99\x3e\x3a\x18\x42\x3f\xc0\x06"
      "\x5c\x04\xaf\x79\xdf\xa3\x7e\x47\x0f\x0f\xbd\x08\xd8\x3d\xa9\xd9"
      "\xf9\xdd\x3e\x57\x30\xa5\x36\x7e\xcc\x96\x57\xfa\x11\x41\x71\xdd"
      "\x1b\x8d\xa1\x79\xa5\x5c\xe4\x3e\xb4\xde\xde\xdf\x9c\xe4\xee\x4f"
      "\x28\xf8\x3e\x4c\xe2\xfa\x36\xfb\x3f\x13\x97\x1a\xc9\x34\xef\xc0"
      "\x17\x9a\x15\x92\x03\x4b\x83\xd5\x62\xf3\xc4\x20\xc7\xf3\x1c\x4c"
      "\x0d\xc2\xe1\x8c\x39\xc8\x64\xdc\xc8\xa5\x7b\x93\x18\xec\xec\xc5"
      "\xe0\x0a\x2f\xf0\x95\x12\x05\x0d\x60\x92\xa1\xf0\x0e\x65\x80\xa5"
      "\x52\xa1\xf3\x94\x3f\x6f\xc7\x0a\x45\x94\xc8\x1a\xc5\xf0\x34\xcd"
      "\xe3\x1d\x9b\xaf\x70\xfe\x8f\x19\x1d\x1f\x69\xba\x1d\xc2\xdf\xd9"
      "\x0b\x1f\xa6\x38\x02\x66\x78\x88\x72\x84\x76\xad\x04\x80\xd3\x69"
      "\x44\x71\x05\x71\xd2\xeb\xdf\xbf\xf3\x29\x70\x76\x02\xf6\x85\xf8"
      "\xf7\xef\xde\x90\x7f\xff\xf6\x15\x41\x96\x0b\x02\xd7\x15\xfb\xbe"
      "\x81\x18\x6c\x1d\xb2\x10\x18\xe2\x07\xea\x12\x40\x9b\x44\x58\xf1"
      "\x75\x85\x37\x0f\xd0\x68\x96\x7c\x39\x85\xf8\xea\xf7\x62\x47\xb0"
      "\x42\xeb\x43\x06\x70\xe4\x15\x96\x2a\x8b\x65\x3e\x4d\x98\x51\x03"
      "\x63\xf6\x14\xf5\xe5\xe0\x7a\x0e\xdf\x3e\x1b\x45\x9a\xef\x87\xe1"
      "\x3f\xcf\x69\x5c\x43\xda\x68\xde\x84\x26\x38\x6a\xf0\x35\xcb\x08");
  std::vector<uint8_t> icc_data;
  icc_data.assign(raw_icc_data, raw_icc_data + 378);
  return icc_data;
}

std::vector<uint8_t> ReadTestData(const std::string& filename) {
  std::string full_path = GetTestDataPath(filename);
  fprintf(stderr, "ReadTestData %s\n", full_path.c_str());
  std::ifstream file(full_path, std::ios::binary);
  std::vector<char> str((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
  JXL_CHECK(file.good());
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(str.data());
  std::vector<uint8_t> data(raw, raw + str.size());
  printf("Test data %s is %d bytes long.\n", filename.c_str(),
         static_cast<int>(data.size()));
  return data;
}

void DefaultAcceptedFormats(extras::JXLDecompressParams& dparams) {
  if (dparams.accepted_formats.empty()) {
    for (const uint32_t num_channels : {1, 2, 3, 4}) {
      dparams.accepted_formats.push_back(
          {num_channels, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, /*align=*/0});
    }
  }
}

Status DecodeFile(extras::JXLDecompressParams dparams,
                  const Span<const uint8_t> file, CodecInOut* JXL_RESTRICT io,
                  ThreadPool* pool) {
  DefaultAcceptedFormats(dparams);
  SetThreadParallelRunner(dparams, pool);
  extras::PackedPixelFile ppf;
  JXL_RETURN_IF_ERROR(DecodeImageJXL(file.data(), file.size(), dparams,
                                     /*decoded_bytes=*/nullptr, &ppf));
  JXL_RETURN_IF_ERROR(ConvertPackedPixelFileToCodecInOut(ppf, pool, io));
  return true;
}

void JxlBasicInfoSetFromPixelFormat(JxlBasicInfo* basic_info,
                                    const JxlPixelFormat* pixel_format) {
  JxlEncoderInitBasicInfo(basic_info);
  switch (pixel_format->data_type) {
    case JXL_TYPE_FLOAT:
      basic_info->bits_per_sample = 32;
      basic_info->exponent_bits_per_sample = 8;
      break;
    case JXL_TYPE_FLOAT16:
      basic_info->bits_per_sample = 16;
      basic_info->exponent_bits_per_sample = 5;
      break;
    case JXL_TYPE_UINT8:
      basic_info->bits_per_sample = 8;
      basic_info->exponent_bits_per_sample = 0;
      break;
    case JXL_TYPE_UINT16:
      basic_info->bits_per_sample = 16;
      basic_info->exponent_bits_per_sample = 0;
      break;
    default:
      JXL_ABORT("Unhandled JxlDataType");
  }
  if (pixel_format->num_channels < 3) {
    basic_info->num_color_channels = 1;
  } else {
    basic_info->num_color_channels = 3;
  }
  if (pixel_format->num_channels == 2 || pixel_format->num_channels == 4) {
    basic_info->alpha_exponent_bits = basic_info->exponent_bits_per_sample;
    basic_info->alpha_bits = basic_info->bits_per_sample;
    basic_info->num_extra_channels = 1;
  } else {
    basic_info->alpha_exponent_bits = 0;
    basic_info->alpha_bits = 0;
  }
}

ColorEncoding ColorEncodingFromDescriptor(const ColorEncodingDescriptor& desc) {
  ColorEncoding c;
  c.SetColorSpace(desc.color_space);
  if (desc.color_space != ColorSpace::kXYB) {
    JXL_CHECK(c.SetWhitePointType(desc.white_point));
    if (desc.color_space != ColorSpace::kGray) {
      JXL_CHECK(c.SetPrimariesType(desc.primaries));
    }
    c.Tf().SetTransferFunction(desc.tf);
  }
  c.SetRenderingIntent(desc.rendering_intent);
  JXL_CHECK(c.CreateICC());
  return c;
}

namespace {
void CheckSameEncodings(const std::vector<ColorEncoding>& a,
                        const std::vector<ColorEncoding>& b,
                        const std::string& check_name,
                        std::stringstream& failures) {
  JXL_CHECK(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    if ((a[i].ICC() == b[i].ICC()) ||
        ((a[i].GetPrimariesType() == b[i].GetPrimariesType()) &&
         a[i].Tf().IsSame(b[i].Tf()))) {
      continue;
    }
    failures << "CheckSameEncodings " << check_name << ": " << i
             << "-th encoding mismatch\n";
  }
}
}  // namespace

bool Roundtrip(const CodecInOut* io, const CompressParams& cparams,
               extras::JXLDecompressParams dparams,
               CodecInOut* JXL_RESTRICT io2, std::stringstream& failures,
               size_t* compressed_size, ThreadPool* pool) {
  DefaultAcceptedFormats(dparams);
  if (compressed_size) {
    *compressed_size = static_cast<size_t>(-1);
  }
  std::vector<uint8_t> compressed;

  std::vector<ColorEncoding> original_metadata_encodings;
  std::vector<ColorEncoding> original_current_encodings;
  std::vector<ColorEncoding> metadata_encodings_1;
  std::vector<ColorEncoding> metadata_encodings_2;
  std::vector<ColorEncoding> current_encodings_2;
  original_metadata_encodings.reserve(io->frames.size());
  original_current_encodings.reserve(io->frames.size());
  metadata_encodings_1.reserve(io->frames.size());
  metadata_encodings_2.reserve(io->frames.size());
  current_encodings_2.reserve(io->frames.size());

  for (const ImageBundle& ib : io->frames) {
    // Remember original encoding, will be returned by decoder.
    original_metadata_encodings.push_back(ib.metadata()->color_encoding);
    // c_current should not change during encoding.
    original_current_encodings.push_back(ib.c_current());
  }

  JXL_CHECK(test::EncodeFile(cparams, io, &compressed, pool));

  for (const ImageBundle& ib1 : io->frames) {
    metadata_encodings_1.push_back(ib1.metadata()->color_encoding);
  }

  // Should still be in the same color space after encoding.
  CheckSameEncodings(metadata_encodings_1, original_metadata_encodings,
                     "original vs after encoding", failures);

  JXL_CHECK(DecodeFile(dparams, Bytes(compressed), io2, pool));
  JXL_CHECK(io2->frames.size() == io->frames.size());

  for (const ImageBundle& ib2 : io2->frames) {
    metadata_encodings_2.push_back(ib2.metadata()->color_encoding);
    current_encodings_2.push_back(ib2.c_current());
  }

  // We always produce the original color encoding if a color transform hook is
  // set.
  CheckSameEncodings(current_encodings_2, original_current_encodings,
                     "current: original vs decoded", failures);

  // Decoder returns the originals passed to the encoder.
  CheckSameEncodings(metadata_encodings_2, original_metadata_encodings,
                     "metadata: original vs decoded", failures);

  if (compressed_size) {
    *compressed_size = compressed.size();
  }

  return failures.str().empty();
}

size_t Roundtrip(const extras::PackedPixelFile& ppf_in,
                 const extras::JXLCompressParams& cparams,
                 extras::JXLDecompressParams dparams, ThreadPool* pool,
                 extras::PackedPixelFile* ppf_out) {
  DefaultAcceptedFormats(dparams);
  SetThreadParallelRunner(cparams, pool);
  SetThreadParallelRunner(dparams, pool);
  std::vector<uint8_t> compressed;
  JXL_CHECK(extras::EncodeImageJXL(cparams, ppf_in, /*jpeg_bytes=*/nullptr,
                                   &compressed));
  size_t decoded_bytes = 0;
  JXL_CHECK(extras::DecodeImageJXL(compressed.data(), compressed.size(),
                                   dparams, &decoded_bytes, ppf_out));
  JXL_CHECK(decoded_bytes == compressed.size());
  return compressed.size();
}

std::vector<ColorEncodingDescriptor> AllEncodings() {
  std::vector<ColorEncodingDescriptor> all_encodings;
  all_encodings.reserve(300);

  for (ColorSpace cs : Values<ColorSpace>()) {
    if (cs == ColorSpace::kUnknown || cs == ColorSpace::kXYB ||
        cs == ColorSpace::kGray) {
      continue;
    }

    for (WhitePoint wp : Values<WhitePoint>()) {
      if (wp == WhitePoint::kCustom) continue;
      for (Primaries primaries : Values<Primaries>()) {
        if (primaries == Primaries::kCustom) continue;
        for (TransferFunction tf : Values<TransferFunction>()) {
          if (tf == TransferFunction::kUnknown) continue;
          for (RenderingIntent ri : Values<RenderingIntent>()) {
            ColorEncodingDescriptor cdesc;
            cdesc.color_space = cs;
            cdesc.white_point = wp;
            cdesc.primaries = primaries;
            cdesc.tf = tf;
            cdesc.rendering_intent = ri;
            all_encodings.push_back(cdesc);
          }
        }
      }
    }
  }

  return all_encodings;
}

jxl::CodecInOut SomeTestImageToCodecInOut(const std::vector<uint8_t>& buf,
                                          size_t num_channels, size_t xsize,
                                          size_t ysize) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  jxl::CodecInOut io{memory_manager};
  io.SetSize(xsize, ysize);
  io.metadata.m.SetAlphaBits(16);
  io.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(
      /*is_gray=*/num_channels == 1 || num_channels == 2);
  JxlPixelFormat format = {static_cast<uint32_t>(num_channels), JXL_TYPE_UINT16,
                           JXL_BIG_ENDIAN, 0};
  JXL_CHECK(ConvertFromExternal(
      jxl::Bytes(buf.data(), buf.size()), xsize, ysize,
      jxl::ColorEncoding::SRGB(/*is_gray=*/num_channels < 3),
      /*bits_per_sample=*/16, format,
      /*pool=*/nullptr,
      /*ib=*/&io.Main()));
  return io;
}

bool Near(double expected, double value, double max_dist) {
  double dist = expected > value ? expected - value : value - expected;
  return dist <= max_dist;
}

float LoadLEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadLE16(p);
  return detail::LoadFloat16(bits16);
}

float LoadBEFloat16(const uint8_t* p) {
  uint16_t bits16 = LoadBE16(p);
  return detail::LoadFloat16(bits16);
}

size_t GetPrecision(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_FLOAT:
      // Floating point mantissa precision
      return 24;
    case JXL_TYPE_FLOAT16:
      return 11;
    default:
      JXL_ABORT("Unhandled JxlDataType");
  }
}

size_t GetDataBits(JxlDataType data_type) {
  switch (data_type) {
    case JXL_TYPE_UINT8:
      return 8;
    case JXL_TYPE_UINT16:
      return 16;
    case JXL_TYPE_FLOAT:
      return 32;
    case JXL_TYPE_FLOAT16:
      return 16;
    default:
      JXL_ABORT("Unhandled JxlDataType");
  }
}

std::vector<double> ConvertToRGBA32(const uint8_t* pixels, size_t xsize,
                                    size_t ysize, const JxlPixelFormat& format,
                                    double factor) {
  std::vector<double> result(xsize * ysize * 4);
  size_t num_channels = format.num_channels;
  bool gray = num_channels == 1 || num_channels == 2;
  bool alpha = num_channels == 2 || num_channels == 4;
  JxlEndianness endianness = format.endianness;
  // Compute actual type:
  if (endianness == JXL_NATIVE_ENDIAN) {
    endianness = IsLittleEndian() ? JXL_LITTLE_ENDIAN : JXL_BIG_ENDIAN;
  }

  size_t stride =
      xsize * jxl::DivCeil(GetDataBits(format.data_type) * num_channels,
                           jxl::kBitsPerByte);
  if (format.align > 1) stride = jxl::RoundUpTo(stride, format.align);

  if (format.data_type == JXL_TYPE_UINT8) {
    // Multiplier to bring to 0-1.0 range
    double mul = factor > 0.0 ? factor : 1.0 / 255.0;
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels;
        double r = pixels[i];
        double g = gray ? r : pixels[i + 1];
        double b = gray ? r : pixels[i + 2];
        double a = alpha ? pixels[i + num_channels - 1] : 255;
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_UINT16) {
    JXL_ASSERT(endianness != JXL_NATIVE_ENDIAN);
    // Multiplier to bring to 0-1.0 range
    double mul = factor > 0.0 ? factor : 1.0 / 65535.0;
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = (pixels[i + 0] << 8) + pixels[i + 1];
          g = gray ? r : (pixels[i + 2] << 8) + pixels[i + 3];
          b = gray ? r : (pixels[i + 4] << 8) + pixels[i + 5];
          a = alpha ? (pixels[i + num_channels * 2 - 2] << 8) +
                          pixels[i + num_channels * 2 - 1]
                    : 65535;
        } else {
          r = (pixels[i + 1] << 8) + pixels[i + 0];
          g = gray ? r : (pixels[i + 3] << 8) + pixels[i + 2];
          b = gray ? r : (pixels[i + 5] << 8) + pixels[i + 4];
          a = alpha ? (pixels[i + num_channels * 2 - 1] << 8) +
                          pixels[i + num_channels * 2 - 2]
                    : 65535;
        }
        result[j + 0] = r * mul;
        result[j + 1] = g * mul;
        result[j + 2] = b * mul;
        result[j + 3] = a * mul;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT) {
    JXL_ASSERT(endianness != JXL_NATIVE_ENDIAN);
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 4;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat(pixels + i);
          g = gray ? r : LoadBEFloat(pixels + i + 4);
          b = gray ? r : LoadBEFloat(pixels + i + 8);
          a = alpha ? LoadBEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        } else {
          r = LoadLEFloat(pixels + i);
          g = gray ? r : LoadLEFloat(pixels + i + 4);
          b = gray ? r : LoadLEFloat(pixels + i + 8);
          a = alpha ? LoadLEFloat(pixels + i + num_channels * 4 - 4) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else if (format.data_type == JXL_TYPE_FLOAT16) {
    JXL_ASSERT(endianness != JXL_NATIVE_ENDIAN);
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        size_t j = (y * xsize + x) * 4;
        size_t i = y * stride + x * num_channels * 2;
        double r;
        double g;
        double b;
        double a;
        if (endianness == JXL_BIG_ENDIAN) {
          r = LoadBEFloat16(pixels + i);
          g = gray ? r : LoadBEFloat16(pixels + i + 2);
          b = gray ? r : LoadBEFloat16(pixels + i + 4);
          a = alpha ? LoadBEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        } else {
          r = LoadLEFloat16(pixels + i);
          g = gray ? r : LoadLEFloat16(pixels + i + 2);
          b = gray ? r : LoadLEFloat16(pixels + i + 4);
          a = alpha ? LoadLEFloat16(pixels + i + num_channels * 2 - 2) : 1.0;
        }
        result[j + 0] = r;
        result[j + 1] = g;
        result[j + 2] = b;
        result[j + 3] = a;
      }
    }
  } else {
    JXL_ASSERT(false);  // Unsupported type
  }
  return result;
}

size_t ComparePixels(const uint8_t* a, const uint8_t* b, size_t xsize,
                     size_t ysize, const JxlPixelFormat& format_a,
                     const JxlPixelFormat& format_b,
                     double threshold_multiplier) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format_a);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format_b);
  bool gray_a = format_a.num_channels < 3;
  bool gray_b = format_b.num_channels < 3;
  bool alpha_a = ((format_a.num_channels & 1) == 0);
  bool alpha_b = ((format_b.num_channels & 1) == 0);
  size_t bits_a = GetPrecision(format_a.data_type);
  size_t bits_b = GetPrecision(format_b.data_type);
  size_t bits = std::min(bits_a, bits_b);
  // How much distance is allowed in case of pixels with lower bit depths, given
  // that the double precision float images use range 0-1.0.
  // E.g. in case of 1-bit this is 0.5 since 0.499 must map to 0 and 0.501 must
  // map to 1.
  double precision = 0.5 * threshold_multiplier / ((1ull << bits) - 1ull);
  if (format_a.data_type == JXL_TYPE_FLOAT16 ||
      format_b.data_type == JXL_TYPE_FLOAT16) {
    // Lower the precision for float16, because it currently looks like the
    // scalar and wasm implementations of hwy have 1 less bit of precision
    // than the x86 implementations.
    // TODO(lode): Set the required precision back to 11 bits when possible.
    precision = 0.5 * threshold_multiplier / ((1ull << (bits - 1)) - 1ull);
  }
  if (format_b.data_type == JXL_TYPE_UINT8) {
    // Increase the threshold by the maximum difference introduced by dithering.
    precision += 63.0 / 128.0;
  }
  size_t numdiff = 0;
  for (size_t y = 0; y < ysize; y++) {
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      bool ok = true;
      if (gray_a || gray_b) {
        if (!Near(a_full[i + 0], b_full[i + 0], precision)) ok = false;
        // If the input was grayscale and the output not, then the output must
        // have all channels equal.
        if (gray_a && b_full[i + 0] != b_full[i + 1] &&
            b_full[i + 2] != b_full[i + 2]) {
          ok = false;
        }
      } else {
        if (!Near(a_full[i + 0], b_full[i + 0], precision) ||
            !Near(a_full[i + 1], b_full[i + 1], precision) ||
            !Near(a_full[i + 2], b_full[i + 2], precision)) {
          ok = false;
        }
      }
      if (alpha_a && alpha_b) {
        if (!Near(a_full[i + 3], b_full[i + 3], precision)) ok = false;
      } else {
        // If the input had no alpha channel, the output should be opaque
        // after roundtrip.
        if (alpha_b && !Near(1.0, b_full[i + 3], precision)) ok = false;
      }
      if (!ok) numdiff++;
    }
  }
  return numdiff;
}

double DistanceRMS(const uint8_t* a, const uint8_t* b, size_t xsize,
                   size_t ysize, const JxlPixelFormat& format) {
  // Convert both images to equal full precision for comparison.
  std::vector<double> a_full = ConvertToRGBA32(a, xsize, ysize, format);
  std::vector<double> b_full = ConvertToRGBA32(b, xsize, ysize, format);
  double sum = 0.0;
  for (size_t y = 0; y < ysize; y++) {
    double row_sum = 0.0;
    for (size_t x = 0; x < xsize; x++) {
      size_t i = (y * xsize + x) * 4;
      for (size_t c = 0; c < format.num_channels; ++c) {
        double diff = a_full[i + c] - b_full[i + c];
        row_sum += diff * diff;
      }
    }
    sum += row_sum;
  }
  sum /= (xsize * ysize);
  return sqrt(sum);
}

float ButteraugliDistance(const extras::PackedPixelFile& a,
                          const extras::PackedPixelFile& b, ThreadPool* pool) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  // TODO(eustas): simplify?
  return ButteraugliDistance(io0.frames, io1.frames, ButteraugliParams(),
                             *JxlGetDefaultCms(),
                             /*distmap=*/nullptr, pool);
}

float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms, ImageF* distmap,
                          ThreadPool* pool, bool ignore_alpha) {
  JxlButteraugliComparator comparator(params, cms);
  float distance;
  JXL_CHECK(ComputeScore(rgb0, rgb1, &comparator, cms, &distance, distmap, pool,
                         ignore_alpha));
  return distance;
}

float ButteraugliDistance(const std::vector<ImageBundle>& frames0,
                          const std::vector<ImageBundle>& frames1,
                          const ButteraugliParams& params,
                          const JxlCmsInterface& cms, ImageF* distmap,
                          ThreadPool* pool) {
  JxlButteraugliComparator comparator(params, cms);
  JXL_ASSERT(frames0.size() == frames1.size());
  float max_dist = 0.0f;
  for (size_t i = 0; i < frames0.size(); ++i) {
    float frame_score;
    JXL_CHECK(ComputeScore(frames0[i], frames1[i], &comparator, cms,
                           &frame_score, distmap, pool));
    max_dist = std::max(max_dist, frame_score);
  }
  return max_dist;
}

float Butteraugli3Norm(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b, ThreadPool* pool) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, pool, &io0));
  CodecInOut io1{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, pool, &io1));
  ButteraugliParams butteraugli_params;
  ImageF distmap;
  ButteraugliDistance(io0.frames, io1.frames, butteraugli_params,
                      *JxlGetDefaultCms(), &distmap, pool);
  return ComputeDistanceP(distmap, butteraugli_params, 3);
}

float ComputeDistance2(const extras::PackedPixelFile& a,
                       const extras::PackedPixelFile& b) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, nullptr, &io0));
  CodecInOut io1{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, nullptr, &io1));
  return ComputeDistance2(io0.Main(), io1.Main(), *JxlGetDefaultCms());
}

float ComputePSNR(const extras::PackedPixelFile& a,
                  const extras::PackedPixelFile& b) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  CodecInOut io0{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(a, nullptr, &io0));
  CodecInOut io1{memory_manager};
  JXL_CHECK(ConvertPackedPixelFileToCodecInOut(b, nullptr, &io1));
  return ComputePSNR(io0.Main(), io1.Main(), *JxlGetDefaultCms());
}

bool SameAlpha(const extras::PackedPixelFile& a,
               const extras::PackedPixelFile& b) {
  JXL_CHECK(a.info.xsize == b.info.xsize);
  JXL_CHECK(a.info.ysize == b.info.ysize);
  JXL_CHECK(a.info.alpha_bits == b.info.alpha_bits);
  JXL_CHECK(a.info.alpha_exponent_bits == b.info.alpha_exponent_bits);
  JXL_CHECK(a.info.alpha_bits > 0);
  JXL_CHECK(a.frames.size() == b.frames.size());
  for (size_t i = 0; i < a.frames.size(); ++i) {
    const extras::PackedImage& color_a = a.frames[i].color;
    const extras::PackedImage& color_b = b.frames[i].color;
    JXL_CHECK(color_a.format.num_channels == color_b.format.num_channels);
    JXL_CHECK(color_a.format.data_type == color_b.format.data_type);
    JXL_CHECK(color_a.format.endianness == color_b.format.endianness);
    JXL_CHECK(color_a.pixels_size == color_b.pixels_size);
    size_t pwidth =
        extras::PackedImage::BitsPerChannel(color_a.format.data_type) / 8;
    size_t num_color = color_a.format.num_channels < 3 ? 1 : 3;
    const uint8_t* p_a = reinterpret_cast<const uint8_t*>(color_a.pixels());
    const uint8_t* p_b = reinterpret_cast<const uint8_t*>(color_b.pixels());
    for (size_t y = 0; y < a.info.ysize; ++y) {
      for (size_t x = 0; x < a.info.xsize; ++x) {
        size_t idx =
            ((y * a.info.xsize + x) * color_a.format.num_channels + num_color) *
            pwidth;
        if (memcmp(&p_a[idx], &p_b[idx], pwidth) != 0) {
          return false;
        }
      }
    }
  }
  return true;
}

bool SamePixels(const extras::PackedImage& a, const extras::PackedImage& b) {
  JXL_CHECK(a.xsize == b.xsize);
  JXL_CHECK(a.ysize == b.ysize);
  JXL_CHECK(a.format.num_channels == b.format.num_channels);
  JXL_CHECK(a.format.data_type == b.format.data_type);
  JXL_CHECK(a.format.endianness == b.format.endianness);
  JXL_CHECK(a.pixels_size == b.pixels_size);
  const uint8_t* p_a = reinterpret_cast<const uint8_t*>(a.pixels());
  const uint8_t* p_b = reinterpret_cast<const uint8_t*>(b.pixels());
  for (size_t y = 0; y < a.ysize; ++y) {
    for (size_t x = 0; x < a.xsize; ++x) {
      size_t idx = (y * a.xsize + x) * a.pixel_stride();
      if (memcmp(&p_a[idx], &p_b[idx], a.pixel_stride()) != 0) {
        printf("Mismatch at row %" PRIuS " col %" PRIuS "\n", y, x);
        printf("  a: ");
        for (size_t j = 0; j < a.pixel_stride(); ++j) {
          printf(" %3u", p_a[idx + j]);
        }
        printf("\n  b: ");
        for (size_t j = 0; j < a.pixel_stride(); ++j) {
          printf(" %3u", p_b[idx + j]);
        }
        printf("\n");
        return false;
      }
    }
  }
  return true;
}

bool SamePixels(const extras::PackedPixelFile& a,
                const extras::PackedPixelFile& b) {
  JXL_CHECK(a.info.xsize == b.info.xsize);
  JXL_CHECK(a.info.ysize == b.info.ysize);
  JXL_CHECK(a.info.bits_per_sample == b.info.bits_per_sample);
  JXL_CHECK(a.info.exponent_bits_per_sample == b.info.exponent_bits_per_sample);
  JXL_CHECK(a.frames.size() == b.frames.size());
  for (size_t i = 0; i < a.frames.size(); ++i) {
    const auto& frame_a = a.frames[i];
    const auto& frame_b = b.frames[i];
    if (!SamePixels(frame_a.color, frame_b.color)) {
      return false;
    }
    JXL_CHECK(frame_a.extra_channels.size() == frame_b.extra_channels.size());
    for (size_t j = 0; j < frame_a.extra_channels.size(); ++j) {
      if (!SamePixels(frame_a.extra_channels[i], frame_b.extra_channels[i])) {
        return false;
      }
    }
  }
  return true;
}

Status ReadICC(BitReader* JXL_RESTRICT reader,
               std::vector<uint8_t>* JXL_RESTRICT icc, size_t output_limit) {
  JxlMemoryManager* memort_manager = jxl::test::MemoryManager();
  icc->clear();
  ICCReader icc_reader{memort_manager};
  PaddedBytes icc_buffer{memort_manager};
  JXL_RETURN_IF_ERROR(icc_reader.Init(reader, output_limit));
  JXL_RETURN_IF_ERROR(icc_reader.Process(reader, &icc_buffer));
  Bytes(icc_buffer).AppendTo(*icc);
  return true;
}

namespace {  // For EncodeFile
Status PrepareCodecMetadataFromIO(const CompressParams& cparams,
                                  const CodecInOut* io,
                                  CodecMetadata* metadata) {
  *metadata = io->metadata;
  size_t ups = 1;
  if (cparams.already_downsampled) ups = cparams.resampling;

  JXL_RETURN_IF_ERROR(metadata->size.Set(io->xsize() * ups, io->ysize() * ups));

  // Keep ICC profile in lossless modes because a reconstructed profile may be
  // slightly different (quantization).
  // Also keep ICC in JPEG reconstruction mode as we need byte-exact profiles.
  if (!cparams.IsLossless() && !io->Main().IsJPEG() && cparams.cms_set) {
    metadata->m.color_encoding.DecideIfWantICC(cparams.cms);
  }

  metadata->m.xyb_encoded =
      cparams.color_transform == ColorTransform::kXYB ? true : false;

  // TODO(firsching): move this EncodeFile to test_utils / re-implement this
  // using API functions
  return true;
}

Status EncodePreview(const CompressParams& cparams, const ImageBundle& ib,
                     const CodecMetadata* metadata, const JxlCmsInterface& cms,
                     ThreadPool* pool, BitWriter* JXL_RESTRICT writer) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  BitWriter preview_writer{memory_manager};
  // TODO(janwas): also support generating preview by downsampling
  if (ib.HasColor()) {
    AuxOut aux_out;
    // TODO(lode): check if we want all extra channels and matching xyb_encoded
    // for the preview, such that using the main ImageMetadata object for
    // encoding this frame is warrented.
    FrameInfo frame_info;
    frame_info.is_preview = true;
    JXL_RETURN_IF_ERROR(EncodeFrame(memory_manager, cparams, frame_info,
                                    metadata, ib, cms, pool, &preview_writer,
                                    &aux_out));
    preview_writer.ZeroPadToByte();
  }

  if (preview_writer.BitsWritten() != 0) {
    writer->ZeroPadToByte();
    writer->AppendByteAligned(preview_writer);
  }

  return true;
}

}  // namespace

Status EncodeFile(const CompressParams& params, const CodecInOut* io,
                  std::vector<uint8_t>* compressed, ThreadPool* pool) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  compressed->clear();
  const JxlCmsInterface& cms = *JxlGetDefaultCms();
  io->CheckMetadata();
  BitWriter writer{memory_manager};

  CompressParams cparams = params;
  if (io->Main().color_transform != ColorTransform::kNone) {
    // Set the color transform to YCbCr or XYB if the original image is such.
    cparams.color_transform = io->Main().color_transform;
  }

  JXL_RETURN_IF_ERROR(ParamsPostInit(&cparams));

  std::unique_ptr<CodecMetadata> metadata = jxl::make_unique<CodecMetadata>();
  JXL_RETURN_IF_ERROR(PrepareCodecMetadataFromIO(cparams, io, metadata.get()));
  JXL_RETURN_IF_ERROR(
      WriteCodestreamHeaders(metadata.get(), &writer, /*aux_out*/ nullptr));

  // Only send ICC (at least several hundred bytes) if fields aren't enough.
  if (metadata->m.color_encoding.WantICC()) {
    JXL_RETURN_IF_ERROR(WriteICC(metadata->m.color_encoding.ICC(), &writer,
                                 kLayerHeader, /* aux_out */ nullptr));
  }

  if (metadata->m.have_preview) {
    JXL_RETURN_IF_ERROR(EncodePreview(cparams, io->preview_frame,
                                      metadata.get(), cms, pool, &writer));
  }

  // Each frame should start on byte boundaries.
  BitWriter::Allotment allotment(&writer, 8);
  writer.ZeroPadToByte();
  allotment.ReclaimAndCharge(&writer, kLayerHeader, /* aux_out */ nullptr);

  for (size_t i = 0; i < io->frames.size(); i++) {
    FrameInfo info;
    info.is_last = i == io->frames.size() - 1;
    if (io->frames[i].use_for_next_frame) {
      info.save_as_reference = 1;
    }
    JXL_RETURN_IF_ERROR(EncodeFrame(memory_manager, cparams, info,
                                    metadata.get(), io->frames[i], cms, pool,
                                    &writer,
                                    /* aux_out */ nullptr));
  }

  PaddedBytes output = std::move(writer).TakeBytes();
  Bytes(output).AppendTo(*compressed);
  return true;
}

}  // namespace test

bool operator==(const jxl::Bytes& a, const jxl::Bytes& b) {
  if (a.size() != b.size()) return false;
  if (memcmp(a.data(), b.data(), a.size()) != 0) return false;
  return true;
}

// Allow using EXPECT_EQ on jxl::Bytes
bool operator!=(const jxl::Bytes& a, const jxl::Bytes& b) { return !(a == b); }

}  // namespace jxl
