/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP8_ENCODER_BITSTREAM_H_
#define VP8_ENCODER_BITSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

void vp8_pack_tokens(vp8_writer *w, const TOKENEXTRA *p, int xcount);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP8_ENCODER_BITSTREAM_H_
