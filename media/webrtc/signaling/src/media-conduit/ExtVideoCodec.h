/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExtVideoCodec_h__
#define ExtVideoCodec_h__

#include "MediaConduitInterface.h"

namespace mozilla {
class ExtVideoCodec {
public:
  static VideoEncoder* CreateEncoder();
  static VideoDecoder* CreateDecoder();
};

}

#endif // ExtVideoCodec_h__
