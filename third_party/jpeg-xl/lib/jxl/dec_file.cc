// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/dec_file.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "jxl/decode.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/dec_frame.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/icc_codec.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"

namespace jxl {
namespace {

Status DecodeHeaders(BitReader* reader, CodecInOut* io) {
  JXL_RETURN_IF_ERROR(ReadSizeHeader(reader, &io->metadata.size));

  JXL_RETURN_IF_ERROR(ReadImageMetadata(reader, &io->metadata.m));

  io->metadata.transform_data.nonserialized_xyb_encoded =
      io->metadata.m.xyb_encoded;
  JXL_RETURN_IF_ERROR(Bundle::Read(reader, &io->metadata.transform_data));

  return true;
}

}  // namespace

// To avoid the complexity of file I/O and buffering, we assume the bitstream
// is loaded (or for large images/sequences: mapped into) memory.
Status DecodeFile(const DecompressParams& dparams,
                  const Span<const uint8_t> file, CodecInOut* JXL_RESTRICT io,
                  ThreadPool* pool) {
  PROFILER_ZONE("DecodeFile uninstrumented");

  // Marker
  JxlSignature signature = JxlSignatureCheck(file.data(), file.size());
  if (signature == JXL_SIG_NOT_ENOUGH_BYTES || signature == JXL_SIG_INVALID) {
    return JXL_FAILURE("File does not start with known JPEG XL signature");
  }

  std::unique_ptr<jpeg::JPEGData> jpeg_data = nullptr;
  if (dparams.keep_dct) {
    if (io->Main().jpeg_data == nullptr) {
      return JXL_FAILURE("Caller must set jpeg_data");
    }
    jpeg_data = std::move(io->Main().jpeg_data);
  }

  Status ret = true;
  {
    BitReader reader(file);
    BitReaderScopedCloser reader_closer(&reader, &ret);
    if (reader.ReadFixedBits<16>() != 0x0AFF) {
      // We don't have a naked codestream. Make a quick & dirty attempt to find
      // the codestream.
      // TODO(jon): get rid of this whole function
      const unsigned char* begin = file.data();
      const unsigned char* end = file.data() + file.size() - 4;
      while (begin < end) {
        if (!memcmp(begin, "jxlc", 4)) break;
        begin++;
      }
      if (begin >= end) return JXL_FAILURE("Couldn't find jxl codestream");
      reader.SkipBits(8 * (begin - file.data() + 2));
      unsigned int firstbytes = reader.ReadFixedBits<16>();
      if (firstbytes != 0x0AFF)
        return JXL_FAILURE("Codestream didn't start with FF0A but with %X",
                           firstbytes);
    }

    {
      JXL_RETURN_IF_ERROR(DecodeHeaders(&reader, io));
      size_t xsize = io->metadata.xsize();
      size_t ysize = io->metadata.ysize();
      JXL_RETURN_IF_ERROR(VerifyDimensions(&io->constraints, xsize, ysize));
    }

    if (io->metadata.m.color_encoding.WantICC()) {
      PaddedBytes icc;
      JXL_RETURN_IF_ERROR(ReadICC(&reader, &icc));
      JXL_RETURN_IF_ERROR(io->metadata.m.color_encoding.SetICC(std::move(icc)));
    }
    // Set ICC profile in jpeg_data.
    if (jpeg_data) {
      Status res = jpeg::SetJPEGDataFromICC(io->metadata.m.color_encoding.ICC(),
                                            jpeg_data.get());
      if (!res) {
        return res;
      }
    }

    PassesDecoderState dec_state;
    JXL_RETURN_IF_ERROR(dec_state.output_encoding_info.Set(
        io->metadata,
        ColorEncoding::LinearSRGB(io->metadata.m.color_encoding.IsGray())));

    if (io->metadata.m.have_preview) {
      JXL_RETURN_IF_ERROR(reader.JumpToByteBoundary());
      JXL_RETURN_IF_ERROR(DecodeFrame(dparams, &dec_state, pool, &reader,
                                      &io->preview_frame, io->metadata,
                                      &io->constraints, /*is_preview=*/true));
    }

    // Only necessary if no ICC and no preview.
    JXL_RETURN_IF_ERROR(reader.JumpToByteBoundary());
    if (io->metadata.m.have_animation && dparams.keep_dct) {
      return JXL_FAILURE("Cannot decode to JPEG an animation");
    }

    io->frames.clear();
    Status dec_ok(false);
    do {
      io->frames.emplace_back(&io->metadata.m);
      if (jpeg_data) {
        io->frames.back().jpeg_data = std::move(jpeg_data);
      }
      // Skip frames that are not displayed.
      bool found_displayed_frame = true;
      do {
        dec_ok =
            DecodeFrame(dparams, &dec_state, pool, &reader, &io->frames.back(),
                        io->metadata, &io->constraints);
        if (!dparams.allow_partial_files) {
          JXL_RETURN_IF_ERROR(dec_ok);
        } else if (!dec_ok) {
          io->frames.pop_back();
          found_displayed_frame = false;
          break;
        }
      } while (dec_state.shared->frame_header.frame_type !=
                   FrameType::kRegularFrame &&
               dec_state.shared->frame_header.frame_type !=
                   FrameType::kSkipProgressive);
      if (found_displayed_frame) {
        // if found_displayed_frame is true io->frames shouldn't be empty
        // because we added a frame before the loop.
        JXL_ASSERT(!io->frames.empty());
        io->dec_pixels += io->frames.back().xsize() * io->frames.back().ysize();
      }
    } while (!dec_state.shared->frame_header.is_last && dec_ok);

    if (io->frames.empty()) return JXL_FAILURE("Not enough data.");

    if (dparams.check_decompressed_size && !dparams.allow_partial_files &&
        dparams.max_downsampling == 1) {
      if (reader.TotalBitsConsumed() != file.size() * kBitsPerByte) {
        return JXL_FAILURE("DecodeFile reader position not at EOF.");
      }
    }
    // Suppress errors when decoding partial files with DC frames.
    if (!reader.AllReadsWithinBounds() && dparams.allow_partial_files) {
      reader_closer.CloseAndSuppressError();
    }

    io->CheckMetadata();
    // reader is closed here.
  }
  return ret;
}

}  // namespace jxl
