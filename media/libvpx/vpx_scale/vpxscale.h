/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPXSCALE_H
#define VPXSCALE_H

#include "vpx_scale/yv12config.h"
#include "vpx_scale/yv12extend.h"
void vp8cx_horizontal_line_4_5_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_4_5_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_last_vertical_band_4_5_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_2_3_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_2_3_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_last_vertical_band_2_3_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_3_5_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_3_5_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_last_vertical_band_3_5_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_3_4_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_3_4_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_last_vertical_band_3_4_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_1_2_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_1_2_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_last_vertical_band_1_2_scale_c(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_5_4_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_5_4_scale_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_5_3_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_5_3_scale_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_horizontal_line_2_1_scale_c(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
void vp8cx_vertical_band_2_1_scale_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
void vp8cx_vertical_band_2_1_scale_i_c(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);


extern void (*vp8_vertical_band_4_5_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_last_vertical_band_4_5_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_2_3_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_last_vertical_band_2_3_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_3_5_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_last_vertical_band_3_5_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_3_4_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_last_vertical_band_3_4_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_horizontal_line_1_2_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_horizontal_line_3_4_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_horizontal_line_3_5_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_horizontal_line_2_3_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_horizontal_line_4_5_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_vertical_band_1_2_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_last_vertical_band_1_2_scale)(unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_5_4_scale)(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_5_3_scale)(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_2_1_scale)(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_vertical_band_2_1_scale_i)(unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width);
extern void (*vp8_horizontal_line_2_1_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_horizontal_line_5_3_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);
extern void (*vp8_horizontal_line_5_4_scale)(const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width);

extern void vp8_yv12_scale_or_center
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
);
extern void vp8_scale_frame
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
);
extern void vp8_scale_machine_specific_config(void);

#endif
