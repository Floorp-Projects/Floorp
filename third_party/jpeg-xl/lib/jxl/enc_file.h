// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_ENC_FILE_H_
#define LIB_JXL_ENC_FILE_H_

// Facade for JXL encoding.

#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"

namespace jxl {

// Write preview from `io`.
Status EncodePreview(const CompressParams& cparams, const ImageBundle& ib,
                     const CodecMetadata* metadata, ThreadPool* pool,
                     BitWriter* JXL_RESTRICT writer);

// Write headers from the CodecMetadata. Also may modify nonserialized_...
// fields of the metadata.
Status WriteHeaders(CodecMetadata* metadata, BitWriter* writer,
                    AuxOut* aux_out);

// Compresses pixels from `io` (given in any ColorEncoding).
// `io->metadata.m.original` must be set.
Status EncodeFile(const CompressParams& params, const CodecInOut* io,
                  PassesEncoderState* passes_enc_state, PaddedBytes* compressed,
                  AuxOut* aux_out = nullptr, ThreadPool* pool = nullptr);

// Backwards-compatible interface. Don't use in new code.
// TODO(deymo): Remove this function once we migrate users to C encoder API.
struct FrameEncCache {};
JXL_INLINE Status EncodeFile(const CompressParams& params, const CodecInOut* io,
                             FrameEncCache* /* unused */,
                             PaddedBytes* compressed, AuxOut* aux_out = nullptr,
                             ThreadPool* pool = nullptr) {
  PassesEncoderState passes_enc_state;
  return EncodeFile(params, io, &passes_enc_state, compressed, aux_out, pool);
}

}  // namespace jxl

#endif  // LIB_JXL_ENC_FILE_H_
