/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIDEO_RECEIVE_STREAM_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIDEO_RECEIVE_STREAM_IMPL_H_

#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/internal/transport_adapter.h"
#include "webrtc/video_engine/new_include/video_receive_stream.h"

namespace webrtc {

class VideoEngine;
class ViEBase;
class ViECodec;
class ViEExternalCodec;
class ViENetwork;
class ViERender;
class ViERTP_RTCP;

namespace internal {

class VideoReceiveStream : public webrtc::VideoReceiveStream,
                           public webrtc::ExternalRenderer {
 public:
  VideoReceiveStream(webrtc::VideoEngine* video_engine,
                     const VideoReceiveStream::Config& config,
                     newapi::Transport* transport);
  virtual ~VideoReceiveStream();

  virtual void StartReceive() OVERRIDE;
  virtual void StopReceive() OVERRIDE;

  virtual void GetCurrentReceiveCodec(VideoCodec* receive_codec) OVERRIDE;

  virtual int FrameSizeChange(unsigned int width, unsigned int height,
                              unsigned int /*number_of_streams*/) OVERRIDE;
  virtual int DeliverFrame(uint8_t* frame, int buffer_size, uint32_t timestamp,
                           int64_t render_time, void* /*handle*/) OVERRIDE;

  virtual bool IsTextureSupported() OVERRIDE;

 public:
  virtual bool DeliverRtcp(const uint8_t* packet, size_t length);
  virtual bool DeliverRtp(const uint8_t* packet, size_t length);

 private:
  TransportAdapter transport_adapter_;
  VideoReceiveStream::Config config_;
  Clock* clock_;

  ViEBase* video_engine_base_;
  ViECodec* codec_;
  ViEExternalCodec* external_codec_;
  ViENetwork* network_;
  ViERender* render_;
  ViERTP_RTCP* rtp_rtcp_;

  int channel_;

  // TODO(pbos): Remove VideoReceiveStream can operate on I420 frames directly.
  unsigned int height_;
  unsigned int width_;
};
}  // internal
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_INTERNAL_VIDEO_RECEIVE_STREAM_H_
