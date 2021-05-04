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

#ifndef LIB_JXL_ENC_BUTTERAUGLI_COMPARATOR_H_
#define LIB_JXL_ENC_BUTTERAUGLI_COMPARATOR_H_

#include <stddef.h>

#include <memory>

#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/enc_comparator.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"

namespace jxl {

class JxlButteraugliComparator : public Comparator {
 public:
  explicit JxlButteraugliComparator(const ButteraugliParams& params);

  Status SetReferenceImage(const ImageBundle& ref) override;

  Status CompareWith(const ImageBundle& actual, ImageF* diffmap,
                     float* score) override;

  float GoodQualityScore() const override;
  float BadQualityScore() const override;

 private:
  ButteraugliParams params_;
  std::unique_ptr<ButteraugliComparator> comparator_;
  size_t xsize_ = 0;
  size_t ysize_ = 0;
};

// Returns the butteraugli distance between rgb0 and rgb1.
// If distmap is not null, it must be the same size as rgb0 and rgb1.
float ButteraugliDistance(const ImageBundle& rgb0, const ImageBundle& rgb1,
                          const ButteraugliParams& params,
                          ImageF* distmap = nullptr,
                          ThreadPool* pool = nullptr);

float ButteraugliDistance(const CodecInOut& rgb0, const CodecInOut& rgb1,
                          const ButteraugliParams& params,
                          ImageF* distmap = nullptr,
                          ThreadPool* pool = nullptr);

}  // namespace jxl

#endif  // LIB_JXL_ENC_BUTTERAUGLI_COMPARATOR_H_
