/* Copyright (c) the JPEG XL Project Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef LIB_JXL_ENCODE_INTERNAL_H_
#define LIB_JXL_ENCODE_INTERNAL_H_

#include <vector>

#include "jxl/encode.h"
#include "jxl/memory_manager.h"
#include "jxl/parallel_runner.h"
#include "jxl/types.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/memory_manager_internal.h"

namespace jxl {

// The encoder options (such as quality, compression speed, ...) for a single
// frame, but not encoder-wide options such as box-related options.
typedef struct JxlEncoderFrameSettingsValuesStruct {
  // lossless is a separate setting from cparams because it is a combination
  // setting that overrides multiple settings inside of cparams.
  bool lossless;
  CompressParams cparams;
  JxlFrameHeader header;
  std::vector<JxlBlendInfo> extra_channel_blend_info;
  std::string frame_name;
} JxlEncoderFrameSettingsValues;

typedef std::array<uint8_t, 4> BoxType;

// Utility function that makes a BoxType from a string literal. The string must
// have 4 characters, a 5th null termination character is optional.
constexpr BoxType MakeBoxType(const char* type) {
  return BoxType(
      {{static_cast<uint8_t>(type[0]), static_cast<uint8_t>(type[1]),
        static_cast<uint8_t>(type[2]), static_cast<uint8_t>(type[3])}});
}

constexpr unsigned char kContainerHeader[] = {
    0,   0,   0, 0xc, 'J',  'X', 'L', ' ', 0xd, 0xa, 0x87,
    0xa, 0,   0, 0,   0x14, 'f', 't', 'y', 'p', 'j', 'x',
    'l', ' ', 0, 0,   0,    0,   'j', 'x', 'l', ' '};

constexpr unsigned char kLevelBoxHeader[] = {0, 0, 0, 0x9, 'j', 'x', 'l', 'l'};

struct JxlEncoderQueuedFrame {
  JxlEncoderFrameSettingsValues option_values;
  ImageBundle frame;
};

struct JxlEncoderQueuedBox {
  BoxType type;
  std::vector<uint8_t> contents;
  bool compress_box;
};

// Either a frame, or a box, not both.
struct JxlEncoderQueuedInput {
  JxlEncoderQueuedInput(const JxlMemoryManager& memory_manager)
      : frame(nullptr, jxl::MemoryManagerDeleteHelper(&memory_manager)),
        box(nullptr, jxl::MemoryManagerDeleteHelper(&memory_manager)) {}
  MemoryManagerUniquePtr<JxlEncoderQueuedFrame> frame;
  MemoryManagerUniquePtr<JxlEncoderQueuedBox> box;
};

namespace {
template <typename T>
uint8_t* Extend(T* vec, size_t size) {
  vec->resize(vec->size() + size, 0);
  return vec->data() + vec->size() - size;
}
}  // namespace

// Appends a JXL container box header with given type, size, and unbounded
// properties to output.
template <typename T>
void AppendBoxHeader(const jxl::BoxType& type, size_t size, bool unbounded,
                     T* output) {
  uint64_t box_size = 0;
  bool large_size = false;
  if (!unbounded) {
    box_size = size + 8;
    if (box_size >= 0x100000000ull) {
      large_size = true;
    }
  }

  StoreBE32(large_size ? 1 : box_size, Extend(output, 4));

  for (size_t i = 0; i < 4; i++) {
    output->push_back(*(type.data() + i));
  }

  if (large_size) {
    StoreBE64(box_size, Extend(output, 8));
  }
}

}  // namespace jxl

// Internal use only struct, can only be initialized correctly by
// JxlEncoderCreate.
struct JxlEncoderStruct {
  JxlMemoryManager memory_manager;
  jxl::MemoryManagerUniquePtr<jxl::ThreadPool> thread_pool{
      nullptr, jxl::MemoryManagerDeleteHelper(&memory_manager)};
  JxlCmsInterface cms;
  std::vector<jxl::MemoryManagerUniquePtr<JxlEncoderFrameSettings>>
      encoder_options;

  size_t num_queued_frames;
  size_t num_queued_boxes;
  std::vector<jxl::JxlEncoderQueuedInput> input_queue;
  std::vector<uint8_t> output_byte_queue;

  // Force using the container even if not needed
  bool use_container;
  // User declared they will add metadata boxes
  bool use_boxes;

  // TODO(lode): move level into jxl::CompressParams since some C++
  // implementation decisions should be based on it: level 10 allows more
  // features to be used.
  uint32_t codestream_level;
  bool store_jpeg_metadata;
  jxl::CodecMetadata metadata;
  std::vector<uint8_t> jpeg_metadata;

  // Wrote any output at all, so wrote the data before the first user added
  // frame or box, such as signature, basic info, ICC profile or jpeg
  // reconstruction box.
  bool wrote_bytes;
  jxl::CompressParams last_used_cparams;

  // Encoder wrote a jxlp (partial codestream) box, so any next codestream
  // parts must also be written in jxlp boxes, a single jxlc box cannot be
  // used. The counter is used for the 4-byte jxlp box index header.
  size_t jxlp_counter;

  bool frames_closed;
  bool boxes_closed;
  bool basic_info_set;
  bool color_encoding_set;

  // Takes the first frame in the input_queue, encodes it, and appends
  // the bytes to the output_byte_queue.
  JxlEncoderStatus RefillOutputByteQueue();

  bool MustUseContainer() const {
    return use_container || codestream_level != 5 || store_jpeg_metadata ||
           use_boxes;
  }

  // Appends the bytes of a JXL box header with the provided type and size to
  // the end of the output_byte_queue. If unbounded is true, the size won't be
  // added to the header and the box will be assumed to continue until EOF.
  void AppendBoxHeader(const jxl::BoxType& type, size_t size, bool unbounded);
};

struct JxlEncoderFrameSettingsStruct {
  JxlEncoder* enc;
  jxl::JxlEncoderFrameSettingsValues values;
};

#endif  // LIB_JXL_ENCODE_INTERNAL_H_
