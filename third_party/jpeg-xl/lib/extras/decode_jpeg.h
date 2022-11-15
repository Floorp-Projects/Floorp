// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DECODE_JPEG_H_
#define LIB_EXTRAS_DECODE_JPEG_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "hwy/aligned_allocator.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image.h"

namespace jxl {
namespace extras {

constexpr int kMaxComponents = 4;

typedef int16_t coeff_t;

// Represents one component of a jpeg file.
struct JPEGComponent {
  JPEGComponent()
      : id(0),
        h_samp_factor(1),
        v_samp_factor(1),
        quant_idx(0),
        width_in_blocks(0),
        height_in_blocks(0) {}

  // One-byte id of the component.
  uint32_t id;
  // Horizontal and vertical sampling factors.
  // In interleaved mode, each minimal coded unit (MCU) has
  // h_samp_factor x v_samp_factor DCT blocks from this component.
  int h_samp_factor;
  int v_samp_factor;
  // The index of the quantization table used for this component.
  uint32_t quant_idx;
  // The dimensions of the component measured in 8x8 blocks.
  uint32_t width_in_blocks;
  uint32_t height_in_blocks;
  // The DCT coefficients of this component, laid out block-by-block, divided
  // through the quantization matrix values.
  hwy::AlignedFreeUniquePtr<coeff_t[]> coeffs;
};

struct HuffmanTableEntry {
  // Initialize the value to an invalid symbol so that we can recognize it
  // when reading the bit stream using a Huffman code with space > 0.
  HuffmanTableEntry() : bits(0), value(0xffff) {}

  uint8_t bits;    // number of bits used for this symbol
  uint16_t value;  // symbol value or table offset
};

// Quantization values for an 8x8 pixel block.
struct JPEGQuantTable {
  std::array<int32_t, kDCTBlockSize> values;
  // The index of this quantization table as it was parsed from the input JPEG.
  // Each DQT marker segment contains an 'index' field, and we save this index
  // here. Valid values are 0 to 3.
  uint32_t index = 0;
};

// Huffman table indexes and MCU dimensions used for one component of one scan.
struct JPEGComponentScanInfo {
  uint32_t comp_idx;
  uint32_t dc_tbl_idx;
  uint32_t ac_tbl_idx;
  uint32_t mcu_ysize_blocks;
  uint32_t mcu_xsize_blocks;
};

// Contains information that is used in one scan.
struct JPEGScanInfo {
  // Parameters used for progressive scans (named the same way as in the spec):
  //   Ss : Start of spectral band in zig-zag sequence.
  //   Se : End of spectral band in zig-zag sequence.
  //   Ah : Successive approximation bit position, high.
  //   Al : Successive approximation bit position, low.
  uint32_t Ss;
  uint32_t Se;
  uint32_t Ah;
  uint32_t Al;
  uint32_t num_components = 0;
  std::array<JPEGComponentScanInfo, kMaxComponents> components;
  size_t MCU_rows;
  size_t MCU_cols;
};

// State of the decoder that has to be saved before decoding one MCU in case
// we run out of the bitstream.
struct MCUCodingState {
  coeff_t last_dc_coeff[kMaxComponents];
  int eobrun;
  std::vector<coeff_t> coeffs;
};

// Streaming JPEG decoding object.
class JpegDecoder {
 public:
  enum class Status {
    kSuccess,
    kNeedMoreInput,
    kError,
  };

  // Sets the next chunk of input. It must be called before the first call to
  // ReadHeaders() and every time a reder function returns
  // Status::kNeedMoreInput.
  Status SetInput(const uint8_t* data, size_t len);

  // Sets the output image. Must be called between ReadHeaders() and
  // ReadScanLines(). The provided image must have the dimensions and number of
  // channels as the underlying JPEG bitstream.
  Status SetOutput(PackedImage* image);

  // Reads the header markers up to and including SOF marker. After this returns
  // kSuccess, the image attribute accessors can be called.
  Status ReadHeaders();

  // Reads the bitstream after the SOF marker, and fills in at most
  // max_output_rows scan lines of the provided image. Set *num_output_rows to
  // the actual number of lines produced.
  Status ReadScanLines(size_t* num_output_rows, size_t max_output_rows);

  // Image attribute accessors, can be called after ReadHeaders() returns
  // kSuccess.
  size_t xsize() const { return xsize_; }
  size_t ysize() const { return ysize_; }
  size_t num_channels() const { return components_.size(); }
  const std::vector<uint8_t>& icc_profile() const { return icc_profile_; }

 private:
  enum class State {
    kStart,
    kProcessMarkers,
    kScan,
    kRender,
    kEnd,
  };
  State state_ = State::kStart;

