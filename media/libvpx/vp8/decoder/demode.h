/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "onyxd_int.h"

/* Read (intra) modes for all blocks in a keyframe */

void vp8_kfread_modes(VP8D_COMP *pbi);

/* Intra mode for a Y subblock */

int vp8_read_bmode(vp8_reader *, const vp8_prob *);

/* MB intra Y mode trees differ for key and inter frames. */

int   vp8_read_ymode(vp8_reader *, const vp8_prob *);
int vp8_kfread_ymode(vp8_reader *, const vp8_prob *);

/* MB intra UV mode trees are the same for key and inter frames. */

int vp8_read_uv_mode(vp8_reader *, const vp8_prob *);

/* Read any macroblock-level features that may be present. */

void vp8_read_mb_features(vp8_reader *, MB_MODE_INFO *, MACROBLOCKD *);
