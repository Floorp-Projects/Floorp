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

#include "lib/jxl/quantizer.h"

#include <stdio.h>
#include <string.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/robust_statistics.h"
#include "lib/jxl/field_encodings.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/quant_weights.h"

namespace jxl {

static const int kDefaultQuant = 64;

constexpr int Quantizer::kQuantMax;

Quantizer::Quantizer(const DequantMatrices* dequant)
    : Quantizer(dequant, kDefaultQuant, kGlobalScaleDenom / kDefaultQuant) {}

Quantizer::Quantizer(const DequantMatrices* dequant, int quant_dc,
                     int global_scale)
    : global_scale_(global_scale), quant_dc_(quant_dc), dequant_(dequant) {
  JXL_ASSERT(dequant_ != nullptr);
  RecomputeFromGlobalScale();
  inv_quant_dc_ = inv_global_scale_ / quant_dc_;

  memcpy(zero_bias_, kZeroBiasDefault, sizeof(kZeroBiasDefault));
}

void Quantizer::ComputeGlobalScaleAndQuant(float quant_dc, float quant_median,
                                           float quant_median_absd) {
  // Target value for the median value in the quant field.
  const float kQuantFieldTarget = 3.80987740592518214386f;
  // We reduce the median of the quant field by the median absolute deviation:
  // higher resolution on highly varying quant fields.
  float scale = kGlobalScaleDenom * (quant_median - quant_median_absd) /
                kQuantFieldTarget;
  // Ensure that new_global_scale is positive and no more than 1<<15.
  if (scale < 1) scale = 1;
  if (scale > (1 << 15)) scale = 1 << 15;
  int new_global_scale = static_cast<int>(scale);
  // Ensure that quant_dc_ will always be at least
  // kGlobalScaleDenom/kGlobalScaleNumerator.
  const int scaled_quant_dc =
      static_cast<int>(quant_dc * kGlobalScaleNumerator);
  if (new_global_scale > scaled_quant_dc) {
    new_global_scale = scaled_quant_dc;
    if (new_global_scale <= 0) new_global_scale = 1;
  }
  global_scale_ = new_global_scale;
  // Code below uses inv_global_scale_.
  RecomputeFromGlobalScale();

  float fval = quant_dc * inv_global_scale_ + 0.5f;
  fval = std::min<float>(1 << 16, fval);
  const int new_quant_dc = static_cast<int>(fval);
  quant_dc_ = new_quant_dc;

  // quant_dc_ was updated, recompute values.
  RecomputeFromGlobalScale();
}

void Quantizer::SetQuantFieldRect(const ImageF& qf, const Rect& rect,
                                  ImageI* JXL_RESTRICT raw_quant_field) {
  for (size_t y = 0; y < rect.ysize(); ++y) {
    const float* JXL_RESTRICT row_qf = rect.ConstRow(qf, y);
    int32_t* JXL_RESTRICT row_qi = rect.Row(raw_quant_field, y);
    for (size_t x = 0; x < rect.xsize(); ++x) {
      int val = ClampVal(row_qf[x] * inv_global_scale_ + 0.5f);
      row_qi[x] = val;
    }
  }
}

void Quantizer::SetQuantField(const float quant_dc, const ImageF& qf,
                              ImageI* JXL_RESTRICT raw_quant_field) {
  JXL_CHECK(SameSize(*raw_quant_field, qf));
  std::vector<float> data(qf.xsize() * qf.ysize());
  for (size_t y = 0; y < qf.ysize(); ++y) {
    const float* JXL_RESTRICT row_qf = qf.Row(y);
    for (size_t x = 0; x < qf.xsize(); ++x) {
      float quant = row_qf[x];
      data[qf.xsize() * y + x] = quant;
    }
  }
  const float quant_median = Median(&data);
  const float quant_median_absd = MedianAbsoluteDeviation(data, quant_median);
  ComputeGlobalScaleAndQuant(quant_dc, quant_median, quant_median_absd);
  SetQuantFieldRect(qf, Rect(qf), raw_quant_field);
}

void Quantizer::SetQuant(float quant_dc, float quant_ac,
                         ImageI* JXL_RESTRICT raw_quant_field) {
  ComputeGlobalScaleAndQuant(quant_dc, quant_ac, 0);
  int val = ClampVal(quant_ac * inv_global_scale_ + 0.5f);
  FillImage(val, raw_quant_field);
}

Status QuantizerParams::VisitFields(Visitor* JXL_RESTRICT visitor) {
  JXL_QUIET_RETURN_IF_ERROR(visitor->U32(
      BitsOffset(11, 1), BitsOffset(11, 2049), BitsOffset(12, 4097),
      BitsOffset(16, 8193), 1, &global_scale));
  JXL_QUIET_RETURN_IF_ERROR(visitor->U32(Val(16), BitsOffset(5, 1),
                                         BitsOffset(8, 1), BitsOffset(16, 1), 1,
                                         &quant_dc));
  return true;
}

Status Quantizer::Encode(BitWriter* writer, size_t layer,
                         AuxOut* aux_out) const {
  QuantizerParams params;
  params.global_scale = global_scale_;
  params.quant_dc = quant_dc_;
  return Bundle::Write(params, writer, layer, aux_out);
}

Status Quantizer::Decode(BitReader* reader) {
  QuantizerParams params;
  JXL_RETURN_IF_ERROR(Bundle::Read(reader, &params));
  global_scale_ = static_cast<int>(params.global_scale);
  quant_dc_ = static_cast<int>(params.quant_dc);
  RecomputeFromGlobalScale();
  return true;
}

void Quantizer::DumpQuantizationMap(const ImageI& raw_quant_field) const {
  printf("Global scale: %d (%.7f)\nDC quant: %d\n", global_scale_,
         global_scale_ * 1.0 / kGlobalScaleDenom, quant_dc_);
  printf("AC quantization Map:\n");
  for (size_t y = 0; y < raw_quant_field.ysize(); ++y) {
    for (size_t x = 0; x < raw_quant_field.xsize(); ++x) {
      printf(" %3d", raw_quant_field.Row(y)[x]);
    }
    printf("\n");
  }
}

static constexpr JXL_INLINE int QuantizeValue(float value, float inv_step) {
  return static_cast<int>(value * inv_step + (value >= 0 ? .5f : -.5f));
}

}  // namespace jxl
