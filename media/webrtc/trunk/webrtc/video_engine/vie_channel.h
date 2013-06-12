/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_

#include <list>

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_file_recorder.h"
#include "webrtc/video_engine/vie_frame_provider_base.h"
#include "webrtc/video_engine/vie_receiver.h"
#include "webrtc/video_engine/vie_sender.h"
#include "webrtc/video_engine/vie_sync_module.h"

namespace webrtc {

class CallStatsObserver;
class ChannelStatsObserver;
class CriticalSectionWrapper;
class Encryption;
class PacedSender;
class ProcessThread;
class RtpRtcp;
class RtcpRttObserver;
class ThreadWrapper;
class VideoCodingModule;
class VideoDecoder;
class VideoRenderCallback;
class ViEDecoderObserver;
class ViEEffectFilter;
class ViENetworkObserver;
class ViERTCPObserver;
class ViERTPObserver;
class VoEVideoSync;

class ViEChannel
    : public VCMFrameTypeCallback,
      public VCMReceiveCallback,
      public VCMReceiveStatisticsCallback,
      public VCMPacketRequestCallback,
      public VCMFrameStorageCallback,
      public RtcpFeedback,
      public RtpFeedback,
      public ViEFrameProviderBase {
 public:
  friend class ChannelStatsObserver;

  ViEChannel(int32_t channel_id,
             int32_t engine_id,
             uint32_t number_of_cores,
             ProcessThread& module_process_thread,
             RtcpIntraFrameObserver* intra_frame_observer,
             RtcpBandwidthObserver* bandwidth_observer,
             RemoteBitrateEstimator* remote_bitrate_estimator,
             RtcpRttObserver* rtt_observer,
             PacedSender* paced_sender,
             RtpRtcp* default_rtp_rtcp,
             bool sender);
  ~ViEChannel();

  int32_t Init();

  // Sets the encoder to use for the channel. |new_stream| indicates the encoder
  // type has changed and we should start a new RTP stream.
  int32_t SetSendCodec(const VideoCodec& video_codec, bool new_stream = true);
  int32_t SetReceiveCodec(const VideoCodec& video_codec);
  int32_t GetReceiveCodec(VideoCodec* video_codec);
  int32_t RegisterCodecObserver(ViEDecoderObserver* observer);
  // Registers an external decoder. |buffered_rendering| means that the decoder
  // will render frames after decoding according to the render timestamp
  // provided by the video coding module. |render_delay| indicates the time
  // needed to decode and render a frame.
  int32_t RegisterExternalDecoder(const uint8_t pl_type,
                                  VideoDecoder* decoder,
                                  bool buffered_rendering,
                                  int32_t render_delay);
  int32_t DeRegisterExternalDecoder(const uint8_t pl_type);
  int32_t ReceiveCodecStatistics(uint32_t* num_key_frames,
                                 uint32_t* num_delta_frames);
  uint32_t DiscardedPackets() const;

  // Returns the estimated delay in milliseconds.
  int ReceiveDelay() const;

  // Only affects calls to SetReceiveCodec done after this call.
  int32_t WaitForKeyFrame(bool wait);

  // If enabled, a key frame request will be sent as soon as there are lost
  // packets. If |only_key_frames| are set, requests are only sent for loss in
  // key frames.
  int32_t SetSignalPacketLossStatus(bool enable, bool only_key_frames);

  int32_t SetRTCPMode(const RTCPMethod rtcp_mode);
  int32_t GetRTCPMode(RTCPMethod* rtcp_mode);
  int32_t SetNACKStatus(const bool enable);
  int32_t SetFECStatus(const bool enable,
                       const unsigned char payload_typeRED,
                       const unsigned char payload_typeFEC);
  int32_t SetHybridNACKFECStatus(const bool enable,
                                 const unsigned char payload_typeRED,
                                 const unsigned char payload_typeFEC);
  int SetSenderBufferingMode(int target_delay_ms);
  int SetReceiverBufferingMode(int target_delay_ms);
  int32_t SetKeyFrameRequestMethod(const KeyFrameRequestMethod method);
  bool EnableRemb(bool enable);
  int SetSendTimestampOffsetStatus(bool enable, int id);
  int SetReceiveTimestampOffsetStatus(bool enable, int id);
  void SetTransmissionSmoothingStatus(bool enable);
  int32_t EnableTMMBR(const bool enable);
  int32_t EnableKeyFrameRequestCallback(const bool enable);

  // Sets SSRC for outgoing stream.
  int32_t SetSSRC(const uint32_t SSRC,
                  const StreamType usage,
                  const unsigned char simulcast_idx);

  // Gets SSRC for outgoing stream number |idx|.
  int32_t GetLocalSSRC(uint8_t idx, unsigned int* ssrc);

  // Gets SSRC for the incoming stream.
  int32_t GetRemoteSSRC(uint32_t* ssrc);

  // Gets the CSRC for the incoming stream.
  int32_t GetRemoteCSRC(uint32_t CSRCs[kRtpCsrcSize]);

  int SetRtxSendPayloadType(int payload_type);
  void SetRtxReceivePayloadType(int payload_type);

  // Sets the starting sequence number, must be called before StartSend.
  int32_t SetStartSequenceNumber(uint16_t sequence_number);

  // Sets the CName for the outgoing stream on the channel.
  int32_t SetRTCPCName(const char rtcp_cname[]);

  // Gets the CName for the outgoing stream on the channel.
  int32_t GetRTCPCName(char rtcp_cname[]);

  // Gets the CName of the incoming stream.
  int32_t GetRemoteRTCPCName(char rtcp_cname[]);
  int32_t RegisterRtpObserver(ViERTPObserver* observer);
  int32_t RegisterRtcpObserver(ViERTCPObserver* observer);
  int32_t SendApplicationDefinedRTCPPacket(
      const uint8_t sub_type,
      uint32_t name,
      const uint8_t* data,
      uint16_t data_length_in_bytes);

  // Returns statistics reported by the remote client in an RTCP packet.
  int32_t GetSendRtcpStatistics(uint16_t* fraction_lost,
                                uint32_t* cumulative_lost,
                                uint32_t* extended_max,
                                uint32_t* jitter_samples,
                                int32_t* rtt_ms);

  // Returns our localy created statistics of the received RTP stream.
  int32_t GetReceivedRtcpStatistics(uint16_t* fraction_lost,
                                    uint32_t* cumulative_lost,
                                    uint32_t* extended_max,
                                    uint32_t* jitter_samples,
                                    int32_t* rtt_ms);

  // Gets sent/received packets statistics.
  int32_t GetRtpStatistics(uint32_t* bytes_sent,
                           uint32_t* packets_sent,
                           uint32_t* bytes_received,
                           uint32_t* packets_received) const;
  void GetBandwidthUsage(uint32_t* total_bitrate_sent,
                         uint32_t* video_bitrate_sent,
                         uint32_t* fec_bitrate_sent,
                         uint32_t* nackBitrateSent) const;
  void GetEstimatedReceiveBandwidth(uint32_t* estimated_bandwidth) const;

  int32_t StartRTPDump(const char file_nameUTF8[1024],
                       RTPDirections direction);
  int32_t StopRTPDump(RTPDirections direction);

  // Implements RtcpFeedback.
  // TODO(pwestin) Depricate this functionality.
  virtual void OnApplicationDataReceived(const int32_t id,
                                         const uint8_t sub_type,
                                         const uint32_t name,
                                         const uint16_t length,
                                         const uint8_t* data);
  virtual void OnSendReportReceived(const int32_t id,
                                    const uint32_t senderSSRC,
                                    uint32_t ntp_secs,
                                    uint32_t ntp_frac,
                                    uint32_t timestamp);
  // Implements RtpFeedback.
  virtual int32_t OnInitializeDecoder(
      const int32_t id,
      const int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const uint8_t channels,
      const uint32_t rate);
  virtual void OnPacketTimeout(const int32_t id);
  virtual void OnReceivedPacket(const int32_t id,
                                const RtpRtcpPacketType packet_type);
  virtual void OnPeriodicDeadOrAlive(const int32_t id,
                                     const RTPAliveType alive);
  virtual void OnIncomingSSRCChanged(const int32_t id,
                                     const uint32_t SSRC);
  virtual void OnIncomingCSRCChanged(const int32_t id,
                                     const uint32_t CSRC,
                                     const bool added);

  int32_t SetLocalReceiver(const uint16_t rtp_port,
                           const uint16_t rtcp_port,
                           const char* ip_address);
  int32_t GetLocalReceiver(uint16_t* rtp_port,
                           uint16_t* rtcp_port,
                           char* ip_address) const;
  int32_t SetSendDestination(const char* ip_address,
                             const uint16_t rtp_port,
                             const uint16_t rtcp_port,
                             const uint16_t source_rtp_port,
                             const uint16_t source_rtcp_port);
  int32_t GetSendDestination(char* ip_address,
                             uint16_t* rtp_port,
                             uint16_t* rtcp_port,
                             uint16_t* source_rtp_port,
                             uint16_t* source_rtcp_port) const;
  int32_t GetSourceInfo(uint16_t* rtp_port,
                        uint16_t* rtcp_port,
                        char* ip_address,
                        uint32_t ip_address_length);

  int32_t SetRemoteSSRCType(const StreamType usage, const uint32_t SSRC) const;

  int32_t StartSend();
  int32_t StopSend();
  bool Sending();
  int32_t StartReceive();
  int32_t StopReceive();

  int32_t RegisterSendTransport(Transport* transport);
  int32_t DeregisterSendTransport();

  // Incoming packet from external transport.
  int32_t ReceivedRTPPacket(const void* rtp_packet,
                            const int32_t rtp_packet_length);

  // Incoming packet from external transport.
  int32_t ReceivedRTCPPacket(const void* rtcp_packet,
                             const int32_t rtcp_packet_length);

  // Sets the maximum transfer unit size for the network link, i.e. including
  // IP, UDP and RTP headers.
  int32_t SetMTU(uint16_t mtu);

  // Returns maximum allowed payload size, i.e. the maximum allowed size of
  // encoded data in each packet.
  uint16_t MaxDataPayloadLength() const;
  int32_t SetMaxPacketBurstSize(uint16_t max_number_of_packets);
  int32_t SetPacketBurstSpreadState(bool enable, const uint16_t frame_periodMS);

  int32_t SetPacketTimeoutNotification(bool enable, uint32_t timeout_seconds);
  int32_t RegisterNetworkObserver(ViENetworkObserver* observer);
  bool NetworkObserverRegistered();
  int32_t SetPeriodicDeadOrAliveStatus(
      const bool enable, const uint32_t sample_time_seconds);

  int32_t EnableColorEnhancement(bool enable);

  // Gets the modules used by the channel.
  RtpRtcp* rtp_rtcp();

  CallStatsObserver* GetStatsObserver();

  // Implements VCMReceiveCallback.
  virtual int32_t FrameToRender(I420VideoFrame& video_frame);  // NOLINT

  // Implements VCMReceiveCallback.
  virtual int32_t ReceivedDecodedReferenceFrame(
      const uint64_t picture_id);

  // Implements VCM.
  virtual int32_t StoreReceivedFrame(
      const EncodedVideoData& frame_to_store);

  // Implements VideoReceiveStatisticsCallback.
  virtual int32_t ReceiveStatistics(const uint32_t bit_rate,
                                    const uint32_t frame_rate);

  // Implements VideoFrameTypeCallback.
  virtual int32_t RequestKeyFrame();

  // Implements VideoFrameTypeCallback.
  virtual int32_t SliceLossIndicationRequest(
      const uint64_t picture_id);

  // Implements VideoPacketRequestCallback.
  virtual int32_t ResendPackets(const uint16_t* sequence_numbers,
                                uint16_t length);

  int32_t RegisterExternalEncryption(Encryption* encryption);
  int32_t DeRegisterExternalEncryption();

  int32_t SetVoiceChannel(int32_t ve_channel_id,
                          VoEVideoSync* ve_sync_interface);
  int32_t VoiceChannel();

  // Implements ViEFrameProviderBase.
  virtual int FrameCallbackChanged() {return -1;}

  int32_t RegisterEffectFilter(ViEEffectFilter* effect_filter);

  ViEFileRecorder& GetIncomingFileRecorder();
  void ReleaseIncomingFileRecorder();

 protected:
  static bool ChannelDecodeThreadFunction(void* obj);
  bool ChannelDecodeProcess();

  void OnRttUpdate(uint32_t rtt);

 private:
  // Assumed to be protected.
  int32_t StartDecodeThread();
  int32_t StopDecodeThread();

  int32_t ProcessNACKRequest(const bool enable);
  int32_t ProcessFECRequest(const bool enable,
                            const unsigned char payload_typeRED,
                            const unsigned char payload_typeFEC);
  // Compute NACK list parameters for the buffering mode.
  int GetRequiredNackListSize(int target_delay_ms);

  int32_t channel_id_;
  int32_t engine_id_;
  uint32_t number_of_cores_;
  uint8_t num_socket_threads_;

  // Used for all registered callbacks except rendering.
  scoped_ptr<CriticalSectionWrapper> callback_cs_;
  scoped_ptr<CriticalSectionWrapper> rtp_rtcp_cs_;

  RtpRtcp* default_rtp_rtcp_;

  // Owned modules/classes.
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  std::list<RtpRtcp*> simulcast_rtp_rtcp_;
  std::list<RtpRtcp*> removed_rtp_rtcp_;
  VideoCodingModule& vcm_;
  ViEReceiver vie_receiver_;
  ViESender vie_sender_;
  ViESyncModule vie_sync_;

  // Helper to report call statistics.
  scoped_ptr<ChannelStatsObserver> stats_observer_;

  // Not owned.
  ProcessThread& module_process_thread_;
  ViEDecoderObserver* codec_observer_;
  bool do_key_frame_callbackRequest_;
  ViERTPObserver* rtp_observer_;
  ViERTCPObserver* rtcp_observer_;
  ViENetworkObserver* networkObserver_;
  RtcpIntraFrameObserver* intra_frame_observer_;
  RtcpRttObserver* rtt_observer_;
  PacedSender* paced_sender_;

  scoped_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  bool rtp_packet_timeout_;
  int send_timestamp_extension_id_;
  bool using_packet_spread_;

  Transport* external_transport_;

  bool decoder_reset_;
  bool wait_for_key_frame_;
  ThreadWrapper* decode_thread_;

  Encryption* external_encryption_;

  ViEEffectFilter* effect_filter_;
  bool color_enhancement_;

  ViEFileRecorder file_recorder_;

  // User set MTU, -1 if not set.
  uint16_t mtu_;
  const bool sender_;

  int nack_history_size_sender_;
  int max_nack_reordering_threshold_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_
