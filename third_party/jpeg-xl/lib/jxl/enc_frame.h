// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_FRAME_H_
#define LIB_JXL_ENC_FRAME_H_

#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

// Information needed for encoding a frame that is not contained elsewhere and
// does not belong to `cparams`.
struct FrameInfo {
  // TODO(veluca): consider adding more parameters, such as custom patches.
  bool save_before_color_transform = false;
  // Whether or not the input image bundle is already in the codestream
  // colorspace (as deduced by cparams).
  // TODO(veluca): this is a hack - ImageBundle doesn't have a simple way to say
  // "this is already in XYB".
  bool ib_needs_color_transform = true;
  FrameType frame_type = FrameType::kRegularFrame;
  size_t dc_level = 0;
  // Only used for kRegularFrame.
  bool is_last = true;
  bool is_preview = false;
  // Information for storing this frame for future use (only for non-DC frames).
  size_t save_as_reference = 0;
};

// Encodes a single frame (including its header) into a byte stream.  Groups may
// be processed in parallel by `pool`. metadata is the ImageMetadata encoded in
// the codestream, and must be used for the FrameHeaders, do not use
// ib.metadata.
Status EncodeFrame(const CompressParams& cparams_orig,
                   const FrameInfo& frame_info, const CodecMetadata* metadata,
                   const ImageBundle& ib, PassesEncoderState* passes_enc_state,
                   ThreadPool* pool, BitWriter* writer, AuxOut* aux_out);

}  // namespace jxl

#endif  // LIB_JXL_ENC_FRAME_H_
