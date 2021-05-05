/* Copyright (c) the JPEG XL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file types.h
 * @brief Data types for the JPEG XL API, for both encoding and decoding.
 */

#ifndef JXL_TYPES_H_
#define JXL_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/**
 * A portable @c bool replacement.
 *
 * ::JXL_BOOL is a "documentation" type: actually it is @c int, but in API it
 * denotes a type, whose only values are ::JXL_TRUE and ::JXL_FALSE.
 */
#define JXL_BOOL int
/** Portable @c true replacement. */
#define JXL_TRUE 1
/** Portable @c false replacement. */
#define JXL_FALSE 0

/** Data type for the sample values per channel per pixel.
 */
typedef enum {
  /** Use 32-bit single-precision floating point values, with range 0.0-1.0
   * (within gamut, may go outside this range for wide color gamut). Floating
   * point output, either JXL_TYPE_FLOAT or JXL_TYPE_FLOAT16, is recommended
   * for HDR and wide gamut images when color profile conversion is required. */
  JXL_TYPE_FLOAT = 0,

  /** Use 1-bit packed in uint8_t, first pixel in LSB, padded to uint8_t per
   * row.
   * TODO(lode): support first in MSB, other padding.
   */
  JXL_TYPE_BOOLEAN,

  /** Use type uint8_t. May clip wide color gamut data.
   */
  JXL_TYPE_UINT8,

  /** Use type uint16_t. May clip wide color gamut data.
   */
  JXL_TYPE_UINT16,

  /** Use type uint32_t. May clip wide color gamut data.
   */
  JXL_TYPE_UINT32,

  /** Use 16-bit IEEE 754 half-precision floating point values */
  JXL_TYPE_FLOAT16,
} JxlDataType;

/** Ordering of multi-byte data.
 */
typedef enum {
  /** Use the endianness of the system, either little endian or big endian,
   * without forcing either specific endianness. Do not use if pixel data
   * should be exported to a well defined format.
   */
  JXL_NATIVE_ENDIAN = 0,
  /** Force little endian */
  JXL_LITTLE_ENDIAN = 1,
  /** Force big endian */
  JXL_BIG_ENDIAN = 2,
} JxlEndianness;

/** Data type for the sample values per channel per pixel for the output buffer
 * for pixels. This is not necessarily the same as the data type encoded in the
 * codestream. The channels are interleaved per pixel. The pixels are
 * organized row by row, left to right, top to bottom.
 * TODO(lode): implement padding / alignment (row stride)
 * TODO(lode): support different channel orders if needed (RGB, BGR, ...)
 */
typedef struct {
  /** Amount of channels available in a pixel buffer.
   * 1: single-channel data, e.g. grayscale
   * 2: single-channel + alpha
   * 3: trichromatic, e.g. RGB
   * 4: trichromatic + alpha
   * TODO(lode): this needs finetuning. It is not yet defined how the user
   * chooses output color space. CMYK+alpha needs 5 channels.
   */
  uint32_t num_channels;

  /** Data type of each channel.
   */
  JxlDataType data_type;

  /** Whether multi-byte data types are represented in big endian or little
   * endian format. This applies to JXL_TYPE_UINT16, JXL_TYPE_UINT32
   * and JXL_TYPE_FLOAT.
   */
  JxlEndianness endianness;

  /** Align scanlines to a multiple of align bytes, or 0 to require no
   * alignment at all (which has the same effect as value 1)
   */
  size_t align;
} JxlPixelFormat;

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* JXL_TYPES_H_ */
