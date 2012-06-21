/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_IMPL_H_

#include "engine_configurations.h"
#include "video_engine/vie_defines.h"

#include "video_engine/vie_base_impl.h"

#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
#include "video_engine/vie_capture_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
#include "video_engine/vie_codec_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
#include "video_engine/vie_encryption_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
#include "video_engine/vie_file_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
#include "video_engine/vie_image_process_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
#include "video_engine/vie_network_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
#include "video_engine/vie_render_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
#include "video_engine/vie_rtp_rtcp_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
#include "video_engine/vie_external_codec_impl.h"
#endif

namespace webrtc {

class VideoEngineImpl
    : public ViEBaseImpl
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
      , public ViECodecImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
      , public ViECaptureImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
      , public ViEEncryptionImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
      , public ViEFileImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
      , public ViEImageProcessImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
      , public ViENetworkImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
      , public ViERenderImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
      , public ViERTP_RTCPImpl
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
      , public ViEExternalCodecImpl
#endif
{  // NOLINT
 public:
  VideoEngineImpl()
      :
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
        ViECodecImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
        , ViECaptureImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
        , ViEEncryptionImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
        , ViEFileImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
        , ViEImageProcessImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
        , ViENetworkImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
        , ViERenderImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
        , ViERTP_RTCPImpl(ViEBaseImpl::shared_data())
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
        , ViEExternalCodecImpl(ViEBaseImpl::shared_data())
#endif
  {}
  virtual ~VideoEngineImpl() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_IMPL_H_
