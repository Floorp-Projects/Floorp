// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/gain_map.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/common.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_bit_reader.h"
#include "lib/jxl/enc_bit_writer.h"
#include "lib/jxl/fields.h"
#include "lib/jxl/memory_manager_internal.h"

namespace {

template <size_t N>
class FixedSizeMemoryManager {
 public:
  FixedSizeMemoryManager() = default;

  JxlMemoryManager* memory_manager() { return &manager_; }

 private:
  static void* FixedSizeMemoryManagerAlloc(void* opaque, size_t capacity) {
    auto manager = static_cast<FixedSizeMemoryManager<N>*>(opaque);
    if (capacity > N + jxl::memory_manager_internal::kAlias) {
      return nullptr;
    }
    return manager->memory_;
  }
  static void FixedSizeMemoryManagerFree(void* opaque, void* pointer) {}

  uint8_t memory_[N + jxl::memory_manager_internal::kAlias];
  JxlMemoryManager manager_ = {
      /*opaque=*/this,
      /*alloc=*/&FixedSizeMemoryManagerAlloc,
      /*free=*/&FixedSizeMemoryManagerFree,
  };
};

}  // namespace

JXL_BOOL JxlGainMapGetBundleSize(const JxlGainMapBundle* map_bundle,
                                 size_t* bundle_size) {
  if (map_bundle == nullptr) return JXL_FALSE;

  jxl::ColorEncoding internal_color_encoding;
  size_t color_encoding_size = 0;
  size_t extension_bits = 0;
  if (map_bundle->has_color_encoding) {
    JXL_RETURN_IF_ERROR(
        internal_color_encoding.FromExternal(map_bundle->color_encoding));
    if (!jxl::Bundle::CanEncode(internal_color_encoding, &extension_bits,
                                &color_encoding_size)) {
      return JXL_FALSE;
    }
  }

  *bundle_size =
      1 +                                     // size of jhgm_version
      2 +                                     // size_of gain_map_metadata_size
      map_bundle->gain_map_metadata_size +    // size of gain_map_metadata
      1 +                                     // size of color_encoding_size
      jxl::DivCeil(color_encoding_size, 8) +  // size of the color_encoding
      4 +                                     // size of compressed_icc_size
      map_bundle->alt_icc_size +              // size of compressed_icc
      map_bundle->gain_map_size;              // size of gain map
  return JXL_TRUE;
}

JXL_BOOL JxlGainMapWriteBundle(const JxlGainMapBundle* map_bundle,
                               uint8_t* output_buffer,
                               size_t output_buffer_size,
                               size_t* bytes_written) {
  if (map_bundle == nullptr) return JXL_FALSE;

  uint8_t jhgm_version = map_bundle->jhgm_version;

  FixedSizeMemoryManager<sizeof(jxl::ColorEncoding)> memory_manager;
  jxl::ColorEncoding internal_color_encoding;
  jxl::BitWriter color_encoding_writer(memory_manager.memory_manager());
  if (map_bundle->has_color_encoding) {
    JXL_RETURN_IF_ERROR(
        internal_color_encoding.FromExternal(map_bundle->color_encoding));
    if (!jxl::Bundle::Write(internal_color_encoding, &color_encoding_writer,
                            /*layer=*/0, nullptr)) {
      return JXL_FALSE;
    }
  }

  color_encoding_writer.ZeroPadToByte();

  uint64_t cursor = 0;
  uint64_t next_cursor = 0;

#define SAFE_CURSOR_UPDATE(n)                    \
  do {                                           \
    cursor = next_cursor;                        \
    if (!jxl::SafeAdd(cursor, n, next_cursor) || \
        next_cursor > output_buffer_size) {      \
      return JXL_FALSE;                          \
    }                                            \
  } while (false)

  SAFE_CURSOR_UPDATE(1);
  memcpy(output_buffer + cursor, &jhgm_version, 1);

  SAFE_CURSOR_UPDATE(2);
  StoreBE16(map_bundle->gain_map_metadata_size, output_buffer + cursor);

  SAFE_CURSOR_UPDATE(map_bundle->gain_map_metadata_size);
  memcpy(output_buffer + cursor, map_bundle->gain_map_metadata,
         map_bundle->gain_map_metadata_size);

  uint8_t color_enc_size =
      static_cast<uint8_t>(color_encoding_writer.GetSpan().size());
  SAFE_CURSOR_UPDATE(1);
  memcpy(output_buffer + cursor, &color_enc_size, 1);

  SAFE_CURSOR_UPDATE(color_enc_size);
  memcpy(output_buffer + cursor, color_encoding_writer.GetSpan().data(),
         color_enc_size);

  SAFE_CURSOR_UPDATE(4);
  StoreBE32(map_bundle->alt_icc_size, output_buffer + cursor);

  SAFE_CURSOR_UPDATE(map_bundle->alt_icc_size);
  memcpy(output_buffer + cursor, map_bundle->alt_icc, map_bundle->alt_icc_size);

  SAFE_CURSOR_UPDATE(map_bundle->gain_map_size);
  memcpy(output_buffer + cursor, map_bundle->gain_map,
         map_bundle->gain_map_size);

#undef SAFE_CURSOR_UPDATE

  cursor = next_cursor;

  if (bytes_written != nullptr)
    *bytes_written = cursor;  // Ensure size_t compatibility
  return cursor == output_buffer_size ? JXL_TRUE : JXL_FALSE;
}

