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

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "modules/udp_transport/interface/udp_transport.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "system_wrappers/interface/tick_util.h"
#include "typedefs.h"  // NOLINT
#include "video_engine/include/vie_network.h"
#include "video_engine/include/vie_rtp_rtcp.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_file_recorder.h"
#include "video_engine/vie_frame_provider_base.h"
#include "video_engine/vie_receiver.h"
#include "video_engine/vie_sender.h"
#include "video_engine/vie_sync_module.h"

namespace webrtc {

class CriticalSectionWrapper;
class Encryption;
class ProcessThread;
class RtpRtcp;
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
  ViEChannel(WebRtc_Word32 channel_id,
             WebRtc_Word32 engine_id,
             WebRtc_UWord32 number_of_cores,
             ProcessThread& module_process_thread,
             RtcpIntraFrameObserver* intra_frame_observer,
             RtcpBandwidthObserver* bandwidth_observer,
             RemoteBitrateEstimator* remote_bitrate_estimator,
             RtpRtcp* default_rtp_rtcp,
             bool sender);
  ~ViEChannel();

  WebRtc_Word32 Init();

  // Sets the encoder to use for the channel. |new_stream| indicates the encoder
  // type has changed and we should start a new RTP stream.
  WebRtc_Word32 SetSendCodec(const VideoCodec& video_codec,
                             bool new_stream = true);
  WebRtc_Word32 SetReceiveCodec(const VideoCodec& video_codec);
  WebRtc_Word32 GetReceiveCodec(VideoCodec* video_codec);
  WebRtc_Word32 RegisterCodecObserver(ViEDecoderObserver* observer);
  // Registers an external decoder. |decoder_render| is set to true if the
  // decoder will do the rendering. If |decoder_render| is set,|render_delay|
  // indicates the time needed to decode and render a frame.
  WebRtc_Word32 RegisterExternalDecoder(const WebRtc_UWord8 pl_type,
                                        VideoDecoder* decoder,
                                        bool decoder_render,
                                        WebRtc_Word32 render_delay);
  WebRtc_Word32 DeRegisterExternalDecoder(const WebRtc_UWord8 pl_type);
  WebRtc_Word32 ReceiveCodecStatistics(WebRtc_UWord32* num_key_frames,
                                       WebRtc_UWord32* num_delta_frames);
  WebRtc_UWord32 DiscardedPackets() const;

  // Only affects calls to SetReceiveCodec done after this call.
  WebRtc_Word32 WaitForKeyFrame(bool wait);

  // If enabled, a key frame request will be sent as soon as there are lost
  // packets. If |only_key_frames| are set, requests are only sent for loss in
  // key frames.
  WebRtc_Word32 SetSignalPacketLossStatus(bool enable, bool only_key_frames);

  WebRtc_Word32 SetRTCPMode(const RTCPMethod rtcp_mode);
  WebRtc_Word32 GetRTCPMode(RTCPMethod* rtcp_mode);
  WebRtc_Word32 SetNACKStatus(const bool enable);
  WebRtc_Word32 SetFECStatus(const bool enable,
                             const unsigned char payload_typeRED,
                             const unsigned char payload_typeFEC);
  WebRtc_Word32 SetHybridNACKFECStatus(const bool enable,
                                       const unsigned char payload_typeRED,
                                       const unsigned char payload_typeFEC);
  WebRtc_Word32 SetKeyFrameRequestMethod(const KeyFrameRequestMethod method);
  bool EnableRemb(bool enable);
  int SetSendTimestampOffsetStatus(bool enable, int id);
  int SetReceiveTimestampOffsetStatus(bool enable, int id);
  WebRtc_Word32 EnableTMMBR(const bool enable);
  WebRtc_Word32 EnableKeyFrameRequestCallback(const bool enable);

  // Sets SSRC for outgoing stream.
  WebRtc_Word32 SetSSRC(const uint32_t SSRC,
                        const StreamType usage,
                        const unsigned char simulcast_idx);

  // Gets SSRC for outgoing stream.
  WebRtc_Word32 GetLocalSSRC(uint32_t* ssrc);

