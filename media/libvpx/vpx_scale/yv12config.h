/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef YV12_CONFIG_H
#define YV12_CONFIG_H
#ifdef __cplusplus
extern "C"
{
#endif

#define VP7BORDERINPIXELS       48
#define VP8BORDERINPIXELS       32

    /*************************************
     For INT_YUV:

     Y = (R+G*2+B)/4;
     U = (R-B)/2;
     V =  (G*2 - R - B)/4;
    And
     R = Y+U-V;
     G = Y+V;
     B = Y-U-V;
    ************************************/
    typedef enum
    {
        REG_YUV = 0,    /* Regular yuv */
        INT_YUV = 1     /* The type of yuv that can be tranfer to and from RGB through integer transform */
              }
              YUV_TYPE;

    typedef struct
    {
        int   y_width;
        int   y_height;
        int   y_stride;
/*    int   yinternal_width; */

        int   uv_width;
        int   uv_height;
        int   uv_stride;
/*    int   uvinternal_width; */

        unsigned char *y_buffer;
        unsigned char *u_buffer;
        unsigned char *v_buffer;

        unsigned char *buffer_alloc;
        int border;
        int frame_size;
        YUV_TYPE clrtype;
    } YV12_BUFFER_CONFIG;

    int vp8_yv12_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height, int border);
    int vp8_yv12_de_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf);
    int vp8_yv12_black_frame_buffer(YV12_BUFFER_CONFIG *ybf);

#ifdef __cplusplus
}
#endif


#endif /*YV12_CONFIG_H*/
