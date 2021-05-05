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

#include "lib/jxl/enc_toc.h"

#include <stdint.h>

#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/coeff_order.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_coeff_order.h"
#include "lib/jxl/field_encodings.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/toc.h"

namespace jxl {
Status WriteGroupOffsets(const std::vector<BitWriter>& group_codes,
                         const std::vector<coeff_order_t>* permutation,
                         BitWriter* JXL_RESTRICT writer, AuxOut* aux_out) {
  BitWriter::Allotment allotment(writer, MaxBits(group_codes.size()));
  if (permutation && !group_codes.empty()) {
    // Don't write a permutation at all for an empty group_codes.
    writer->Write(1, 1);  // permutation
    JXL_DASSERT(permutation->size() == group_codes.size());
    EncodePermutation(permutation->data(), /*skip=*/0, permutation->size(),
                      writer, /* layer= */ 0, aux_out);

  } else {
    writer->Write(1, 0);  // no permutation
  }
  writer->ZeroPadToByte();  // before TOC entries

  for (size_t i = 0; i < group_codes.size(); i++) {
    JXL_ASSERT(group_codes[i].BitsWritten() % kBitsPerByte == 0);
    const size_t group_size = group_codes[i].BitsWritten() / kBitsPerByte;
    JXL_RETURN_IF_ERROR(U32Coder::Write(kTocDist, group_size, writer));
  }
  writer->ZeroPadToByte();  // before first group
  ReclaimAndCharge(writer, &allotment, kLayerTOC, aux_out);
  return true;
}

}  // namespace jxl
