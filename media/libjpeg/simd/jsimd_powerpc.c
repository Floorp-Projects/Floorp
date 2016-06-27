/*
 * jsimd_powerpc.c
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2009-2011, 2014-2015, D. R. Commander.
 * Copyright (C) 2015, Matthieu Darbois.
 *
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 * This file contains the interface between the "normal" portions
 * of the library and the SIMD implementations when running on a
 * PowerPC architecture.
 */

#define JPEG_INTERNALS
#include "../jinclude.h"
#include "../jpeglib.h"
#include "../jsimd.h"
#include "../jdct.h"
#include "../jsimddct.h"
#include "jsimd.h"

static unsigned int simd_support = ~0;

LOCAL(void)
init_simd (void)
{
  char *env = NULL;

  if (simd_support != ~0U)
    return;

  simd_support = JSIMD_ALTIVEC;

  /* Force different settings through environment variables */
  env = getenv("JSIMD_FORCENONE");
  if ((env != NULL) && (strcmp(env, "1") == 0))
    simd_support = 0;
}

GLOBAL(int)
jsimd_can_rgb_ycc (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_rgb_gray (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if ((RGB_PIXELSIZE != 3) && (RGB_PIXELSIZE != 4))
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_ycc_rgb565 (void)
{
  return 0;
}

GLOBAL(void)
jsimd_rgb_ycc_convert (j_compress_ptr cinfo,
                       JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                       JDIMENSION output_row, int num_rows)
{
  void (*altivecfct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch(cinfo->in_color_space) {
    case JCS_EXT_RGB:
      altivecfct=jsimd_extrgb_ycc_convert_altivec;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      altivecfct=jsimd_extrgbx_ycc_convert_altivec;
      break;
    case JCS_EXT_BGR:
      altivecfct=jsimd_extbgr_ycc_convert_altivec;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      altivecfct=jsimd_extbgrx_ycc_convert_altivec;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      altivecfct=jsimd_extxbgr_ycc_convert_altivec;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      altivecfct=jsimd_extxrgb_ycc_convert_altivec;
      break;
    default:
      altivecfct=jsimd_rgb_ycc_convert_altivec;
      break;
  }

  altivecfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_rgb_gray_convert (j_compress_ptr cinfo,
                        JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
                        JDIMENSION output_row, int num_rows)
{
  void (*altivecfct)(JDIMENSION, JSAMPARRAY, JSAMPIMAGE, JDIMENSION, int);

  switch(cinfo->in_color_space) {
    case JCS_EXT_RGB:
      altivecfct=jsimd_extrgb_gray_convert_altivec;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      altivecfct=jsimd_extrgbx_gray_convert_altivec;
      break;
    case JCS_EXT_BGR:
      altivecfct=jsimd_extbgr_gray_convert_altivec;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      altivecfct=jsimd_extbgrx_gray_convert_altivec;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      altivecfct=jsimd_extxbgr_gray_convert_altivec;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      altivecfct=jsimd_extxrgb_gray_convert_altivec;
      break;
    default:
      altivecfct=jsimd_rgb_gray_convert_altivec;
      break;
  }

  altivecfct(cinfo->image_width, input_buf, output_buf, output_row, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb_convert (j_decompress_ptr cinfo,
                       JSAMPIMAGE input_buf, JDIMENSION input_row,
                       JSAMPARRAY output_buf, int num_rows)
{
  void (*altivecfct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY, int);

  switch(cinfo->out_color_space) {
    case JCS_EXT_RGB:
      altivecfct=jsimd_ycc_extrgb_convert_altivec;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      altivecfct=jsimd_ycc_extrgbx_convert_altivec;
      break;
    case JCS_EXT_BGR:
      altivecfct=jsimd_ycc_extbgr_convert_altivec;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      altivecfct=jsimd_ycc_extbgrx_convert_altivec;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      altivecfct=jsimd_ycc_extxbgr_convert_altivec;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      altivecfct=jsimd_ycc_extxrgb_convert_altivec;
      break;
    default:
      altivecfct=jsimd_ycc_rgb_convert_altivec;
      break;
  }

  altivecfct(cinfo->output_width, input_buf, input_row, output_buf, num_rows);
}

GLOBAL(void)
jsimd_ycc_rgb565_convert (j_decompress_ptr cinfo,
                          JSAMPIMAGE input_buf, JDIMENSION input_row,
                          JSAMPARRAY output_buf, int num_rows)
{
}

GLOBAL(int)
jsimd_can_h2v2_downsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_downsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_downsample (j_compress_ptr cinfo, jpeg_component_info *compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  jsimd_h2v2_downsample_altivec(cinfo->image_width, cinfo->max_v_samp_factor,
                                compptr->v_samp_factor,
                                compptr->width_in_blocks,
                                input_data, output_data);
}

GLOBAL(void)
jsimd_h2v1_downsample (j_compress_ptr cinfo, jpeg_component_info *compptr,
                       JSAMPARRAY input_data, JSAMPARRAY output_data)
{
  jsimd_h2v1_downsample_altivec(cinfo->image_width, cinfo->max_v_samp_factor,
                                compptr->v_samp_factor,
                                compptr->width_in_blocks,
                                input_data, output_data);
}

GLOBAL(int)
jsimd_can_h2v2_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info *compptr,
                     JSAMPARRAY input_data,
                     JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v2_upsample_altivec(cinfo->max_v_samp_factor, cinfo->output_width,
                              input_data, output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_upsample (j_decompress_ptr cinfo,
                     jpeg_component_info *compptr,
                     JSAMPARRAY input_data,
                     JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v1_upsample_altivec(cinfo->max_v_samp_factor, cinfo->output_width,
                              input_data, output_data_ptr);
}

GLOBAL(int)
jsimd_can_h2v2_fancy_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_fancy_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info *compptr,
                           JSAMPARRAY input_data,
                           JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v2_fancy_upsample_altivec(cinfo->max_v_samp_factor,
                                    compptr->downsampled_width, input_data,
                                    output_data_ptr);
}

GLOBAL(void)
jsimd_h2v1_fancy_upsample (j_decompress_ptr cinfo,
                           jpeg_component_info *compptr,
                           JSAMPARRAY input_data,
                           JSAMPARRAY *output_data_ptr)
{
  jsimd_h2v1_fancy_upsample_altivec(cinfo->max_v_samp_factor,
                                    compptr->downsampled_width, input_data,
                                    output_data_ptr);
}

GLOBAL(int)
jsimd_can_h2v2_merged_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_h2v1_merged_upsample (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(void)
jsimd_h2v2_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf)
{
  void (*altivecfct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch(cinfo->out_color_space) {
    case JCS_EXT_RGB:
      altivecfct=jsimd_h2v2_extrgb_merged_upsample_altivec;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      altivecfct=jsimd_h2v2_extrgbx_merged_upsample_altivec;
      break;
    case JCS_EXT_BGR:
      altivecfct=jsimd_h2v2_extbgr_merged_upsample_altivec;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      altivecfct=jsimd_h2v2_extbgrx_merged_upsample_altivec;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      altivecfct=jsimd_h2v2_extxbgr_merged_upsample_altivec;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      altivecfct=jsimd_h2v2_extxrgb_merged_upsample_altivec;
      break;
    default:
      altivecfct=jsimd_h2v2_merged_upsample_altivec;
      break;
  }

  altivecfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(void)
jsimd_h2v1_merged_upsample (j_decompress_ptr cinfo,
                            JSAMPIMAGE input_buf,
                            JDIMENSION in_row_group_ctr,
                            JSAMPARRAY output_buf)
{
  void (*altivecfct)(JDIMENSION, JSAMPIMAGE, JDIMENSION, JSAMPARRAY);

  switch(cinfo->out_color_space) {
    case JCS_EXT_RGB:
      altivecfct=jsimd_h2v1_extrgb_merged_upsample_altivec;
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      altivecfct=jsimd_h2v1_extrgbx_merged_upsample_altivec;
      break;
    case JCS_EXT_BGR:
      altivecfct=jsimd_h2v1_extbgr_merged_upsample_altivec;
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      altivecfct=jsimd_h2v1_extbgrx_merged_upsample_altivec;
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      altivecfct=jsimd_h2v1_extxbgr_merged_upsample_altivec;
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      altivecfct=jsimd_h2v1_extxrgb_merged_upsample_altivec;
      break;
    default:
      altivecfct=jsimd_h2v1_merged_upsample_altivec;
      break;
  }

  altivecfct(cinfo->output_width, input_buf, in_row_group_ctr, output_buf);
}

GLOBAL(int)
jsimd_can_convsamp (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (BITS_IN_JSAMPLE != 8)
    return 0;
  if (sizeof(JDIMENSION) != 4)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_convsamp_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_convsamp (JSAMPARRAY sample_data, JDIMENSION start_col,
                DCTELEM *workspace)
{
  jsimd_convsamp_altivec(sample_data, start_col, workspace);
}

GLOBAL(void)
jsimd_convsamp_float (JSAMPARRAY sample_data, JDIMENSION start_col,
                      FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_fdct_islow (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_ifast (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_fdct_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_fdct_islow (DCTELEM *data)
{
  jsimd_fdct_islow_altivec(data);
}

GLOBAL(void)
jsimd_fdct_ifast (DCTELEM *data)
{
  jsimd_fdct_ifast_altivec(data);
}

GLOBAL(void)
jsimd_fdct_float (FAST_FLOAT *data)
{
}

GLOBAL(int)
jsimd_can_quantize (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;
  if (sizeof(DCTELEM) != 2)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_quantize_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_quantize (JCOEFPTR coef_block, DCTELEM *divisors,
                DCTELEM *workspace)
{
  jsimd_quantize_altivec(coef_block, divisors, workspace);
}

GLOBAL(void)
jsimd_quantize_float (JCOEFPTR coef_block, FAST_FLOAT *divisors,
                      FAST_FLOAT *workspace)
{
}

GLOBAL(int)
jsimd_can_idct_2x2 (void)
{
  return 0;
}

GLOBAL(int)
jsimd_can_idct_4x4 (void)
{
  return 0;
}

GLOBAL(void)
jsimd_idct_2x2 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
}

GLOBAL(void)
jsimd_idct_4x4 (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                JCOEFPTR coef_block, JSAMPARRAY output_buf,
                JDIMENSION output_col)
{
}

GLOBAL(int)
jsimd_can_idct_islow (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_ifast (void)
{
  init_simd();

  /* The code is optimised for these values only */
  if (DCTSIZE != 8)
    return 0;
  if (sizeof(JCOEF) != 2)
    return 0;

  if (simd_support & JSIMD_ALTIVEC)
    return 1;

  return 0;
}

GLOBAL(int)
jsimd_can_idct_float (void)
{
  return 0;
}

GLOBAL(void)
jsimd_idct_islow (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
  jsimd_idct_islow_altivec(compptr->dct_table, coef_block, output_buf,
                           output_col);
}

GLOBAL(void)
jsimd_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
  jsimd_idct_ifast_altivec(compptr->dct_table, coef_block, output_buf,
                           output_col);
}

GLOBAL(void)
jsimd_idct_float (j_decompress_ptr cinfo, jpeg_component_info *compptr,
                  JCOEFPTR coef_block, JSAMPARRAY output_buf,
                  JDIMENSION output_col)
{
}

GLOBAL(int)
jsimd_can_huff_encode_one_block (void)
{
  return 0;
}

GLOBAL(JOCTET*)
jsimd_huff_encode_one_block (void *state, JOCTET *buffer, JCOEFPTR block,
                             int last_dc_val, c_derived_tbl *dctbl,
                             c_derived_tbl *actbl)
{
  return NULL;
}