  // Gets SSRC for the incoming stream.
  WebRtc_Word32 GetRemoteSSRC(uint32_t* ssrc);

  // Gets the CSRC for the incoming stream.
  WebRtc_Word32 GetRemoteCSRC(uint32_t CSRCs[kRtpCsrcSize]);

  // Sets the starting sequence number, must be called before StartSend.
  WebRtc_Word32 SetStartSequenceNumber(WebRtc_UWord16 sequence_number);

  // Sets the CName for the outgoing stream on the channel.
  WebRtc_Word32 SetRTCPCName(const char rtcp_cname[]);

  // Gets the CName for the outgoing stream on the channel.
  WebRtc_Word32 GetRTCPCName(char rtcp_cname[]);

  // Gets the CName of the incoming stream.
  WebRtc_Word32 GetRemoteRTCPCName(char rtcp_cname[]);
  WebRtc_Word32 RegisterRtpObserver(ViERTPObserver* observer);
  WebRtc_Word32 RegisterRtcpObserver(ViERTCPObserver* observer);
  WebRtc_Word32 SendApplicationDefinedRTCPPacket(
      const WebRtc_UWord8 sub_type,
      WebRtc_UWord32 name,
      const WebRtc_UWord8* data,
      WebRtc_UWord16 data_length_in_bytes);

  // Returns statistics reported by the remote client in an RTCP packet.
  WebRtc_Word32 GetSendRtcpStatistics(uint16_t* fraction_lost,
                                      uint32_t* cumulative_lost,
                                      uint32_t* extended_max,
                                      uint32_t* jitter_samples,
                                      int32_t* rtt_ms);

  // Returns our localy created statistics of the received RTP stream.
  WebRtc_Word32 GetReceivedRtcpStatistics(uint16_t* fraction_lost,
                                          uint32_t* cumulative_lost,
                                          uint32_t* extended_max,
                                          uint32_t* jitter_samples,
                                          int32_t* rtt_ms);

  // Gets sent/received packets statistics.
  WebRtc_Word32 GetRtpStatistics(uint32_t* bytes_sent,
                                 uint32_t* packets_sent,
                                 uint32_t* bytes_received,
                                 uint32_t* packets_received) const;
  void GetBandwidthUsage(uint32_t* total_bitrate_sent,
                         uint32_t* video_bitrate_sent,
                         uint32_t* fec_bitrate_sent,
                         uint32_t* nackBitrateSent) const;
  int GetEstimatedReceiveBandwidth(uint32_t* estimated_bandwidth) const;

  WebRtc_Word32 StartRTPDump(const char file_nameUTF8[1024],
                             RTPDirections direction);
  WebRtc_Word32 StopRTPDump(RTPDirections direction);

