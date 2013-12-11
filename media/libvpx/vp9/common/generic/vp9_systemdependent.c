/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "./vpx_config.h"
#include "./vp9_rtcd.h"
#include "vp9/common/vp9_onyxc_int.h"

void vp9_machine_specific_config(VP9_COMMON *cm) {
  (void)cm;
  vp9_rtcd();
}