  //
  // Input handling state.
  //
  const uint8_t* next_in_ = nullptr;
  size_t avail_in_ = 0;
  // Codestream input data is copied here temporarily when the decoder needs
  // more input bytes to process the next part of the stream.
  std::vector<uint8_t> codestream_copy_;
  // Number of bytes at the end of codestream_copy_ that were not yet consumed
  // by calling AdvanceInput().
  size_t codestream_unconsumed_ = 0;
  // Position in the codestream_copy_ vector that the decoder already finished
  // processing.
  size_t codestream_pos_ = 0;
  // Number of bits after codestream_pos_ that were already processed.
  size_t codestream_bits_ahead_ = 0;

  //
  // Marker data processing state.
  //
  bool found_soi_ = false;
  bool found_app0_ = false;
  bool found_dri_ = false;
  bool found_sof_ = false;
  bool found_eoi_ = false;
  size_t xsize_ = 0;
  size_t ysize_ = 0;
  bool is_ycbcr_ = true;
  size_t icc_index_ = 0;
  size_t icc_total_ = 0;
  std::vector<uint8_t> icc_profile_;
  size_t restart_interval_ = 0;
  std::vector<JPEGQuantTable> quant_;
  std::vector<JPEGComponent> components_;
  std::vector<HuffmanTableEntry> dc_huff_lut_;
  std::vector<HuffmanTableEntry> ac_huff_lut_;
  uint8_t huff_slot_defined_[256] = {};

  // Fields defined by SOF marker.
  bool is_progressive_;
  int max_h_samp_;
  int max_v_samp_;
  size_t iMCU_rows_;
  size_t iMCU_cols_;
  size_t iMCU_width_;
  size_t iMCU_height_;

  // Initialized at strat of frame.
  uint16_t scan_progression_[kMaxComponents][kDCTBlockSize];

  //
  // Per scan state.
  //
  JPEGScanInfo scan_info_;
  size_t scan_mcu_row_;
  size_t scan_mcu_col_;
  coeff_t last_dc_coeff_[kMaxComponents];
  int eobrun_;
  int restarts_to_go_;
  int next_restart_marker_;

  MCUCodingState mcu_;

  //
  // Rendering state.
  //
  PackedImage* output_;

  Image3F MCU_row_buf_;
  size_t MCU_buf_current_row_;
  size_t MCU_buf_ready_rows_;

  size_t output_row_;
  size_t output_mcu_row_;
  size_t output_ci_;

  // Temporary buffers for vertically upsampled chroma components. We keep a
  // ringbuffer of 3 * kBlockDim rows so that we have access for previous and
  // next rows.
  std::vector<ImageF> chroma_;
  // In the rendering order, vertically upsampled chroma components come first.
  std::vector<size_t> component_order_;
  hwy::AlignedFreeUniquePtr<float[]> idct_scratch_;
  hwy::AlignedFreeUniquePtr<float[]> upsample_scratch_;
  hwy::AlignedFreeUniquePtr<uint8_t[]> output_scratch_;

  hwy::AlignedFreeUniquePtr<float[]> dequant_;
  // Per channel and per frequency statistics about the number of nonzeros and
  // the sum of coefficient absolute values, used in dequantization bias
  // computation.
  hwy::AlignedFreeUniquePtr<int[]> nonzeros_;
  hwy::AlignedFreeUniquePtr<int[]> sumabs_;
  std::vector<size_t> num_processed_blocks_;
  hwy::AlignedFreeUniquePtr<float[]> biases_;

  void AdvanceInput(size_t size);
  void AdvanceCodestream(size_t size);
  Status RequestMoreInput();
  Status GetCodestreamInput(const uint8_t** data, size_t* len);

  Status ProcessMarker(const uint8_t* data, size_t len, size_t* pos);
  Status ProcessSOF(const uint8_t* data, size_t len);
  Status ProcessSOS(const uint8_t* data, size_t len);
  Status ProcessDHT(const uint8_t* data, size_t len);
  Status ProcessDQT(const uint8_t* data, size_t len);
  Status ProcessDRI(const uint8_t* data, size_t len);
  Status ProcessAPP(const uint8_t* data, size_t len);
  Status ProcessCOM(const uint8_t* data, size_t len);

  Status ProcessScan(const uint8_t* data, size_t len, size_t* pos);

  void SaveMCUCodingState();
  void RestoreMCUCodingState();

  void PrepareForOutput();
  void ProcessOutput(size_t* num_output_rows, size_t max_output_rows);
};

Status DecodeJpeg(const std::vector<uint8_t>& compressed,
                  JxlDataType output_data_type, ThreadPool* pool,
                  PackedPixelFile* ppf);

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DECODE_JPEG_H_
