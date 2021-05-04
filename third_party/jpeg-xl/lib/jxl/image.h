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

#ifndef LIB_JXL_IMAGE_H_
#define LIB_JXL_IMAGE_H_

// SIMD/multicore-friendly planar image representation with row accessors.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <utility>  // std::move

#include "lib/jxl/base/cache_aligned.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

namespace jxl {

// Type-independent parts of Plane<> - reduces code duplication and facilitates
// moving member function implementations to cc file.
struct PlaneBase {
  PlaneBase()
      : xsize_(0),
        ysize_(0),
        orig_xsize_(0),
        orig_ysize_(0),
        bytes_per_row_(0),
        bytes_(nullptr) {}
  PlaneBase(size_t xsize, size_t ysize, size_t sizeof_t);

  // Copy construction/assignment is forbidden to avoid inadvertent copies,
  // which can be very expensive. Use CopyImageTo() instead.
  PlaneBase(const PlaneBase& other) = delete;
  PlaneBase& operator=(const PlaneBase& other) = delete;

  // Move constructor (required for returning Image from function)
  PlaneBase(PlaneBase&& other) noexcept = default;

  // Move assignment (required for std::vector)
  PlaneBase& operator=(PlaneBase&& other) noexcept = default;

  void Swap(PlaneBase& other);

  // Useful for pre-allocating image with some padding for alignment purposes
  // and later reporting the actual valid dimensions. May also be used to
  // un-shrink the image. Caller is responsible for ensuring xsize/ysize are <=
  // the original dimensions.
  void ShrinkTo(const size_t xsize, const size_t ysize) {
    JXL_CHECK(xsize <= orig_xsize_);
    JXL_CHECK(ysize <= orig_ysize_);
    xsize_ = static_cast<uint32_t>(xsize);
    ysize_ = static_cast<uint32_t>(ysize);
    // NOTE: we can't recompute bytes_per_row for more compact storage and
    // better locality because that would invalidate the image contents.
  }

  // How many pixels.
  JXL_INLINE size_t xsize() const { return xsize_; }
  JXL_INLINE size_t ysize() const { return ysize_; }

  // NOTE: do not use this for copying rows - the valid xsize may be much less.
  JXL_INLINE size_t bytes_per_row() const { return bytes_per_row_; }

  // Raw access to byte contents, for interfacing with other libraries.
  // Unsigned char instead of char to avoid surprises (sign extension).
  JXL_INLINE uint8_t* bytes() {
    void* p = bytes_.get();
    return static_cast<uint8_t * JXL_RESTRICT>(JXL_ASSUME_ALIGNED(p, 64));
  }
  JXL_INLINE const uint8_t* bytes() const {
    const void* p = bytes_.get();
    return static_cast<const uint8_t * JXL_RESTRICT>(JXL_ASSUME_ALIGNED(p, 64));
  }

 protected:
  // Returns pointer to the start of a row.
  JXL_INLINE void* VoidRow(const size_t y) const {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
    if (y >= ysize_) {
      JXL_ABORT("Row(%zu) in (%u x %u) image\n", y, xsize_, ysize_);
    }
#endif

    void* row = bytes_.get() + y * bytes_per_row_;
    return JXL_ASSUME_ALIGNED(row, 64);
  }

  enum class Padding {
    // Allow Load(d, row + x) for x = 0; x < xsize(); x += Lanes(d). Default.
    kRoundUp,
    // Allow LoadU(d, row + x) for x = xsize() - 1. This requires an extra
    // vector to be initialized. If done by default, this would suppress
    // legitimate msan warnings. We therefore require users to explicitly call
    // InitializePadding before using unaligned loads (e.g. convolution).
    kUnaligned
  };

  // Initializes the minimum bytes required to suppress msan warnings from
  // legitimate (according to Padding mode) vector loads/stores on the right
  // border, where some lanes are uninitialized and assumed to be unused.
  void InitializePadding(size_t sizeof_t, Padding padding);

