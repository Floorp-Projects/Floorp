// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// This file conatins the C API of the encoder part of the libjpegli library,
// which is based on the C API of libjpeg, with the function names changed from
// jpeg_* to jpegli_*, while compressor object definitions are included directly
// from jpeglib.h
//
// Applications can use the libjpegli library in one of the following ways:
//
//  (1) Include jpegli/encode.h and/or jpegli/decode.h, update the function
//      names of the API and link against libjpegli.
//
//  (2) Leave the application code unchanged, but replace the libjpeg.so library
//      with the one built by this project that is API- and ABI-compatible with
//      libjpeg-turbo's version of libjpeg.so.

#ifndef LIB_JPEGLI_ENCODE_H_
#define LIB_JPEGLI_ENCODE_H_

/* clang-format off */
#include <stdio.h>
#include <jpeglib.h>
/* clang-format on */

#include "lib/jpegli/common.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define jpegli_create_compress(cinfo)              \
  jpegli_CreateCompress((cinfo), JPEG_LIB_VERSION, \
                        (size_t)sizeof(struct jpeg_compress_struct))
void jpegli_CreateCompress(j_compress_ptr cinfo, int version,
                           size_t structsize);

void jpegli_stdio_dest(j_compress_ptr cinfo, FILE* outfile);

void jpegli_mem_dest(j_compress_ptr cinfo, unsigned char** outbuffer,
                     unsigned long* outsize);

void jpegli_set_defaults(j_compress_ptr cinfo);

void jpegli_default_colorspace(j_compress_ptr cinfo);

void jpegli_set_colorspace(j_compress_ptr cinfo, J_COLOR_SPACE colorspace);

void jpegli_set_quality(j_compress_ptr cinfo, int quality,
                        boolean force_baseline);

void jpegli_set_linear_quality(j_compress_ptr cinfo, int scale_factor,
                               boolean force_baseline);

int jpegli_quality_scaling(int quality);

void jpegli_add_quant_table(j_compress_ptr cinfo, int which_tbl,
                            const unsigned int* basic_table, int scale_factor,
                            boolean force_baseline);

void jpegli_simple_progression(j_compress_ptr cinfo);

void jpegli_suppress_tables(j_compress_ptr cinfo, boolean suppress);

void jpegli_write_m_header(j_compress_ptr cinfo, int marker,
                           unsigned int datalen);

void jpegli_write_m_byte(j_compress_ptr cinfo, int val);

void jpegli_write_icc_profile(j_compress_ptr cinfo, const JOCTET* icc_data_ptr,
                              unsigned int icc_data_len);

void jpegli_start_compress(j_compress_ptr cinfo, boolean write_all_tables);

JDIMENSION jpegli_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines,
                                  JDIMENSION num_lines);

void jpegli_finish_compress(j_compress_ptr cinfo);

void jpegli_destroy_compress(j_compress_ptr cinfo);

//
// New API functions that are not available in libjpeg
//
// NOTE: This part of the API is still experimental and will probably change in
// the future.
//

// Sets the butteraugli target distance for the compressor.
void jpegli_set_distance(j_compress_ptr cinfo, float distance);

// Returns the butteraugli target distance for the given quality parameter.
float jpegli_quality_to_distance(int quality);

// Writes an ICC profile for the XYB color space and internally converts the
// input image to XYB.
void jpegli_set_xyb_mode(j_compress_ptr cinfo);

typedef enum {
  JPEGLI_TYPE_FLOAT = 0,
  JPEGLI_TYPE_UINT8 = 2,
  JPEGLI_TYPE_UINT16 = 3,
} JpegliDataType;

typedef enum {
  JPEGLI_NATIVE_ENDIAN = 0,
  JPEGLI_LITTLE_ENDIAN = 1,
  JPEGLI_BIG_ENDIAN = 2,
} JpegliEndianness;

void jpegli_set_input_format(j_compress_ptr cinfo, JpegliDataType data_type,
                             JpegliEndianness endianness);

#if defined(__cplusplus) || defined(c_plusplus)
}  // extern "C"
#endif

#endif  // LIB_JPEGLI_ENCODE_H_
