// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_EXTRAS_DEC_PNM_H_
#define LIB_EXTRAS_DEC_PNM_H_

// Decodes PBM/PGM/PPM/PFM pixels in memory.

#include <stddef.h>
#include <stdint.h>

// TODO(janwas): workaround for incorrect Win64 codegen (cause unknown)
#include <hwy/highway.h>
#include <mutex>

#include "lib/extras/dec/color_hints.h"
#include "lib/extras/mmap.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"

namespace jxl {

struct SizeConstraints;

namespace extras {

// Decodes `bytes` into `ppf`. color_hints may specify "color_space", which
// defaults to sRGB.
Status DecodeImagePNM(Span<const uint8_t> bytes, const ColorHints& color_hints,
                      PackedPixelFile* ppf,
                      const SizeConstraints* constraints = nullptr);

void TestCodecPNM();

struct HeaderPNM {
  size_t xsize;
  size_t ysize;
  bool is_gray;    // PGM
  bool has_alpha;  // PAM
  size_t bits_per_sample;
  bool floating_point;
  bool big_endian;
  std::vector<JxlExtraChannelType> ec_types;  // PAM
};

class ChunkedPNMDecoder {
 public:
  static StatusOr<ChunkedPNMDecoder> Init(const char* file_path);
  // Initializes `ppf` with a pointer to this `ChunkedPNMDecoder`.
  jxl::Status InitializePPF(const ColorHints& color_hints,
                            PackedPixelFile* ppf);

 private:
  HeaderPNM header_ = {};
  size_t data_start_ = 0;
  MemoryMappedFile pnm_;

  friend struct PNMChunkedInputFrame;
};

}  // namespace extras
}  // namespace jxl

#endif  // LIB_EXTRAS_DEC_PNM_H_
