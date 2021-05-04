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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <atomic>

#include "jxl/butteraugli.h"
#include "jxl/parallel_runner.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/butteraugli/butteraugli.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_butteraugli_pnorm.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/memory_manager_internal.h"

namespace {

void SetMetadataFromPixelFormat(const JxlPixelFormat* pixel_format,
                                jxl::ImageMetadata* metadata) {
  uint32_t potential_alpha_bits = 0;
  switch (pixel_format->data_type) {
    case JXL_TYPE_FLOAT:
      metadata->SetFloat32Samples();
      potential_alpha_bits = 16;
      break;
    case JXL_TYPE_FLOAT16:
      metadata->SetFloat16Samples();
      potential_alpha_bits = 16;
      break;
    case JXL_TYPE_UINT32:
      metadata->SetUintSamples(32);
      potential_alpha_bits = 16;
      break;
    case JXL_TYPE_UINT16:
      metadata->SetUintSamples(16);
      potential_alpha_bits = 16;
      break;
    case JXL_TYPE_UINT8:
      metadata->SetUintSamples(8);
      potential_alpha_bits = 8;
      break;
    case JXL_TYPE_BOOLEAN:
      metadata->SetUintSamples(2);
      potential_alpha_bits = 2;
      break;
  }
  if (pixel_format->num_channels == 2 || pixel_format->num_channels == 4) {
    metadata->SetAlphaBits(potential_alpha_bits);
  }
}

}  // namespace

struct JxlButteraugliResultStruct {
  JxlMemoryManager memory_manager;

  jxl::ImageF distmap;
  jxl::ButteraugliParams params;
};

struct JxlButteraugliApiStruct {
  // Multiplier for penalizing new HF artifacts more than blurring away
  // features. 1.0=neutral.
  float hf_asymmetry = 1.0f;

  // Multiplier for the psychovisual difference in the X channel.
  float xmul = 1.0f;

  // Number of nits that correspond to 1.0f input values.
  float intensity_target = jxl::kDefaultIntensityTarget;

  bool approximate_border = false;

  JxlMemoryManager memory_manager;
  std::unique_ptr<jxl::ThreadPool> thread_pool{nullptr};
};

JxlButteraugliApi* JxlButteraugliApiCreate(
    const JxlMemoryManager* memory_manager) {
  JxlMemoryManager local_memory_manager;
  if (!jxl::MemoryManagerInit(&local_memory_manager, memory_manager))
    return nullptr;

  void* alloc =
      jxl::MemoryManagerAlloc(&local_memory_manager, sizeof(JxlButteraugliApi));
  if (!alloc) return nullptr;
  // Placement new constructor on allocated memory
  JxlButteraugliApi* ret = new (alloc) JxlButteraugliApi();
  ret->memory_manager = local_memory_manager;
  return ret;
}

void JxlButteraugliApiSetParallelRunner(JxlButteraugliApi* api,
                                        JxlParallelRunner parallel_runner,
                                        void* parallel_runner_opaque) {
  api->thread_pool = jxl::make_unique<jxl::ThreadPool>(parallel_runner,
                                                       parallel_runner_opaque);
}

void JxlButteraugliApiSetHFAsymmetry(JxlButteraugliApi* api, float v) {
  api->hf_asymmetry = v;
}

void JxlButteraugliApiSetIntensityTarget(JxlButteraugliApi* api, float v) {
  api->intensity_target = v;
}

void JxlButteraugliApiDestroy(JxlButteraugliApi* api) {
  if (api) {
    // Call destructor directly since custom free function is used.
    api->~JxlButteraugliApi();
    jxl::MemoryManagerFree(&api->memory_manager, api);
  }
}

JxlButteraugliResult* JxlButteraugliCompute(
    const JxlButteraugliApi* api, uint32_t xsize, uint32_t ysize,
    const JxlPixelFormat* pixel_format_orig, const void* buffer_orig,
    size_t size_orig, const JxlPixelFormat* pixel_format_dist,
    const void* buffer_dist, size_t size_dist) {
  jxl::ImageMetadata orig_metadata;
  SetMetadataFromPixelFormat(pixel_format_orig, &orig_metadata);
  jxl::ImageBundle orig_ib(&orig_metadata);
  jxl::ColorEncoding c_current;
  if (pixel_format_orig->data_type == JXL_TYPE_FLOAT) {
    c_current =
        jxl::ColorEncoding::LinearSRGB(pixel_format_orig->num_channels < 3);
  } else {
    c_current = jxl::ColorEncoding::SRGB(pixel_format_orig->num_channels < 3);
  }
  if (!jxl::BufferToImageBundle(*pixel_format_orig, xsize, ysize, buffer_orig,
                                size_orig, api->thread_pool.get(), c_current,
                                &orig_ib)) {
    return nullptr;
  }

  jxl::ImageMetadata dist_metadata;
  SetMetadataFromPixelFormat(pixel_format_dist, &dist_metadata);
  jxl::ImageBundle dist_ib(&dist_metadata);
  if (pixel_format_dist->data_type == JXL_TYPE_FLOAT) {
    c_current =
        jxl::ColorEncoding::LinearSRGB(pixel_format_dist->num_channels < 3);
  } else {
    c_current = jxl::ColorEncoding::SRGB(pixel_format_dist->num_channels < 3);
  }
  if (!jxl::BufferToImageBundle(*pixel_format_dist, xsize, ysize, buffer_dist,
                                size_dist, api->thread_pool.get(), c_current,
                                &dist_ib)) {
    return nullptr;
  }

  void* alloc = jxl::MemoryManagerAlloc(&api->memory_manager,
                                        sizeof(JxlButteraugliResult));
  if (!alloc) return nullptr;
  // Placement new constructor on allocated memory
  JxlButteraugliResult* result = new (alloc) JxlButteraugliResult();
  result->memory_manager = api->memory_manager;
  result->params.hf_asymmetry = api->hf_asymmetry;
  result->params.xmul = api->xmul;
  result->params.intensity_target = api->intensity_target;
  result->params.approximate_border = api->approximate_border;
  jxl::ButteraugliDistance(orig_ib, dist_ib, result->params, &result->distmap,
                           api->thread_pool.get());

  return result;
}

float JxlButteraugliResultGetDistance(const JxlButteraugliResult* result,
                                      float pnorm) {
  return static_cast<float>(
      jxl::ComputeDistanceP(result->distmap, result->params, pnorm));
}

void JxlButteraugliResultGetDistmap(const JxlButteraugliResult* result,
                                    const float** buffer,
                                    uint32_t* row_stride) {
  *buffer = result->distmap.Row(0);
  *row_stride = result->distmap.PixelsPerRow();
}

float JxlButteraugliResultGetMaxDistance(const JxlButteraugliResult* result) {
  float max_distance = 0.0;
  for (uint32_t y = 0; y < result->distmap.ysize(); y++) {
    for (uint32_t x = 0; x < result->distmap.xsize(); x++) {
      if (result->distmap.ConstRow(y)[x] > max_distance) {
        max_distance = result->distmap.ConstRow(y)[x];
      }
    }
  }
  return max_distance;
}

void JxlButteraugliResultDestroy(JxlButteraugliResult* result) {
  if (result) {
    // Call destructor directly since custom free function is used.
    result->~JxlButteraugliResult();
    jxl::MemoryManagerFree(&result->memory_manager, result);
  }
}
