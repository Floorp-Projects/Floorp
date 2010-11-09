/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/****************************************************************************
*
*   Module Title :     preproc.h
*
*   Description  :     simple preprocessor
*
****************************************************************************/

#ifndef __INC_PREPROC_H
#define __INC_PREPROC_H

/****************************************************************************
*  Types
****************************************************************************/

typedef struct
{
    unsigned char *frame_buffer;
    int frame;
    unsigned int *fixed_divide;

    unsigned char *frame_buffer_alloc;
    unsigned int *fixed_divide_alloc;
} pre_proc_instance;

/****************************************************************************
*  Functions.
****************************************************************************/
void pre_proc_machine_specific_config(void);
void delete_pre_proc(pre_proc_instance *ppi);
int init_pre_proc(pre_proc_instance *ppi, int frame_size);
extern void spatial_filter_c(pre_proc_instance *ppi, unsigned char *s, unsigned char *d, int width, int height, int pitch, int strength);
extern void (*temp_filter)(pre_proc_instance *ppi, unsigned char *s, unsigned char *d, int bytes, int strength);

#endif
