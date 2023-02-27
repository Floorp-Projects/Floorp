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

void jpegli_copy_critical_parameters(j_decompress_ptr srcinfo,
                                     j_compress_ptr dstinfo);

void jpegli_write_m_header(j_compress_ptr cinfo, int marker,
                           unsigned int datalen);

void jpegli_write_m_byte(j_compress_ptr cinfo, int val);

void jpegli_write_marker(j_compress_ptr cinfo, int marker,
                         const JOCTET* dataptr, unsigned int datalen);

void jpegli_write_icc_profile(j_compress_ptr cinfo, const JOCTET* icc_data_ptr,
                              unsigned int icc_data_len);

void jpegli_start_compress(j_compress_ptr cinfo, boolean write_all_tables);

void jpegli_write_tables(j_compress_ptr cinfo);

JDIMENSION jpegli_write_scanlines(j_compress_ptr cinfo, JSAMPARRAY scanlines,
                                  JDIMENSION num_lines);

JDIMENSION jpegli_write_raw_data(j_compress_ptr cinfo, JSAMPIMAGE data,
                                 JDIMENSION num_lines);

void jpegli_write_coefficients(j_compress_ptr cinfo,
                               jvirt_barray_ptr* coef_arrays);

void jpegli_finish_compress(j_compress_ptr cinfo);

void jpegli_abort_compress(j_compress_ptr cinfo);

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

// Changes the default behaviour of the encoder in the selection of quantization
// matrices and chroma subsampling. Must be called before jpegli_set_defaults()
// because some default setting depend on the XYB mode.
void jpegli_set_xyb_mode(j_compress_ptr cinfo);

void jpegli_set_input_format(j_compress_ptr cinfo, JpegliDataType data_type,
                             JpegliEndianness endianness);

// Sets whether or not the encoder uses adaptive quantization for createing more
// zero coefficients based on the local properties of the image.
// Enabled by default.
void jpegli_enable_adaptive_quantization(j_compress_ptr cinfo, boolean value);

// Sets the default progression parameters, where level 0 is sequential, and
// greater level value means more progression steps. Default is 2.
void jpegli_set_progressive_level(j_compress_ptr cinfo, int level);

// If this function is called before starting compression, the quality and
// linear quality parameters will be used to scale the standard quantization
// tables from Annex K of the JPEG standard. By default jpegli uses a different
// set of quantization tables and used different scaling parameters for DC and
// AC coefficients.
void jpegli_use_standard_quant_tables(j_compress_ptr cinfo);

#if defined(__cplusplus) || defined(c_plusplus)
}  // extern "C"
#endif

#endif  // LIB_JPEGLI_ENCODE_H_
