/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */





#ifndef _DECODER_THREADING_H
#define _DECODER_THREADING_H


extern void vp8_mtdecode_mb_rows(VP8D_COMP *pbi,
                                 MACROBLOCKD *xd);
extern void vp8_mt_loop_filter_frame(VP8D_COMP *pbi);
extern void vp8_stop_lfthread(VP8D_COMP *pbi);
extern void vp8_start_lfthread(VP8D_COMP *pbi);
extern void vp8_decoder_remove_threads(VP8D_COMP *pbi);
extern void vp8_decoder_create_threads(VP8D_COMP *pbi);
#endif
