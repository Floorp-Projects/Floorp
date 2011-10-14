/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_VP8_TEMPORAL_FILTER_X86_H
#define __INC_VP8_TEMPORAL_FILTER_X86_H

#if HAVE_SSE2
extern prototype_apply(vp8_temporal_filter_apply_sse2);

#if !CONFIG_RUNTIME_CPU_DETECT

#undef  vp8_temporal_filter_apply
#define vp8_temporal_filter_apply vp8_temporal_filter_apply_sse2

#endif

#endif

#endif // __INC_VP8_TEMPORAL_FILTER_X86_H
