/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_ALLOCCOMMON_H
#define __INC_ALLOCCOMMON_H

#include "onyxc_int.h"

void vp8_create_common(VP8_COMMON *oci);
void vp8_remove_common(VP8_COMMON *oci);
void vp8_de_alloc_frame_buffers(VP8_COMMON *oci);
int vp8_alloc_frame_buffers(VP8_COMMON *oci, int width, int height);
void vp8_setup_version(VP8_COMMON *oci);

#endif