  // (Members are non-const to enable assignment during move-assignment.)
  uint32_t xsize_;  // In valid pixels, not including any padding.
  uint32_t ysize_;
  uint32_t orig_xsize_;
  uint32_t orig_ysize_;
  size_t bytes_per_row_;  // Includes padding.
  CacheAlignedUniquePtr bytes_;
};

// Single channel, aligned rows separated by padding. T must be POD.
//
// 'Single channel' (one 2D array per channel) simplifies vectorization
// (repeating the same operation on multiple adjacent components) without the
// complexity of a hybrid layout (8 R, 8 G, 8 B, ...). In particular, clients
// can easily iterate over all components in a row and Image requires no
// knowledge of the pixel format beyond the component type "T".
//
// 'Aligned' means each row is aligned to the L1 cache line size. This prevents
// false sharing between two threads operating on adjacent rows.
//
// 'Padding' is still relevant because vectors could potentially be larger than
// a cache line. By rounding up row sizes to the vector size, we allow
// reading/writing ALIGNED vectors whose first lane is a valid sample. This
// avoids needing a separate loop to handle remaining unaligned lanes.
//
// This image layout could also be achieved with a vector and a row accessor
// function, but a class wrapper with support for "deleter" allows wrapping
// existing memory allocated by clients without copying the pixels. It also
// provides convenient accessors for xsize/ysize, which shortens function
// argument lists. Supports move-construction so it can be stored in containers.
template <typename ComponentType>
class Plane : public PlaneBase {
 public:
  using T = ComponentType;
  static constexpr size_t kNumPlanes = 1;

  Plane() = default;
  Plane(const size_t xsize, const size_t ysize)
      : PlaneBase(xsize, ysize, sizeof(T)) {}

  void InitializePaddingForUnalignedAccesses() {
    InitializePadding(sizeof(T), Padding::kUnaligned);
  }

  JXL_INLINE T* Row(const size_t y) { return static_cast<T*>(VoidRow(y)); }

  // Returns pointer to const (see above).
  JXL_INLINE const T* Row(const size_t y) const {
    return static_cast<const T*>(VoidRow(y));
  }

  // Documents that the access is const.
  JXL_INLINE const T* ConstRow(const size_t y) const {
    return static_cast<const T*>(VoidRow(y));
  }

  // Returns number of pixels (some of which are padding) per row. Useful for
  // computing other rows via pointer arithmetic. WARNING: this must
  // NOT be used to determine xsize.
  JXL_INLINE intptr_t PixelsPerRow() const {
    return static_cast<intptr_t>(bytes_per_row_ / sizeof(T));
  }
};

using ImageSB = Plane<int8_t>;
using ImageB = Plane<uint8_t>;
using ImageS = Plane<int16_t>;  // signed integer or half-float
using ImageU = Plane<uint16_t>;
using ImageI = Plane<int32_t>;
using ImageF = Plane<float>;
using ImageD = Plane<double>;

// Also works for Image3 and mixed argument types.
template <class Image1, class Image2>
bool SameSize(const Image1& image1, const Image2& image2) {
  return image1.xsize() == image2.xsize() && image1.ysize() == image2.ysize();
}

template <typename T>
class Image3;

// Rectangular region in image(s). Factoring this out of Image instead of
// shifting the pointer by x0/y0 allows this to apply to multiple images with
// different resolutions (e.g. color transform and quantization field).
// Can compare using SameSize(rect1, rect2).
class Rect {
 public:
  // Most windows are xsize_max * ysize_max, except those on the borders where
  // begin + size_max > end.
  constexpr Rect(size_t xbegin, size_t ybegin, size_t xsize_max,
                 size_t ysize_max, size_t xend, size_t yend)
      : x0_(xbegin),
        y0_(ybegin),
        xsize_(ClampedSize(xbegin, xsize_max, xend)),
        ysize_(ClampedSize(ybegin, ysize_max, yend)) {}

  // Construct with origin and known size (typically from another Rect).
  constexpr Rect(size_t xbegin, size_t ybegin, size_t xsize, size_t ysize)
      : x0_(xbegin), y0_(ybegin), xsize_(xsize), ysize_(ysize) {}

  // Construct a rect that covers a whole image/plane/ImageBundle etc.
  template <typename Image>
  explicit Rect(const Image& image)
      : Rect(0, 0, image.xsize(), image.ysize()) {}

  Rect() : Rect(0, 0, 0, 0) {}

  Rect(const Rect&) = default;
  Rect& operator=(const Rect&) = default;

