/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OMX_VIDEO_CODEC_H_
#define OMX_VIDEO_CODEC_H_

#include "MediaConduitInterface.h"

namespace mozilla {
class OMXVideoCodec {
 public:
  enum CodecType {
    CODEC_H264,
  };

  /**
   * Create encoder object for codec type |aCodecType|. Return |nullptr| when
   * failed.
   */
  static VideoEncoder* CreateEncoder(CodecType aCodecType);

  /**
   * Create decoder object for codec type |aCodecType|. Return |nullptr| when
   * failed.
   */
  static VideoDecoder* CreateDecoder(CodecType aCodecType);
};

}

#endif // OMX_VIDEO_CODEC_H_
