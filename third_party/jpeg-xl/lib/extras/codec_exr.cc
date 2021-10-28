// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/codec_exr.h"

#include <ImfChromaticitiesAttribute.h>
#include <ImfIO.h>
#include <ImfRgbaFile.h>
#include <ImfStandardAttributes.h>

#include <vector>

#include "lib/jxl/alpha.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/enc_image_bundle.h"

namespace jxl {
namespace extras {

namespace {

namespace OpenEXR = OPENEXR_IMF_NAMESPACE;
namespace Imath = IMATH_NAMESPACE;

// OpenEXR::Int64 is deprecated in favor of using uint64_t directly, but using
// uint64_t as recommended causes build failures with previous OpenEXR versions
// on macOS, where the definition for OpenEXR::Int64 was actually not equivalent
// to uint64_t. This alternative should work in all cases.
using ExrInt64 = decltype(std::declval<OpenEXR::IStream>().tellg());

constexpr int kExrBitsPerSample = 16;
constexpr int kExrAlphaBits = 16;

float GetIntensityTarget(float target_nits, const OpenEXR::Header& exr_header) {
  if (OpenEXR::hasWhiteLuminance(exr_header)) {
    const float exr_luminance = OpenEXR::whiteLuminance(exr_header);
    if (target_nits != 0) {
      JXL_WARNING(
          "overriding OpenEXR whiteLuminance of %g with user-specified value "
          "of %g",
          exr_luminance, target_nits);
      return target_nits;
    }
    return exr_luminance;
  }
  if (target_nits != 0) {
    return target_nits;
  }
  JXL_WARNING(
      "no OpenEXR whiteLuminance tag found and no intensity_target specified, "
      "defaulting to %g",
      kDefaultIntensityTarget);
  return kDefaultIntensityTarget;
}

size_t GetNumThreads(ThreadPool* pool) {
  size_t exr_num_threads = 1;
  RunOnPool(
      pool, 0, 1,
      [&](size_t num_threads) {
        exr_num_threads = num_threads;
        return true;
      },
      [&](const int /* task */, const int /*thread*/) {},
      "DecodeImageEXRThreads");
  return exr_num_threads;
}

class InMemoryIStream : public OpenEXR::IStream {
 public:
  // The data pointed to by `bytes` must outlive the InMemoryIStream.
  explicit InMemoryIStream(const Span<const uint8_t> bytes)
      : IStream(/*fileName=*/""), bytes_(bytes) {}

  bool isMemoryMapped() const override { return true; }
  char* readMemoryMapped(const int n) override {
    JXL_ASSERT(pos_ + n <= bytes_.size());
    char* const result =
        const_cast<char*>(reinterpret_cast<const char*>(bytes_.data() + pos_));
    pos_ += n;
    return result;
  }
  bool read(char c[], const int n) override {
    std::copy_n(readMemoryMapped(n), n, c);
    return pos_ < bytes_.size();
  }

  ExrInt64 tellg() override { return pos_; }
  void seekg(const ExrInt64 pos) override {
    JXL_ASSERT(pos + 1 <= bytes_.size());
    pos_ = pos;
  }

 private:
  const Span<const uint8_t> bytes_;
  size_t pos_ = 0;
};

class InMemoryOStream : public OpenEXR::OStream {
 public:
  // `bytes` must outlive the InMemoryOStream.
  explicit InMemoryOStream(PaddedBytes* const bytes)
      : OStream(/*fileName=*/""), bytes_(*bytes) {}

  void write(const char c[], const int n) override {
    if (bytes_.size() < pos_ + n) {
      bytes_.resize(pos_ + n);
    }
    std::copy_n(c, n, bytes_.begin() + pos_);
    pos_ += n;
  }

  ExrInt64 tellp() override { return pos_; }
  void seekp(const ExrInt64 pos) override {
    if (bytes_.size() + 1 < pos) {
      bytes_.resize(pos - 1);
    }
    pos_ = pos;
  }