  // Construct a subrect that resides in an image/plane/ImageBundle etc.
  template <typename Image>
  Rect Crop(const Image& image) const {
    return Rect(x0_, y0_, xsize_, ysize_, image.xsize(), image.ysize());
  }

  // Returns a rect that only contains `num` lines with offset `y` from `y0()`.
  Rect Lines(size_t y, size_t num) const {
    JXL_DASSERT(y + num <= ysize_);
    return Rect(x0_, y0_ + y, xsize_, num);
  }

  Rect Line(size_t y) const { return Lines(y, 1); }

  JXL_MUST_USE_RESULT Rect Intersection(const Rect& other) const {
    return Rect(std::max(x0_, other.x0_), std::max(y0_, other.y0_), xsize_,
                ysize_, std::min(x0_ + xsize_, other.x0_ + other.xsize_),
                std::min(y0_ + ysize_, other.y0_ + other.ysize_));
  }

  template <typename T>
  T* Row(Plane<T>* image, size_t y) const {
    return image->Row(y + y0_) + x0_;
  }

  template <typename T>
  const T* Row(const Plane<T>* image, size_t y) const {
    return image->Row(y + y0_) + x0_;
  }

  template <typename T>
  T* PlaneRow(Image3<T>* image, const size_t c, size_t y) const {
    return image->PlaneRow(c, y + y0_) + x0_;
  }

  template <typename T>
  const T* ConstRow(const Plane<T>& image, size_t y) const {
    return image.ConstRow(y + y0_) + x0_;
  }

  template <typename T>
  const T* ConstPlaneRow(const Image3<T>& image, size_t c, size_t y) const {
    return image.ConstPlaneRow(c, y + y0_) + x0_;
  }

  // Returns true if this Rect fully resides in the given image. ImageT could be
  // Plane<T> or Image3<T>; however if ImageT is Rect, results are nonsensical.
  template <class ImageT>
  bool IsInside(const ImageT& image) const {
    return (x0_ + xsize_ <= image.xsize()) && (y0_ + ysize_ <= image.ysize());
  }

  size_t x0() const { return x0_; }
  size_t y0() const { return y0_; }
  size_t xsize() const { return xsize_; }
  size_t ysize() const { return ysize_; }

 private:
  // Returns size_max, or whatever is left in [begin, end).
  static constexpr size_t ClampedSize(size_t begin, size_t size_max,
                                      size_t end) {
    return (begin + size_max <= end) ? size_max
                                     : (end > begin ? end - begin : 0);
  }

  size_t x0_;
  size_t y0_;

  size_t xsize_;
  size_t ysize_;
};

// Currently, we abuse Image to either refer to an image that owns its storage
// or one that doesn't. In similar vein, we abuse Image* function parameters to
// either mean "assign to me" or "fill the provided image with data".
// Hopefully, the "assign to me" meaning will go away and most images in the
// codebase will not be backed by own storage. When this happens we can redesign
// Image to be a non-storage-holding view class and introduce BackedImage in
// those places that actually need it.

// NOTE: we can't use Image as a view because invariants are violated
// (alignment and the presence of padding before/after each "row").

// A bundle of 3 same-sized images. Typically constructed by moving from three
// rvalue references to Image. To overwrite an existing Image3 using
// single-channel producers, we also need access to Image*. Constructing
// temporary non-owning Image pointing to one plane of an existing Image3 risks
// dangling references, especially if the wrapper is moved. Therefore, we
// store an array of Image (which are compact enough that size is not a concern)
// and provide Plane+Row accessors.
template <typename ComponentType>
class Image3 {
 public:
  using T = ComponentType;
  using PlaneT = jxl::Plane<T>;
  static constexpr size_t kNumPlanes = 3;

  Image3() : planes_{PlaneT(), PlaneT(), PlaneT()} {}

  Image3(const size_t xsize, const size_t ysize)
      : planes_{PlaneT(xsize, ysize), PlaneT(xsize, ysize),
                PlaneT(xsize, ysize)} {}

  Image3(Image3&& other) noexcept {
    for (size_t i = 0; i < kNumPlanes; i++) {
      planes_[i] = std::move(other.planes_[i]);
    }
  }

