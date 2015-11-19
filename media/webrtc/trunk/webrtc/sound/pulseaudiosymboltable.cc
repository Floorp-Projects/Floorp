/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef HAVE_LIBPULSE

#include "webrtc/sound/pulseaudiosymboltable.h"

namespace rtc {

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME PULSE_AUDIO_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST PULSE_AUDIO_SYMBOLS_LIST
#define LATE_BINDING_SYMBOL_TABLE_DLL_NAME "libpulse.so.0"
#include "webrtc/base/latebindingsymboltable.cc.def"

}  // namespace rtc

#endif  // HAVE_LIBPULSE