 private:
  PaddedBytes& bytes_;
  size_t pos_ = 0;
};

}  // namespace

Status DecodeImageEXR(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      const SizeConstraints& constraints, float target_nits,
                      ThreadPool* pool, PackedPixelFile* ppf) {
  // Get the number of threads we should be using for OpenEXR.
  // OpenEXR creates its own set of threads, independent from ours. `pool` is
  // only used for converting from a buffer of OpenEXR::Rgba to Image3F.
  // TODO(sboukortt): look into changing that with OpenEXR 2.3 which allows
  // custom thread pools according to its changelog.
  OpenEXR::setGlobalThreadCount(GetNumThreads(pool));

  InMemoryIStream is(bytes);

#ifdef __EXCEPTIONS
  std::unique_ptr<OpenEXR::RgbaInputFile> input_ptr;
  try {
    input_ptr.reset(new OpenEXR::RgbaInputFile(is));
  } catch (...) {
    return JXL_FAILURE("OpenEXR failed to parse input");
  }
  OpenEXR::RgbaInputFile& input = *input_ptr;
#else
  OpenEXR::RgbaInputFile input(is);
#endif

  if ((input.channels() & OpenEXR::RgbaChannels::WRITE_RGB) !=
      OpenEXR::RgbaChannels::WRITE_RGB) {
    return JXL_FAILURE("only RGB OpenEXR files are supported");
  }
  const bool has_alpha = (input.channels() & OpenEXR::RgbaChannels::WRITE_A) ==
                         OpenEXR::RgbaChannels::WRITE_A;

  const float intensity_target =
      GetIntensityTarget(target_nits, input.header());

  auto image_size = input.displayWindow().size();
  // Size is computed as max - min, but both bounds are inclusive.
  ++image_size.x;
  ++image_size.y;

  ppf->info.xsize = image_size.x;
  ppf->info.ysize = image_size.y;
  ppf->info.num_color_channels = 3;

  const JxlDataType data_type =
      kExrBitsPerSample == 16 ? JXL_TYPE_FLOAT16 : JXL_TYPE_FLOAT;
  const JxlPixelFormat format{
      /*num_channels=*/3u + (has_alpha ? 1u : 0u),
      /*data_type=*/data_type,
      /*endianness=*/JXL_NATIVE_ENDIAN,
      /*align=*/0,
  };
  ppf->frames.clear();
  // Allocates the frame buffer.
  ppf->frames.emplace_back(image_size.x, image_size.y, format);
  const auto& frame = ppf->frames.back();

  const int row_size = input.dataWindow().size().x + 1;
  // Number of rows to read at a time.
  // https://www.openexr.com/documentation/ReadingAndWritingImageFiles.pdf
  // recommends reading the whole file at once.
  const int y_chunk_size = input.displayWindow().size().y + 1;
  std::vector<OpenEXR::Rgba> input_rows(row_size * y_chunk_size);
  for (int start_y =
           std::max(input.dataWindow().min.y, input.displayWindow().min.y);
       start_y <=
       std::min(input.dataWindow().max.y, input.displayWindow().max.y);
       start_y += y_chunk_size) {
    // Inclusive.
    const int end_y = std::min(
        start_y + y_chunk_size - 1,
        std::min(input.dataWindow().max.y, input.displayWindow().max.y));
    input.setFrameBuffer(
        input_rows.data() - input.dataWindow().min.x - start_y * row_size,
        /*xStride=*/1, /*yStride=*/row_size);
    input.readPixels(start_y, end_y);
    RunOnPool(
        pool, start_y, end_y + 1, ThreadPool::SkipInit(),
        [&](const int exr_y, const int /*thread*/) {
          const int image_y = exr_y - input.displayWindow().min.y;
          const OpenEXR::Rgba* const JXL_RESTRICT input_row =
              &input_rows[(exr_y - start_y) * row_size];
          uint8_t* row = static_cast<uint8_t*>(frame.color.pixels()) +
                         frame.color.stride * image_y;
          const uint32_t pixel_size =
              (3 + (has_alpha ? 1 : 0)) * kExrBitsPerSample / 8;
          for (int exr_x = std::max(input.dataWindow().min.x,
                                    input.displayWindow().min.x);
               exr_x <=
               std::min(input.dataWindow().max.x, input.displayWindow().max.x);
               ++exr_x) {
            const int image_x = exr_x - input.displayWindow().min.x;
            memcpy(row + image_x * pixel_size,
                   input_row + (exr_x - input.dataWindow().min.x), pixel_size);
          }
        },
        "DecodeImageEXR");
  }

  ppf->color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_LINEAR;
  ppf->color_encoding.color_space = JXL_COLOR_SPACE_RGB;
  ppf->color_encoding.primaries = JXL_PRIMARIES_SRGB;
  ppf->color_encoding.white_point = JXL_WHITE_POINT_D65;
  if (OpenEXR::hasChromaticities(input.header())) {
    ppf->color_encoding.primaries = JXL_PRIMARIES_CUSTOM;
    ppf->color_encoding.white_point = JXL_WHITE_POINT_CUSTOM;
    const auto& chromaticities = OpenEXR::chromaticities(input.header());
    ppf->color_encoding.primaries_red_xy[0] = chromaticities.red.x;
    ppf->color_encoding.primaries_red_xy[1] = chromaticities.red.y;
    ppf->color_encoding.primaries_green_xy[0] = chromaticities.green.x;
    ppf->color_encoding.primaries_green_xy[1] = chromaticities.green.y;
    ppf->color_encoding.primaries_blue_xy[0] = chromaticities.blue.x;
    ppf->color_encoding.primaries_blue_xy[1] = chromaticities.blue.y;
    ppf->color_encoding.white_point_xy[0] = chromaticities.white.x;
    ppf->color_encoding.white_point_xy[1] = chromaticities.white.y;
  }

  // EXR uses binary16 or binary32 floating point format.
  ppf->info.bits_per_sample = kExrBitsPerSample;
  ppf->info.exponent_bits_per_sample = kExrBitsPerSample == 16 ? 5 : 8;
  if (has_alpha) {
    ppf->info.alpha_bits = kExrAlphaBits;
    ppf->info.alpha_exponent_bits = ppf->info.exponent_bits_per_sample;
    ppf->info.alpha_premultiplied = true;
  }
  ppf->info.intensity_target = intensity_target;
  return true;
}

Status EncodeImageEXR(const CodecInOut* io, const ColorEncoding& c_desired,
                      ThreadPool* pool, PaddedBytes* bytes) {
  // As in `DecodeImageEXR`, `pool` is only used for pixel conversion, not for
  // actual OpenEXR I/O.
  OpenEXR::setGlobalThreadCount(GetNumThreads(pool));

  ColorEncoding c_linear = c_desired;
  c_linear.tf.SetTransferFunction(TransferFunction::kLinear);
  JXL_RETURN_IF_ERROR(c_linear.CreateICC());
  ImageMetadata metadata = io->metadata.m;
  ImageBundle store(&metadata);
  const ImageBundle* linear;
  JXL_RETURN_IF_ERROR(
      TransformIfNeeded(io->Main(), c_linear, pool, &store, &linear));

  const bool has_alpha = io->Main().HasAlpha();
  const bool alpha_is_premultiplied = io->Main().AlphaIsPremultiplied();

  OpenEXR::Header header(io->xsize(), io->ysize());
  const PrimariesCIExy& primaries = c_linear.HasPrimaries()
                                        ? c_linear.GetPrimaries()
                                        : ColorEncoding::SRGB().GetPrimaries();
  OpenEXR::Chromaticities chromaticities;
  chromaticities.red = Imath::V2f(primaries.r.x, primaries.r.y);
  chromaticities.green = Imath::V2f(primaries.g.x, primaries.g.y);
  chromaticities.blue = Imath::V2f(primaries.b.x, primaries.b.y);
  chromaticities.white =
      Imath::V2f(c_linear.GetWhitePoint().x, c_linear.GetWhitePoint().y);
  OpenEXR::addChromaticities(header, chromaticities);
  OpenEXR::addWhiteLuminance(header, io->metadata.m.IntensityTarget());

  // Ensure that the destructor of RgbaOutputFile has run before we look at the
  // size of `bytes`.
  {
    InMemoryOStream os(bytes);
    OpenEXR::RgbaOutputFile output(
        os, header, has_alpha ? OpenEXR::WRITE_RGBA : OpenEXR::WRITE_RGB);
    // How many rows to write at once. Again, the OpenEXR documentation
    // recommends writing the whole image in one call.
    const int y_chunk_size = io->ysize();
    std::vector<OpenEXR::Rgba> output_rows(io->xsize() * y_chunk_size);

    for (size_t start_y = 0; start_y < io->ysize(); start_y += y_chunk_size) {
      // Inclusive.
      const size_t end_y =
          std::min(start_y + y_chunk_size - 1, io->ysize() - 1);
      output.setFrameBuffer(output_rows.data() - start_y * io->xsize(),
                            /*xStride=*/1, /*yStride=*/io->xsize());
      RunOnPool(
          pool, start_y, end_y + 1, ThreadPool::SkipInit(),
          [&](const int y, const int /*thread*/) {
            const float* const JXL_RESTRICT input_rows[] = {
                linear->color().ConstPlaneRow(0, y),
                linear->color().ConstPlaneRow(1, y),
                linear->color().ConstPlaneRow(2, y),
            };
            OpenEXR::Rgba* const JXL_RESTRICT row_data =
                &output_rows[(y - start_y) * io->xsize()];
            if (has_alpha) {
              const float* const JXL_RESTRICT alpha_row =
                  io->Main().alpha().ConstRow(y);
              if (alpha_is_premultiplied) {
                for (size_t x = 0; x < io->xsize(); ++x) {
                  row_data[x] =
                      OpenEXR::Rgba(input_rows[0][x], input_rows[1][x],
                                    input_rows[2][x], alpha_row[x]);
                }
              } else {
                for (size_t x = 0; x < io->xsize(); ++x) {
                  row_data[x] = OpenEXR::Rgba(alpha_row[x] * input_rows[0][x],
                                              alpha_row[x] * input_rows[1][x],
                                              alpha_row[x] * input_rows[2][x],
                                              alpha_row[x]);
                }
              }
            } else {
              for (size_t x = 0; x < io->xsize(); ++x) {
                row_data[x] = OpenEXR::Rgba(input_rows[0][x], input_rows[1][x],
                                            input_rows[2][x], 1.f);
              }
            }
          },
          "EncodeImageEXR");
      output.writePixels(/*numScanLines=*/end_y - start_y + 1);
    }
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
