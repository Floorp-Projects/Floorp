/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_COMMON_ARM_BILINEARFILTER_ARM_H_
#define VP8_COMMON_ARM_BILINEARFILTER_ARM_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void vp8_filter_block2d_bil_first_pass_armv6
(
    const unsigned char  *src_ptr,
    unsigned short       *dst_ptr,
    unsigned int          src_pitch,
    unsigned int          height,
    unsigned int          width,
    const short          *vp8_filter
);

extern void vp8_filter_block2d_bil_second_pass_armv6
(
    const unsigned short *src_ptr,
    unsigned char        *dst_ptr,
    int                   dst_pitch,
    unsigned int          height,
    unsigned int          width,
    const short         *vp8_filter
);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP8_COMMON_ARM_BILINEARFILTER_ARM_H_
