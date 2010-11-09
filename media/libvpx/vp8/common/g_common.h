/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


extern void (*vp8_clear_system_state)(void);
extern void (*vp8_plane_add_noise)(unsigned char *Start, unsigned int Width, unsigned int Height, int Pitch, int DPitch, int q);
extern void (*de_interlace)
(
    unsigned char *src_ptr,
    unsigned char *dst_ptr,
    int Width,
    int Height,
    int Stride
);
