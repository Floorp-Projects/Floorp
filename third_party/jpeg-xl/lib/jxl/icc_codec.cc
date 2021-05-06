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

#include "lib/jxl/icc_codec.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "lib/jxl/aux_out.h"
#include "lib/jxl/aux_out_fwd.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_ans.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/icc_codec_common.h"

namespace jxl {
namespace {

uint64_t DecodeVarInt(const uint8_t* input, size_t inputSize, size_t* pos) {
  size_t i;
  uint64_t ret = 0;
  for (i = 0; *pos + i < inputSize && i < 10; ++i) {
    ret |= uint64_t(input[*pos + i] & 127) << uint64_t(7 * i);
    // If the next-byte flag is not set, stop
    if ((input[*pos + i] & 128) == 0) break;
  }
  // TODO: Return a decoding error if i == 10.
  *pos += i + 1;
  return ret;
}

// Shuffles or interleaves bytes, for example with width 2, turns "ABCDabcd"
// into "AaBbCcDc". Transposes a matrix of ceil(size / width) columns and
// width rows. There are size elements, size may be < width * height, if so the
// last elements of the rightmost column are missing, the missing spots are
// transposed along with the filled spots, and the result has the missing
// elements at the end of the bottom row. The input is the input matrix in
// scanline order but with missing elements skipped (which may occur in multiple
// locations), the output is the result matrix in scanline order (with
// no need to skip missing elements as they are past the end of the data).
void Shuffle(uint8_t* data, size_t size, size_t width) {
  size_t height = (size + width - 1) / width;  // amount of rows of output
  PaddedBytes result(size);
  // i = output index, j input index
  size_t s = 0, j = 0;
  for (size_t i = 0; i < size; i++) {
    result[i] = data[j];
    j += height;
    if (j >= size) j = ++s;
  }

  for (size_t i = 0; i < size; i++) {
    data[i] = result[i];
  }
}

// TODO(eustas): should be 20, or even 18, once DecodeVarInt is improved;
//               currently DecodeVarInt does not signal the errors, and marks
//               11 bytes as used even if only 10 are used (and 9 is enough for
//               63-bit values).
constexpr const size_t kPreambleSize = 22;  // enough for reading 2 VarInts

}  // namespace

// Mimics the beginning of UnpredictICC for quick validity check.
// At least kPreambleSize bytes of data should be valid at invocation time.
Status CheckPreamble(const PaddedBytes& data, size_t enc_size,
                     size_t output_limit) {
  const uint8_t* enc = data.data();
  size_t size = data.size();
  size_t pos = 0;
  uint64_t osize = DecodeVarInt(enc, size, &pos);
  JXL_RETURN_IF_ERROR(CheckIs32Bit(osize));
  if (pos >= size) return JXL_FAILURE("Out of bounds");
  uint64_t csize = DecodeVarInt(enc, size, &pos);
  JXL_RETURN_IF_ERROR(CheckIs32Bit(csize));
  JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, csize, size));
  // We expect that UnpredictICC inflates input, not the other way round.
  if (osize + 65536 < enc_size) return JXL_FAILURE("Malformed ICC");
  if (output_limit && osize > output_limit) {
    return JXL_FAILURE("Decoded ICC is too large");
  }
  return true;
}

