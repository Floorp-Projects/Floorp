/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_VP8D_INT_H
#define __INC_VP8D_INT_H
#include "vpx_ports/config.h"
#include "onyxd.h"
#include "treereader.h"
#include "onyxc_int.h"
#include "threading.h"
#include "dequantize.h"

typedef struct
{
    int ithread;
    void *ptr1;
    void *ptr2;
} DECODETHREAD_DATA;

typedef struct
{
    MACROBLOCKD  mbd;
    int mb_row;
    int current_mb_col;
    short *coef_ptr;
} MB_ROW_DEC;

typedef struct
{
    INT64 time_stamp;
    int size;
} DATARATE;

typedef struct
{
    INT16         min_val;
    INT16         Length;
    UINT8 Probs[12];
} TOKENEXTRABITS;

typedef struct
{
    int const *scan;
    UINT8 const *ptr_block2leftabove;
    vp8_tree_index const *vp8_coef_tree_ptr;
    TOKENEXTRABITS const *teb_base_ptr;
    unsigned char *norm_ptr;
    UINT8 *ptr_coef_bands_x;

    ENTROPY_CONTEXT_PLANES *A;
    ENTROPY_CONTEXT_PLANES *L;

    INT16 *qcoeff_start_ptr;
    BOOL_DECODER *current_bc;

    vp8_prob const *coef_probs[4];

    UINT8 eob[25];

} DETOK;

typedef struct VP8Decompressor
{
    DECLARE_ALIGNED(16, MACROBLOCKD, mb);

    DECLARE_ALIGNED(16, VP8_COMMON, common);

    vp8_reader bc, bc2;

    VP8D_CONFIG oxcf;


    const unsigned char *Source;
    unsigned int   source_sz;


    unsigned int CPUFreq;
    unsigned int decode_microseconds;
    unsigned int time_decoding;
    unsigned int time_loop_filtering;

    volatile int b_multithreaded_rd;
    volatile int b_multithreaded_lf;
    int max_threads;
    int last_mb_row_decoded;
    int current_mb_col_main;
    int decoding_thread_count;
    int allocated_decoding_thread_count;
    int *current_mb_col;                  //Each row remembers its already decoded column.
    int mt_baseline_filter_level[MAX_MB_SEGMENTS];

    // variable for threading
    DECLARE_ALIGNED(16, MACROBLOCKD, lpfmb);
#if CONFIG_MULTITHREAD
    //pthread_t           h_thread_lpf;         // thread for postprocessing
    sem_t               h_event_end_lpf;          // Event for post_proc completed
    sem_t               *h_event_start_lpf;
#endif
    MB_ROW_DEC           *mb_row_di;
    DECODETHREAD_DATA   *de_thread_data;
#if CONFIG_MULTITHREAD
    pthread_t           *h_decoding_thread;
    sem_t               *h_event_start_decoding;
    sem_t               h_event_end_decoding;
    // end of threading data
#endif
    vp8_reader *mbc;
    INT64 last_time_stamp;
    int   ready_for_new_data;

    DATARATE dr[16];

    DETOK detoken;

#if CONFIG_RUNTIME_CPU_DETECT
    vp8_dequant_rtcd_vtable_t        dequant;
    struct vp8_dboolhuff_rtcd_vtable dboolhuff;
#endif

} VP8D_COMP;

int vp8_decode_frame(VP8D_COMP *cpi);
void vp8_dmachine_specific_config(VP8D_COMP *pbi);


#if CONFIG_DEBUG
#define CHECK_MEM_ERROR(lval,expr) do {\
        lval = (expr); \
        if(!lval) \
            vpx_internal_error(&pbi->common.error, VPX_CODEC_MEM_ERROR,\
                               "Failed to allocate "#lval" at %s:%d", \
                               __FILE__,__LINE__);\
    } while(0)
#else
#define CHECK_MEM_ERROR(lval,expr) do {\
        lval = (expr); \
        if(!lval) \
            vpx_internal_error(&pbi->common.error, VPX_CODEC_MEM_ERROR,\
                               "Failed to allocate "#lval);\
    } while(0)
#endif

#endif
