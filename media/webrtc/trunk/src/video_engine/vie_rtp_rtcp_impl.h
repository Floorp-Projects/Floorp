/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_

#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "typedefs.h"  // NOLINT
#include "video_engine/include/vie_rtp_rtcp.h"
#include "video_engine/vie_ref_count.h"

namespace webrtc {

class ViESharedData;

class ViERTP_RTCPImpl
    : public ViERTP_RTCP,
      public ViERefCount {
 public:
  // Implements ViERTP_RTCP.
  virtual int Release();
  virtual int SetLocalSSRC(const int video_channel,
                           const unsigned int SSRC,
                           const StreamType usage,
                           const unsigned char simulcast_idx);
  virtual int GetLocalSSRC(const int video_channel,
                           unsigned int& SSRC) const;  // NOLINT
  virtual int SetRemoteSSRCType(const int video_channel,
                                const StreamType usage,
                                const unsigned int SSRC) const;
  virtual int GetRemoteSSRC(const int video_channel,
                            unsigned int& SSRC) const;  // NOLINT
  virtual int GetRemoteCSRCs(const int video_channel,
                             unsigned int CSRCs[kRtpCsrcSize]) const;
  virtual int SetStartSequenceNumber(const int video_channel,
                                     uint16_t sequence_number);
  virtual int SetRTCPStatus(const int video_channel,
                            const ViERTCPMode rtcp_mode);
  virtual int GetRTCPStatus(const int video_channel,
                            ViERTCPMode& rtcp_mode) const;
  virtual int SetRTCPCName(const int video_channel,
                           const char rtcp_cname[KMaxRTCPCNameLength]);
  virtual int GetRTCPCName(const int video_channel,
                           char rtcp_cname[KMaxRTCPCNameLength]) const;
  virtual int GetRemoteRTCPCName(const int video_channel,
                                 char rtcp_cname[KMaxRTCPCNameLength]) const;
  virtual int SendApplicationDefinedRTCPPacket(
      const int video_channel,
      const unsigned char sub_type,
      unsigned int name,
      const char* data,
      uint16_t data_length_in_bytes);
  virtual int SetNACKStatus(const int video_channel, const bool enable);
  virtual int SetFECStatus(const int video_channel, const bool enable,
                           const unsigned char payload_typeRED,
                           const unsigned char payload_typeFEC);
  virtual int SetHybridNACKFECStatus(const int video_channel, const bool enable,
                                     const unsigned char payload_typeRED,
                                     const unsigned char payload_typeFEC);
  virtual int SetKeyFrameRequestMethod(const int video_channel,
                                       const ViEKeyFrameRequestMethod method);
  virtual int SetTMMBRStatus(const int video_channel, const bool enable);
  virtual int SetRembStatus(int video_channel, bool sender, bool receiver);
  virtual int SetSendTimestampOffsetStatus(int video_channel,
                                           bool enable,
                                           int id);
  virtual int SetReceiveTimestampOffsetStatus(int video_channel,
                                              bool enable,
                                              int id);
  virtual int GetReceivedRTCPStatistics(const int video_channel,
                                        uint16_t& fraction_lost,
                                        unsigned int& cumulative_lost,
                                        unsigned int& extended_max,
                                        unsigned int& jitter,
                                        int& rtt_ms) const;
  virtual int GetSentRTCPStatistics(const int video_channel,
                                    uint16_t& fraction_lost,
                                    unsigned int& cumulative_lost,
                                    unsigned int& extended_max,
                                    unsigned int& jitter, int& rtt_ms) const;
  virtual int GetRTPStatistics(const int video_channel,
                               unsigned int& bytes_sent,
                               unsigned int& packets_sent,
                               unsigned int& bytes_received,
                               unsigned int& packets_received) const;
  virtual int GetBandwidthUsage(const int video_channel,
                                unsigned int& total_bitrate_sent,
                                unsigned int& video_bitrate_sent,
                                unsigned int& fec_bitrate_sent,
                                unsigned int& nackBitrateSent) const;
  virtual int GetEstimatedSendBandwidth(
      const int video_channel,
      unsigned int* estimated_bandwidth) const;
  virtual int GetEstimatedReceiveBandwidth(
      const int video_channel,
      unsigned int* estimated_bandwidth) const;
  virtual int SetOverUseDetectorOptions(
      const OverUseDetectorOptions& options) const;
  virtual int StartRTPDump(const int video_channel,
                           const char file_nameUTF8[1024],
                           RTPDirections direction);
  virtual int StopRTPDump(const int video_channel, RTPDirections direction);
  virtual int RegisterRTPObserver(const int video_channel,
                                  ViERTPObserver& observer);
  virtual int DeregisterRTPObserver(const int video_channel);
  virtual int RegisterRTCPObserver(const int video_channel,
                                   ViERTCPObserver& observer);
  virtual int DeregisterRTCPObserver(const int video_channel);

 protected:
  explicit ViERTP_RTCPImpl(ViESharedData* shared_data);
  virtual ~ViERTP_RTCPImpl();

 private:
  ViESharedData* shared_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_
