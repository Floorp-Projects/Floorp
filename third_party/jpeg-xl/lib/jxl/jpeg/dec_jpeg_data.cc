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

#include "lib/jxl/jpeg/dec_jpeg_data.h"

#include <brotli/decode.h>

#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_bit_reader.h"

namespace jxl {
namespace jpeg {
Status DecodeJPEGData(Span<const uint8_t> encoded, JPEGData* jpeg_data) {
  Status ret = true;
  const uint8_t* in = encoded.data();
  size_t available_in = encoded.size();
  {
    BitReader br(encoded);
    BitReaderScopedCloser br_closer(&br, &ret);
    JXL_RETURN_IF_ERROR(Bundle::Read(&br, jpeg_data));
    JXL_RETURN_IF_ERROR(br.JumpToByteBoundary());
    in += br.TotalBitsConsumed() / 8;
    available_in -= br.TotalBitsConsumed() / 8;
  }
  JXL_RETURN_IF_ERROR(ret);

  BrotliDecoderState* brotli_dec =
      BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);

  struct BrotliDecDeleter {
    BrotliDecoderState* brotli_dec;
    ~BrotliDecDeleter() { BrotliDecoderDestroyInstance(brotli_dec); }
  } brotli_dec_deleter{brotli_dec};

  BrotliDecoderResult result =
      BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS;

  auto br_read = [&](std::vector<uint8_t>& data) -> Status {
    size_t available_out = data.size();
    uint8_t* out = data.data();
    while (available_out != 0) {
      if (BrotliDecoderIsFinished(brotli_dec)) {
        return JXL_FAILURE("Not enough decompressed output");
      }
      result = BrotliDecoderDecompressStream(brotli_dec, &available_in, &in,
                                             &available_out, &out, nullptr);
      if (result !=
              BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT &&
          result != BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS) {
        return JXL_FAILURE(
            "Brotli decoding error: %s\n",
            BrotliDecoderErrorString(BrotliDecoderGetErrorCode(brotli_dec)));
      }
    }
    return true;
  };
  size_t num_icc = 0;
  for (size_t i = 0; i < jpeg_data->app_data.size(); i++) {
    auto& marker = jpeg_data->app_data[i];
    if (jpeg_data->app_marker_type[i] != AppMarkerType::kUnknown) {
      // Set the size of the marker.
      size_t size_minus_1 = marker.size() - 1;
      marker[1] = size_minus_1 >> 8;
      marker[2] = size_minus_1 & 0xFF;
      if (jpeg_data->app_marker_type[i] == AppMarkerType::kICC) {
        if (marker.size() < 17) {
          return JXL_FAILURE("ICC markers must be at least 17 bytes");
        }
        marker[0] = 0xE2;
        memcpy(&marker[3], kIccProfileTag, sizeof kIccProfileTag);
        marker[15] = ++num_icc;
      }
    } else {
      JXL_RETURN_IF_ERROR(br_read(marker));
      if (marker[1] * 256u + marker[2] + 1u != marker.size()) {
        return JXL_FAILURE("Incorrect marker size");
      }
    }
  }
  for (size_t i = 0; i < jpeg_data->app_data.size(); i++) {
    auto& marker = jpeg_data->app_data[i];
    if (jpeg_data->app_marker_type[i] == AppMarkerType::kICC) {
      marker[16] = num_icc;
    }
    if (jpeg_data->app_marker_type[i] == AppMarkerType::kExif) {
      marker[0] = 0xE1;
      if (marker.size() < 3 + sizeof kExifTag) {
        return JXL_FAILURE("Incorrect Exif marker size");
      }
      memcpy(&marker[3], kExifTag, sizeof kExifTag);
    }
    if (jpeg_data->app_marker_type[i] == AppMarkerType::kXMP) {
      marker[0] = 0xE1;
      if (marker.size() < 3 + sizeof kXMPTag) {
        return JXL_FAILURE("Incorrect XMP marker size");
      }
      memcpy(&marker[3], kXMPTag, sizeof kXMPTag);
    }
  }
  // TODO(eustas): actually inject ICC profile and check it fits perfectly.
  for (size_t i = 0; i < jpeg_data->com_data.size(); i++) {
    auto& marker = jpeg_data->com_data[i];
    JXL_RETURN_IF_ERROR(br_read(marker));
    if (marker[1] * 256u + marker[2] + 1u != marker.size()) {
      return JXL_FAILURE("Incorrect marker size");
    }
  }
  for (size_t i = 0; i < jpeg_data->inter_marker_data.size(); i++) {
    JXL_RETURN_IF_ERROR(br_read(jpeg_data->inter_marker_data[i]));
  }
  JXL_RETURN_IF_ERROR(br_read(jpeg_data->tail_data));

  // Check if there is more decompressed output.
  size_t available_out = 1;
  uint64_t dummy;
  uint8_t* next_out = reinterpret_cast<uint8_t*>(&dummy);
  result = BrotliDecoderDecompressStream(brotli_dec, &available_in, &in,
                                         &available_out, &next_out, nullptr);
  if (available_out == 0 ||
      result == BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
    return JXL_FAILURE("Excess data in compressed stream");
  }
  if (result == BrotliDecoderResult::BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
    return JXL_FAILURE("Incomplete brotli-stream");
  }
  if (!BrotliDecoderIsFinished(brotli_dec) ||
      result != BrotliDecoderResult::BROTLI_DECODER_RESULT_SUCCESS) {
    return JXL_FAILURE("Corrupted brotli-stream");
  }
  if (available_in != 0) {
    return JXL_FAILURE("Unused data after brotli stream");
  }

  return true;
}
}  // namespace jpeg
}  // namespace jxl