// Decodes the result of PredictICC back to a valid ICC profile.
Status UnpredictICC(const uint8_t* enc, size_t size, PaddedBytes* result) {
  if (!result->empty()) return JXL_FAILURE("result must be empty initially");
  size_t pos = 0;
  // TODO(lode): technically speaking we need to check that the entire varint
  // decoding never goes out of bounds, not just the first byte. This requires
  // a DecodeVarInt function that returns an error code. It is safe to use
  // DecodeVarInt with out of bounds values, it silently returns, but the
  // specification requires an error. Idem for all DecodeVarInt below.
  if (pos >= size) return JXL_FAILURE("Out of bounds");
  uint64_t osize = DecodeVarInt(enc, size, &pos);  // Output size
  JXL_RETURN_IF_ERROR(CheckIs32Bit(osize));
  if (pos >= size) return JXL_FAILURE("Out of bounds");
  uint64_t csize = DecodeVarInt(enc, size, &pos);  // Commands size
  // Every command is translated to at least on byte.
  JXL_RETURN_IF_ERROR(CheckIs32Bit(csize));
  size_t cpos = pos;  // pos in commands stream
  JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, csize, size));
  size_t commands_end = cpos + csize;
  pos = commands_end;  // pos in data stream

  // Header
  PaddedBytes header = ICCInitialHeaderPrediction();
  EncodeUint32(0, osize, &header);
  for (size_t i = 0; i <= kICCHeaderSize; i++) {
    if (result->size() == osize) {
      if (cpos != commands_end) return JXL_FAILURE("Not all commands used");
      if (pos != size) return JXL_FAILURE("Not all data used");
      return true;  // Valid end
    }
    if (i == kICCHeaderSize) break;  // Done
    ICCPredictHeader(result->data(), result->size(), header.data(), i);
    if (pos >= size) return JXL_FAILURE("Out of bounds");
    result->push_back(enc[pos++] + header[i]);
  }
  if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");

  // Tag list
  uint64_t numtags = DecodeVarInt(enc, size, &cpos);

  if (numtags != 0) {
    numtags--;
    JXL_RETURN_IF_ERROR(CheckIs32Bit(numtags));
    AppendUint32(numtags, result);
    uint64_t prevtagstart = kICCHeaderSize + numtags * 12;
    uint64_t prevtagsize = 0;
    for (;;) {
      if (result->size() > osize) return JXL_FAILURE("Invalid result size");
      if (cpos > commands_end) return JXL_FAILURE("Out of bounds");
      if (cpos == commands_end) break;  // Valid end
      uint8_t command = enc[cpos++];
      uint8_t tagcode = command & 63;
      Tag tag;
      if (tagcode == 0) {
        break;
      } else if (tagcode == kCommandTagUnknown) {
        JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, 4, size));
        tag = DecodeKeyword(enc, size, pos);
        pos += 4;
      } else if (tagcode == kCommandTagTRC) {
        tag = kRtrcTag;
      } else if (tagcode == kCommandTagXYZ) {
        tag = kRxyzTag;
      } else {
        if (tagcode - kCommandTagStringFirst >= kNumTagStrings) {
          return JXL_FAILURE("Unknown tagcode");
        }
        tag = *kTagStrings[tagcode - kCommandTagStringFirst];
      }
      AppendKeyword(tag, result);

      uint64_t tagstart;
      uint64_t tagsize = prevtagsize;
      if (tag == kRxyzTag || tag == kGxyzTag || tag == kBxyzTag ||
          tag == kKxyzTag || tag == kWtptTag || tag == kBkptTag ||
          tag == kLumiTag) {
        tagsize = 20;
      }

      if (command & kFlagBitOffset) {
        if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");
        tagstart = DecodeVarInt(enc, size, &cpos);
      } else {
        JXL_RETURN_IF_ERROR(CheckIs32Bit(prevtagstart));
        tagstart = prevtagstart + prevtagsize;
      }
      JXL_RETURN_IF_ERROR(CheckIs32Bit(tagstart));
      AppendUint32(tagstart, result);
      if (command & kFlagBitSize) {
        if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");
        tagsize = DecodeVarInt(enc, size, &cpos);
      }
      JXL_RETURN_IF_ERROR(CheckIs32Bit(tagsize));
      AppendUint32(tagsize, result);
      prevtagstart = tagstart;
      prevtagsize = tagsize;

      if (tagcode == kCommandTagTRC) {
        AppendKeyword(kGtrcTag, result);
        AppendUint32(tagstart, result);
        AppendUint32(tagsize, result);
        AppendKeyword(kBtrcTag, result);
        AppendUint32(tagstart, result);
        AppendUint32(tagsize, result);
      }

      if (tagcode == kCommandTagXYZ) {
        JXL_RETURN_IF_ERROR(CheckIs32Bit(tagstart + tagsize * 2));
        AppendKeyword(kGxyzTag, result);
        AppendUint32(tagstart + tagsize, result);
        AppendUint32(tagsize, result);
        AppendKeyword(kBxyzTag, result);
        AppendUint32(tagstart + tagsize * 2, result);
        AppendUint32(tagsize, result);
      }
    }
  }

  // Main Content
  for (;;) {
    if (result->size() > osize) return JXL_FAILURE("Invalid result size");
    if (cpos > commands_end) return JXL_FAILURE("Out of bounds");
    if (cpos == commands_end) break;  // Valid end
    uint8_t command = enc[cpos++];
    if (command == kCommandInsert) {
      if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");
      uint64_t num = DecodeVarInt(enc, size, &cpos);
      JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, num, size));
      for (size_t i = 0; i < num; i++) {
        result->push_back(enc[pos++]);
      }
    } else if (command == kCommandShuffle2 || command == kCommandShuffle4) {
      if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");
      uint64_t num = DecodeVarInt(enc, size, &cpos);
      JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, num, size));
      PaddedBytes shuffled(num);
      for (size_t i = 0; i < num; i++) {
        shuffled[i] = enc[pos + i];
      }
      if (command == kCommandShuffle2) {
        Shuffle(shuffled.data(), num, 2);
      } else if (command == kCommandShuffle4) {
        Shuffle(shuffled.data(), num, 4);
      }
      for (size_t i = 0; i < num; i++) {
        result->push_back(shuffled[i]);
        pos++;
      }
    } else if (command == kCommandPredict) {
      JXL_RETURN_IF_ERROR(CheckOutOfBounds(cpos, 2, commands_end));
      uint8_t flags = enc[cpos++];

      size_t width = (flags & 3) + 1;
      if (width == 3) return JXL_FAILURE("Invalid width");

      int order = (flags & 12) >> 2;
      if (order == 3) return JXL_FAILURE("Invalid order");

      uint64_t stride = width;
      if (flags & 16) {
        if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");
        stride = DecodeVarInt(enc, size, &cpos);
        if (stride < width) {
          return JXL_FAILURE("Invalid stride");
        }
      }
      // If stride * 4 >= result->size(), return failure. The check
      // "size == 0 || ((size - 1) >> 2) < stride" corresponds to
      // "stride * 4 >= size", but does not suffer from integer overflow.
      // This check is more strict than necessary but follows the specification
      // and the encoder should ensure this is followed.
      if (result->empty() || ((result->size() - 1u) >> 2u) < stride) {
        return JXL_FAILURE("Invalid stride");
      }

      if (cpos >= commands_end) return JXL_FAILURE("Out of bounds");
      uint64_t num = DecodeVarInt(enc, size, &cpos);  // in bytes
      JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, num, size));

      PaddedBytes shuffled(num);
      for (size_t i = 0; i < num; i++) {
        shuffled[i] = enc[pos + i];
      }
      if (width > 1) Shuffle(shuffled.data(), num, width);

      size_t start = result->size();
      for (size_t i = 0; i < num; i++) {
        uint8_t predicted = LinearPredictICCValue(result->data(), start, i,
                                                  stride, width, order);
        result->push_back(predicted + shuffled[i]);
      }
      pos += num;
    } else if (command == kCommandXYZ) {
      AppendKeyword(kXyz_Tag, result);
      for (int i = 0; i < 4; i++) result->push_back(0);
      JXL_RETURN_IF_ERROR(CheckOutOfBounds(pos, 12, size));
      for (size_t i = 0; i < 12; i++) {
        result->push_back(enc[pos++]);
      }
    } else if (command >= kCommandTypeStartFirst &&
               command < kCommandTypeStartFirst + kNumTypeStrings) {
      AppendKeyword(*kTypeStrings[command - kCommandTypeStartFirst], result);
      for (size_t i = 0; i < 4; i++) {
        result->push_back(0);
      }
    } else {
      return JXL_FAILURE("Unknown command");
    }
  }

  if (pos != size) return JXL_FAILURE("Not all data used");
  if (result->size() != osize) return JXL_FAILURE("Invalid result size");

  return true;
}

