/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaCodecVideoCodec_h__
#define MediaCodecVideoCodec_h__

#include "MediaConduitInterface.h"

namespace mozilla {
class MediaCodecVideoCodec {
 public:
 enum CodecType {
    CODEC_VP8,
  };
  /**
   * Create encoder object for codec type |aCodecType|. Return |nullptr| when
   * failed.
   */
  static WebrtcVideoEncoder* CreateEncoder(CodecType aCodecType);

  /**
   * Create decoder object for codec type |aCodecType|. Return |nullptr| when
   * failed.
   */
  static WebrtcVideoDecoder* CreateDecoder(CodecType aCodecType);
};

}

#endif // MediaCodecVideoCodec_h__
