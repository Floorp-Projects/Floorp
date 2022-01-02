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

#include "common/rawenc.h"

void raw_write_image_file(const aom_image_t *img, const int *planes,
                          const int num_planes, FILE *file) {
  const int bytes_per_sample = ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
  for (int i = 0; i < num_planes; ++i) {
    const int plane = planes[i];
    const unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = aom_img_plane_width(img, plane);
    const int h = aom_img_plane_height(img, plane);
    for (int y = 0; y < h; ++y) {
      fwrite(buf, bytes_per_sample, w, file);
      buf += stride;
    }
  }
}

void raw_update_image_md5(const aom_image_t *img, const int *planes,
                          const int num_planes, MD5Context *md5) {
  for (int i = 0; i < num_planes; ++i) {
    const int plane = planes[i];
    const unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = aom_img_plane_width(img, plane) *
                  ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = aom_img_plane_height(img, plane);
    for (int y = 0; y < h; ++y) {
      MD5Update(md5, buf, w);
      buf += stride;
    }
  }
}
