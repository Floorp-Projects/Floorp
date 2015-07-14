/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGmpVideoCodec.h"
#include "GmpVideoCodec.h"

namespace mozilla {

VideoEncoder* GmpVideoCodec::CreateEncoder() {
  return static_cast<VideoEncoder*>(new WebrtcVideoEncoderProxy());
}

VideoDecoder* GmpVideoCodec::CreateDecoder() {
  return static_cast<VideoDecoder*>(new WebrtcVideoDecoderProxy());
}

}
