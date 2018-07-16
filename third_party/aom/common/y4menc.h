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

#ifndef Y4MENC_H_
#define Y4MENC_H_

#include "aom/aom_decoder.h"
#include "common/tools_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define Y4M_BUFFER_SIZE 128

int y4m_write_file_header(char *buf, size_t len, int width, int height,
                          const struct AvxRational *framerate,
                          aom_img_fmt_t fmt, unsigned int bit_depth);
int y4m_write_frame_header(char *buf, size_t len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // Y4MENC_H_
