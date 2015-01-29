/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_NETWORK_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_NETWORK_IMPL_H_

#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/vie_ref_count.h"

namespace webrtc {

class ViESharedData;

class ViENetworkImpl
    : public ViENetwork,
      public ViERefCount {
 public:
  // Implements ViENetwork.
  virtual int Release() OVERRIDE;
  virtual void SetNetworkTransmissionState(const int video_channel,
                                           const bool is_transmitting) OVERRIDE;
  virtual int RegisterSendTransport(const int video_channel,
                                    Transport& transport) OVERRIDE;
  virtual int DeregisterSendTransport(const int video_channel) OVERRIDE;
  virtual int ReceivedRTPPacket(const int video_channel,
                                const void* data,
                                const int length,
                                const PacketTime& packet_time) OVERRIDE;
  virtual int ReceivedRTCPPacket(const int video_channel,
                                 const void* data,
                                 const int length) OVERRIDE;
  virtual int SetMTU(int video_channel, unsigned int mtu) OVERRIDE;

  virtual int ReceivedBWEPacket(const int video_channel,
                                int64_t arrival_time_ms,
                                int payload_size,
                                const RTPHeader& header) OVERRIDE;

  virtual bool SetBandwidthEstimationConfig(
      int video_channel,
      const webrtc::Config& config) OVERRIDE;

 protected:
  explicit ViENetworkImpl(ViESharedData* shared_data);
  virtual ~ViENetworkImpl();

 private:
  ViESharedData* shared_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_NETWORK_IMPL_H_
