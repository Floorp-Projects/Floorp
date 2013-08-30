/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_VIDEO_CALL_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIDEO_CALL_IMPL_H_

#include <map>
#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/internal/video_receive_stream.h"
#include "webrtc/video_engine/internal/video_send_stream.h"
#include "webrtc/video_engine/new_include/video_engine.h"

namespace webrtc {

class VideoEngine;
class ViERTP_RTCP;
class ViECodec;

namespace internal {

// TODO(pbos): Split out the packet receiver, should be sharable between
//             VideoEngine and VoiceEngine.
class VideoCall : public newapi::VideoCall, public newapi::PacketReceiver {
 public:
  VideoCall(webrtc::VideoEngine* video_engine,
            newapi::Transport* send_transport);
  virtual ~VideoCall();

  virtual newapi::PacketReceiver* Receiver() OVERRIDE;
  virtual std::vector<VideoCodec> GetVideoCodecs() OVERRIDE;

  virtual newapi::VideoSendStream::Config GetDefaultSendConfig() OVERRIDE;

  virtual newapi::VideoSendStream* CreateSendStream(
      const newapi::VideoSendStream::Config& config) OVERRIDE;

  virtual newapi::SendStreamState* DestroySendStream(
      newapi::VideoSendStream* send_stream) OVERRIDE;

  virtual newapi::VideoReceiveStream::Config GetDefaultReceiveConfig() OVERRIDE;

  virtual newapi::VideoReceiveStream* CreateReceiveStream(
      const newapi::VideoReceiveStream::Config& config) OVERRIDE;

  virtual void DestroyReceiveStream(newapi::VideoReceiveStream* receive_stream)
      OVERRIDE;

  virtual uint32_t SendBitrateEstimate() OVERRIDE;
  virtual uint32_t ReceiveBitrateEstimate() OVERRIDE;

  virtual bool DeliverPacket(const void* packet, size_t length) OVERRIDE;

 private:
  bool DeliverRtp(ModuleRTPUtility::RTPHeaderParser* rtp_parser,
                  const void* packet, size_t length);
  bool DeliverRtcp(ModuleRTPUtility::RTPHeaderParser* rtp_parser,
                   const void* packet, size_t length);

  newapi::Transport* send_transport;

  std::map<uint32_t, newapi::VideoReceiveStream*> receive_ssrcs_;
  scoped_ptr<RWLockWrapper> receive_lock_;
  std::map<uint32_t, newapi::VideoSendStream*> send_ssrcs_;
  scoped_ptr<RWLockWrapper> send_lock_;

  webrtc::VideoEngine* video_engine_;
  ViERTP_RTCP* rtp_rtcp_;
  ViECodec* codec_;

  DISALLOW_COPY_AND_ASSIGN(VideoCall);
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_INTERNAL_VIDEO_CALL_H_
