/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_IMAGE_PROCESS_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_IMAGE_PROCESS_IMPL_H_

#include "typedefs.h"
#include "video_engine/include/vie_image_process.h"
#include "video_engine/vie_ref_count.h"

namespace webrtc {

class ViESharedData;

class ViEImageProcessImpl
    : public ViEImageProcess,
      public ViERefCount {
 public:
  // Implements ViEImageProcess.
  virtual int Release();
  virtual int RegisterCaptureEffectFilter(const int capture_id,
                                          ViEEffectFilter& capture_filter);
  virtual int DeregisterCaptureEffectFilter(const int capture_id);
  virtual int RegisterSendEffectFilter(const int video_channel,
                                       ViEEffectFilter& send_filter);
  virtual int DeregisterSendEffectFilter(const int video_channel);
  virtual int RegisterRenderEffectFilter(const int video_channel,
                                         ViEEffectFilter& render_filter);
  virtual int DeregisterRenderEffectFilter(const int video_channel);
  virtual int EnableDeflickering(const int capture_id, const bool enable);
  virtual int EnableDenoising(const int capture_id, const bool enable);
  virtual int EnableColorEnhancement(const int video_channel,
                                     const bool enable);

 protected:
  ViEImageProcessImpl(ViESharedData* shared_data);
  virtual ~ViEImageProcessImpl();

 private:
  ViESharedData* shared_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_IMAGE_PROCESS_IMPL_H_
