// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_DECODE_TO_JPEG_H_
#define LIB_JXL_DECODE_TO_JPEG_H_

// JPEG XL to JPEG bytes decoder logic. The JxlToJpegDecoder class keeps track
// of the decoder state needed to parse the JPEG reconstruction box and provide
// the reconstructed JPEG to the output buffer.

#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "jxl/decode.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"  // JPEGXL_ENABLE_TRANSCODE_JPEG
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/jpeg/dec_jpeg_data.h"
#if JPEGXL_ENABLE_TRANSCODE_JPEG
#include "lib/jxl/jpeg/dec_jpeg_data_writer.h"
#endif  // JPEGXL_ENABLE_TRANSCODE_JPEG

namespace jxl {

#if JPEGXL_ENABLE_TRANSCODE_JPEG

class JxlToJpegDecoder {
 public:
  // Returns whether an output buffer is set.
  bool IsOutputSet() const { return next_out_ != nullptr; }

  // Returns whether the decoder is parsing a boxa JPEG box was parsed.
  bool IsParsingBox() const { return inside_box_; }

  const jpeg::JPEGData* JpegData() const { return jpeg_data_.get(); }

  // Return the parsed jpeg::JPEGData object and removes it from the
  // JxlToJpegDecoder.
  jpeg::JPEGData* ReleaseJpegData() { return jpeg_data_.release(); }

  // Sets the output buffer used when producing JPEG output.
  JxlDecoderStatus SetOutputBuffer(uint8_t* data, size_t size) {
    if (next_out_) return JXL_DEC_ERROR;
    next_out_ = data;
    avail_size_ = size;
    return JXL_DEC_SUCCESS;
  }

  // Releases the buffer set with SetOutputBuffer().
  size_t ReleaseOutputBuffer() {
    size_t result = avail_size_;
    next_out_ = nullptr;
    avail_size_ = 0;
    return result;
  }

  void StartBox(uint64_t box_size, size_t contents_size) {
    // A new box implies that we clear the buffer.
    buffer_.clear();
    inside_box_ = true;
    if (box_size == 0) {
      box_until_eof_ = true;
    } else {
      box_size_ = contents_size;
    }
  }

  // Consumes data from next_in/avail_in to reconstruct JPEG data.
  // Uses box_size_, inside_box_ and box_until_eof_ to calculate how much to
  // consume. Potentially stores unparsed data in buffer_.
  // Potentially populates jpeg_data_. Potentially updates inside_box_.
  JxlDecoderStatus Process(const uint8_t** next_in, size_t* avail_in);

  // Sets the JpegData of the ImageBundle passed if there is anything to set.
  // Releases the JpegData from this decoder if set.
  Status SetImageBundleJpegData(ImageBundle* ib) {
    if (IsOutputSet() && jpeg_data_ != nullptr) {
      if (!jpeg::SetJPEGDataFromICC(ib->metadata()->color_encoding.ICC(),
                                    jpeg_data_.get())) {
        return false;
      }
      ib->jpeg_data.reset(jpeg_data_.release());
    }
    return true;
  }

  JxlDecoderStatus WriteOutput(const jpeg::JPEGData& jpeg_data) {
    // Copy JPEG bytestream if desired.
    uint8_t* tmp_next_out = next_out_;
    size_t tmp_avail_size = avail_size_;
    auto write = [&tmp_next_out, &tmp_avail_size](const uint8_t* buf,
                                                  size_t len) {
      size_t to_write = std::min<size_t>(tmp_avail_size, len);
      memcpy(tmp_next_out, buf, to_write);
      tmp_next_out += to_write;
      tmp_avail_size -= to_write;
      return to_write;
    };
    Status write_result = jpeg::WriteJpeg(jpeg_data, write);
    if (!write_result) {
      if (tmp_avail_size == 0) {
        return JXL_DEC_JPEG_NEED_MORE_OUTPUT;
      }
      return JXL_DEC_ERROR;
    }
    next_out_ = tmp_next_out;
    avail_size_ = tmp_avail_size;
    return JXL_DEC_SUCCESS;
  }

 private:
  // Content of the most recently parsed JPEG reconstruction box if any.
  std::vector<uint8_t> buffer_;

  // Decoded content of the most recently parsed JPEG reconstruction box is
  // stored here.
  std::unique_ptr<jpeg::JPEGData> jpeg_data_;

  // True if the decoder is currently reading bytes inside a JPEG reconstruction
  // box.
  bool inside_box_ = false;

  // True if the JPEG reconstruction box had undefined size (all remaining
  // bytes).
  bool box_until_eof_ = false;
  // Size of most recently parsed JPEG reconstruction box contents.
  size_t box_size_ = 0;

  // Next bytes to write JPEG reconstruction to.
  uint8_t* next_out_ = nullptr;
  // Available bytes to write JPEG reconstruction to.
  size_t avail_size_ = 0;
};

#else

// Fake class that disables support for decoding JPEG XL to JPEG.
class JxlToJpegDecoder {
 public:
  bool IsOutputSet() const { return false; }
  bool IsParsingBox() const { return false; }

  const jpeg::JPEGData* JpegData() const { return nullptr; }
  jpeg::JPEGData* ReleaseJpegData() { return nullptr; }

  JxlDecoderStatus SetOutputBuffer(uint8_t* /* data */, size_t /* size */) {
    return JXL_DEC_ERROR;
  }
  size_t ReleaseOutputBuffer() { return 0; }

  void StartBox(uint64_t /* box_size */, size_t /* contents_size */) {}

  JxlDecoderStatus Process(const uint8_t** next_in, size_t* avail_in) {
    return JXL_DEC_ERROR;
  }

  Status SetImageBundleJpegData(ImageBundle* /* ib */) { return true; }

  JxlDecoderStatus WriteOutput(const jpeg::JPEGData& /* jpeg_data */) {
    return JXL_DEC_SUCCESS;
  }
};

#endif  // JPEGXL_ENABLE_TRANSCODE_JPEG

}  // namespace jxl

#endif  // LIB_JXL_DECODE_TO_JPEG_H_
