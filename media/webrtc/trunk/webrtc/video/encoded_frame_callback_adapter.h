/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENCODED_FRAME_CALLBACK_ADAPTER_H_
#define WEBRTC_VIDEO_ENCODED_FRAME_CALLBACK_ADAPTER_H_

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/frame_callback.h"

namespace webrtc {
namespace internal {

class EncodedFrameCallbackAdapter : public EncodedImageCallback {
 public:
  explicit EncodedFrameCallbackAdapter(EncodedFrameObserver* observer);
  virtual ~EncodedFrameCallbackAdapter();

  virtual int32_t Encoded(const EncodedImage& encodedImage,
                          const CodecSpecificInfo* codecSpecificInfo,
                          const RTPFragmentationHeader* fragmentation);

 private:
  EncodedFrameObserver* observer_;
};

}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENCODED_FRAME_CALLBACK_ADAPTER_H_
