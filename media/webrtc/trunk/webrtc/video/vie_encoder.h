/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIE_ENCODER_H_
#define WEBRTC_VIDEO_VIE_ENCODER_H_

#include <map>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call/bitrate_allocator.h"
#include "webrtc/common_types.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/video_capture_input.h"

namespace webrtc {

class BitrateAllocator;
class BitrateObserver;
class Config;
class CriticalSectionWrapper;
class EncodedImageCallback;
class PacedSender;
class PayloadRouter;
class ProcessThread;
class QMVideoSettingsCallback;
class SendStatisticsProxy;
class ViEBitrateObserver;
class ViEEffectFilter;
class VideoCodingModule;

class ViEEncoder : public RtcpIntraFrameObserver,
                   public VideoEncoderRateObserver,
                   public VCMPacketizationCallback,
                   public VCMSendStatisticsCallback,
                   public VideoCaptureCallback {
 public:
  friend class ViEBitrateObserver;

  ViEEncoder(uint32_t number_of_cores,
             ProcessThread* module_process_thread,
             SendStatisticsProxy* stats_proxy,
             I420FrameCallback* pre_encode_callback,
             PacedSender* pacer,
             BitrateAllocator* bitrate_allocator);
  ~ViEEncoder();

  bool Init();

  // This function is assumed to be called before any frames are delivered and
  // only once.
  // Ideally this would be done in Init, but the dependencies between ViEEncoder
  // and ViEChannel makes it really hard to do in a good way.
  void StartThreadsAndSetSharedMembers(
      rtc::scoped_refptr<PayloadRouter> send_payload_router,
      VCMProtectionCallback* vcm_protection_callback);

  // This function must be called before the corresponding ViEChannel is
  // deleted.
  void StopThreadsAndRemoveSharedMembers();

  void SetNetworkTransmissionState(bool is_transmitting);

  // Returns the id of the owning channel.
  int Owner() const;

  // Drops incoming packets before they get to the encoder.
  void Pause();
  void Restart();

  // Codec settings.
  int32_t RegisterExternalEncoder(VideoEncoder* encoder,
                                  uint8_t pl_type,
                                  bool internal_source);
  int32_t DeRegisterExternalEncoder(uint8_t pl_type);
  int32_t SetEncoder(const VideoCodec& video_codec);

  // Implementing VideoCaptureCallback.
  void DeliverFrame(VideoFrame video_frame) override;

  int32_t SendKeyFrame();

  uint32_t LastObservedBitrateBps() const;
  int CodecTargetBitrate(uint32_t* bitrate) const;
  // Loss protection. Must be called before SetEncoder() to have max packet size
  // updated according to protection.
  // TODO(pbos): Set protection method on construction or extract vcm_ outside
  // this class and set it on construction there.
  void SetProtectionMethod(bool nack, bool fec);

  // Buffering mode.
  void SetSenderBufferingMode(int target_delay_ms);

  // Implements VideoEncoderRateObserver.
  void OnSetRates(uint32_t bitrate_bps, int framerate) override;

  // Implements VCMPacketizationCallback.
  int32_t SendData(uint8_t payload_type,
                   const EncodedImage& encoded_image,
                   const RTPFragmentationHeader& fragmentation_header,
                   const RTPVideoHeader* rtp_video_hdr) override;
  void OnEncoderImplementationName(const char* implementation_name) override;

  // Implements VideoSendStatisticsCallback.
  int32_t SendStatistics(const uint32_t bit_rate,
                         const uint32_t frame_rate) override;

  // Implements RtcpIntraFrameObserver.
  void OnReceivedIntraFrameRequest(uint32_t ssrc) override;
  void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id) override;
  void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id) override;
  void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) override;

  // Sets SSRCs for all streams.
  void SetSsrcs(const std::vector<uint32_t>& ssrcs);

  void SetMinTransmitBitrate(int min_transmit_bitrate_kbps);

  // Lets the sender suspend video when the rate drops below
  // |threshold_bps|, and turns back on when the rate goes back up above
  // |threshold_bps| + |window_bps|.
  void SuspendBelowMinBitrate();

  // New-style callbacks, used by VideoSendStream.
  void RegisterPostEncodeImageCallback(
        EncodedImageCallback* post_encode_callback);

  int GetPaddingNeededBps() const;

 protected:
  // Called by BitrateObserver.
  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms);

 private:
  bool EncoderPaused() const EXCLUSIVE_LOCKS_REQUIRED(data_cs_);
  void TraceFrameDropStart() EXCLUSIVE_LOCKS_REQUIRED(data_cs_);
  void TraceFrameDropEnd() EXCLUSIVE_LOCKS_REQUIRED(data_cs_);

  const uint32_t number_of_cores_;

  const rtc::scoped_ptr<VideoProcessing> vp_;
  const rtc::scoped_ptr<QMVideoSettingsCallback> qm_callback_;
  const rtc::scoped_ptr<VideoCodingModule> vcm_;
  rtc::scoped_refptr<PayloadRouter> send_payload_router_;

  rtc::scoped_ptr<CriticalSectionWrapper> data_cs_;
  rtc::scoped_ptr<BitrateObserver> bitrate_observer_;

  SendStatisticsProxy* const stats_proxy_;
  I420FrameCallback* const pre_encode_callback_;
  PacedSender* const pacer_;
  BitrateAllocator* const bitrate_allocator_;

  // The time we last received an input frame or encoded frame. This is used to
  // track when video is stopped long enough that we also want to stop sending
  // padding.
  int64_t time_of_last_frame_activity_ms_ GUARDED_BY(data_cs_);
  VideoCodec encoder_config_ GUARDED_BY(data_cs_);
  int min_transmit_bitrate_kbps_ GUARDED_BY(data_cs_);
  uint32_t last_observed_bitrate_bps_ GUARDED_BY(data_cs_);
  int target_delay_ms_ GUARDED_BY(data_cs_);
  bool network_is_transmitting_ GUARDED_BY(data_cs_);
  bool encoder_paused_ GUARDED_BY(data_cs_);
  bool encoder_paused_and_dropped_frame_ GUARDED_BY(data_cs_);
  std::map<unsigned int, int64_t> time_last_intra_request_ms_
      GUARDED_BY(data_cs_);

  ProcessThread* module_process_thread_;

  bool has_received_sli_ GUARDED_BY(data_cs_);
  uint8_t picture_id_sli_ GUARDED_BY(data_cs_);
  bool has_received_rpsi_ GUARDED_BY(data_cs_);
  uint64_t picture_id_rpsi_ GUARDED_BY(data_cs_);
  std::map<uint32_t, int> ssrc_streams_ GUARDED_BY(data_cs_);

  bool video_suspended_ GUARDED_BY(data_cs_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIE_ENCODER_H_
