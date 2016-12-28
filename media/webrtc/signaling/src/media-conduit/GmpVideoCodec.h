/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVIDEOCODEC_H_
#define GMPVIDEOCODEC_H_

#include "MediaConduitInterface.h"

namespace mozilla {
class GmpVideoCodec {
 public:
  static WebrtcVideoEncoder* CreateEncoder();
  static WebrtcVideoDecoder* CreateDecoder();
};

}

#endif