JXL_BOOL JxlGainMapReadBundle(JxlGainMapBundle* map_bundle,
                              const uint8_t* input_buffer,
                              const size_t input_buffer_size,
                              size_t* bytes_read) {
  if (map_bundle == nullptr || input_buffer == nullptr ||
      input_buffer_size == 0) {
    return JXL_FALSE;
  }

  uint64_t cursor = 0;
  uint64_t next_cursor = 0;

#define SAFE_CURSOR_UPDATE(n)                    \
  do {                                           \
    cursor = next_cursor;                        \
    if (!jxl::SafeAdd(cursor, n, next_cursor) || \
        next_cursor > input_buffer_size) {       \
      return JXL_FALSE;                          \
    }                                            \
  } while (false)

  // Read the version byte
  SAFE_CURSOR_UPDATE(1);
  map_bundle->jhgm_version = input_buffer[cursor];

  // Read gain_map_metadata_size
  SAFE_CURSOR_UPDATE(2);
  uint16_t gain_map_metadata_size = LoadBE16(input_buffer + cursor);

  SAFE_CURSOR_UPDATE(gain_map_metadata_size);
  map_bundle->gain_map_metadata_size = gain_map_metadata_size;
  map_bundle->gain_map_metadata = input_buffer + cursor;

  // Read compressed_color_encoding_size
  SAFE_CURSOR_UPDATE(1);
  uint8_t compressed_color_encoding_size;
  memcpy(&compressed_color_encoding_size, input_buffer + cursor, 1);

  map_bundle->has_color_encoding = (compressed_color_encoding_size > 0);
  if (map_bundle->has_color_encoding) {
    SAFE_CURSOR_UPDATE(compressed_color_encoding_size);
    // Decode color encoding
    jxl::Span<const uint8_t> color_encoding_span(
        input_buffer + cursor, compressed_color_encoding_size);
    jxl::BitReader color_encoding_reader(color_encoding_span);
    jxl::ColorEncoding internal_color_encoding;
    if (!jxl::Bundle::Read(&color_encoding_reader, &internal_color_encoding)) {
      return JXL_FALSE;
    }
    JXL_RETURN_IF_ERROR(color_encoding_reader.Close());
    map_bundle->color_encoding = internal_color_encoding.ToExternal();
  }

  // Read compressed_icc_size
  SAFE_CURSOR_UPDATE(4);
  uint32_t compressed_icc_size = LoadBE32(input_buffer + cursor);

  SAFE_CURSOR_UPDATE(compressed_icc_size);
  map_bundle->alt_icc_size = compressed_icc_size;
  map_bundle->alt_icc = input_buffer + cursor;

  // Calculate remaining bytes for gain map
  cursor = next_cursor;
  // This calculation is guaranteed not to underflow because `cursor` is always
  // updated to a position within or at the end of `input_buffer` (not beyond).
  // Thus, subtracting `cursor` from `input_buffer_size` (the total size of the
  // buffer) will always result in a non-negative integer representing the
  // remaining buffer size.
  map_bundle->gain_map_size = input_buffer_size - cursor;
  SAFE_CURSOR_UPDATE(map_bundle->gain_map_size);
  map_bundle->gain_map = input_buffer + cursor;

#undef SAFE_CURSOR_UPDATE

  cursor = next_cursor;

  if (bytes_read != nullptr) {
    *bytes_read = cursor;
  }
  return JXL_TRUE;
}
