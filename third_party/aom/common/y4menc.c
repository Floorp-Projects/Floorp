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

#include <assert.h>

#include "common/y4menc.h"

int y4m_write_file_header(char *buf, size_t len, int width, int height,
                          const struct AvxRational *framerate,
                          aom_img_fmt_t fmt, unsigned int bit_depth) {
  const char *color;
  switch (bit_depth) {
    case 8:
      color = fmt == AOM_IMG_FMT_444A
                  ? "C444alpha\n"
                  : fmt == AOM_IMG_FMT_I444
                        ? "C444\n"
                        : fmt == AOM_IMG_FMT_I422 ? "C422\n" : "C420jpeg\n";
      break;
    case 9:
      color = fmt == AOM_IMG_FMT_I44416
                  ? "C444p9 XYSCSS=444P9\n"
                  : fmt == AOM_IMG_FMT_I42216 ? "C422p9 XYSCSS=422P9\n"
                                              : "C420p9 XYSCSS=420P9\n";
      break;
    case 10:
      color = fmt == AOM_IMG_FMT_I44416
                  ? "C444p10 XYSCSS=444P10\n"
                  : fmt == AOM_IMG_FMT_I42216 ? "C422p10 XYSCSS=422P10\n"
                                              : "C420p10 XYSCSS=420P10\n";
      break;
    case 12:
      color = fmt == AOM_IMG_FMT_I44416
                  ? "C444p12 XYSCSS=444P12\n"
                  : fmt == AOM_IMG_FMT_I42216 ? "C422p12 XYSCSS=422P12\n"
                                              : "C420p12 XYSCSS=420P12\n";
      break;
    case 14:
      color = fmt == AOM_IMG_FMT_I44416
                  ? "C444p14 XYSCSS=444P14\n"
                  : fmt == AOM_IMG_FMT_I42216 ? "C422p14 XYSCSS=422P14\n"
                                              : "C420p14 XYSCSS=420P14\n";
      break;
    case 16:
      color = fmt == AOM_IMG_FMT_I44416
                  ? "C444p16 XYSCSS=444P16\n"
                  : fmt == AOM_IMG_FMT_I42216 ? "C422p16 XYSCSS=422P16\n"
                                              : "C420p16 XYSCSS=420P16\n";
      break;
    default: color = NULL; assert(0);
  }
  return snprintf(buf, len, "YUV4MPEG2 W%u H%u F%u:%u I%c %s", width, height,
                  framerate->numerator, framerate->denominator, 'p', color);
}

int y4m_write_frame_header(char *buf, size_t len) {
  return snprintf(buf, len, "FRAME\n");
}
