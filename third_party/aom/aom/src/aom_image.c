/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdlib.h>
#include <string.h>

#include "aom/aom_image.h"
#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"

static INLINE unsigned int align_image_dimension(unsigned int d,
                                                 unsigned int subsampling,
                                                 unsigned int size_align) {
  unsigned int align;

  align = (1 << subsampling) - 1;
  align = (size_align - 1 > align) ? (size_align - 1) : align;
  return ((d + align) & ~align);
}

static aom_image_t *img_alloc_helper(
    aom_image_t *img, aom_img_fmt_t fmt, unsigned int d_w, unsigned int d_h,
    unsigned int buf_align, unsigned int stride_align, unsigned int size_align,
    unsigned char *img_data, unsigned int border) {
  unsigned int h, w, s, xcs, ycs, bps;
  unsigned int stride_in_bytes;

  /* Treat align==0 like align==1 */
  if (!buf_align) buf_align = 1;

  /* Validate alignment (must be power of 2) */
  if (buf_align & (buf_align - 1)) goto fail;

  /* Treat align==0 like align==1 */
  if (!stride_align) stride_align = 1;

  /* Validate alignment (must be power of 2) */
  if (stride_align & (stride_align - 1)) goto fail;

  /* Treat align==0 like align==1 */
  if (!size_align) size_align = 1;

  /* Validate alignment (must be power of 2) */
  if (size_align & (size_align - 1)) goto fail;

  /* Get sample size for this format */
  switch (fmt) {
    case AOM_IMG_FMT_I420:
    case AOM_IMG_FMT_YV12:
    case AOM_IMG_FMT_AOMI420:
    case AOM_IMG_FMT_AOMYV12: bps = 12; break;
    case AOM_IMG_FMT_I422:
    case AOM_IMG_FMT_I444: bps = 24; break;
    case AOM_IMG_FMT_I42016: bps = 24; break;
    case AOM_IMG_FMT_I42216:
    case AOM_IMG_FMT_I44416: bps = 48; break;
    default: bps = 16; break;
  }

  /* Get chroma shift values for this format */
  switch (fmt) {
    case AOM_IMG_FMT_I420:
    case AOM_IMG_FMT_YV12:
    case AOM_IMG_FMT_AOMI420:
    case AOM_IMG_FMT_AOMYV12:
    case AOM_IMG_FMT_I422:
    case AOM_IMG_FMT_I42016:
    case AOM_IMG_FMT_I42216: xcs = 1; break;
    default: xcs = 0; break;
  }

  switch (fmt) {
    case AOM_IMG_FMT_I420:
    case AOM_IMG_FMT_YV12:
    case AOM_IMG_FMT_AOMI420:
    case AOM_IMG_FMT_AOMYV12:
    case AOM_IMG_FMT_I42016: ycs = 1; break;
    default: ycs = 0; break;
  }

  /* Calculate storage sizes given the chroma subsampling */
  w = align_image_dimension(d_w, xcs, size_align);
  h = align_image_dimension(d_h, ycs, size_align);

  s = (fmt & AOM_IMG_FMT_PLANAR) ? w : bps * w / 8;
  s = (s + 2 * border + stride_align - 1) & ~(stride_align - 1);
  stride_in_bytes = (fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? s * 2 : s;

  /* Allocate the new image */
  if (!img) {
    img = (aom_image_t *)calloc(1, sizeof(aom_image_t));

    if (!img) goto fail;

    img->self_allocd = 1;
  } else {
    memset(img, 0, sizeof(aom_image_t));
  }

  img->img_data = img_data;

  if (!img_data) {
    const uint64_t alloc_size =
        (fmt & AOM_IMG_FMT_PLANAR)
            ? (uint64_t)(h + 2 * border) * stride_in_bytes * bps / 8
            : (uint64_t)(h + 2 * border) * stride_in_bytes;

    if (alloc_size != (size_t)alloc_size) goto fail;

    img->img_data = (uint8_t *)aom_memalign(buf_align, (size_t)alloc_size);
    img->img_data_owner = 1;
  }

  if (!img->img_data) goto fail;

  img->fmt = fmt;
  img->bit_depth = (fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 16 : 8;
  // aligned width and aligned height
  img->w = w;
  img->h = h;
  img->x_chroma_shift = xcs;
  img->y_chroma_shift = ycs;
  img->bps = bps;

  /* Calculate strides */
  img->stride[AOM_PLANE_Y] = img->stride[AOM_PLANE_ALPHA] = stride_in_bytes;
  img->stride[AOM_PLANE_U] = img->stride[AOM_PLANE_V] = stride_in_bytes >> xcs;

  /* Default viewport to entire image */
  if (!aom_img_set_rect(img, 0, 0, d_w, d_h, border)) return img;

fail:
  aom_img_free(img);
  return NULL;
}

aom_image_t *aom_img_alloc(aom_image_t *img, aom_img_fmt_t fmt,
                           unsigned int d_w, unsigned int d_h,
                           unsigned int align) {
  return img_alloc_helper(img, fmt, d_w, d_h, align, align, 1, NULL, 0);
}

aom_image_t *aom_img_wrap(aom_image_t *img, aom_img_fmt_t fmt, unsigned int d_w,
                          unsigned int d_h, unsigned int stride_align,
                          unsigned char *img_data) {
  /* By setting buf_align = 1, we don't change buffer alignment in this
   * function. */
  return img_alloc_helper(img, fmt, d_w, d_h, 1, stride_align, 1, img_data, 0);
}

aom_image_t *aom_img_alloc_with_border(aom_image_t *img, aom_img_fmt_t fmt,
                                       unsigned int d_w, unsigned int d_h,
                                       unsigned int align,
                                       unsigned int size_align,
                                       unsigned int border) {
  return img_alloc_helper(img, fmt, d_w, d_h, align, align, size_align, NULL,
                          border);
}

int aom_img_set_rect(aom_image_t *img, unsigned int x, unsigned int y,
                     unsigned int w, unsigned int h, unsigned int border) {
  unsigned char *data;

  if (x + w <= img->w && y + h <= img->h) {
    img->d_w = w;
    img->d_h = h;

    x += border;
    y += border;

    /* Calculate plane pointers */
    if (!(img->fmt & AOM_IMG_FMT_PLANAR)) {
      img->planes[AOM_PLANE_PACKED] =
          img->img_data + x * img->bps / 8 + y * img->stride[AOM_PLANE_PACKED];
    } else {
      const int bytes_per_sample =
          (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1;
      data = img->img_data;

      if (img->fmt & AOM_IMG_FMT_HAS_ALPHA) {
        img->planes[AOM_PLANE_ALPHA] =
            data + x * bytes_per_sample + y * img->stride[AOM_PLANE_ALPHA];
        data += (img->h + 2 * border) * img->stride[AOM_PLANE_ALPHA];
      }

      img->planes[AOM_PLANE_Y] =
          data + x * bytes_per_sample + y * img->stride[AOM_PLANE_Y];
      data += (img->h + 2 * border) * img->stride[AOM_PLANE_Y];

      unsigned int uv_border_h = border >> img->y_chroma_shift;
      unsigned int uv_x = x >> img->x_chroma_shift;
      unsigned int uv_y = y >> img->y_chroma_shift;
      if (!(img->fmt & AOM_IMG_FMT_UV_FLIP)) {
        img->planes[AOM_PLANE_U] =
            data + uv_x * bytes_per_sample + uv_y * img->stride[AOM_PLANE_U];
        data += ((img->h >> img->y_chroma_shift) + 2 * uv_border_h) *
                img->stride[AOM_PLANE_U];
        img->planes[AOM_PLANE_V] =
            data + uv_x * bytes_per_sample + uv_y * img->stride[AOM_PLANE_V];
      } else {
        img->planes[AOM_PLANE_V] =
            data + uv_x * bytes_per_sample + uv_y * img->stride[AOM_PLANE_V];
        data += ((img->h >> img->y_chroma_shift) + 2 * uv_border_h) *
                img->stride[AOM_PLANE_V];
        img->planes[AOM_PLANE_U] =
            data + uv_x * bytes_per_sample + uv_y * img->stride[AOM_PLANE_U];
      }
    }
    return 0;
  }
  return -1;
}

void aom_img_flip(aom_image_t *img) {
  /* Note: In the calculation pointer adjustment calculation, we want the
   * rhs to be promoted to a signed type. Section 6.3.1.8 of the ISO C99
   * standard indicates that if the adjustment parameter is unsigned, the
   * stride parameter will be promoted to unsigned, causing errors when
   * the lhs is a larger type than the rhs.
   */
  img->planes[AOM_PLANE_Y] += (signed)(img->d_h - 1) * img->stride[AOM_PLANE_Y];
  img->stride[AOM_PLANE_Y] = -img->stride[AOM_PLANE_Y];

  img->planes[AOM_PLANE_U] += (signed)((img->d_h >> img->y_chroma_shift) - 1) *
                              img->stride[AOM_PLANE_U];
  img->stride[AOM_PLANE_U] = -img->stride[AOM_PLANE_U];

  img->planes[AOM_PLANE_V] += (signed)((img->d_h >> img->y_chroma_shift) - 1) *
                              img->stride[AOM_PLANE_V];
  img->stride[AOM_PLANE_V] = -img->stride[AOM_PLANE_V];

  img->planes[AOM_PLANE_ALPHA] +=
      (signed)(img->d_h - 1) * img->stride[AOM_PLANE_ALPHA];
  img->stride[AOM_PLANE_ALPHA] = -img->stride[AOM_PLANE_ALPHA];
}

void aom_img_free(aom_image_t *img) {
  if (img) {
    if (img->img_data && img->img_data_owner) aom_free(img->img_data);

    if (img->self_allocd) free(img);
  }
}

int aom_img_plane_width(const aom_image_t *img, int plane) {
  if (plane > 0 && img->x_chroma_shift > 0)
    return (img->d_w + 1) >> img->x_chroma_shift;
  else
    return img->d_w;
}

int aom_img_plane_height(const aom_image_t *img, int plane) {
  if (plane > 0 && img->y_chroma_shift > 0)
    return (img->d_h + 1) >> img->y_chroma_shift;
  else
    return img->d_h;
}
