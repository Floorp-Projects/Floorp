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
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_frame_provider_base.h"
#include "webrtc/video_engine/vie_receiver.h"
#include "webrtc/video_engine/vie_sender.h"
#include "webrtc/video_engine/vie_sync_module.h"

namespace webrtc {

class CallStatsObserver;
class ChannelStatsObserver;
class Config;
class CriticalSectionWrapper;
class EncodedImageCallback;
class I420FrameCallback;
class PacedSender;
class ProcessThread;
class RtcpRttStats;
class RtpRtcp;
class ThreadWrapper;
class ViEDecoderObserver;
class ViEEffectFilter;
class ViERTCPObserver;
class ViERTPObserver;
class VideoCodingModule;
class VideoDecoder;
class VideoRenderCallback;
class VoEVideoSync;

struct SenderInfo;

class ViEChannel
    : public VCMFrameTypeCallback,
      public VCMReceiveCallback,
      public VCMReceiveStatisticsCallback,
      public VCMDecoderTimingCallback,
      public VCMPacketRequestCallback,
      public VCMReceiveStateCallback,
      public RtcpFeedback,
      public RtpFeedback,
      public ViEFrameProviderBase {
 public:
  friend class ChannelStatsObserver;

  ViEChannel(int32_t channel_id,
             int32_t engine_id,
             uint32_t number_of_cores,
             const Config& config,
             ProcessThread& module_process_thread,
             RtcpIntraFrameObserver* intra_frame_observer,
             RtcpBandwidthObserver* bandwidth_observer,
             RemoteBitrateEstimator* remote_bitrate_estimator,
             RtcpRttStats* rtt_stats,
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
  int SetSendAbsoluteSendTimeStatus(bool enable, int id);
  int SetReceiveAbsoluteSendTimeStatus(bool enable, int id);
  bool GetReceiveAbsoluteSendTimeStatus() const;
  void SetRtcpXrRrtrStatus(bool enable);
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
  // Only has an effect once RTX is enabled.
  void SetPadWithRedundantPayloads(bool enable);
  void SetRtxReceivePayloadType(int payload_type);

  // Sets the starting sequence number, must be called before StartSend.
  int32_t SetStartSequenceNumber(uint16_t sequence_number);

  void SetRtpStateForSsrc(uint32_t ssrc, const RtpState& rtp_state);
  RtpState GetRtpStateForSsrc(uint32_t ssrc);

  // Sets the CName for the outgoing stream on the channel.
  int32_t SetRTCPCName(const char rtcp_cname[]);

  // Gets the CName of the incoming stream.
  int32_t GetRemoteRTCPCName(char rtcp_cname[]);
  int32_t RegisterRtpObserver(ViERTPObserver* observer);
  int32_t RegisterRtcpObserver(ViERTCPObserver* observer);
  int32_t SendApplicationDefinedRTCPPacket(
      const uint8_t sub_type,
      uint32_t name,
      const uint8_t* data,
      uint16_t data_length_in_bytes);

  // Gets info (including timestamp) from last rr + remote packetcount
  // (derived from rr report + cached sender-side info).
  int32_t GetRemoteRTCPReceiverInfo(uint32_t& NTPHigh, uint32_t& NTPLow,
                                    uint32_t& receivedPacketCount,
                                    uint64_t& receivedOctetCount,
                                    uint32_t* jitterSamples,
                                    uint16_t* fractionLost,
                                    uint32_t* cumulativeLost,
                                    int32_t* rttMs);

  // Returns statistics reported by the remote client in an RTCP packet.
  int32_t GetSendRtcpStatistics(uint16_t* fraction_lost,
                                uint32_t* cumulative_lost,
                                uint32_t* extended_max,
                                uint32_t* jitter_samples,
                                int32_t* rtt_ms);

  // Called on receipt of RTCP report block from remote side.
  void RegisterSendChannelRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback);

  // Returns our localy created statistics of the received RTP stream.
  int32_t GetReceivedRtcpStatistics(uint16_t* fraction_lost,
                                    uint32_t* cumulative_lost,
                                    uint32_t* extended_max,
                                    uint32_t* jitter_samples,
                                    int32_t* rtt_ms);

  // Called on generation of RTCP stats
  void RegisterReceiveChannelRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback);

  // Gets sent/received packets statistics.
  int32_t GetRtpStatistics(uint32_t* bytes_sent,
                           uint32_t* packets_sent,
                           uint32_t* bytes_received,
                           uint32_t* packets_received) const;

