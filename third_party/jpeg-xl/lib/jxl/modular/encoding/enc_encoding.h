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

#ifndef LIB_JXL_MODULAR_ENCODING_ENC_ENCODING_H_
#define LIB_JXL_MODULAR_ENCODING_ENC_ENCODING_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/enc_ans.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/image.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/encoding/enc_ma.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/options.h"
#include "lib/jxl/modular/transform/transform.h"

namespace jxl {

void PrintTree(const Tree &tree, const std::string &path);
Tree LearnTree(TreeSamples &&tree_samples, size_t total_pixels,
               const ModularOptions &options,
               const std::vector<ModularMultiplierInfo> &multiplier_info = {},
               StaticPropRange static_prop_range = {});

// TODO(veluca): make cleaner interfaces.

Status ModularGenericCompress(
    Image &image, const ModularOptions &opts, BitWriter *writer,
    AuxOut *aux_out = nullptr, size_t layer = 0, size_t group_id = 0,
    // For gathering data for producing a global tree.
    TreeSamples *tree_samples = nullptr, size_t *total_pixels = nullptr,
    // For encoding with global tree.
    const Tree *tree = nullptr, GroupHeader *header = nullptr,
    std::vector<Token> *tokens = nullptr, size_t *widths = nullptr);
}  // namespace jxl

#endif  // LIB_JXL_MODULAR_ENCODING_ENC_ENCODING_H_
