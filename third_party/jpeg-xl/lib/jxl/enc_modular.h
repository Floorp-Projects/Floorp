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

#ifndef LIB_JXL_ENC_MODULAR_H_
#define LIB_JXL_ENC_MODULAR_H_

#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/dec_modular.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/modular_image.h"

namespace jxl {

class ModularFrameEncoder {
 public:
  ModularFrameEncoder(const FrameHeader& frame_header,
                      const CompressParams& cparams_orig);
  Status ComputeEncodingData(const FrameHeader& frame_header,
                             const ImageBundle& ib, Image3F* JXL_RESTRICT color,
                             PassesEncoderState* JXL_RESTRICT enc_state,
                             ThreadPool* pool, AuxOut* aux_out, bool do_color);
  // Encodes global info (tree + histograms) in the `writer`.
  Status EncodeGlobalInfo(BitWriter* writer, AuxOut* aux_out);
  // Encodes a specific modular image (identified by `stream`) in the `writer`,
  // assigning bits to the provided `layer`.
  Status EncodeStream(BitWriter* writer, AuxOut* aux_out, size_t layer,
                      const ModularStreamId& stream);
  // Creates a modular image for a given DC group of VarDCT mode. `dc` is the
  // input DC image, not quantized; the group is specified by `group_index`, and
  // `nl_dc` decides whether to apply a near-lossless processing to the DC or
  // not.
  void AddVarDCTDC(const Image3F& dc, size_t group_index, bool nl_dc,
                   PassesEncoderState* enc_state);
  // Creates a modular image for the AC metadata of the given group
  // (`group_index`).
  void AddACMetadata(size_t group_index, bool jpeg_transcode,
                     PassesEncoderState* enc_state);
  // Encodes a RAW quantization table in `writer`. If `modular_frame_encoder` is
  // null, the quantization table in `encoding` is used, with dimensions `size_x
  // x size_y`. Otherwise, the table with ID `idx` is encoded from the given
  // `modular_frame_encoder`.
  static void EncodeQuantTable(size_t size_x, size_t size_y, BitWriter* writer,
                               const QuantEncoding& encoding, size_t idx,
                               ModularFrameEncoder* modular_frame_encoder);
  // Stores a quantization table for future usage with `EncodeQuantTable`.
  void AddQuantTable(size_t size_x, size_t size_y,
                     const QuantEncoding& encoding, size_t idx);

  std::vector<size_t> ac_metadata_size;
  std::vector<uint8_t> extra_dc_precision;

 private:
  Status PrepareEncoding(ThreadPool* pool, const FrameDimensions& frame_dim,
                         EncoderHeuristics* heuristics,
                         AuxOut* aux_out = nullptr);
  Status PrepareStreamParams(const Rect& rect, const CompressParams& cparams,
                             int minShift, int maxShift,
                             const ModularStreamId& stream, bool do_color);
  std::vector<Image> stream_images;
  std::vector<ModularOptions> stream_options;

  Tree tree;
  std::vector<std::vector<Token>> tree_tokens;
  std::vector<GroupHeader> stream_headers;
  std::vector<std::vector<Token>> tokens;
  EntropyEncodingData code;
  std::vector<uint8_t> context_map;
  FrameDimensions frame_dim;
  CompressParams cparams;
  float quality = cparams.quality_pair.first;
  float cquality = cparams.quality_pair.second;
  std::vector<size_t> tree_splits;
  std::vector<ModularMultiplierInfo> multiplier_info;
  std::vector<std::vector<uint32_t>> gi_channel;
  std::vector<size_t> image_widths;
};

}  // namespace jxl

#endif  // LIB_JXL_ENC_MODULAR_H_