  // Called on update of RTP statistics.
  void RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback);

  // Called on update of RTP statistics.
  void RegisterReceiveChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback);

  void GetRtcpPacketTypeCounters(RtcpPacketTypeCounter* packets_sent,
                                 RtcpPacketTypeCounter* packets_received) const;


  int32_t GetRemoteRTCPSenderInfo(SenderInfo* sender_info) const;

  void GetBandwidthUsage(uint32_t* total_bitrate_sent,
                         uint32_t* video_bitrate_sent,
                         uint32_t* fec_bitrate_sent,
                         uint32_t* nackBitrateSent) const;
  // TODO(holmer): Deprecated. We should use the SendSideDelayObserver instead
  // to avoid deadlocks.
  bool GetSendSideDelay(int* avg_send_delay, int* max_send_delay) const;
  void RegisterSendSideDelayObserver(SendSideDelayObserver* observer);
  void GetReceiveBandwidthEstimatorStats(
      ReceiveBandwidthEstimatorStats* output) const;

  // Called on any new send bitrate estimate.
  void RegisterSendBitrateObserver(BitrateStatisticsObserver* observer);

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
  // Implements RtpFeedback.
  virtual int32_t OnInitializeDecoder(
      const int32_t id,
      const int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const uint8_t channels,
      const uint32_t rate);
  virtual void OnIncomingSSRCChanged(const int32_t id,
                                     const uint32_t ssrc);
  virtual void OnIncomingCSRCChanged(const int32_t id,
                                     const uint32_t CSRC,
                                     const bool added);
  virtual void ResetStatistics(uint32_t);

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

  int32_t SetRemoteSSRCType(const StreamType usage, const uint32_t SSRC);

  int32_t StartSend();
  int32_t StopSend();
  bool Sending();
  int32_t StartReceive();
  int32_t StopReceive();

  int32_t RegisterSendTransport(Transport* transport);
  int32_t DeregisterSendTransport();

  // Incoming packet from external transport.
  int32_t ReceivedRTPPacket(const void* rtp_packet,
                            const int32_t rtp_packet_length,
                            const PacketTime& packet_time);

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

  int32_t EnableColorEnhancement(bool enable);

  // Gets the modules used by the channel.
  RtpRtcp* rtp_rtcp();

  CallStatsObserver* GetStatsObserver();

  // Implements VCMReceiveCallback.
  virtual int32_t FrameToRender(I420VideoFrame& video_frame);  // NOLINT

  // Implements VCMReceiveCallback.
  virtual int32_t ReceivedDecodedReferenceFrame(
      const uint64_t picture_id);

  // Implements VCMReceiveCallback.
  virtual void IncomingCodecChanged(const VideoCodec& codec);

  // Implements VCMReceiveStatisticsCallback.
  virtual int32_t OnReceiveStatisticsUpdate(const uint32_t bit_rate,
                                    const uint32_t frame_rate);

  // Implements VCMDecoderTimingCallback.
  virtual void OnDecoderTiming(int decode_ms,
                               int max_decode_ms,
                               int current_delay_ms,
                               int target_delay_ms,
                               int jitter_buffer_ms,
                               int min_playout_delay_ms,
                               int render_delay_ms);

  // Implements VideoFrameTypeCallback.
  virtual int32_t RequestKeyFrame();

  // Implements VideoFrameTypeCallback.
  virtual int32_t SliceLossIndicationRequest(
      const uint64_t picture_id);

  // Implements VideoPacketRequestCallback.
  virtual int32_t ResendPackets(const uint16_t* sequence_numbers,
                                uint16_t length);

  // Implements ReceiveStateCallback.
  virtual void ReceiveStateChange(VideoReceiveState state);

  int32_t SetVoiceChannel(int32_t ve_channel_id,
                          VoEVideoSync* ve_sync_interface);
  int32_t VoiceChannel();

  // Implements ViEFrameProviderBase.
  virtual int FrameCallbackChanged() {return -1;}

  int32_t RegisterEffectFilter(ViEEffectFilter* effect_filter);

  // New-style callbacks, used by VideoReceiveStream.
  void RegisterPreRenderCallback(I420FrameCallback* pre_render_callback);
  void RegisterPreDecodeImageCallback(
      EncodedImageCallback* pre_decode_callback);

  void RegisterSendFrameCountObserver(FrameCountObserver* observer);

  void ReceivedBWEPacket(int64_t arrival_time_ms, int payload_size,
                         const RTPHeader& header);

 protected:
  static bool ChannelDecodeThreadFunction(void* obj);
  bool ChannelDecodeProcess();

  void OnRttUpdate(uint32_t rtt);

 private:
  void ReserveRtpRtcpModules(size_t total_modules)
      EXCLUSIVE_LOCKS_REQUIRED(rtp_rtcp_cs_);
  RtpRtcp* GetRtpRtcpModule(size_t simulcast_idx) const
      EXCLUSIVE_LOCKS_REQUIRED(rtp_rtcp_cs_);
  RtpRtcp* CreateRtpRtcpModule();
  // Assumed to be protected.
  int32_t StartDecodeThread();
  int32_t StopDecodeThread();

  int32_t ProcessNACKRequest(const bool enable);
  int32_t ProcessFECRequest(const bool enable,
                            const unsigned char payload_typeRED,
                            const unsigned char payload_typeFEC);
  // Compute NACK list parameters for the buffering mode.
  int GetRequiredNackListSize(int target_delay_ms);
  void SetRtxSendStatus(bool enable);

  void UpdateHistograms();

  // ViEChannel exposes methods that allow to modify observers and callbacks
  // to be modified. Such an API-style is cumbersome to implement and maintain
  // at all the levels when comparing to only setting them at construction. As
  // so this class instantiates its children with a wrapper that can be modified
  // at a later time.
  template <class T>
  class RegisterableCallback : public T {
   public:
    RegisterableCallback()
        : critsect_(CriticalSectionWrapper::CreateCriticalSection()),
          callback_(NULL) {}

    void Set(T* callback) {
      CriticalSectionScoped cs(critsect_.get());
      callback_ = callback;
    }

   protected:
    // Note: this should be implemented with a RW-lock to allow simultaneous
    // calls into the callback. However that doesn't seem to be needed for the
    // current type of callbacks covered by this class.
    scoped_ptr<CriticalSectionWrapper> critsect_;
    T* callback_ GUARDED_BY(critsect_);

   private:
    DISALLOW_COPY_AND_ASSIGN(RegisterableCallback);
  };

  class RegisterableBitrateStatisticsObserver:
    public RegisterableCallback<BitrateStatisticsObserver> {
    virtual void Notify(const BitrateStatistics& total_stats,
                        const BitrateStatistics& retransmit_stats,
                        uint32_t ssrc) {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->Notify(total_stats, retransmit_stats, ssrc);
    }
  }
  send_bitrate_observer_;

  class RegisterableFrameCountObserver
      : public RegisterableCallback<FrameCountObserver> {
    virtual void FrameCountUpdated(FrameType frame_type,
                                   uint32_t frame_count,
                                   const unsigned int ssrc) {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->FrameCountUpdated(frame_type, frame_count, ssrc);
    }
  } send_frame_count_observer_;

  class RegisterableSendSideDelayObserver :
      public RegisterableCallback<SendSideDelayObserver> {
    virtual void SendSideDelayUpdated(int avg_delay_ms,
                                      int max_delay_ms,
                                      uint32_t ssrc) OVERRIDE {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->SendSideDelayUpdated(avg_delay_ms, max_delay_ms, ssrc);
    }
  } send_side_delay_observer_;

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
  VideoCodingModule* const vcm_;
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
  RtcpIntraFrameObserver* intra_frame_observer_;
  RtcpRttStats* rtt_stats_;
  PacedSender* paced_sender_;
  bool pad_with_redundant_payloads_;

  scoped_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  int send_timestamp_extension_id_;
  int absolute_send_time_extension_id_;

  Transport* external_transport_;

  bool decoder_reset_;
  // Current receive codec used for codec change callback.
  VideoCodec receive_codec_;
  bool wait_for_key_frame_;
  ThreadWrapper* decode_thread_;

  ViEEffectFilter* effect_filter_;
  bool color_enhancement_;

  // User set MTU, -1 if not set.
  uint16_t mtu_;
  const bool sender_;

  int nack_history_size_sender_;
  int max_nack_reordering_threshold_;
  I420FrameCallback* pre_render_callback_;
  const int64_t start_ms_;

  std::map<uint32_t, RTCPReportBlock> prev_report_blocks_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_
