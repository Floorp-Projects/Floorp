/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CSFLog.h"
#include "nspr.h"

#include "WebrtcExtVideoCodec.h"
#include "ExtVideoCodec.h"

namespace mozilla {

static const char* logTag ="ExtVideoCodec";

VideoEncoder* ExtVideoCodec::CreateEncoder() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  WebrtcExtVideoEncoder *enc =
      new WebrtcExtVideoEncoder();
  return enc;
}

VideoDecoder* ExtVideoCodec::CreateDecoder() {
  CSFLogDebug(logTag,  "%s ", __FUNCTION__);
  WebrtcExtVideoDecoder *dec =
      new WebrtcExtVideoDecoder();
  return dec;
}

}