Status ReadICC(BitReader* JXL_RESTRICT reader, PaddedBytes* JXL_RESTRICT icc,
               size_t output_limit) {
  icc->clear();
  const auto checkEndOfInput = [&]() -> Status {
    if (reader->AllReadsWithinBounds()) return true;
    return JXL_STATUS(StatusCode::kNotEnoughBytes,
                      "Not enough bytes for reading ICC profile");
  };
  JXL_RETURN_IF_ERROR(checkEndOfInput());
  uint64_t enc_size = U64Coder::Read(reader);
  if (enc_size > 268435456) {
    // Avoid too large memory allocation for invalid file.
    // TODO(lode): a more accurate limit would be the filesize of the JXL file,
    // if we can have it available here.
    return JXL_FAILURE("Too large encoded profile");
  }
  PaddedBytes decompressed;
  std::vector<uint8_t> context_map;
  ANSCode code;
  JXL_RETURN_IF_ERROR(
      DecodeHistograms(reader, kNumICCContexts, &code, &context_map));
  ANSSymbolReader ans_reader(&code, reader);
  size_t used_bits_base = reader->TotalBitsConsumed();
  size_t i = 0;
  decompressed.resize(std::min<size_t>(i + 0x400, enc_size));

  for (; i < std::min<size_t>(2, enc_size); i++) {
    decompressed[i] = ans_reader.ReadHybridUint(
        ICCANSContext(i, i > 0 ? decompressed[i - 1] : 0,
                      i > 1 ? decompressed[i - 2] : 0),
        reader, context_map);
  }
  if (enc_size > kPreambleSize) {
    for (; i < kPreambleSize; i++) {
      decompressed[i] = ans_reader.ReadHybridUint(
          ICCANSContext(i, decompressed[i - 1], decompressed[i - 2]), reader,
          context_map);
    }
    JXL_RETURN_IF_ERROR(checkEndOfInput());
    JXL_RETURN_IF_ERROR(CheckPreamble(decompressed, enc_size, output_limit));
  }
  for (; i < enc_size; i++) {
    if ((i & 0x3FF) == 0) {
      JXL_RETURN_IF_ERROR(checkEndOfInput());
      if ((i > 0) && (((i & 0xFFFF) == 0))) {
        float used_bytes =
            (reader->TotalBitsConsumed() - used_bits_base) / 8.0f;
        if (i > used_bytes * 256) return JXL_FAILURE("Corrupted stream");
      }
      decompressed.resize(std::min<size_t>(i + 0x400, enc_size));
    }
    JXL_DASSERT(i >= 2);
    decompressed[i] = ans_reader.ReadHybridUint(
        ICCANSContext(i, decompressed[i - 1], decompressed[i - 2]), reader,
        context_map);
  }
  JXL_RETURN_IF_ERROR(checkEndOfInput());
  if (!ans_reader.CheckANSFinalState()) {
    return JXL_FAILURE("Corrupted ICC profile");
  }

  JXL_RETURN_IF_ERROR(
      UnpredictICC(decompressed.data(), decompressed.size(), icc));
  return true;
}

}  // namespace jxl
