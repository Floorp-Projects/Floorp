/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/****************************************************************************
 *
 *   Module Title :     scale.c
 *
 *   Description  :     Image scaling functions.
 *
 ***************************************************************************/

/****************************************************************************
*  Header Files
****************************************************************************/
#include "./vpx_rtcd.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/yv12config.h"
#include "vpx_scale/scale_mode.h"

typedef struct {
  int     expanded_frame_width;
  int     expanded_frame_height;

  int HScale;
  int HRatio;
  int VScale;
  int VRatio;

  YV12_BUFFER_CONFIG *src_yuv_config;
  YV12_BUFFER_CONFIG *dst_yuv_config;

} SCALE_VARS;

/****************************************************************************
 *
 *  ROUTINE       :     horizontal_line_copy
 *
 *  INPUTS        :     None
 *
 *
 *  OUTPUTS       :     None.
 *
 *  RETURNS       :     None
 *
 *  FUNCTION      :     1 to 1 scaling up for a horizontal line of pixles
 *
 *  SPECIAL NOTES :     None.
 *
 *  ERRORS        :     None.
 *
 ****************************************************************************/
static
void horizontal_line_copy(
  const unsigned char *source,
  unsigned int source_width,
  unsigned char *dest,
  unsigned int dest_width
) {
  (void) dest_width;

  duck_memcpy(dest, source, source_width);
}
/****************************************************************************
 *
 *  ROUTINE       :     null_scale
 *
 *  INPUTS        :     None
 *
 *
 *  OUTPUTS       :     None.
 *
 *  RETURNS       :     None
 *
 *  FUNCTION      :     1 to 1 scaling up for a vertical band
 *
 *  SPECIAL NOTES :     None.
 *
 *  ERRORS        :     None.
 *
 ****************************************************************************/
static
void null_scale(
  unsigned char *dest,
  unsigned int dest_pitch,
  unsigned int dest_width
) {
  (void) dest;
  (void) dest_pitch;
  (void) dest_width;

  return;
}

