// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/render_pipeline/stage_patches.h"

namespace jxl {
namespace {
class PatchDictionaryStage : public RenderPipelineStage {
 public:
  explicit PatchDictionaryStage(const PatchDictionary* patches)
      : RenderPipelineStage(RenderPipelineStage::Settings()),
        patches_(*patches) {}

  void ProcessRow(const RowInfo& input_rows, const RowInfo& output_rows,
                  size_t xextra, size_t xsize, size_t xpos, size_t ypos,
                  size_t thread_id) const final {
    PROFILER_ZONE("RenderPatches");
    std::vector<float*> row_ptrs(input_rows.size());
    for (size_t i = 0; i < row_ptrs.size(); i++) {
      row_ptrs[i] = GetInputRow(input_rows, i, 0) - xextra;
    }
    patches_.AddOneRow(row_ptrs.data(), ypos, xpos - xextra,
                       xsize + 2 * xextra);
  }

  RenderPipelineChannelMode GetChannelMode(size_t c) const final {
    return RenderPipelineChannelMode::kInPlace;
  }

  const char* GetName() const override { return "Patches"; }

 private:
  const PatchDictionary& patches_;
};
}  // namespace

std::unique_ptr<RenderPipelineStage> GetPatchesStage(
    const PatchDictionary* patches) {
  return jxl::make_unique<PatchDictionaryStage>(patches);
}

}  // namespace jxl
