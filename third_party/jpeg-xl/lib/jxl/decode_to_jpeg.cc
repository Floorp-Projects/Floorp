// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/decode_to_jpeg.h"

namespace jxl {

#if JPEGXL_ENABLE_TRANSCODE_JPEG

JxlDecoderStatus JxlToJpegDecoder::Process(const uint8_t** next_in,
                                           size_t* avail_in) {
  if (!inside_box_) {
    JXL_ABORT(
        "processing of JPEG reconstruction data outside JPEG reconstruction "
        "box");
  }
  Span<const uint8_t> to_decode;
  if (box_until_eof_) {
    // Until EOF means consume all data.
    to_decode = Span<const uint8_t>(*next_in, *avail_in);
    *next_in += *avail_in;
    *avail_in = 0;
  } else {
    // Defined size means consume min(available, needed).
    size_t avail_recon_in =
        std::min<size_t>(*avail_in, box_size_ - buffer_.size());
    to_decode = Span<const uint8_t>(*next_in, avail_recon_in);
    *next_in += avail_recon_in;
    *avail_in -= avail_recon_in;
  }
  bool old_data_exists = !buffer_.empty();
  if (old_data_exists) {
    // Append incoming data to buffer if we already had data in the buffer.
    buffer_.insert(buffer_.end(), to_decode.data(),
                   to_decode.data() + to_decode.size());
    to_decode = Span<const uint8_t>(buffer_.data(), buffer_.size());
  }
  if (!box_until_eof_ && to_decode.size() > box_size_) {
    JXL_ABORT("JPEG reconstruction data to decode larger than expected");
  }
  if (box_until_eof_ || to_decode.size() == box_size_) {
    // If undefined size, or the right size, try to decode.
    jpeg_data_ = make_unique<jpeg::JPEGData>();
    const auto status = jpeg::DecodeJPEGData(to_decode, jpeg_data_.get());
    if (status.IsFatalError()) return JXL_DEC_ERROR;
    if (status) {
      // Successful decoding, emit event after updating state to track that we
      // are no longer parsing JPEG reconstruction data.
      inside_box_ = false;
      return JXL_DEC_JPEG_RECONSTRUCTION;
    }
    if (box_until_eof_) {
      // Unsuccessful decoding and undefined size, assume incomplete data. Copy
      // the data if we haven't already.
      if (!old_data_exists) {
        buffer_.insert(buffer_.end(), to_decode.data(),
                       to_decode.data() + to_decode.size());
      }
    } else {
      // Unsuccessful decoding of correct amount of data, assume error.
      return JXL_DEC_ERROR;
    }
  } else {
    // Not enough data, copy the data if we haven't already.
    if (!old_data_exists) {
      buffer_.insert(buffer_.end(), to_decode.data(),
                     to_decode.data() + to_decode.size());
    }
  }
  return JXL_DEC_NEED_MORE_INPUT;
}

#endif  // JPEGXL_ENABLE_TRANSCODE_JPEG

}  // namespace jxl