/****************************************************************************
 *
 *  ROUTINE       : scale1d_2t1_i
 *
 *  INPUTS        : const unsigned char *source : Pointer to data to be scaled.
 *                  int source_step              : Number of pixels to step on in source.
 *                  unsigned int source_scale    : Scale for source (UNUSED).
 *                  unsigned int source_length   : Length of source (UNUSED).
 *                  unsigned char *dest         : Pointer to output data array.
 *                  int dest_step                : Number of pixels to step on in destination.
 *                  unsigned int dest_scale      : Scale for destination (UNUSED).
 *                  unsigned int dest_length     : Length of destination.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-to-1 interpolated scaling.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static
void scale1d_2t1_i
(
  const unsigned char *source,
  int source_step,
  unsigned int source_scale,
  unsigned int source_length,
  unsigned char *dest,
  int dest_step,
  unsigned int dest_scale,
  unsigned int dest_length
) {
  unsigned int i, j;
  unsigned int temp;
  int source_pitch = source_step;
  (void) source_length;
  (void) source_scale;
  (void) dest_scale;

  source_step *= 2;
  dest[0] = source[0];

  for (i = dest_step, j = source_step; i < dest_length * dest_step; i += dest_step, j += source_step) {
    temp = 8;
    temp += 3 * source[j - source_pitch];
    temp += 10 * source[j];
    temp += 3 * source[j + source_pitch];
    temp >>= 4;
    dest[i] = (char)(temp);
  }
}

/****************************************************************************
 *
 *  ROUTINE       : scale1d_2t1_ps
 *
 *  INPUTS        : const unsigned char *source : Pointer to data to be scaled.
 *                  int source_step              : Number of pixels to step on in source.
 *                  unsigned int source_scale    : Scale for source (UNUSED).
 *                  unsigned int source_length   : Length of source (UNUSED).
 *                  unsigned char *dest         : Pointer to output data array.
 *                  int dest_step                : Number of pixels to step on in destination.
 *                  unsigned int dest_scale      : Scale for destination (UNUSED).
 *                  unsigned int dest_length     : Length of destination.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-to-1 point subsampled scaling.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static
void scale1d_2t1_ps
(
  const unsigned char *source,
  int source_step,
  unsigned int source_scale,
  unsigned int source_length,
  unsigned char *dest,
  int dest_step,
  unsigned int dest_scale,
  unsigned int dest_length
) {
  unsigned int i, j;

  (void) source_length;
  (void) source_scale;
  (void) dest_scale;

  source_step *= 2;
  j = 0;

  for (i = 0; i < dest_length * dest_step; i += dest_step, j += source_step)
    dest[i] = source[j];
}
/****************************************************************************
 *
 *  ROUTINE       : scale1d_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to data to be scaled.
 *                  int source_step              : Number of pixels to step on in source.
 *                  unsigned int source_scale    : Scale for source.
 *                  unsigned int source_length   : Length of source (UNUSED).
 *                  unsigned char *dest         : Pointer to output data array.
 *                  int dest_step                : Number of pixels to step on in destination.
 *                  unsigned int dest_scale      : Scale for destination.
 *                  unsigned int dest_length     : Length of destination.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs linear interpolation in one dimension.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static
void scale1d_c
(
  const unsigned char *source,
  int source_step,
  unsigned int source_scale,
  unsigned int source_length,
  unsigned char *dest,
  int dest_step,
  unsigned int dest_scale,
  unsigned int dest_length
) {
  unsigned int i;
  unsigned int round_value = dest_scale / 2;
  unsigned int left_modifier = dest_scale;
  unsigned int right_modifier = 0;
  unsigned char left_pixel = *source;
  unsigned char right_pixel = *(source + source_step);

  (void) source_length;

  /* These asserts are needed if there are boundary issues... */
  /*assert ( dest_scale > source_scale );*/
  /*assert ( (source_length-1) * dest_scale >= (dest_length-1) * source_scale );*/

  for (i = 0; i < dest_length * dest_step; i += dest_step) {
    dest[i] = (char)((left_modifier * left_pixel + right_modifier * right_pixel + round_value) / dest_scale);

    right_modifier += source_scale;

    while (right_modifier > dest_scale) {
      right_modifier -= dest_scale;
      source += source_step;
      left_pixel = *source;
      right_pixel = *(source + source_step);
    }

    left_modifier = dest_scale - right_modifier;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : Scale2D
 *
 *  INPUTS        : const unsigned char *source  : Pointer to data to be scaled.
 *                  int source_pitch              : Stride of source image.
 *                  unsigned int source_width     : Width of input image.
 *                  unsigned int source_height    : Height of input image.
 *                  unsigned char *dest          : Pointer to output data array.
 *                  int dest_pitch                : Stride of destination image.
 *                  unsigned int dest_width       : Width of destination image.
 *                  unsigned int dest_height      : Height of destination image.
 *                  unsigned char *temp_area      : Pointer to temp work area.
 *                  unsigned char temp_area_height : Height of temp work area.
 *                  unsigned int hscale          : Horizontal scale factor numerator.
 *                  unsigned int hratio          : Horizontal scale factor denominator.
 *                  unsigned int vscale          : Vertical scale factor numerator.
 *                  unsigned int vratio          : Vertical scale factor denominator.
 *                  unsigned int interlaced      : Interlace flag.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-tap linear interpolation in two dimensions.
 *
 *  SPECIAL NOTES : Expansion is performed one band at a time to help with
 *                  caching.
 *
 ****************************************************************************/
static
void Scale2D
(
  /*const*/
  unsigned char *source,
  int source_pitch,
  unsigned int source_width,
  unsigned int source_height,
  unsigned char *dest,
  int dest_pitch,
  unsigned int dest_width,
  unsigned int dest_height,
  unsigned char *temp_area,
  unsigned char temp_area_height,
  unsigned int hscale,
  unsigned int hratio,
  unsigned int vscale,
  unsigned int vratio,
  unsigned int interlaced
) {
  /*unsigned*/
  int i, j, k;
  int bands;
  int dest_band_height;
  int source_band_height;

  typedef void (*Scale1D)(const unsigned char * source, int source_step, unsigned int source_scale, unsigned int source_length,
                          unsigned char * dest, int dest_step, unsigned int dest_scale, unsigned int dest_length);

  Scale1D Scale1Dv = scale1d_c;
  Scale1D Scale1Dh = scale1d_c;

  void (*horiz_line_scale)(const unsigned char *, unsigned int, unsigned char *, unsigned int) = NULL;
  void (*vert_band_scale)(unsigned char *, unsigned int, unsigned char *, unsigned int, unsigned int) = NULL;

  int ratio_scalable = 1;
  int interpolation = 0;

  unsigned char *source_base; /* = (unsigned char *) ((source_pitch >= 0) ? source : (source + ((source_height-1) * source_pitch))); */
  unsigned char *line_src;


  source_base = (unsigned char *)source;

  if (source_pitch < 0) {
    int offset;

    offset = (source_height - 1);
    offset *= source_pitch;

    source_base += offset;
  }

  /* find out the ratio for each direction */
  switch (hratio * 10 / hscale) {
    case 8:
      /* 4-5 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_5_4_scale;
      break;
    case 6:
      /* 3-5 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_5_3_scale;
      break;
    case 5:
      /* 1-2 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_2_1_scale;
      break;
    default:
      /* The ratio is not acceptable now */
      /* throw("The ratio is not acceptable for now!"); */
      ratio_scalable = 0;
      break;
  }

  switch (vratio * 10 / vscale) {
    case 8:
      /* 4-5 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_5_4_scale;
      source_band_height  = 5;
      dest_band_height    = 4;
      break;
    case 6:
      /* 3-5 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_5_3_scale;
      source_band_height  = 5;
      dest_band_height    = 3;
      break;
    case 5:
      /* 1-2 Scale in vertical direction */

      if (interlaced) {
        /* if the content is interlaced, point sampling is used */
        vert_band_scale     = vp8_vertical_band_2_1_scale;
      } else {

        interpolation = 1;
        /* if the content is progressive, interplo */
        vert_band_scale     = vp8_vertical_band_2_1_scale_i;

      }

      source_band_height  = 2;
      dest_band_height    = 1;
      break;
    default:
      /* The ratio is not acceptable now */
      /* throw("The ratio is not acceptable for now!"); */
      ratio_scalable = 0;
      break;
  }

  if (ratio_scalable) {
    if (source_height == dest_height) {
      /* for each band of the image */
      for (k = 0; k < (int)dest_height; k++) {
        horiz_line_scale(source, source_width, dest, dest_width);
        source += source_pitch;
        dest   += dest_pitch;
      }

      return;
    }

    if (interpolation) {
      if (source < source_base)
        source = source_base;

      horiz_line_scale(source, source_width, temp_area, dest_width);
    }

    for (k = 0; k < (int)(dest_height + dest_band_height - 1) / dest_band_height; k++) {
      /* scale one band horizontally */
      for (i = 0; i < source_band_height; i++) {
        /* Trap case where we could read off the base of the source buffer */

        line_src = (unsigned char *)source + i * source_pitch;

        if (line_src < source_base)
          line_src = source_base;

        horiz_line_scale(line_src, source_width,
                         temp_area + (i + 1)*dest_pitch, dest_width);
      }

      /* Vertical scaling is in place */
      vert_band_scale(temp_area + dest_pitch, dest_pitch, dest, dest_pitch, dest_width);

      if (interpolation)
        vpx_memcpy(temp_area, temp_area + source_band_height * dest_pitch, dest_width);

      /* Next band... */
      source += (unsigned long) source_band_height  * source_pitch;
      dest   += (unsigned long) dest_band_height * dest_pitch;
    }

    return;
  }

  if (hscale == 2 && hratio == 1)
    Scale1Dh = scale1d_2t1_ps;

  if (vscale == 2 && vratio == 1) {
    if (interlaced)
      Scale1Dv = scale1d_2t1_ps;
    else
      Scale1Dv = scale1d_2t1_i;
  }

  if (source_height == dest_height) {
    /* for each band of the image */
    for (k = 0; k < (int)dest_height; k++) {
      Scale1Dh(source, 1, hscale, source_width + 1, dest, 1, hratio, dest_width);
      source += source_pitch;
      dest   += dest_pitch;
    }

    return;
  }

  if (dest_height > source_height) {
    dest_band_height   = temp_area_height - 1;
    source_band_height = dest_band_height * source_height / dest_height;
  } else {
    source_band_height = temp_area_height - 1;
    dest_band_height   = source_band_height * vratio / vscale;
  }

  /* first row needs to be done so that we can stay one row ahead for vertical zoom */
  Scale1Dh(source, 1, hscale, source_width + 1, temp_area, 1, hratio, dest_width);

  /* for each band of the image */
  bands = (dest_height + dest_band_height - 1) / dest_band_height;

  for (k = 0; k < bands; k++) {
    /* scale one band horizontally */
    for (i = 1; i < source_band_height + 1; i++) {
      if (k * source_band_height + i < (int) source_height) {
        Scale1Dh(source + i * source_pitch, 1, hscale, source_width + 1,
                 temp_area + i * dest_pitch, 1, hratio, dest_width);
      } else { /*  Duplicate the last row */
        /* copy temp_area row 0 over from last row in the past */
        duck_memcpy(temp_area + i * dest_pitch, temp_area + (i - 1)*dest_pitch, dest_pitch);
      }
    }

    /* scale one band vertically */
    for (j = 0; j < (int)dest_width; j++) {
      Scale1Dv(&temp_area[j], dest_pitch, vscale, source_band_height + 1,
               &dest[j], dest_pitch, vratio, dest_band_height);
    }

    /* copy temp_area row 0 over from last row in the past */
    duck_memcpy(temp_area, temp_area + source_band_height * dest_pitch, dest_pitch);

    /* move to the next band */
    source += source_band_height * source_pitch;
    dest   += dest_band_height * dest_pitch;
  }
}

/****************************************************************************
 *
 *  ROUTINE       :
 *
 *  INPUTS        : YV12_BUFFER_CONFIG *src       : Pointer to frame to be scaled.
 *                  YV12_BUFFER_CONFIG *dst       : Pointer to buffer to hold scaled frame.
 *                  unsigned char *temp_area      : Pointer to temp work area.
 *                  unsigned char temp_area_height : Height of temp work area.
 *                  unsigned int hscale          : Horizontal scale factor numerator.
 *                  unsigned int hratio          : Horizontal scale factor denominator.
 *                  unsigned int vscale          : Vertical scale factor numerator.
 *                  unsigned int vratio          : Vertical scale factor denominator.
 *                  unsigned int interlaced      : Interlace flag.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Performs 2-tap linear interpolation in two dimensions.
 *
 *  SPECIAL NOTES : Expansion is performed one band at a time to help with
 *                  caching.
 *
 ****************************************************************************/
void vp8_scale_frame
(
  YV12_BUFFER_CONFIG *src,
  YV12_BUFFER_CONFIG *dst,
  unsigned char *temp_area,
  unsigned char temp_height,
  unsigned int hscale,
  unsigned int hratio,
  unsigned int vscale,
  unsigned int vratio,
  unsigned int interlaced
) {
  int i;
  int dw = (hscale - 1 + src->y_width * hratio) / hscale;
  int dh = (vscale - 1 + src->y_height * vratio) / vscale;

  /* call our internal scaling routines!! */
  Scale2D((unsigned char *) src->y_buffer, src->y_stride, src->y_width, src->y_height,
          (unsigned char *) dst->y_buffer, dst->y_stride, dw, dh,
          temp_area, temp_height, hscale, hratio, vscale, vratio, interlaced);

  if (dw < (int)dst->y_width)
    for (i = 0; i < dh; i++)
      duck_memset(dst->y_buffer + i * dst->y_stride + dw - 1, dst->y_buffer[i * dst->y_stride + dw - 2], dst->y_width - dw + 1);

  if (dh < (int)dst->y_height)
    for (i = dh - 1; i < (int)dst->y_height; i++)
      duck_memcpy(dst->y_buffer + i * dst->y_stride, dst->y_buffer + (dh - 2) * dst->y_stride, dst->y_width + 1);

  Scale2D((unsigned char *) src->u_buffer, src->uv_stride, src->uv_width, src->uv_height,
          (unsigned char *) dst->u_buffer, dst->uv_stride, dw / 2, dh / 2,
          temp_area, temp_height, hscale, hratio, vscale, vratio, interlaced);

  if (dw / 2 < (int)dst->uv_width)
    for (i = 0; i < dst->uv_height; i++)
      duck_memset(dst->u_buffer + i * dst->uv_stride + dw / 2 - 1, dst->u_buffer[i * dst->uv_stride + dw / 2 - 2], dst->uv_width - dw / 2 + 1);

  if (dh / 2 < (int)dst->uv_height)
    for (i = dh / 2 - 1; i < (int)dst->y_height / 2; i++)
      duck_memcpy(dst->u_buffer + i * dst->uv_stride, dst->u_buffer + (dh / 2 - 2)*dst->uv_stride, dst->uv_width);

  Scale2D((unsigned char *) src->v_buffer, src->uv_stride, src->uv_width, src->uv_height,
          (unsigned char *) dst->v_buffer, dst->uv_stride, dw / 2, dh / 2,
          temp_area, temp_height, hscale, hratio, vscale, vratio, interlaced);

  if (dw / 2 < (int)dst->uv_width)
    for (i = 0; i < dst->uv_height; i++)
      duck_memset(dst->v_buffer + i * dst->uv_stride + dw / 2 - 1, dst->v_buffer[i * dst->uv_stride + dw / 2 - 2], dst->uv_width - dw / 2 + 1);

  if (dh / 2 < (int) dst->uv_height)
    for (i = dh / 2 - 1; i < (int)dst->y_height / 2; i++)
      duck_memcpy(dst->v_buffer + i * dst->uv_stride, dst->v_buffer + (dh / 2 - 2)*dst->uv_stride, dst->uv_width);
}
/****************************************************************************
 *
 *  ROUTINE       : any_ratio_2d_scale
 *
 *  INPUTS        : SCALE_INSTANCE *si      : Pointer to post-processor instance (NOT USED).
 *                  const unsigned char *source : Pointer to source image.
 *                  unsigned int source_pitch    : Stride of source image.
 *                  unsigned int source_width    : Width of source image.
 *                  unsigned int source_height   : Height of source image (NOT USED).
 *                  unsigned char *dest         : Pointer to destination image.
 *                  unsigned int dest_pitch      : Stride of destination image.
 *                  unsigned int dest_width      : Width of destination image.
 *                  unsigned int dest_height     : Height of destination image.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : int: 1 if image scaled, 0 if image could not be scaled.
 *
 *  FUNCTION      : Scale the image with changing apect ratio.
 *
 *  SPECIAL NOTES : This scaling is a bi-linear scaling. Need to re-work the
 *                  whole function for new scaling algorithm.
 *
 ****************************************************************************/
static
int any_ratio_2d_scale
(
  SCALE_VARS *si,
  const unsigned char *source,
  int source_pitch,
  unsigned int source_width,
  unsigned int source_height,
  unsigned char *dest,
  unsigned int dest_pitch,
  unsigned int dest_width,
  unsigned int dest_height
) {
  unsigned int i, k;
  unsigned int src_band_height  = 0;
  unsigned int dest_band_height = 0;

  /* suggested scale factors */
  int hs = si->HScale;
  int hr = si->HRatio;
  int vs = si->VScale;
  int vr = si->VRatio;

  /* assume the ratios are scalable instead of should be centered */
  int ratio_scalable = 1;

  const unsigned char *source_base = ((source_pitch >= 0) ? source : (source + ((source_height - 1) * source_pitch)));
  const unsigned char *line_src;

  void (*horiz_line_scale)(const unsigned char *, unsigned int, unsigned char *, unsigned int) = NULL;
  void (*vert_band_scale)(unsigned char *, unsigned int, unsigned int) = NULL;
  void (*last_vert_band_scale)(unsigned char *, unsigned int, unsigned int) = NULL;

  (void) si;

  /* find out the ratio for each direction */
  switch (hr * 30 / hs) {
    case 24:
      /* 4-5 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_4_5_scale;
      break;
    case 22:
      /* 3-4 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_3_4_scale;
      break;

    case 20:
      /* 4-5 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_2_3_scale;
      break;
    case 18:
      /* 3-5 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_3_5_scale;
      break;
    case 15:
      /* 1-2 Scale in Width direction */
      horiz_line_scale = vp8_horizontal_line_1_2_scale;
      break;
    case 30:
      /* no scale in Width direction */
      horiz_line_scale = horizontal_line_copy;
      break;
    default:
      /* The ratio is not acceptable now */
      /* throw("The ratio is not acceptable for now!"); */
      ratio_scalable = 0;
      break;
  }

  switch (vr * 30 / vs) {
    case 24:
      /* 4-5 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_4_5_scale;
      last_vert_band_scale = vp8_last_vertical_band_4_5_scale;
      src_band_height     = 4;
      dest_band_height    = 5;
      break;
    case 22:
      /* 3-4 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_3_4_scale;
      last_vert_band_scale = vp8_last_vertical_band_3_4_scale;
      src_band_height     = 3;
      dest_band_height    = 4;
      break;
    case 20:
      /* 2-3 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_2_3_scale;
      last_vert_band_scale = vp8_last_vertical_band_2_3_scale;
      src_band_height     = 2;
      dest_band_height    = 3;
      break;
    case 18:
      /* 3-5 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_3_5_scale;
      last_vert_band_scale = vp8_last_vertical_band_3_5_scale;
      src_band_height     = 3;
      dest_band_height    = 5;
      break;
    case 15:
      /* 1-2 Scale in vertical direction */
      vert_band_scale     = vp8_vertical_band_1_2_scale;
      last_vert_band_scale = vp8_last_vertical_band_1_2_scale;
      src_band_height     = 1;
      dest_band_height    = 2;
      break;
    case 30:
      /* no scale in Width direction */
      vert_band_scale     = null_scale;
      last_vert_band_scale = null_scale;
      src_band_height     = 4;
      dest_band_height    = 4;
      break;
    default:
      /* The ratio is not acceptable now */
      /* throw("The ratio is not acceptable for now!"); */
      ratio_scalable = 0;
      break;
  }

  if (ratio_scalable == 0)
    return ratio_scalable;

  horiz_line_scale(source, source_width, dest, dest_width);

  /* except last band */
  for (k = 0; k < (dest_height + dest_band_height - 1) / dest_band_height - 1; k++) {
    /* scale one band horizontally */
    for (i = 1; i < src_band_height; i++) {
      /* Trap case where we could read off the base of the source buffer */
      line_src = source + i * source_pitch;

      if (line_src < source_base)
        line_src = source_base;

      horiz_line_scale(line_src, source_width,
                       dest + i * dest_pitch, dest_width);
    }

    /* first line of next band */
    /* Trap case where we could read off the base of the source buffer */
    line_src = source + src_band_height * source_pitch;

    if (line_src < source_base)
      line_src = source_base;

    horiz_line_scale(line_src, source_width,
                     dest + dest_band_height * dest_pitch,
                     dest_width);

    /* Vertical scaling is in place */
    vert_band_scale(dest, dest_pitch, dest_width);

    /* Next band... */
    source += src_band_height  * source_pitch;
    dest   += dest_band_height * dest_pitch;
  }

  /* scale one band horizontally */
  for (i = 1; i < src_band_height; i++) {
    /* Trap case where we could read off the base of the source buffer */
    line_src = source + i * source_pitch;

    if (line_src < source_base)
      line_src = source_base;

    horiz_line_scale(line_src, source_width,
                     dest + i * dest_pitch,
                     dest_width);
  }

  /* Vertical scaling is in place */
  last_vert_band_scale(dest, dest_pitch, dest_width);

  return ratio_scalable;
}

/****************************************************************************
 *
 *  ROUTINE       : any_ratio_frame_scale
 *
 *  INPUTS        : SCALE_INSTANCE *si       : Pointer to post-processor instance (NOT USED).
 *                  unsigned char *frame_buffer           : Pointer to source image.
 *                  int YOffset                : Offset from start of buffer to Y samples.
 *                  int UVOffset               : Offset from start of buffer to UV samples.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : int: 1 if image scaled, 0 if image could not be scaled.
 *
 *  FUNCTION      : Scale the image with changing apect ratio.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static
int any_ratio_frame_scale(SCALE_VARS *scale_vars, int YOffset, int UVOffset) {
  int i;
  int ew;
  int eh;

  /* suggested scale factors */
  int hs = scale_vars->HScale;
  int hr = scale_vars->HRatio;
  int vs = scale_vars->VScale;
  int vr = scale_vars->VRatio;

  int ratio_scalable = 1;

  int sw = (scale_vars->expanded_frame_width * hr + hs - 1) / hs;
  int sh = (scale_vars->expanded_frame_height * vr + vs - 1) / vs;
  int dw = scale_vars->expanded_frame_width;
  int dh = scale_vars->expanded_frame_height;
  YV12_BUFFER_CONFIG *src_yuv_config = scale_vars->src_yuv_config;
  YV12_BUFFER_CONFIG *dst_yuv_config = scale_vars->dst_yuv_config;

  if (hr == 3)
    ew = (sw + 2) / 3 * 3 * hs / hr;
  else
    ew = (sw + 7) / 8 * 8 * hs / hr;

  if (vr == 3)
    eh = (sh + 2) / 3 * 3 * vs / vr;
  else
    eh = (sh + 7) / 8 * 8 * vs / vr;

  ratio_scalable = any_ratio_2d_scale(scale_vars,
                                      (const unsigned char *)src_yuv_config->y_buffer,
                                      src_yuv_config->y_stride, sw, sh,
                                      (unsigned char *) dst_yuv_config->y_buffer + YOffset,
                                      dst_yuv_config->y_stride, dw, dh);

  for (i = 0; i < eh; i++)
    duck_memset(dst_yuv_config->y_buffer + YOffset + i * dst_yuv_config->y_stride + dw, 0, ew - dw);

  for (i = dh; i < eh; i++)
    duck_memset(dst_yuv_config->y_buffer + YOffset + i * dst_yuv_config->y_stride, 0, ew);

  if (ratio_scalable == 0)
    return ratio_scalable;

  sw = (sw + 1) >> 1;
  sh = (sh + 1) >> 1;
  dw = (dw + 1) >> 1;
  dh = (dh + 1) >> 1;

  any_ratio_2d_scale(scale_vars,
                     (const unsigned char *)src_yuv_config->u_buffer,
                     src_yuv_config->y_stride / 2, sw, sh,
                     (unsigned char *)dst_yuv_config->u_buffer + UVOffset,
                     dst_yuv_config->uv_stride, dw, dh);

  any_ratio_2d_scale(scale_vars,
                     (const unsigned char *)src_yuv_config->v_buffer,
                     src_yuv_config->y_stride / 2, sw, sh,
                     (unsigned char *)dst_yuv_config->v_buffer + UVOffset,
                     dst_yuv_config->uv_stride, dw, dh);

  return ratio_scalable;
}

