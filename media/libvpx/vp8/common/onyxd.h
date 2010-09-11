/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_VP8D_H
#define __INC_VP8D_H


/* Create/destroy static data structures. */
#ifdef __cplusplus
extern "C"
{
#endif
#include "type_aliases.h"
#include "vpx_scale/yv12config.h"
#include "ppflags.h"
#include "vpx_ports/mem.h"

    typedef void   *VP8D_PTR;
    typedef struct
    {
        int     Width;
        int     Height;
        int     Version;
        int     postprocess;
        int     max_threads;
    } VP8D_CONFIG;
    typedef enum
    {
        VP8_LAST_FLAG = 1,
        VP8_GOLD_FLAG = 2,
        VP8_ALT_FLAG = 4
    } VP8_REFFRAME;

    typedef enum
    {
        VP8D_OK = 0
    } VP8D_SETTING;

    void vp8dx_initialize(void);

    void vp8dx_set_setting(VP8D_PTR comp, VP8D_SETTING oxst, int x);

    int vp8dx_get_setting(VP8D_PTR comp, VP8D_SETTING oxst);

    int vp8dx_receive_compressed_data(VP8D_PTR comp, unsigned long size, const unsigned char *dest, INT64 time_stamp);
    int vp8dx_get_raw_frame(VP8D_PTR comp, YV12_BUFFER_CONFIG *sd, INT64 *time_stamp, INT64 *time_end_stamp, int deblock_level,  int noise_level, int flags);

    int vp8dx_get_reference(VP8D_PTR comp, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd);
    int vp8dx_set_reference(VP8D_PTR comp, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd);

    VP8D_PTR vp8dx_create_decompressor(VP8D_CONFIG *oxcf);

    void vp8dx_remove_decompressor(VP8D_PTR comp);

#ifdef __cplusplus
}
#endif


#endif
