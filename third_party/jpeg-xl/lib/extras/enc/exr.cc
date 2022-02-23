// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/enc/exr.h"

#include <ImfChromaticitiesAttribute.h>
#include <ImfIO.h>
#include <ImfRgbaFile.h>
#include <ImfStandardAttributes.h>

#include <vector>

#include "lib/jxl/alpha.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/enc_color_management.h"
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

size_t GetNumThreads(ThreadPool* pool) {
  size_t exr_num_threads = 1;
  JXL_CHECK(RunOnPool(
      pool, 0, 1,
      [&](size_t num_threads) {
        exr_num_threads = num_threads;
        return true;
      },
      [&](uint32_t /* task */, size_t /*thread*/) {}, "DecodeImageEXRThreads"));
  return exr_num_threads;
}

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
  JXL_RETURN_IF_ERROR(TransformIfNeeded(io->Main(), c_linear, GetJxlCms(), pool,
                                        &store, &linear));

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
      JXL_RETURN_IF_ERROR(RunOnPool(
          pool, start_y, end_y + 1, ThreadPool::NoInit,
          [&](const uint32_t y, size_t /* thread */) {
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
          "EncodeImageEXR"));
      output.writePixels(/*numScanLines=*/end_y - start_y + 1);
    }
  }

  return true;
}

}  // namespace extras
}  // namespace jxl
