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

#ifndef LIB_JXL_COMMON_H_
#define LIB_JXL_COMMON_H_

// Shared constants and helper functions.

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include <limits>  // numeric_limits
#include <memory>  // unique_ptr
#include <string>

#include "lib/jxl/base/compiler_specific.h"

#ifndef JXL_HIGH_PRECISION
#define JXL_HIGH_PRECISION 1
#endif

namespace jxl {
// Some enums and typedefs used by more than one header file.

constexpr size_t kBitsPerByte = 8;  // more clear than CHAR_BIT

constexpr inline size_t RoundUpBitsToByteMultiple(size_t bits) {
  return (bits + 7) & ~size_t(7);
}

constexpr inline size_t RoundUpToBlockDim(size_t dim) {
  return (dim + 7) & ~size_t(7);
}

static inline bool JXL_MAYBE_UNUSED SafeAdd(const uint64_t a, const uint64_t b,
                                            uint64_t& sum) {
  sum = a + b;
  return sum >= a;  // no need to check b - either sum >= both or < both.
}

template <typename T1, typename T2>
constexpr inline T1 DivCeil(T1 a, T2 b) {
  return (a + b - 1) / b;
}

// Works for any `align`; if a power of two, compiler emits ADD+AND.
constexpr inline size_t RoundUpTo(size_t what, size_t align) {
  return DivCeil(what, align) * align;
}

constexpr double kPi = 3.14159265358979323846264338327950288;

// Reasonable default for sRGB, matches common monitors. We map white to this
// many nits (cd/m^2) by default. Butteraugli was tuned for 250 nits, which is
// very close.
static constexpr float kDefaultIntensityTarget = 255;

template <typename T>
constexpr T Pi(T multiplier) {
  return static_cast<T>(multiplier * kPi);
}

// Block is the square grid of pixels to which an "energy compaction"
// transformation (e.g. DCT) is applied. Each block has its own AC quantizer.
constexpr size_t kBlockDim = 8;

constexpr size_t kDCTBlockSize = kBlockDim * kBlockDim;

// Group is the rectangular grid of blocks that can be decoded in parallel. This
// is different for DC.
// TODO(jon) : signal kDcGroupDimInBlocks and kGroupDim (and make them
// variables),
//             allowing powers of two between (say) 64 and 1024
constexpr size_t kDcGroupDimInBlocks = 256;
constexpr size_t kDcGroupDim = kDcGroupDimInBlocks * kBlockDim;
// 512x512 DC = 4096x4096, enough for a 4K frame (3840x2160)
// (setting it to 256 results in four DC groups of size 256x256, 224x256,
// 256x14, 224x14)
constexpr size_t kGroupDim = 256;
static_assert(kGroupDim % kBlockDim == 0,
              "Group dim should be divisible by block dim");
constexpr size_t kGroupDimInBlocks = kGroupDim / kBlockDim;

// Maximum number of passes in an image.
constexpr size_t kMaxNumPasses = 11;

// Maximum number of reference frames.
constexpr size_t kMaxNumReferenceFrames = 3;

// Dimensions of a frame, in pixels, and other derived dimensions.
// Computed from FrameHeader.
// TODO(veluca): add extra channels.
struct FrameDimensions {
  void Set(size_t xsize, size_t ysize, size_t group_size_shift,
           size_t max_hshift, size_t max_vshift, bool modular_mode,
           size_t upsampling) {
    group_dim = (kGroupDim >> 1) << group_size_shift;
    static_assert(
        kGroupDim == kDcGroupDimInBlocks,
        "DC groups (in blocks) and groups (in pixels) have different size");
    xsize_upsampled = xsize;
    ysize_upsampled = ysize;
    this->xsize = DivCeil(xsize, upsampling);
    this->ysize = DivCeil(ysize, upsampling);
    xsize_blocks = DivCeil(this->xsize, kBlockDim << max_hshift) << max_hshift;
    ysize_blocks = DivCeil(this->ysize, kBlockDim << max_vshift) << max_vshift;
    xsize_padded = xsize_blocks * kBlockDim;
    ysize_padded = ysize_blocks * kBlockDim;
    if (modular_mode) {
      // Modular mode doesn't have any padding.
      xsize_padded = this->xsize;
      ysize_padded = this->ysize;
    }
    xsize_upsampled_padded = xsize_padded * upsampling;
    ysize_upsampled_padded = ysize_padded * upsampling;
    xsize_groups = DivCeil(this->xsize, group_dim);
    ysize_groups = DivCeil(this->ysize, group_dim);
    xsize_dc_groups = DivCeil(xsize_blocks, group_dim);
    ysize_dc_groups = DivCeil(ysize_blocks, group_dim);
    num_groups = xsize_groups * ysize_groups;
    num_dc_groups = xsize_dc_groups * ysize_dc_groups;
  }

  // Image size without any upsampling, i.e. original_size / upsampling.
  size_t xsize;
  size_t ysize;
  // Original image size.
  size_t xsize_upsampled;
  size_t ysize_upsampled;
  // Image size after upsampling the padded image.
  size_t xsize_upsampled_padded;
  size_t ysize_upsampled_padded;
  // Image size after padding to a multiple of kBlockDim (if VarDCT mode).
  size_t xsize_padded;
  size_t ysize_padded;
  // Image size in kBlockDim blocks.
  size_t xsize_blocks;
  size_t ysize_blocks;
  // Image size in number of groups.
  size_t xsize_groups;
  size_t ysize_groups;
  // Image size in number of DC groups.
  size_t xsize_dc_groups;
  size_t ysize_dc_groups;
  // Number of AC or DC groups.
  size_t num_groups;
  size_t num_dc_groups;
  // Size of a group.
  size_t group_dim;
};

// Prior to C++14 (i.e. C++11): provide our own make_unique
#if __cplusplus < 201402L
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

template <typename T>
JXL_INLINE T Clamp1(T val, T low, T hi) {
  return val < low ? low : val > hi ? hi : val;
}

template <typename T>
JXL_INLINE T ClampToRange(int64_t val) {
  return Clamp1<int64_t>(val, std::numeric_limits<T>::min(),
                         std::numeric_limits<T>::max());
}

template <typename T>
JXL_INLINE T SaturatingMul(int64_t a, int64_t b) {
  return ClampToRange<T>(a * b);
}

template <typename T>
JXL_INLINE T SaturatingAdd(int64_t a, int64_t b) {
  return ClampToRange<T>(a + b);
}

// Encodes non-negative (X) into (2 * X), negative (-X) into (2 * X - 1)
constexpr uint32_t PackSigned(int32_t value) {
  return (static_cast<uint32_t>(value) << 1) ^
         ((static_cast<uint32_t>(~value) >> 31) - 1);
}

// Reverse to PackSigned, i.e. UnpackSigned(PackSigned(X)) == X.
constexpr intptr_t UnpackSigned(size_t value) {
  return static_cast<intptr_t>((value >> 1) ^ (((~value) & 1) - 1));
}

// conversion from integer to string.
template <typename T>
std::string ToString(T n) {
  char data[32] = {};
  if (T(0.1) != T(0)) {
    // float
    snprintf(data, sizeof(data), "%g", static_cast<double>(n));
  } else if (T(-1) > T(0)) {
    // unsigned
    snprintf(data, sizeof(data), "%llu", static_cast<unsigned long long>(n));
  } else {
    // signed
    snprintf(data, sizeof(data), "%lld", static_cast<long long>(n));
  }
  return data;
}
}  // namespace jxl

#endif  // LIB_JXL_COMMON_H_
