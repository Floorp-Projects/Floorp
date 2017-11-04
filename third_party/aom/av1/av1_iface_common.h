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
#ifndef AV1_AV1_IFACE_COMMON_H_
#define AV1_AV1_IFACE_COMMON_H_

#include "aom_ports/mem.h"

static void yuvconfig2image(aom_image_t *img, const YV12_BUFFER_CONFIG *yv12,
                            void *user_priv) {
  /** aom_img_wrap() doesn't allow specifying independent strides for
    * the Y, U, and V planes, nor other alignment adjustments that
    * might be representable by a YV12_BUFFER_CONFIG, so we just
    * initialize all the fields.*/
  int bps;
  if (!yv12->subsampling_y) {
    if (!yv12->subsampling_x) {
      img->fmt = AOM_IMG_FMT_I444;
      bps = 24;
    } else {
      img->fmt = AOM_IMG_FMT_I422;
      bps = 16;
    }
  } else {
    if (!yv12->subsampling_x) {
      img->fmt = AOM_IMG_FMT_I440;
      bps = 16;
    } else {
      img->fmt = AOM_IMG_FMT_I420;
      bps = 12;
    }
  }
  img->cs = yv12->color_space;
#if CONFIG_COLORSPACE_HEADERS
  img->tf = yv12->transfer_function;
  img->csp = yv12->chroma_sample_position;
#endif
  img->range = yv12->color_range;
  img->bit_depth = 8;
  img->w = yv12->y_stride;
  img->h = ALIGN_POWER_OF_TWO(yv12->y_height + 2 * AOM_BORDER_IN_PIXELS, 3);
  img->d_w = yv12->y_crop_width;
  img->d_h = yv12->y_crop_height;
  img->r_w = yv12->render_width;
  img->r_h = yv12->render_height;
  img->x_chroma_shift = yv12->subsampling_x;
  img->y_chroma_shift = yv12->subsampling_y;
  img->planes[AOM_PLANE_Y] = yv12->y_buffer;
  img->planes[AOM_PLANE_U] = yv12->u_buffer;
  img->planes[AOM_PLANE_V] = yv12->v_buffer;
  img->planes[AOM_PLANE_ALPHA] = NULL;
  img->stride[AOM_PLANE_Y] = yv12->y_stride;
  img->stride[AOM_PLANE_U] = yv12->uv_stride;
  img->stride[AOM_PLANE_V] = yv12->uv_stride;
  img->stride[AOM_PLANE_ALPHA] = yv12->y_stride;
#if CONFIG_HIGHBITDEPTH
  if (yv12->flags & YV12_FLAG_HIGHBITDEPTH) {
    // aom_image_t uses byte strides and a pointer to the first byte
    // of the image.
    img->fmt = (aom_img_fmt_t)(img->fmt | AOM_IMG_FMT_HIGHBITDEPTH);
    img->bit_depth = yv12->bit_depth;
    img->planes[AOM_PLANE_Y] = (uint8_t *)CONVERT_TO_SHORTPTR(yv12->y_buffer);
    img->planes[AOM_PLANE_U] = (uint8_t *)CONVERT_TO_SHORTPTR(yv12->u_buffer);
    img->planes[AOM_PLANE_V] = (uint8_t *)CONVERT_TO_SHORTPTR(yv12->v_buffer);
    img->planes[AOM_PLANE_ALPHA] = NULL;
    img->stride[AOM_PLANE_Y] = 2 * yv12->y_stride;
    img->stride[AOM_PLANE_U] = 2 * yv12->uv_stride;
    img->stride[AOM_PLANE_V] = 2 * yv12->uv_stride;
    img->stride[AOM_PLANE_ALPHA] = 2 * yv12->y_stride;
  }
#endif  // CONFIG_HIGHBITDEPTH
  img->bps = bps;
  img->user_priv = user_priv;
  img->img_data = yv12->buffer_alloc;
  img->img_data_owner = 0;
  img->self_allocd = 0;
}

static aom_codec_err_t image2yuvconfig(const aom_image_t *img,
                                       YV12_BUFFER_CONFIG *yv12) {
  yv12->y_buffer = img->planes[AOM_PLANE_Y];
  yv12->u_buffer = img->planes[AOM_PLANE_U];
  yv12->v_buffer = img->planes[AOM_PLANE_V];

  yv12->y_crop_width = img->d_w;
  yv12->y_crop_height = img->d_h;
  yv12->render_width = img->r_w;
  yv12->render_height = img->r_h;
  yv12->y_width = img->d_w;
  yv12->y_height = img->d_h;

  yv12->uv_width =
      img->x_chroma_shift == 1 ? (1 + yv12->y_width) / 2 : yv12->y_width;
  yv12->uv_height =
      img->y_chroma_shift == 1 ? (1 + yv12->y_height) / 2 : yv12->y_height;
  yv12->uv_crop_width = yv12->uv_width;
  yv12->uv_crop_height = yv12->uv_height;

  yv12->y_stride = img->stride[AOM_PLANE_Y];
  yv12->uv_stride = img->stride[AOM_PLANE_U];
  yv12->color_space = img->cs;
#if CONFIG_COLORSPACE_HEADERS
  yv12->transfer_function = img->tf;
  yv12->chroma_sample_position = img->csp;
#endif
  yv12->color_range = img->range;

#if CONFIG_HIGHBITDEPTH
  if (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
    // In aom_image_t
    //     planes point to uint8 address of start of data
    //     stride counts uint8s to reach next row
    // In YV12_BUFFER_CONFIG
    //     y_buffer, u_buffer, v_buffer point to uint16 address of data
    //     stride and border counts in uint16s
    // This means that all the address calculations in the main body of code
    // should work correctly.
    // However, before we do any pixel operations we need to cast the address
    // to a uint16 ponter and double its value.
    yv12->y_buffer = CONVERT_TO_BYTEPTR(yv12->y_buffer);
    yv12->u_buffer = CONVERT_TO_BYTEPTR(yv12->u_buffer);
    yv12->v_buffer = CONVERT_TO_BYTEPTR(yv12->v_buffer);
    yv12->y_stride >>= 1;
    yv12->uv_stride >>= 1;
    yv12->flags = YV12_FLAG_HIGHBITDEPTH;
  } else {
    yv12->flags = 0;
  }
  yv12->border = (yv12->y_stride - img->w) / 2;
#else
  yv12->border = (img->stride[AOM_PLANE_Y] - img->w) / 2;
#endif  // CONFIG_HIGHBITDEPTH
  yv12->subsampling_x = img->x_chroma_shift;
  yv12->subsampling_y = img->y_chroma_shift;
  return AOM_CODEC_OK;
}

#endif  // AV1_AV1_IFACE_COMMON_H_
