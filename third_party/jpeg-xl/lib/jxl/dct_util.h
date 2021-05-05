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

#ifndef LIB_JXL_DCT_UTIL_H_
#define LIB_JXL_DCT_UTIL_H_

#include <stddef.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

union ACPtr {
  int32_t* ptr32;
  int16_t* ptr16;
  ACPtr() = default;
  explicit ACPtr(int16_t* p) : ptr16(p) {}
  explicit ACPtr(int32_t* p) : ptr32(p) {}
};

union ConstACPtr {
  const int32_t* ptr32;
  const int16_t* ptr16;
  ConstACPtr() = default;
  explicit ConstACPtr(const int16_t* p) : ptr16(p) {}
  explicit ConstACPtr(const int32_t* p) : ptr32(p) {}
};

enum class ACType { k16 = 0, k32 = 1 };

class ACImage {
 public:
  virtual ~ACImage() = default;
  virtual ACType Type() const = 0;
  virtual ACPtr PlaneRow(size_t c, size_t y, size_t xbase) = 0;
  virtual ConstACPtr PlaneRow(size_t c, size_t y, size_t xbase) const = 0;
  virtual size_t PixelsPerRow() const = 0;
  virtual void ZeroFill() = 0;
  virtual void ZeroFillPlane(size_t c) = 0;
  virtual bool IsEmpty() const = 0;
};

template <typename T>
class ACImageT final : public ACImage {
 public:
  ACImageT() = default;
  ACImageT(size_t xsize, size_t ysize) {
    static_assert(
        std::is_same<T, int16_t>::value || std::is_same<T, int32_t>::value,
        "ACImage must be either 32- or 16- bit");
    img_ = Image3<T>(xsize, ysize);
  }
  ACType Type() const override {
    return sizeof(T) == 2 ? ACType::k16 : ACType::k32;
  }
  ACPtr PlaneRow(size_t c, size_t y, size_t xbase) override {
    return ACPtr(img_.PlaneRow(c, y) + xbase);
  }
  ConstACPtr PlaneRow(size_t c, size_t y, size_t xbase) const override {
    return ConstACPtr(img_.PlaneRow(c, y) + xbase);
  }

  size_t PixelsPerRow() const override { return img_.PixelsPerRow(); }

  void ZeroFill() override { ZeroFillImage(&img_); }

  void ZeroFillPlane(size_t c) override { ZeroFillImage(&img_.Plane(c)); }

  bool IsEmpty() const override {
    return img_.xsize() == 0 || img_.ysize() == 0;
  }

 private:
  Image3<T> img_;
};

}  // namespace jxl

#endif  // LIB_JXL_DCT_UTIL_H_
