/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file overrides the inclusion of webrtc/base/constructormagic.h
// We do this because constructor magic defines DISALLOW_EVIL_CONSTRUCTORS,
// but we want to use the version from Chromium.

#ifndef OVERRIDES_WEBRTC_BASE_CONSTRUCTORMAGIC_H__
#define OVERRIDES_WEBRTC_BASE_CONSTRUCTORMAGIC_H__

#include "base/macros.h"

#endif  // OVERRIDES_WEBRTC_BASE_CONSTRUCTORMAGIC_H__
