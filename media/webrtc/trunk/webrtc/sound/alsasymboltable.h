/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SOUND_ALSASYMBOLTABLE_H_
#define WEBRTC_SOUND_ALSASYMBOLTABLE_H_

#include <alsa/asoundlib.h>

#include "webrtc/base/latebindingsymboltable.h"

namespace rtc {

#define ALSA_SYMBOLS_CLASS_NAME AlsaSymbolTable
// The ALSA symbols we need, as an X-Macro list.
// This list must contain precisely every libasound function that is used in
// alsasoundsystem.cc.
#define ALSA_SYMBOLS_LIST \
  X(snd_device_name_free_hint) \
  X(snd_device_name_get_hint) \
  X(snd_device_name_hint) \
  X(snd_pcm_avail_update) \
  X(snd_pcm_close) \
  X(snd_pcm_delay) \
  X(snd_pcm_drop) \
  X(snd_pcm_open) \
  X(snd_pcm_prepare) \
  X(snd_pcm_readi) \
  X(snd_pcm_recover) \
  X(snd_pcm_set_params) \
  X(snd_pcm_start) \
  X(snd_pcm_stream) \
  X(snd_pcm_wait) \
  X(snd_pcm_writei) \
  X(snd_strerror)

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME ALSA_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST ALSA_SYMBOLS_LIST
#include "webrtc/base/latebindingsymboltable.h.def"

}  // namespace rtc

#endif  // WEBRTC_SOUND_ALSASYMBOLTABLE_H_
