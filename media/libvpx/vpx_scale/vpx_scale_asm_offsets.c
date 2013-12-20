/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "./vpx_config.h"
#include "vpx/vpx_codec.h"
#include "vpx_ports/asm_offsets.h"
#include "vpx_scale/yv12config.h"

BEGIN

/* vpx_scale */
DEFINE(yv12_buffer_config_y_width,              offsetof(YV12_BUFFER_CONFIG, y_width));
DEFINE(yv12_buffer_config_y_height,             offsetof(YV12_BUFFER_CONFIG, y_height));
DEFINE(yv12_buffer_config_y_stride,             offsetof(YV12_BUFFER_CONFIG, y_stride));
DEFINE(yv12_buffer_config_uv_width,             offsetof(YV12_BUFFER_CONFIG, uv_width));
DEFINE(yv12_buffer_config_uv_height,            offsetof(YV12_BUFFER_CONFIG, uv_height));
DEFINE(yv12_buffer_config_uv_stride,            offsetof(YV12_BUFFER_CONFIG, uv_stride));
DEFINE(yv12_buffer_config_y_buffer,             offsetof(YV12_BUFFER_CONFIG, y_buffer));
DEFINE(yv12_buffer_config_u_buffer,             offsetof(YV12_BUFFER_CONFIG, u_buffer));
DEFINE(yv12_buffer_config_v_buffer,             offsetof(YV12_BUFFER_CONFIG, v_buffer));
DEFINE(yv12_buffer_config_border,               offsetof(YV12_BUFFER_CONFIG, border));
DEFINE(VP8BORDERINPIXELS_VAL,                   VP8BORDERINPIXELS);

END

/* add asserts for any offset that is not supported by assembly code */
/* add asserts for any size that is not supported by assembly code */

#if HAVE_NEON
/* vp8_yv12_extend_frame_borders_neon makes several assumptions based on this */
ct_assert(VP8BORDERINPIXELS_VAL, VP8BORDERINPIXELS == 32)
#endif
