/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_NORMAL_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_NORMAL_H_

#include <string.h>  // Access to size_t.

#include <vector>

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"
#include "webrtc/modules/audio_coding/neteq4/defines.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Forward declarations.
class BackgroundNoise;
class DecoderDatabase;
class Expand;

// This class provides the "Normal" DSP operation, that is performed when
// there is no data loss, no need to stretch the timing of the signal, and
// no other "special circumstances" are at hand.
class Normal {
 public:
  Normal(int fs_hz, DecoderDatabase* decoder_database,
         const BackgroundNoise& background_noise,
         Expand* expand)
      : fs_hz_(fs_hz),
        decoder_database_(decoder_database),
        background_noise_(background_noise),
        expand_(expand) {
  }

  virtual ~Normal() {}

  // Performs the "Normal" operation. The decoder data is supplied in |input|,
  // having |length| samples in total for all channels (interleaved). The
  // result is written to |output|. The number of channels allocated in
  // |output| defines the number of channels that will be used when
  // de-interleaving |input|. |last_mode| contains the mode used in the previous
  // GetAudio call (i.e., not the current one), and |external_mute_factor| is
  // a pointer to the mute factor in the NetEqImpl class.
  int Process(const int16_t* input, size_t length,
              Modes last_mode,
              int16_t* external_mute_factor_array,
              AudioMultiVector<int16_t>* output);

 private:
  int fs_hz_;
  DecoderDatabase* decoder_database_;
  const BackgroundNoise& background_noise_;
  Expand* expand_;

  DISALLOW_COPY_AND_ASSIGN(Normal);
};

}  // namespace webrtc
#endif  // SRC_MODULES_AUDIO_CODING_NETEQ4_NORMAL_H_