  Image3(PlaneT&& plane0, PlaneT&& plane1, PlaneT&& plane2) {
    JXL_CHECK(SameSize(plane0, plane1));
    JXL_CHECK(SameSize(plane0, plane2));
    planes_[0] = std::move(plane0);
    planes_[1] = std::move(plane1);
    planes_[2] = std::move(plane2);
  }

  // Copy construction/assignment is forbidden to avoid inadvertent copies,
  // which can be very expensive. Use CopyImageTo instead.
  Image3(const Image3& other) = delete;
  Image3& operator=(const Image3& other) = delete;

  Image3& operator=(Image3&& other) noexcept {
    for (size_t i = 0; i < kNumPlanes; i++) {
      planes_[i] = std::move(other.planes_[i]);
    }
    return *this;
  }

  // Returns row pointer; usage: PlaneRow(idx_plane, y)[x] = val.
  JXL_INLINE T* PlaneRow(const size_t c, const size_t y) {
    // Custom implementation instead of calling planes_[c].Row ensures only a
    // single multiplication is needed for PlaneRow(0..2, y).
    PlaneRowBoundsCheck(c, y);
    const size_t row_offset = y * planes_[0].bytes_per_row();
    void* row = planes_[c].bytes() + row_offset;
    return static_cast<T * JXL_RESTRICT>(JXL_ASSUME_ALIGNED(row, 64));
  }

  // Returns const row pointer; usage: val = PlaneRow(idx_plane, y)[x].
  JXL_INLINE const T* PlaneRow(const size_t c, const size_t y) const {
    PlaneRowBoundsCheck(c, y);
    const size_t row_offset = y * planes_[0].bytes_per_row();
    const void* row = planes_[c].bytes() + row_offset;
    return static_cast<const T * JXL_RESTRICT>(JXL_ASSUME_ALIGNED(row, 64));
  }

  // Returns const row pointer, even if called from a non-const Image3.
  JXL_INLINE const T* ConstPlaneRow(const size_t c, const size_t y) const {
    PlaneRowBoundsCheck(c, y);
    return PlaneRow(c, y);
  }

  JXL_INLINE const PlaneT& Plane(size_t idx) const { return planes_[idx]; }

  JXL_INLINE PlaneT& Plane(size_t idx) { return planes_[idx]; }

  void Swap(Image3& other) {
    for (size_t c = 0; c < 3; ++c) {
      other.planes_[c].Swap(planes_[c]);
    }
  }

  // Useful for pre-allocating image with some padding for alignment purposes
  // and later reporting the actual valid dimensions. May also be used to
  // un-shrink the image. Caller is responsible for ensuring xsize/ysize are <=
  // the original dimensions.
  void ShrinkTo(const size_t xsize, const size_t ysize) {
    for (PlaneT& plane : planes_) {
      plane.ShrinkTo(xsize, ysize);
    }
  }

  // Sizes of all three images are guaranteed to be equal.
  JXL_INLINE size_t xsize() const { return planes_[0].xsize(); }
  JXL_INLINE size_t ysize() const { return planes_[0].ysize(); }
  // Returns offset [bytes] from one row to the next row of the same plane.
  // WARNING: this must NOT be used to determine xsize, nor for copying rows -
  // the valid xsize may be much less.
  JXL_INLINE size_t bytes_per_row() const { return planes_[0].bytes_per_row(); }
  // Returns number of pixels (some of which are padding) per row. Useful for
  // computing other rows via pointer arithmetic. WARNING: this must NOT be used
  // to determine xsize.
  JXL_INLINE intptr_t PixelsPerRow() const { return planes_[0].PixelsPerRow(); }

 private:
  void PlaneRowBoundsCheck(const size_t c, const size_t y) const {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
    if (c >= kNumPlanes || y >= ysize()) {
      JXL_ABORT("PlaneRow(%zu, %zu) in (%zu x %zu) image\n", c, y, xsize(),
                ysize());
    }
#endif
  }

 private:
  PlaneT planes_[kNumPlanes];
};

using Image3B = Image3<uint8_t>;
using Image3S = Image3<int16_t>;
using Image3U = Image3<uint16_t>;
using Image3I = Image3<int32_t>;
using Image3F = Image3<float>;
using Image3D = Image3<double>;

}  // namespace jxl

#endif  // LIB_JXL_IMAGE_H_
