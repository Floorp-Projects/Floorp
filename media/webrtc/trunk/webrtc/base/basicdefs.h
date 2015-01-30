/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_BASICDEFS_H_
#define WEBRTC_BASE_BASICDEFS_H_

#if HAVE_CONFIG_H
#include "config.h"  // NOLINT
#endif

#define ARRAY_SIZE(x) (static_cast<int>(sizeof(x) / sizeof(x[0])))

#endif  // WEBRTC_BASE_BASICDEFS_H_
