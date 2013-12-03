/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_config.h"
#include "vpx/vpx_codec.h"
#include "vpx_ports/asm_offsets.h"
#include "vpx_scale/yv12config.h"
#include "vp8/common/blockd.h"

#if CONFIG_POSTPROC
#include "postproc.h"
#endif /* CONFIG_POSTPROC */

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

#if CONFIG_POSTPROC
/* mfqe.c / filter_by_weight */
DEFINE(MFQE_PRECISION_VAL,                      MFQE_PRECISION);
#endif /* CONFIG_POSTPROC */

END

/* add asserts for any offset that is not supported by assembly code */
/* add asserts for any size that is not supported by assembly code */

#if HAVE_MEDIA
/* switch case in vp8_intra4x4_predict_armv6 is based on these enumerated values */
ct_assert(B_DC_PRED, B_DC_PRED == 0);
ct_assert(B_TM_PRED, B_TM_PRED == 1);
ct_assert(B_VE_PRED, B_VE_PRED == 2);
ct_assert(B_HE_PRED, B_HE_PRED == 3);
ct_assert(B_LD_PRED, B_LD_PRED == 4);
ct_assert(B_RD_PRED, B_RD_PRED == 5);
ct_assert(B_VR_PRED, B_VR_PRED == 6);
ct_assert(B_VL_PRED, B_VL_PRED == 7);
ct_assert(B_HD_PRED, B_HD_PRED == 8);
ct_assert(B_HU_PRED, B_HU_PRED == 9);
#endif

#if HAVE_NEON
/* vp8_yv12_extend_frame_borders_neon makes several assumptions based on this */
ct_assert(VP8BORDERINPIXELS_VAL, VP8BORDERINPIXELS == 32)
#endif

#if HAVE_SSE2
#if CONFIG_POSTPROC
/* vp8_filter_by_weight16x16 and 8x8 */
ct_assert(MFQE_PRECISION_VAL, MFQE_PRECISION == 4)
#endif /* CONFIG_POSTPROC */
#endif /* HAVE_SSE2 */