  // Implements RtcpFeedback.
  // TODO(pwestin) Depricate this functionality.
  virtual void OnApplicationDataReceived(const WebRtc_Word32 id,
                                         const WebRtc_UWord8 sub_type,
                                         const WebRtc_UWord32 name,
                                         const WebRtc_UWord16 length,
                                         const WebRtc_UWord8* data);
  // Implements RtpFeedback.
  virtual WebRtc_Word32 OnInitializeDecoder(
      const WebRtc_Word32 id,
      const WebRtc_Word8 payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate);
  virtual void OnPacketTimeout(const WebRtc_Word32 id);
  virtual void OnReceivedPacket(const WebRtc_Word32 id,
                                const RtpRtcpPacketType packet_type);
  virtual void OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                     const RTPAliveType alive);
  virtual void OnIncomingSSRCChanged(const WebRtc_Word32 id,
                                     const WebRtc_UWord32 SSRC);
  virtual void OnIncomingCSRCChanged(const WebRtc_Word32 id,
                                     const WebRtc_UWord32 CSRC,
                                     const bool added);

  WebRtc_Word32 SetLocalReceiver(const WebRtc_UWord16 rtp_port,
                                 const WebRtc_UWord16 rtcp_port,
                                 const char* ip_address);
  WebRtc_Word32 GetLocalReceiver(WebRtc_UWord16* rtp_port,
                                 WebRtc_UWord16* rtcp_port,
                                 char* ip_address) const;
  WebRtc_Word32 SetSendDestination(const char* ip_address,
                                   const WebRtc_UWord16 rtp_port,
                                   const WebRtc_UWord16 rtcp_port,
                                   const WebRtc_UWord16 source_rtp_port,
                                   const WebRtc_UWord16 source_rtcp_port);
  WebRtc_Word32 GetSendDestination(char* ip_address,
                                   WebRtc_UWord16* rtp_port,
                                   WebRtc_UWord16* rtcp_port,
                                   WebRtc_UWord16* source_rtp_port,
                                   WebRtc_UWord16* source_rtcp_port) const;
  WebRtc_Word32 GetSourceInfo(WebRtc_UWord16* rtp_port,
                              WebRtc_UWord16* rtcp_port,
                              char* ip_address,
                              WebRtc_UWord32 ip_address_length);

  WebRtc_Word32 SetRemoteSSRCType(const StreamType usage,
                                  const uint32_t SSRC) const;

  WebRtc_Word32 StartSend();
  WebRtc_Word32 StopSend();
  bool Sending();
  WebRtc_Word32 StartReceive();
  WebRtc_Word32 StopReceive();
  bool Receiving();

  WebRtc_Word32 RegisterSendTransport(Transport* transport);
  WebRtc_Word32 DeregisterSendTransport();

  // Incoming packet from external transport.
  WebRtc_Word32 ReceivedRTPPacket(const void* rtp_packet,
                                  const WebRtc_Word32 rtp_packet_length);

  // Incoming packet from external transport.
  WebRtc_Word32 ReceivedRTCPPacket(const void* rtcp_packet,
                                   const WebRtc_Word32 rtcp_packet_length);

  WebRtc_Word32 EnableIPv6();
  bool IsIPv6Enabled();
  WebRtc_Word32 SetSourceFilter(const WebRtc_UWord16 rtp_port,
                                const WebRtc_UWord16 rtcp_port,
                                const char* ip_address);
  WebRtc_Word32 GetSourceFilter(WebRtc_UWord16* rtp_port,
                                WebRtc_UWord16* rtcp_port,
                                char* ip_address) const;

  WebRtc_Word32 SetToS(const WebRtc_Word32 DSCP, const bool use_set_sockOpt);
  WebRtc_Word32 GetToS(WebRtc_Word32* DSCP, bool* use_set_sockOpt) const;
  WebRtc_Word32 SetSendGQoS(const bool enable,
                            const WebRtc_Word32 service_type,
                            const WebRtc_UWord32 max_bitrate,
                            const WebRtc_Word32 overrideDSCP);
  WebRtc_Word32 GetSendGQoS(bool* enabled,
                            WebRtc_Word32* service_type,
                            WebRtc_Word32* overrideDSCP) const;

  // Sets the maximum transfer unit size for the network link, i.e. including
  // IP, UDP and RTP headers.
  WebRtc_Word32 SetMTU(WebRtc_UWord16 mtu);

  // Returns maximum allowed payload size, i.e. the maximum allowed size of
  // encoded data in each packet.
  WebRtc_UWord16 MaxDataPayloadLength() const;
  WebRtc_Word32 SetMaxPacketBurstSize(WebRtc_UWord16 max_number_of_packets);
  WebRtc_Word32 SetPacketBurstSpreadState(bool enable,
                                          const WebRtc_UWord16 frame_periodMS);

  WebRtc_Word32 SetPacketTimeoutNotification(bool enable,
                                             WebRtc_UWord32 timeout_seconds);
  WebRtc_Word32 RegisterNetworkObserver(ViENetworkObserver* observer);
  bool NetworkObserverRegistered();
  WebRtc_Word32 SetPeriodicDeadOrAliveStatus(
      const bool enable, const WebRtc_UWord32 sample_time_seconds);

  WebRtc_Word32 SendUDPPacket(const WebRtc_Word8* data,
                              const WebRtc_UWord32 length,
                              WebRtc_Word32& transmitted_bytes,
                              bool use_rtcp_socket);

  WebRtc_Word32 EnableColorEnhancement(bool enable);

  // Gets the modules used by the channel.
  RtpRtcp* rtp_rtcp();

  // Implements VCMReceiveCallback.
  virtual WebRtc_Word32 FrameToRender(VideoFrame& video_frame);  // NOLINT

  // Implements VCMReceiveCallback.
  virtual WebRtc_Word32 ReceivedDecodedReferenceFrame(
      const WebRtc_UWord64 picture_id);

  // Implements VCM.
  virtual WebRtc_Word32 StoreReceivedFrame(
      const EncodedVideoData& frame_to_store);

  // Implements VideoReceiveStatisticsCallback.
  virtual WebRtc_Word32 ReceiveStatistics(const WebRtc_UWord32 bit_rate,
                                          const WebRtc_UWord32 frame_rate);

  // Implements VideoFrameTypeCallback.
  virtual WebRtc_Word32 RequestKeyFrame();

  // Implements VideoFrameTypeCallback.
  virtual WebRtc_Word32 SliceLossIndicationRequest(
      const WebRtc_UWord64 picture_id);

  // Implements VideoPacketRequestCallback.
  virtual WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequence_numbers,
                                      WebRtc_UWord16 length);

  WebRtc_Word32 RegisterExternalEncryption(Encryption* encryption);
  WebRtc_Word32 DeRegisterExternalEncryption();

  WebRtc_Word32 SetVoiceChannel(WebRtc_Word32 ve_channel_id,
                                VoEVideoSync* ve_sync_interface);
  WebRtc_Word32 VoiceChannel();

  // Implements ViEFrameProviderBase.
  virtual int FrameCallbackChanged() {return -1;}

  WebRtc_Word32 RegisterEffectFilter(ViEEffectFilter* effect_filter);

  ViEFileRecorder& GetIncomingFileRecorder();
  void ReleaseIncomingFileRecorder();

 protected:
  static bool ChannelDecodeThreadFunction(void* obj);
  bool ChannelDecodeProcess();

 private:
  // Assumed to be protected.
  WebRtc_Word32 StartDecodeThread();
  WebRtc_Word32 StopDecodeThread();

  WebRtc_Word32 ProcessNACKRequest(const bool enable);
  WebRtc_Word32 ProcessFECRequest(const bool enable,
                                  const unsigned char payload_typeRED,
                                  const unsigned char payload_typeFEC);

  WebRtc_Word32 channel_id_;
  WebRtc_Word32 engine_id_;
  WebRtc_UWord32 number_of_cores_;
  WebRtc_UWord8 num_socket_threads_;

  // Used for all registered callbacks except rendering.
  scoped_ptr<CriticalSectionWrapper> callback_cs_;
  scoped_ptr<CriticalSectionWrapper> rtp_rtcp_cs_;

  RtpRtcp* default_rtp_rtcp_;

  // Owned modules/classes.
  scoped_ptr<RtpRtcp> rtp_rtcp_;
  std::list<RtpRtcp*> simulcast_rtp_rtcp_;
#ifndef WEBRTC_EXTERNAL_TRANSPORT
  UdpTransport& socket_transport_;
#endif
  VideoCodingModule& vcm_;
  ViEReceiver vie_receiver_;
  ViESender vie_sender_;
  ViESyncModule vie_sync_;

  // Not owned.
  ProcessThread& module_process_thread_;
  ViEDecoderObserver* codec_observer_;
  bool do_key_frame_callbackRequest_;
  ViERTPObserver* rtp_observer_;
  ViERTCPObserver* rtcp_observer_;
  ViENetworkObserver* networkObserver_;
  RtcpIntraFrameObserver* intra_frame_observer_;
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

  // Time when RTT time was last reported to VCM JB.
  TickTime vcm_rttreported_;

  ViEFileRecorder file_recorder_;

  // User set MTU, -1 if not set.
  uint16_t mtu_;
  const bool sender_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_