/****************************************************************************
 *
 *  ROUTINE       : center_image
 *
 *  INPUTS        : SCALE_INSTANCE *si       : Pointer to post-processor instance.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Centers the image without scaling in the output buffer.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
static void
center_image(YV12_BUFFER_CONFIG *src_yuv_config, YV12_BUFFER_CONFIG *dst_yuv_config) {
  int i;
  int row_offset, col_offset;
  unsigned char *src_data_pointer;
  unsigned char *dst_data_pointer;

  /* center values */
  row_offset = (dst_yuv_config->y_height - src_yuv_config->y_height) / 2;
  col_offset = (dst_yuv_config->y_width - src_yuv_config->y_width) / 2;

  /* Y's */
  src_data_pointer = src_yuv_config->y_buffer;
  dst_data_pointer = (unsigned char *)dst_yuv_config->y_buffer + (row_offset * dst_yuv_config->y_stride) + col_offset;

  for (i = 0; i < src_yuv_config->y_height; i++) {
    duck_memcpy(dst_data_pointer, src_data_pointer, src_yuv_config->y_width);
    dst_data_pointer += dst_yuv_config->y_stride;
    src_data_pointer += src_yuv_config->y_stride;
  }

  row_offset /= 2;
  col_offset /= 2;

  /* U's */
  src_data_pointer = src_yuv_config->u_buffer;
  dst_data_pointer = (unsigned char *)dst_yuv_config->u_buffer + (row_offset * dst_yuv_config->uv_stride) + col_offset;

  for (i = 0; i < src_yuv_config->uv_height; i++) {
    duck_memcpy(dst_data_pointer, src_data_pointer, src_yuv_config->uv_width);
    dst_data_pointer += dst_yuv_config->uv_stride;
    src_data_pointer += src_yuv_config->uv_stride;
  }

  /* V's */
  src_data_pointer = src_yuv_config->v_buffer;
  dst_data_pointer = (unsigned char *)dst_yuv_config->v_buffer + (row_offset * dst_yuv_config->uv_stride) + col_offset;

  for (i = 0; i < src_yuv_config->uv_height; i++) {
    duck_memcpy(dst_data_pointer, src_data_pointer, src_yuv_config->uv_width);
    dst_data_pointer += dst_yuv_config->uv_stride;
    src_data_pointer += src_yuv_config->uv_stride;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : scale_or_center
 *
 *  INPUTS        : SCALE_INSTANCE *si       : Pointer to post-processor instance.
 *
 *
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Decides to scale or center image in scale buffer for blit
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void
vp8_yv12_scale_or_center
(
  YV12_BUFFER_CONFIG *src_yuv_config,
  YV12_BUFFER_CONFIG *dst_yuv_config,
  int expanded_frame_width,
  int expanded_frame_height,
  int scaling_mode,
  int HScale,
  int HRatio,
  int VScale,
  int VRatio
) {
  /*if ( ppi->post_processing_level )
        update_umvborder ( ppi, frame_buffer );*/


  switch (scaling_mode) {
    case SCALE_TO_FIT:
    case MAINTAIN_ASPECT_RATIO: {
      SCALE_VARS scale_vars;
      /* center values */
#if 1
      int row = (dst_yuv_config->y_height - expanded_frame_height) / 2;
      int col = (dst_yuv_config->y_width  - expanded_frame_width) / 2;
      /*int YOffset  = row * dst_yuv_config->y_width + col;
      int UVOffset = (row>>1) * dst_yuv_config->uv_width + (col>>1);*/
      int YOffset  = row * dst_yuv_config->y_stride + col;
      int UVOffset = (row >> 1) * dst_yuv_config->uv_stride + (col >> 1);
#else
      int row = (src_yuv_config->y_height - expanded_frame_height) / 2;
      int col = (src_yuv_config->y_width  - expanded_frame_width) / 2;
      int YOffset  = row * src_yuv_config->y_width + col;
      int UVOffset = (row >> 1) * src_yuv_config->uv_width + (col >> 1);
#endif

      scale_vars.dst_yuv_config = dst_yuv_config;
      scale_vars.src_yuv_config = src_yuv_config;
      scale_vars.HScale = HScale;
      scale_vars.HRatio = HRatio;
      scale_vars.VScale = VScale;
      scale_vars.VRatio = VRatio;
      scale_vars.expanded_frame_width = expanded_frame_width;
      scale_vars.expanded_frame_height = expanded_frame_height;

      /* perform center and scale */
      any_ratio_frame_scale(&scale_vars, YOffset, UVOffset);

      break;
    }
    case CENTER:
      center_image(src_yuv_config, dst_yuv_config);
      break;

    default:
      break;
  }
}
