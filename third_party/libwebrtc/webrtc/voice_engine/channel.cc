/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/channel.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/include/module_common_types.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_packet_observer.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "modules/rtp_rtcp/include/rtp_receiver.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "voice_engine/utility.h"

namespace webrtc {
namespace voe {

namespace {

constexpr double kAudioSampleDurationSeconds = 0.01;
constexpr int64_t kMaxRetransmissionWindowMs = 1000;
constexpr int64_t kMinRetransmissionWindowMs = 30;

// Video Sync.
constexpr int kVoiceEngineMinMinPlayoutDelayMs = 0;
constexpr int kVoiceEngineMaxMinPlayoutDelayMs = 10000;

}  // namespace

const int kTelephoneEventAttenuationdB = 10;

class RtcEventLogProxy final : public webrtc::RtcEventLog {
 public:
  RtcEventLogProxy() : event_log_(nullptr) {}

  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms) override {
    RTC_NOTREACHED();
    return false;
  }

  void StopLogging() override { RTC_NOTREACHED(); }

  void Log(std::unique_ptr<RtcEvent> event) override {
    rtc::CritScope lock(&crit_);
    if (event_log_) {
      event_log_->Log(std::move(event));
    }
  }

  void SetEventLog(RtcEventLog* event_log) {
    rtc::CritScope lock(&crit_);
    event_log_ = event_log;
  }

 private:
  rtc::CriticalSection crit_;
  RtcEventLog* event_log_ RTC_GUARDED_BY(crit_);
  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogProxy);
};

class RtcpRttStatsProxy final : public RtcpRttStats {
 public:
  RtcpRttStatsProxy() : rtcp_rtt_stats_(nullptr) {}

  void OnRttUpdate(int64_t rtt) override {
    rtc::CritScope lock(&crit_);
    if (rtcp_rtt_stats_)
      rtcp_rtt_stats_->OnRttUpdate(rtt);
  }

  int64_t LastProcessedRtt() const override {
    rtc::CritScope lock(&crit_);
    if (!rtcp_rtt_stats_)
      return 0;
    return rtcp_rtt_stats_->LastProcessedRtt();
  }

  void SetRtcpRttStats(RtcpRttStats* rtcp_rtt_stats) {
    rtc::CritScope lock(&crit_);
    rtcp_rtt_stats_ = rtcp_rtt_stats;
  }

 private:
  rtc::CriticalSection crit_;
  RtcpRttStats* rtcp_rtt_stats_ RTC_GUARDED_BY(crit_);
  RTC_DISALLOW_COPY_AND_ASSIGN(RtcpRttStatsProxy);
};

// Extend the default RTCP statistics struct with max_jitter, defined as the
// maximum jitter value seen in an RTCP report block.
struct ChannelStatistics : public RtcpStatistics {
  ChannelStatistics() : rtcp(), max_jitter(0) {}

  RtcpStatistics rtcp;
  uint32_t max_jitter;
};

// Statistics callback, called at each generation of a new RTCP report block.
class StatisticsProxy : public RtcpStatisticsCallback,
   public RtcpPacketTypeCounterObserver {
 public:
  StatisticsProxy(uint32_t ssrc) : ssrc_(ssrc) {}
  virtual ~StatisticsProxy() {}

  void StatisticsUpdated(const RtcpStatistics& statistics,
                         uint32_t ssrc) override {
    rtc::CritScope cs(&stats_lock_);
    if (ssrc != ssrc_)
      return;

    stats_.rtcp = statistics;
    if (statistics.jitter > stats_.max_jitter) {
      stats_.max_jitter = statistics.jitter;
    }
  }

  void CNameChanged(const char* cname, uint32_t ssrc) override {}

  void SetSSRC(uint32_t ssrc) {
    rtc::CritScope cs(&stats_lock_);
    ssrc_ = ssrc;
    mReceiverReportDerivedStats.clear();
    mInitialSequenceNumber.reset();
  }

  ChannelStatistics GetStats() {
    rtc::CritScope cs(&stats_lock_);
    return stats_;
  }

  // These can be created before reports are received so that information
  // needed to derive certain stats (e.g. PacketsReceived) can be stored.
  class ReceiverReportDerivedStats {
  public:
    // Event handler for incoming RTCP Receiver Reports
    void UpdateWithReceiverReport(const RTCPReportBlock& aReceiverReport,
                                  rtc::Optional<uint32_t> initialSequenceNum,
                                  int64_t aRoundTripTime,
                                  uint32_t aEncoderFrequencyHz,
                                  int64_t aReceptionTime)
    {
      if (!mFirstExtendedSequenceNumber && initialSequenceNum) {
        mFirstExtendedSequenceNumber = *initialSequenceNum;
      }

      // No initial sequence number available!
      if (!mFirstExtendedSequenceNumber) {
        RTC_LOG(LS_WARNING) <<
                     "ReceiverReportDerivedStats::UpdateWithReceiverReport()"
                     " called before a first sequence number is known to the"
                     " StatisticsProxy";
        // This is as good a guess as we can get if the initial
        // sequence number is not known
        mFirstExtendedSequenceNumber = static_cast<uint32_t>(
            std::max<int64_t>(0, aReceiverReport.extended_highest_sequence_number -
                                 aReceiverReport.packets_lost));
      }

      mReceiverSsrc = aReceiverReport.sender_ssrc;
      mSenderSsrc = aReceiverReport.source_ssrc;
      mLatestHighExtendedSequenceNumber = aReceiverReport.extended_highest_sequence_number;
      mLatestReceiverReportReceptionTime = aReceptionTime;
      mFractionOfPacketsLostInQ8 = aReceiverReport.fraction_lost;
      mJitterInSamples = aReceiverReport.jitter;
      mEncoderFrequencyHz = aEncoderFrequencyHz;
      mCumulativePacketsLost = aReceiverReport.packets_lost;
      mLastSenderReportTimestamp = aReceiverReport.last_sender_report_timestamp;
      mDelaySinceLastSenderReport = aReceiverReport.delay_since_last_sender_report;
      mRoundTripTime = aRoundTripTime;
    }
    bool HasReceivedReport() { return mFirstReceiverReportReceptionTime; }
    // This is the SSRC of the entity sending the RTCP Receiver Reports
    // That is it is the SSRC of the RTP receiver
    uint32_t mReceiverSsrc = 0;
    // This is the SSRC of the entity receiving the RTCP Receiver Reports
    // That is it is the SSRC of the RTP sender
    uint32_t mSenderSsrc = 0;
    // Reception time for the RTCP packet containing this data
    // Only available if an receiver report has been received
    int64_t mLatestReceiverReportReceptionTime = 0;
    // Reception time for the first RTCP packet contianing a
    // Receiver Report with match mReceiverSsrc.
    int64_t mFirstReceiverReportReceptionTime = 0;
    // The total number of packets know to be lost
    uint32_t mCumulativePacketsLost = 0;
    // The RTP sender must record the first sequence number used
    // so that number of packets received can be calculated from ...
    uint32_t mFirstExtendedSequenceNumber = 0;
    // The most recent sequence number seen by the receiver at the time
    // Receiver Report was generated
    uint32_t mLatestHighExtendedSequenceNumber = 0;
    int64_t mRoundTripTime = 0;
    // The amount of jitter measured in MS, derived from the
    // RTCP reported jitter (measured in frames), and the
    // effective playout frequency.
    double JitterMs() const {
      if (!mEncoderFrequencyHz) {
        if (!mHasWarnedAboutNoFrequency) {
          mHasWarnedAboutNoFrequency = true;
          RTC_LOG(LS_WARNING) <<
                  "ReceiverReportDerivedStats::JitterMs() called before"
                  " the playout frequency is known.";
        }
        return 0;
      }
      return (mJitterInSamples * 1000) / mEncoderFrequencyHz;
    }
    // Fraction of packets lost
    double FractionOfPacketsLost() const {
      return (double) mFractionOfPacketsLostInQ8 / 256;
    }
    uint32_t PacketsReceived() const {
      return static_cast<uint32_t>(std::max<int64_t>(0,
        (int64_t) mLatestHighExtendedSequenceNumber -
             (mFirstExtendedSequenceNumber + mCumulativePacketsLost)));
    }
  private:
    // The ratio of packets lost to total packets sent expressed
    // as the dividend in X / 256.
    uint8_t mFractionOfPacketsLostInQ8 = 0;
    // Jitter in the RTCP packet is in Time Units,
    // which is the sample rate of the audio.
    uint32_t mJitterInSamples = 0;
    // Use to calculate the jitter
    uint32_t mEncoderFrequencyHz = 0;
    // Used to calculate the RTT
    uint32_t mLastSenderReportTimestamp = 0;
    // Used to calculate the RTT
    uint32_t mDelaySinceLastSenderReport = 0;
    // Only warn about jitter calculation once per instance
    mutable bool mHasWarnedAboutNoFrequency = false;
  };

  void RtcpPacketTypesCounterUpdated(uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override {
    rtc::CritScope cs(&stats_lock_);
    if (ssrc != ssrc_) {
      return;
    }
    packet_counter_ = packet_counter;
 };

 // Called when we receive RTCP receiver reports
 void OnIncomingReceiverReports(const ReportBlockList & mReceiverReports,
                                const int64_t aRoundTripTime,
                                const int64_t aReceptionTime) {
    if (!mReceiverReports.empty()) { // Don't lock if we have nothing to do.
      rtc::CritScope cs(&stats_lock_);
      for(const auto& report : mReceiverReports) {
        // Creates a new report if necessary before updating
        ReceiverReportDerivedStats newStats;
        mReceiverReportDerivedStats.emplace(report.source_ssrc, newStats)
          .first->second.UpdateWithReceiverReport(report,
                                                  mInitialSequenceNumber,
                                                  aRoundTripTime,
                                                  mPlayoutFrequency,
                                                  aReceptionTime);
      }
    }
  }

  void OnSendCodecFrequencyChanged(uint32_t aFrequency) {
    rtc::CritScope cs(&stats_lock_);
    mPlayoutFrequency = aFrequency;
  }

  void OnInitialSequenceNumberSet(uint32_t aSequenceNumber) {
    rtc::CritScope cs(&stats_lock_);
    mInitialSequenceNumber.emplace(aSequenceNumber);
    mReceiverReportDerivedStats.clear();
  }

  const rtc::Optional<ReceiverReportDerivedStats>
  GetReceiverReportDerivedStats(const uint32_t receiverSsrc) const {
    rtc::CritScope cs(&stats_lock_);
    const auto& it = mReceiverReportDerivedStats.find(receiverSsrc);
    if (it != mReceiverReportDerivedStats.end()) {
      return rtc::Optional<ReceiverReportDerivedStats>(it->second);
    }
    return rtc::Optional<ReceiverReportDerivedStats>();
  }

  void GetPacketTypeCounter(RtcpPacketTypeCounter& aPacketTypeCounter) {
    rtc::CritScope cs(&stats_lock_);
    aPacketTypeCounter = packet_counter_;
  }

 private:
  // StatisticsUpdated calls are triggered from threads in the RTP module,
  // while GetStats calls can be triggered from the public voice engine API,
  // hence synchronization is needed.
  rtc::CriticalSection stats_lock_;
  uint32_t ssrc_;
  ChannelStatistics stats_;
  RtcpPacketTypeCounter packet_counter_;

  // receiver report handling, maps ssrc -> stats
  std::map<uint32_t, ReceiverReportDerivedStats> mReceiverReportDerivedStats;
  // store initial sender sequence number
  rtc::Optional<uint32_t> mInitialSequenceNumber;
  uint32_t mPlayoutFrequency;
 };

class TransportFeedbackProxy : public TransportFeedbackObserver {
 public:
  TransportFeedbackProxy() : feedback_observer_(nullptr) {
    pacer_thread_.DetachFromThread();
    network_thread_.DetachFromThread();
  }

  void SetTransportFeedbackObserver(
      TransportFeedbackObserver* feedback_observer) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    feedback_observer_ = feedback_observer;
  }

  // Implements TransportFeedbackObserver.
  void AddPacket(uint32_t ssrc,
                 uint16_t sequence_number,
                 size_t length,
                 const PacedPacketInfo& pacing_info) override {
    RTC_DCHECK(pacer_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->AddPacket(ssrc, sequence_number, length, pacing_info);
  }

  void OnTransportFeedback(const rtcp::TransportFeedback& feedback) override {
    RTC_DCHECK(network_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (feedback_observer_)
      feedback_observer_->OnTransportFeedback(feedback);
  }
  std::vector<PacketFeedback> GetTransportFeedbackVector() const override {
    RTC_NOTREACHED();
    return std::vector<PacketFeedback>();
  }

 private:
  rtc::CriticalSection crit_;
  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker pacer_thread_;
  rtc::ThreadChecker network_thread_;
  TransportFeedbackObserver* feedback_observer_ RTC_GUARDED_BY(&crit_);
};

class TransportSequenceNumberProxy : public TransportSequenceNumberAllocator {
 public:
  TransportSequenceNumberProxy() : seq_num_allocator_(nullptr) {
    pacer_thread_.DetachFromThread();
  }

  void SetSequenceNumberAllocator(
      TransportSequenceNumberAllocator* seq_num_allocator) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    seq_num_allocator_ = seq_num_allocator;
  }

  // Implements TransportSequenceNumberAllocator.
  uint16_t AllocateSequenceNumber() override {
    RTC_DCHECK(pacer_thread_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    if (!seq_num_allocator_)
      return 0;
    return seq_num_allocator_->AllocateSequenceNumber();
  }

 private:
  rtc::CriticalSection crit_;
  rtc::ThreadChecker thread_checker_;
  rtc::ThreadChecker pacer_thread_;
  TransportSequenceNumberAllocator* seq_num_allocator_ RTC_GUARDED_BY(&crit_);
};

class RtpPacketSenderProxy : public RtpPacketSender {
 public:
  RtpPacketSenderProxy() : rtp_packet_sender_(nullptr) {}

  void SetPacketSender(RtpPacketSender* rtp_packet_sender) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    rtc::CritScope lock(&crit_);
    rtp_packet_sender_ = rtp_packet_sender;
  }

  // Implements RtpPacketSender.
  void InsertPacket(Priority priority,
                    uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    size_t bytes,
                    bool retransmission) override {
    rtc::CritScope lock(&crit_);
    if (rtp_packet_sender_) {
      rtp_packet_sender_->InsertPacket(priority, ssrc, sequence_number,
                                       capture_time_ms, bytes, retransmission);
    }
  }

  void SetAccountForAudioPackets(bool account_for_audio) override {
    RTC_NOTREACHED();
  }

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::CriticalSection crit_;
  RtpPacketSender* rtp_packet_sender_ RTC_GUARDED_BY(&crit_);
};

class VoERtcpObserver : public RtcpBandwidthObserver, public RtcpEventObserver {
 public:
  explicit VoERtcpObserver(Channel* owner)
      : owner_(owner), bandwidth_observer_(nullptr), event_observer_(nullptr) {}
  virtual ~VoERtcpObserver() {}

  void SetBandwidthObserver(RtcpBandwidthObserver* bandwidth_observer) {
    rtc::CritScope lock(&crit_);
    bandwidth_observer_ = bandwidth_observer;
  }

  void SetEventObserver(RtcpEventObserver* event_observer) {
    rtc::CritScope lock(&crit_);
    event_observer_ = event_observer;
  }

  void OnReceivedEstimatedBitrate(uint32_t bitrate) override {
    rtc::CritScope lock(&crit_);
    if (bandwidth_observer_) {
      bandwidth_observer_->OnReceivedEstimatedBitrate(bitrate);
    }
  }

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) override {
    {
      rtc::CritScope lock(&crit_);
      if (bandwidth_observer_) {
        bandwidth_observer_->OnReceivedRtcpReceiverReport(report_blocks, rtt,
                                                          now_ms);
      }
    }
    // TODO(mflodman): Do we need to aggregate reports here or can we jut send
    // what we get? I.e. do we ever get multiple reports bundled into one RTCP
    // report for VoiceEngine?
    if (report_blocks.empty())
      return;

    int fraction_lost_aggregate = 0;
    int total_number_of_packets = 0;

    // If receiving multiple report blocks, calculate the weighted average based
    // on the number of packets a report refers to.
    for (ReportBlockList::const_iterator block_it = report_blocks.begin();
         block_it != report_blocks.end(); ++block_it) {
      // Find the previous extended high sequence number for this remote SSRC,
      // to calculate the number of RTP packets this report refers to. Ignore if
      // we haven't seen this SSRC before.
      std::map<uint32_t, uint32_t>::iterator seq_num_it =
          extended_max_sequence_number_.find(block_it->source_ssrc);
      int number_of_packets = 0;
      if (seq_num_it != extended_max_sequence_number_.end()) {
        number_of_packets =
            block_it->extended_highest_sequence_number - seq_num_it->second;
      }
      fraction_lost_aggregate += number_of_packets * block_it->fraction_lost;
      total_number_of_packets += number_of_packets;

      extended_max_sequence_number_[block_it->source_ssrc] =
          block_it->extended_highest_sequence_number;
    }
    int weighted_fraction_lost = 0;
    if (total_number_of_packets > 0) {
      weighted_fraction_lost =
          (fraction_lost_aggregate + total_number_of_packets / 2) /
          total_number_of_packets;
    }
    owner_->OnUplinkPacketLossRate(weighted_fraction_lost / 255.0f);
    owner_->OnIncomingReceiverReports(report_blocks, rtt, now_ms);
  }

  void OnRtcpBye() override {
    rtc::CritScope lock(&crit_);
    if (event_observer_) {
      event_observer_->OnRtcpBye();
    }
  }

  void OnRtcpTimeout() override {
    rtc::CritScope lock(&crit_);
    if (event_observer_) {
      event_observer_->OnRtcpTimeout();
    }
  }

 private:
  Channel* owner_;
  // Maps remote side ssrc to extended highest sequence number received.
  std::map<uint32_t, uint32_t> extended_max_sequence_number_;
  rtc::CriticalSection crit_;
  RtcpBandwidthObserver* bandwidth_observer_ RTC_GUARDED_BY(crit_);
  RtcpEventObserver* event_observer_ RTC_GUARDED_BY(crit_);
};

class Channel::ProcessAndEncodeAudioTask : public rtc::QueuedTask {
 public:
  ProcessAndEncodeAudioTask(std::unique_ptr<AudioFrame> audio_frame,
                            Channel* channel)
      : audio_frame_(std::move(audio_frame)), channel_(channel) {
    RTC_DCHECK(channel_);
  }

 private:
  bool Run() override {
    RTC_DCHECK_RUN_ON(channel_->encoder_queue_);
    channel_->ProcessAndEncodeAudioOnTaskQueue(audio_frame_.get());
    return true;
  }

  std::unique_ptr<AudioFrame> audio_frame_;
  Channel* const channel_;
};

void Channel::OnIncomingReceiverReports(const ReportBlockList& aReportBlocks,
                                        const int64_t aRoundTripTime,
                                        const int64_t aReceptionTime) {

  statistics_proxy_->OnIncomingReceiverReports(aReportBlocks,
                                               aRoundTripTime,
                                               aReceptionTime);
}

bool Channel::GetRTCPReceiverStatistics(int64_t* timestamp,
                                        uint32_t* jitterMs,
                                        uint32_t* cumulativeLost,
                                        uint32_t* packetsReceived,
                                        uint64_t* bytesReceived,
                                        double* packetsFractionLost,
                                        int64_t* rtt) const {
  uint32_t ssrc = _rtpRtcpModule->SSRC();

  const auto& stats = statistics_proxy_->GetReceiverReportDerivedStats(ssrc);
  if (!stats || !stats->PacketsReceived()) {
    return false;
  }
  *timestamp = stats->mLatestReceiverReportReceptionTime;
  *jitterMs = stats->JitterMs();
  *cumulativeLost = stats->mCumulativePacketsLost;
  *packetsReceived = stats->PacketsReceived();
  *packetsFractionLost = stats->FractionOfPacketsLost();
  *rtt = stats->mRoundTripTime;

  // bytesReceived is only an estimate, which we derive from the locally
  // generated RTCP sender reports, and the remotely genderated receiver
  // reports.
  // There is an open issue in the spec as to if this should be included
  // here where it is only a guess.
  // https://github.com/w3c/webrtc-stats/issues/241
  *bytesReceived = 0;
  if (*packetsReceived) {
    // GetDataCounters has internal CS lock within RtpSender
    StreamDataCounters rtpCounters;
    StreamDataCounters rtxCounters; // unused
    _rtpRtcpModule->GetSendStreamDataCounters(&rtpCounters, &rtxCounters);
    uint64_t sentPackets = rtpCounters.transmitted.packets;
    if (sentPackets) {
      uint64_t sentBytes = rtpCounters.MediaPayloadBytes();
      *bytesReceived = sentBytes * (*packetsReceived) / sentPackets;
    }
  }

  return true;
}

void Channel::SetRtpPacketObserver(RtpPacketObserver* observer) {
  rtp_source_observer_ = observer;
}

int32_t Channel::SendData(FrameType frameType,
                          uint8_t payloadType,
                          uint32_t timeStamp,
                          const uint8_t* payloadData,
                          size_t payloadSize,
                          const RTPFragmentationHeader* fragmentation) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  if (_includeAudioLevelIndication) {
    // Store current audio level in the RTP/RTCP module.
    // The level will be used in combination with voice-activity state
    // (frameType) to add an RTP header extension
    _rtpRtcpModule->SetAudioLevel(rms_level_.Average());
  }

  // Push data from ACM to RTP/RTCP-module to deliver audio frame for
  // packetization.
  // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
  if (!_rtpRtcpModule->SendOutgoingData(
          (FrameType&)frameType, payloadType, timeStamp,
          // Leaving the time when this frame was
          // received from the capture device as
          // undefined for voice for now.
          -1, payloadData, payloadSize, fragmentation, nullptr, nullptr)) {
    RTC_LOG(LS_ERROR)
        << "Channel::SendData() failed to send data to RTP/RTCP module";
    return -1;
  }

  return 0;
}

bool Channel::SendRtp(const uint8_t* data,
                      size_t len,
                      const PacketOptions& options) {
  rtc::CritScope cs(&_callbackCritSect);

  if (_transportPtr == NULL) {
    RTC_LOG(LS_ERROR)
        << "Channel::SendPacket() failed to send RTP packet due to"
        << " invalid transport object";
    return false;
  }

  uint8_t* bufferToSendPtr = (uint8_t*)data;
  size_t bufferLength = len;

  if (!_transportPtr->SendRtp(bufferToSendPtr, bufferLength, options)) {
    RTC_LOG(LS_ERROR) << "Channel::SendPacket() RTP transmission failed";
    return false;
  }
  return true;
}

bool Channel::SendRtcp(const uint8_t* data, size_t len) {
  rtc::CritScope cs(&_callbackCritSect);
  if (_transportPtr == NULL) {
    RTC_LOG(LS_ERROR) << "Channel::SendRtcp() failed to send RTCP packet due to"
                      << " invalid transport object";
    return false;
  }

  uint8_t* bufferToSendPtr = (uint8_t*)data;
  size_t bufferLength = len;

  int n = _transportPtr->SendRtcp(bufferToSendPtr, bufferLength);
  if (n < 0) {
    RTC_LOG(LS_ERROR) << "Channel::SendRtcp() transmission failed";
    return false;
  }
  return true;
}

void Channel::OnIncomingSSRCChanged(uint32_t ssrc) {
  // Update ssrc so that NTP for AV sync can be updated.
  _rtpRtcpModule->SetRemoteSSRC(ssrc);

  // Update stats proxy to receive stats for new ssrc
  statistics_proxy_->SetSSRC(ssrc);
}

void Channel::OnIncomingCSRCChanged(uint32_t CSRC, bool added) {
  // TODO(saza): remove.
}

int32_t Channel::OnInitializeDecoder(int payload_type,
                                     const SdpAudioFormat& audio_format,
                                     uint32_t rate) {
  if (!audio_coding_->RegisterReceiveCodec(payload_type, audio_format)) {
    RTC_LOG(LS_WARNING) << "Channel::OnInitializeDecoder() invalid codec (pt="
                        << payload_type << ", " << audio_format
                        << ") received -1";
    return -1;
  }

  return 0;
}

int32_t Channel::OnReceivedPayloadData(const uint8_t* payloadData,
                                       size_t payloadSize,
                                       const WebRtcRTPHeader* rtpHeader) {
  if (!channel_state_.Get().playing) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.
    return 0;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (audio_coding_->IncomingPacket(payloadData, payloadSize, *rtpHeader) !=
      0) {
    RTC_LOG(LS_ERROR)
        << "Channel::OnReceivedPayloadData() unable to push data to the ACM";
    return -1;
  }

  // Observe incoming packets for getContributingSources and
  // getSynchronizationSources.
  if (rtp_source_observer_) {
    const auto playoutFrequency = audio_coding_->PlayoutFrequency();
    uint32_t jitter = 0;
    if (playoutFrequency > 0) {
      const ChannelStatistics stats = statistics_proxy_->GetStats();
      jitter = stats.rtcp.jitter / (playoutFrequency / 1000);
    }
    rtp_source_observer_->OnRtpPacket(rtpHeader->header,
        webrtc::Clock::GetRealTimeClock()->TimeInMilliseconds(), jitter);
  }

  int64_t round_trip_time = 0;
  _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), &round_trip_time, NULL, NULL,
                      NULL);

  std::vector<uint16_t> nack_list = audio_coding_->GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Can't use nack_list.data() since it's not supported by all
    // compilers.
    ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
  return 0;
}

bool Channel::OnRecoveredPacket(const uint8_t* rtp_packet,
                                size_t rtp_packet_length) {
  RTPHeader header;
  if (!rtp_header_parser_->Parse(rtp_packet, rtp_packet_length, &header)) {
    RTC_LOG(LS_WARNING) << "IncomingPacket invalid RTP header";
    return false;
  }
  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency < 0)
    return false;
  // TODO(nisse): Pass RtpPacketReceived with |recovered()| true.
  return ReceivePacket(rtp_packet, rtp_packet_length, header);
}

AudioMixer::Source::AudioFrameInfo Channel::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
  bool muted;
  if (audio_coding_->PlayoutData10Ms(audio_frame->sample_rate_hz_, audio_frame,
                                     &muted) == -1) {
    RTC_LOG(LS_ERROR) << "Channel::GetAudioFrame() PlayoutData10Ms() failed!";
    // In all likelihood, the audio in this frame is garbage. We return an
    // error so that the audio mixer module doesn't add it to the mix. As
    // a result, it won't be played out and the actions skipped here are
    // irrelevant.
    return AudioMixer::Source::AudioFrameInfo::kError;
  }

  if (muted) {
    // TODO(henrik.lundin): We should be able to do better than this. But we
    // will have to go through all the cases below where the audio samples may
    // be used, and handle the muted case in some way.
    AudioFrameOperations::Mute(audio_frame);
  }

  // Store speech type for dead-or-alive detection
  _outputSpeechType = audio_frame->speech_type_;

  {
    // Pass the audio buffers to an optional sink callback, before applying
    // scaling/panning, as that applies to the mix operation.
    // External recipients of the audio (e.g. via AudioTrack), will do their
    // own mixing/dynamic processing.
    rtc::CritScope cs(&_callbackCritSect);
    if (audio_sink_) {
      AudioSinkInterface::Data data(
          audio_frame->data(), audio_frame->samples_per_channel_,
          audio_frame->sample_rate_hz_, audio_frame->num_channels_,
          audio_frame->timestamp_);
      audio_sink_->OnData(data);
    }
  }

  float output_gain = 1.0f;
  {
    rtc::CritScope cs(&volume_settings_critsect_);
    output_gain = _outputGain;
  }

  // Output volume scaling
  if (output_gain < 0.99f || output_gain > 1.01f) {
    // TODO(solenberg): Combine with mute state - this can cause clicks!
    AudioFrameOperations::ScaleWithSat(output_gain, audio_frame);
  }

  // Measure audio level (0-9)
  // TODO(henrik.lundin) Use the |muted| information here too.
  // TODO(deadbeef): Use RmsLevel for |_outputAudioLevel| (see
  // https://crbug.com/webrtc/7517).
  _outputAudioLevel.ComputeLevel(*audio_frame, kAudioSampleDurationSeconds);

  if (capture_start_rtp_time_stamp_ < 0 && audio_frame->timestamp_ != 0) {
    // The first frame with a valid rtp timestamp.
    capture_start_rtp_time_stamp_ = audio_frame->timestamp_;
  }

  if (capture_start_rtp_time_stamp_ >= 0) {
    // audio_frame.timestamp_ should be valid from now on.

    // Compute elapsed time.
    int64_t unwrap_timestamp =
        rtp_ts_wraparound_handler_->Unwrap(audio_frame->timestamp_);
    audio_frame->elapsed_time_ms_ =
        (unwrap_timestamp - capture_start_rtp_time_stamp_) /
        (GetRtpTimestampRateHz() / 1000);

    {
      rtc::CritScope lock(&ts_stats_lock_);
      // Compute ntp time.
      audio_frame->ntp_time_ms_ =
          ntp_estimator_.Estimate(audio_frame->timestamp_);
      // |ntp_time_ms_| won't be valid until at least 2 RTCP SRs are received.
      if (audio_frame->ntp_time_ms_ > 0) {
        // Compute |capture_start_ntp_time_ms_| so that
        // |capture_start_ntp_time_ms_| + |elapsed_time_ms_| == |ntp_time_ms_|
        capture_start_ntp_time_ms_ =
            audio_frame->ntp_time_ms_ - audio_frame->elapsed_time_ms_;
      }
    }
  }

  {
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.TargetJitterBufferDelayMs",
                              audio_coding_->TargetDelayMs());
    const int jitter_buffer_delay = audio_coding_->FilteredCurrentDelayMs();
    rtc::CritScope lock(&video_sync_lock_);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDelayEstimateMs",
                              jitter_buffer_delay + playout_delay_ms_);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverJitterBufferDelayMs",
                              jitter_buffer_delay);
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDeviceDelayMs",
                              playout_delay_ms_);
  }

  return muted ? AudioMixer::Source::AudioFrameInfo::kMuted
               : AudioMixer::Source::AudioFrameInfo::kNormal;
}

int Channel::PreferredSampleRate() const {
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(audio_coding_->ReceiveFrequency(),
                  audio_coding_->PlayoutFrequency());
}

int32_t Channel::CreateChannel(Channel*& channel,
                               int32_t channelId,
                               uint32_t instanceId,
                               const VoEBase::ChannelConfig& config) {
  channel = new Channel(channelId, instanceId, config);
  if (channel == NULL) {
    RTC_LOG(LS_ERROR) << "unable to allocate memory for new channel";
    return -1;
  }
  return 0;
}

Channel::Channel(int32_t channelId,
                 uint32_t instanceId,
                 const VoEBase::ChannelConfig& config)
    : _instanceId(instanceId),
      _channelId(channelId),
      event_log_proxy_(new RtcEventLogProxy()),
      rtcp_rtt_stats_proxy_(new RtcpRttStatsProxy()),
      rtp_header_parser_(RtpHeaderParser::Create()),
      rtp_payload_registry_(new RTPPayloadRegistry()),
      rtp_receive_statistics_(
          ReceiveStatistics::Create(Clock::GetRealTimeClock())),
      rtp_receiver_(
          RtpReceiver::CreateAudioReceiver(Clock::GetRealTimeClock(),
                                           this,
                                           this,
                                           rtp_payload_registry_.get())),
      telephone_event_handler_(rtp_receiver_->GetTelephoneEventHandler()),
      _outputAudioLevel(),
      _timeStamp(0),  // This is just an offset, RTP module will add it's own
                      // random offset
      ntp_estimator_(Clock::GetRealTimeClock()),
      playout_timestamp_rtp_(0),
      playout_delay_ms_(0),
      send_sequence_number_(0),
      rtp_ts_wraparound_handler_(new rtc::TimestampWrapAroundHandler()),
      capture_start_rtp_time_stamp_(-1),
      capture_start_ntp_time_ms_(-1),
      _moduleProcessThreadPtr(NULL),
      _audioDeviceModulePtr(NULL),
      _transportPtr(NULL),
      input_mute_(false),
      previous_frame_muted_(false),
      _outputGain(1.0f),
      _includeAudioLevelIndication(false),
      transport_overhead_per_packet_(0),
      rtp_overhead_per_packet_(0),
      _outputSpeechType(AudioFrame::kNormalSpeech),
      rtcp_observer_(new VoERtcpObserver(this)),
      associate_send_channel_(ChannelOwner(nullptr)),
      pacing_enabled_(config.enable_voice_pacing),
      feedback_observer_proxy_(new TransportFeedbackProxy()),
      seq_num_allocator_proxy_(new TransportSequenceNumberProxy()),
      rtp_packet_sender_proxy_(new RtpPacketSenderProxy()),
      retransmission_rate_limiter_(new RateLimiter(Clock::GetRealTimeClock(),
                                                   kMaxRetransmissionWindowMs)),
      decoder_factory_(config.acm_config.decoder_factory),
      use_twcc_plr_for_ana_(
          webrtc::field_trial::FindFullName("UseTwccPlrForAna") == "Enabled") {
  AudioCodingModule::Config acm_config(config.acm_config);
  acm_config.neteq_config.enable_muted_state = true;
  audio_coding_.reset(AudioCodingModule::Create(acm_config));

  _outputAudioLevel.Clear();

  RtpRtcp::Configuration configuration;
  configuration.audio = true;
  configuration.outgoing_transport = this;
  configuration.overhead_observer = this;
  configuration.receive_statistics = rtp_receive_statistics_.get();
  configuration.bandwidth_callback = rtcp_observer_.get();
  configuration.event_callback = rtcp_observer_.get();
  if (pacing_enabled_) {
    configuration.paced_sender = rtp_packet_sender_proxy_.get();
    configuration.transport_sequence_number_allocator =
        seq_num_allocator_proxy_.get();
    configuration.transport_feedback_callback = feedback_observer_proxy_.get();
  }
  configuration.event_log = &(*event_log_proxy_);
  configuration.rtt_stats = &(*rtcp_rtt_stats_proxy_);
  configuration.retransmission_rate_limiter =
      retransmission_rate_limiter_.get();

  _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));
  _rtpRtcpModule->SetSendingMediaStatus(false);

  statistics_proxy_.reset(new StatisticsProxy(_rtpRtcpModule->SSRC()));
  rtp_receive_statistics_->RegisterRtcpStatisticsCallback(
    statistics_proxy_.get());
  configuration.rtcp_packet_type_counter_observer = statistics_proxy_.get();
}

Channel::~Channel() {
  RTC_DCHECK(!channel_state_.Get().sending);
  RTC_DCHECK(!channel_state_.Get().playing);
}

int32_t Channel::Init() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());

  channel_state_.Reset();

  // --- Initial sanity

  if (_moduleProcessThreadPtr == NULL) {
    RTC_LOG(LS_ERROR)
        << "Channel::Init() must call SetEngineInformation() first";
    return -1;
  }

  // --- Add modules to process thread (for periodic schedulation)

  _moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get(), RTC_FROM_HERE);

  // --- ACM initialization

  if (audio_coding_->InitializeReceiver() == -1) {
    RTC_LOG(LS_ERROR) << "Channel::Init() unable to initialize the ACM - 1";
    return -1;
  }

  // --- RTP/RTCP module initialization

  // Ensure that RTCP is enabled by default for the created channel.
  // Note that, the module will keep generating RTCP until it is explicitly
  // disabled by the user.
  // After StopListen (when no sockets exists), RTCP packets will no longer
  // be transmitted since the Transport object will then be invalid.
  telephone_event_handler_->SetTelephoneEventForwardToDecoder(true);
  // RTCP is enabled by default.
  _rtpRtcpModule->SetRTCPStatus(RtcpMode::kCompound);
  // --- Register all permanent callbacks
  if (audio_coding_->RegisterTransportCallback(this) == -1) {
    RTC_LOG(LS_ERROR) << "Channel::Init() callbacks not registered";
    return -1;
  }

  return 0;
}

void Channel::Terminate() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());
  // Must be called on the same thread as Init().
  rtp_receive_statistics_->RegisterRtcpStatisticsCallback(NULL);

  StopSend();
  StopPlayout();

  // The order to safely shutdown modules in a channel is:
  // 1. De-register callbacks in modules
  // 2. De-register modules in process thread
  // 3. Destroy modules
  if (audio_coding_->RegisterTransportCallback(NULL) == -1) {
    RTC_LOG(LS_WARNING)
        << "Terminate() failed to de-register transport callback"
        << " (Audio coding module)";
  }

  // De-register modules in process thread
  if (_moduleProcessThreadPtr)
    _moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get());

  // End of modules shutdown
}

int32_t Channel::SetEngineInformation(ProcessThread& moduleProcessThread,
                                      AudioDeviceModule& audioDeviceModule,
                                      rtc::TaskQueue* encoder_queue) {
  RTC_DCHECK(encoder_queue);
  RTC_DCHECK(!encoder_queue_);
  _moduleProcessThreadPtr = &moduleProcessThread;
  _audioDeviceModulePtr = &audioDeviceModule;
  encoder_queue_ = encoder_queue;
  return 0;
}

void Channel::SetSink(std::unique_ptr<AudioSinkInterface> sink) {
  rtc::CritScope cs(&_callbackCritSect);
  audio_sink_ = std::move(sink);
}

const rtc::scoped_refptr<AudioDecoderFactory>&
Channel::GetAudioDecoderFactory() const {
  return decoder_factory_;
}

int32_t Channel::StartPlayout() {
  if (channel_state_.Get().playing) {
    return 0;
  }

  channel_state_.SetPlaying(true);

  return 0;
}

int32_t Channel::StopPlayout() {
  if (!channel_state_.Get().playing) {
    return 0;
  }

  channel_state_.SetPlaying(false);
  _outputAudioLevel.Clear();

  return 0;
}

int32_t Channel::StartSend() {
  if (channel_state_.Get().sending) {
    return 0;
  }
  channel_state_.SetSending(true);
  {
    // It is now OK to start posting tasks to the encoder task queue.
    rtc::CritScope cs(&encoder_queue_lock_);
    encoder_queue_is_active_ = true;
  }
  // Resume the previous sequence number which was reset by StopSend(). This
  // needs to be done before |sending| is set to true on the RTP/RTCP module.
  if (send_sequence_number_) {
    _rtpRtcpModule->SetSequenceNumber(send_sequence_number_);
  }
  _rtpRtcpModule->SetSendingMediaStatus(true);
  if (_rtpRtcpModule->SetSendingStatus(true) != 0) {
    RTC_LOG(LS_ERROR) << "StartSend() RTP/RTCP failed to start sending";
    _rtpRtcpModule->SetSendingMediaStatus(false);
    rtc::CritScope cs(&_callbackCritSect);
    channel_state_.SetSending(false);
    return -1;
  }

  return 0;
}

void Channel::StopSend() {
  if (!channel_state_.Get().sending) {
    return;
  }
  channel_state_.SetSending(false);

  // Post a task to the encoder thread which sets an event when the task is
  // executed. We know that no more encoding tasks will be added to the task
  // queue for this channel since sending is now deactivated. It means that,
  // if we wait for the event to bet set, we know that no more pending tasks
  // exists and it is therfore guaranteed that the task queue will never try
  // to acccess and invalid channel object.
  RTC_DCHECK(encoder_queue_);

  rtc::Event flush(false, false);
  {
    // Clear |encoder_queue_is_active_| under lock to prevent any other tasks
    // than this final "flush task" to be posted on the queue.
    rtc::CritScope cs(&encoder_queue_lock_);
    encoder_queue_is_active_ = false;
    encoder_queue_->PostTask([&flush]() { flush.Set(); });
  }
  flush.Wait(rtc::Event::kForever);

  // Store the sequence number to be able to pick up the same sequence for
  // the next StartSend(). This is needed for restarting device, otherwise
  // it might cause libSRTP to complain about packets being replayed.
  // TODO(xians): Remove this workaround after RtpRtcpModule's refactoring
  // CL is landed. See issue
  // https://code.google.com/p/webrtc/issues/detail?id=2111 .
  send_sequence_number_ = _rtpRtcpModule->SequenceNumber();

  // Reset sending SSRC and sequence number and triggers direct transmission
  // of RTCP BYE
  if (_rtpRtcpModule->SetSendingStatus(false) == -1) {
    RTC_LOG(LS_ERROR) << "StartSend() RTP/RTCP failed to stop sending";
  }
  _rtpRtcpModule->SetSendingMediaStatus(false);
}

bool Channel::SetEncoder(int payload_type,
                         std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);
  // TODO(ossu): Make CodecInsts up, for now: one for the RTP/RTCP module and
  // one for for us to keep track of sample rate and number of channels, etc.

  // The RTP/RTCP module needs to know the RTP timestamp rate (i.e. clockrate)
  // as well as some other things, so we collect this info and send it along.
  CodecInst rtp_codec;
  rtp_codec.pltype = payload_type;
  strncpy(rtp_codec.plname, "audio", sizeof(rtp_codec.plname));
  rtp_codec.plname[sizeof(rtp_codec.plname) - 1] = 0;
  // Seems unclear if it should be clock rate or sample rate. CodecInst
  // supposedly carries the sample rate, but only clock rate seems sensible to
  // send to the RTP/RTCP module.
  rtp_codec.plfreq = encoder->RtpTimestampRateHz();
  rtp_codec.pacsize = rtc::CheckedDivExact(
      static_cast<int>(encoder->Max10MsFramesInAPacket() * rtp_codec.plfreq),
      100);
  rtp_codec.channels = encoder->NumChannels();
  rtp_codec.rate = 0;

  cached_encoder_props_.emplace(
      EncoderProps{encoder->SampleRateHz(), encoder->NumChannels()});

  if (_rtpRtcpModule->RegisterSendPayload(rtp_codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(payload_type);
    if (_rtpRtcpModule->RegisterSendPayload(rtp_codec) != 0) {
      RTC_LOG(LS_ERROR)
          << "SetEncoder() failed to register codec to RTP/RTCP module";
      return false;
    }
  }

  audio_coding_->SetEncoder(std::move(encoder));
  return true;
}

void Channel::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  audio_coding_->ModifyEncoder(modifier);
}

rtc::Optional<Channel::EncoderProps> Channel::GetEncoderProps() const {
  return cached_encoder_props_;
}

int32_t Channel::GetRecCodec(CodecInst& codec) {
  return (audio_coding_->ReceiveCodec(&codec));
}

void Channel::SetBitRate(int bitrate_bps, int64_t probing_interval_ms) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkBandwidth(bitrate_bps, probing_interval_ms);
    }
  });
  retransmission_rate_limiter_->SetMaxRate(bitrate_bps);
}

void Channel::OnTwccBasedUplinkPacketLossRate(float packet_loss_rate) {
  if (!use_twcc_plr_for_ana_)
    return;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
    }
  });
}

void Channel::OnRecoverableUplinkPacketLossRate(
    float recoverable_packet_loss_rate) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkRecoverablePacketLossFraction(
          recoverable_packet_loss_rate);
    }
  });
}

void Channel::SetRtcpEventObserver(RtcpEventObserver* observer) {
  rtcp_observer_->SetEventObserver(observer);
}

void Channel::OnUplinkPacketLossRate(float packet_loss_rate) {
  if (use_twcc_plr_for_ana_)
    return;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
    }
  });
}

void Channel::SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs) {
  rtp_payload_registry_->SetAudioReceivePayloads(codecs);
  audio_coding_->SetReceiveCodecs(codecs);
}

bool Channel::EnableAudioNetworkAdaptor(const std::string& config_string) {
  bool success = false;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      success = (*encoder)->EnableAudioNetworkAdaptor(config_string,
                                                      event_log_proxy_.get());
    }
  });
  return success;
}

void Channel::DisableAudioNetworkAdaptor() {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->DisableAudioNetworkAdaptor();
  });
}

void Channel::SetReceiverFrameLengthRange(int min_frame_length_ms,
                                          int max_frame_length_ms) {
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->SetReceiverFrameLengthRange(min_frame_length_ms,
                                              max_frame_length_ms);
    }
  });
}

void Channel::RegisterTransport(Transport* transport) {
  rtc::CritScope cs(&_callbackCritSect);
  _transportPtr = transport;
}

void Channel::OnRtpPacket(const RtpPacketReceived& packet) {
  RTPHeader header;
  packet.GetHeader(&header);

  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false);

  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency >= 0) {
    bool in_order = IsPacketInOrder(header);
    statistics_proxy_->OnSendCodecFrequencyChanged(header.payload_type_frequency);
    rtp_receive_statistics_->IncomingPacket(
        header, packet.size(), IsPacketRetransmitted(header, in_order));
    rtp_payload_registry_->SetIncomingPayloadType(header);

    ReceivePacket(packet.data(), packet.size(), header);
  }
}

bool Channel::ReceivePacket(const uint8_t* packet,
                            size_t packet_length,
                            const RTPHeader& header) {
  const uint8_t* payload = packet + header.headerLength;
  assert(packet_length >= header.headerLength);
  size_t payload_length = packet_length - header.headerLength;
  const auto pl =
      rtp_payload_registry_->PayloadTypeToPayload(header.payloadType);
  if (!pl) {
    return false;
  }
  return rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                          pl->typeSpecific);
}

bool Channel::IsPacketInOrder(const RTPHeader& header) const {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  return statistician->IsPacketInOrder(header.sequenceNumber);
}

bool Channel::IsPacketRetransmitted(const RTPHeader& header,
                                    bool in_order) const {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  // Check if this is a retransmission.
  int64_t min_rtt = 0;
  _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), NULL, NULL, &min_rtt, NULL);
  return !in_order && statistician->IsRetransmitOfOldPacket(header, min_rtt);
}

int32_t Channel::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Deliver RTCP packet to RTP/RTCP module for parsing
  _rtpRtcpModule->IncomingRtcpPacket(data, length);

  int64_t rtt = GetRTT(true);
  if (rtt == 0) {
    // Waiting for valid RTT.
    return 0;
  }

  int64_t nack_window_ms = rtt;
  if (nack_window_ms < kMinRetransmissionWindowMs) {
    nack_window_ms = kMinRetransmissionWindowMs;
  } else if (nack_window_ms > kMaxRetransmissionWindowMs) {
    nack_window_ms = kMaxRetransmissionWindowMs;
  }
  retransmission_rate_limiter_->SetWindowSize(nack_window_ms);

  // Invoke audio encoders OnReceivedRtt().
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder)
      (*encoder)->OnReceivedRtt(rtt);
  });

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (0 !=
      _rtpRtcpModule->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL,
                                &rtp_timestamp)) {
    // Waiting for RTCP.
    return 0;
  }

  {
    rtc::CritScope lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
  return 0;
}

int Channel::GetSpeechOutputLevel() const {
  return _outputAudioLevel.Level();
}

int Channel::GetSpeechOutputLevelFullRange() const {
  return _outputAudioLevel.LevelFullRange();
}

double Channel::GetTotalOutputEnergy() const {
  return _outputAudioLevel.TotalEnergy();
}

double Channel::GetTotalOutputDuration() const {
  return _outputAudioLevel.TotalDuration();
}

void Channel::SetInputMute(bool enable) {
  rtc::CritScope cs(&volume_settings_critsect_);
  input_mute_ = enable;
}

bool Channel::InputMute() const {
  rtc::CritScope cs(&volume_settings_critsect_);
  return input_mute_;
}

void Channel::SetChannelOutputVolumeScaling(float scaling) {
  rtc::CritScope cs(&volume_settings_critsect_);
  _outputGain = scaling;
}

int Channel::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK_LE(0, event);
  RTC_DCHECK_GE(255, event);
  RTC_DCHECK_LE(0, duration_ms);
  RTC_DCHECK_GE(65535, duration_ms);
  if (!Sending()) {
    return -1;
  }
  if (_rtpRtcpModule->SendTelephoneEventOutband(
      event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_LOG(LS_ERROR) << "SendTelephoneEventOutband() failed to send event";
    return -1;
  }
  return 0;
}

int Channel::SetSendTelephoneEventPayloadType(int payload_type,
                                              int payload_frequency) {
  RTC_DCHECK_LE(0, payload_type);
  RTC_DCHECK_GE(127, payload_type);
  CodecInst codec = {0};
  codec.pltype = payload_type;
  codec.plfreq = payload_frequency;
  memcpy(codec.plname, "telephone-event", 16);
  if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
    _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
      RTC_LOG(LS_ERROR)
          << "SetSendTelephoneEventPayloadType() failed to register "
             "send payload type";
      return -1;
    }
  }
  return 0;
}

int Channel::SetLocalMID(const char* mid) {
  if (channel_state_.Get().sending) {
    return -1;
  }
  _rtpRtcpModule->SetMID(mid);
  return 0;
}

int Channel::SetLocalSSRC(unsigned int ssrc) {
  if (channel_state_.Get().sending) {
    RTC_LOG(LS_ERROR) << "SetLocalSSRC() already sending";
    return -1;
  }
  _rtpRtcpModule->SetSSRC(ssrc);
  return 0;
}
/*
int Channel::GetRemoteSSRC(unsigned int& ssrc) {
  ssrc = rtp_receiver_->SSRC();
  return 0;
}
*/
int Channel::SetSendAudioLevelIndicationStatus(bool enable, unsigned char id) {
  _includeAudioLevelIndication = enable;
  return SetSendRtpHeaderExtension(enable, kRtpExtensionAudioLevel, id);
}

int Channel::SetSendMIDStatus(bool enable, unsigned char id) {
  return SetSendRtpHeaderExtension(enable, kRtpExtensionMid, id);
}

int Channel::SetReceiveAudioLevelIndicationStatus(bool enable,
                                                  unsigned char id,
                                                  bool isLevelSsrc) {
  const webrtc::RTPExtensionType& rtpExt = isLevelSsrc ?
      kRtpExtensionAudioLevel : kRtpExtensionCsrcAudioLevel;
  rtp_header_parser_->DeregisterRtpHeaderExtension(rtpExt);
  if (enable && !rtp_header_parser_->RegisterRtpHeaderExtension(rtpExt, id)) {
    return -1;
  }
  return 0;
}

int Channel::SetReceiveCsrcAudioLevelIndicationStatus(bool enable,
                                                      unsigned char id) {
  rtp_header_parser_->DeregisterRtpHeaderExtension(kRtpExtensionCsrcAudioLevel);
  if (enable &&
      !rtp_header_parser_->RegisterRtpHeaderExtension(kRtpExtensionCsrcAudioLevel,
                                                      id)) {
    return -1;
  }
  return 0;
}


void Channel::EnableSendTransportSequenceNumber(int id) {
  int ret =
      SetSendRtpHeaderExtension(true, kRtpExtensionTransportSequenceNumber, id);
  RTC_DCHECK_EQ(0, ret);
}

void Channel::EnableReceiveTransportSequenceNumber(int id) {
  rtp_header_parser_->DeregisterRtpHeaderExtension(
      kRtpExtensionTransportSequenceNumber);
  bool ret = rtp_header_parser_->RegisterRtpHeaderExtension(
      kRtpExtensionTransportSequenceNumber, id);
  RTC_DCHECK(ret);
}

void Channel::RegisterSenderCongestionControlObjects(
    RtpTransportControllerSendInterface* transport,
    RtcpBandwidthObserver* bandwidth_observer) {
  RtpPacketSender* rtp_packet_sender = transport->packet_sender();
  TransportFeedbackObserver* transport_feedback_observer =
      transport->transport_feedback_observer();
  PacketRouter* packet_router = transport->packet_router();
  //This allows us to re-create streams but keep the same channel
  if (packet_router_ == packet_router) {
    return;
  }

  RTC_DCHECK(rtp_packet_sender);
  RTC_DCHECK(transport_feedback_observer);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  rtcp_observer_->SetBandwidthObserver(bandwidth_observer);
  feedback_observer_proxy_->SetTransportFeedbackObserver(
      transport_feedback_observer);
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(packet_router);
  rtp_packet_sender_proxy_->SetPacketSender(rtp_packet_sender);
  _rtpRtcpModule->SetStorePacketsStatus(true, 600);
  constexpr bool remb_candidate = false;
  packet_router->AddSendRtpModule(_rtpRtcpModule.get(), remb_candidate);
  packet_router_ = packet_router;
}

void Channel::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(packet_router);
  //This allows us to re-create streams but keep the same channel
  if (packet_router_ == packet_router) {
    return;
  }

  RTC_DCHECK(!packet_router_);
  constexpr bool remb_candidate = false;
  packet_router->AddReceiveRtpModule(_rtpRtcpModule.get(), remb_candidate);
  packet_router_ = packet_router;
}

void Channel::ResetSenderCongestionControlObjects() {
  RTC_DCHECK(packet_router_);
  _rtpRtcpModule->SetStorePacketsStatus(false, 600);
  rtcp_observer_->SetBandwidthObserver(nullptr);
  feedback_observer_proxy_->SetTransportFeedbackObserver(nullptr);
  seq_num_allocator_proxy_->SetSequenceNumberAllocator(nullptr);
  packet_router_->RemoveSendRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
  rtp_packet_sender_proxy_->SetPacketSender(nullptr);
}

void Channel::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK(packet_router_);
  packet_router_->RemoveReceiveRtpModule(_rtpRtcpModule.get());
  packet_router_ = nullptr;
}

void Channel::SetRTCPStatus(bool enable) {
  _rtpRtcpModule->SetRTCPStatus(enable ? RtcpMode::kCompound : RtcpMode::kOff);
}

int Channel::SetRTCP_CNAME(const char cName[256]) {
  if (_rtpRtcpModule->SetCNAME(cName) != 0) {
    RTC_LOG(LS_ERROR) << "SetRTCP_CNAME() failed to set RTCP CNAME";
    return -1;
  }
  return 0;
}

int Channel::GetRemoteRTCPReportBlocks(
    std::vector<ReportBlock>* report_blocks) {
  if (report_blocks == NULL) {
    RTC_LOG(LS_ERROR) << "GetRemoteRTCPReportBlock()s invalid report_blocks.";
    return -1;
  }

  // Get the report blocks from the latest received RTCP Sender or Receiver
  // Report. Each element in the vector contains the sender's SSRC and a
  // report block according to RFC 3550.
  std::vector<RTCPReportBlock> rtcp_report_blocks;
  if (_rtpRtcpModule->RemoteRTCPStat(&rtcp_report_blocks) != 0) {
    return -1;
  }

  if (rtcp_report_blocks.empty())
    return 0;

  std::vector<RTCPReportBlock>::const_iterator it = rtcp_report_blocks.begin();
  for (; it != rtcp_report_blocks.end(); ++it) {
    ReportBlock report_block;
    report_block.sender_SSRC = it->sender_ssrc;
    report_block.source_SSRC = it->source_ssrc;
    report_block.fraction_lost = it->fraction_lost;
    report_block.cumulative_num_packets_lost = it->packets_lost;
    report_block.extended_highest_sequence_number =
        it->extended_highest_sequence_number;
    report_block.interarrival_jitter = it->jitter;
    report_block.last_SR_timestamp = it->last_sender_report_timestamp;
    report_block.delay_since_last_SR = it->delay_since_last_sender_report;
    report_blocks->push_back(report_block);
  }
  return 0;
}

int Channel::GetRTPStatistics(CallStatistics& stats) {
  // --- RtcpStatistics
  // GetStatistics() grabs the stream_lock_ inside the object
  // rtp_receiver_->SSRC grabs a lock too.

  // The jitter statistics is updated for each received RTP packet and is
  // based on received packets.
  RtcpStatistics statistics;
  StreamStatistician* statistician =
    rtp_receive_statistics_->GetStatistician(rtp_receiver_->SSRC());
  if (statistician) {
    statistician->GetStatistics(&statistics,
                                _rtpRtcpModule->RTCP() == RtcpMode::kOff);
  }

  stats.fractionLost = statistics.fraction_lost;
  stats.cumulativeLost = statistics.packets_lost;
  stats.extendedMax = statistics.extended_highest_sequence_number;
  stats.jitterSamples = statistics.jitter;

  // --- RTT
  stats.rttMs = GetRTT(true);

  // --- Data counters

  size_t bytesSent(0);
  uint32_t packetsSent(0);
  size_t bytesReceived(0);
  uint32_t packetsReceived(0);

  if (statistician) {
    statistician->GetDataCounters(&bytesReceived, &packetsReceived);
  }

  if (_rtpRtcpModule->DataCountersRTP(&bytesSent, &packetsSent) != 0) {
    RTC_LOG(LS_WARNING)
        << "GetRTPStatistics() failed to retrieve RTP datacounters"
        << " => output will not be complete";
  }

  stats.bytesSent = bytesSent;
  stats.packetsSent = packetsSent;
  stats.bytesReceived = bytesReceived;
  stats.packetsReceived = packetsReceived;

  _rtpRtcpModule->RemoteRTCPSenderInfo(&stats.rtcp_sender_packets_sent,
                                       &stats.rtcp_sender_octets_sent,
                                       &stats.rtcp_sender_ntp_timestamp);

  // --- Timestamps
  {
    rtc::CritScope lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
  }
  return 0;
}

int Channel::GetRTCPPacketTypeCounters(RtcpPacketTypeCounter& stats) {
  if (_rtpRtcpModule->RTCP() == RtcpMode::kOff) {
    return -1;
  }

  statistics_proxy_->GetPacketTypeCounter(stats);
  return 0;
}

void Channel::SetNACKStatus(bool enable, int maxNumberOfPackets) {
  // None of these functions can fail.
  // If pacing is enabled we always store packets.
  if (!pacing_enabled_)
    _rtpRtcpModule->SetStorePacketsStatus(enable, maxNumberOfPackets);
  rtp_receive_statistics_->SetMaxReorderingThreshold(maxNumberOfPackets);
  if (enable)
    audio_coding_->EnableNack(maxNumberOfPackets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int Channel::ResendPackets(const uint16_t* sequence_numbers, int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

void Channel::ProcessAndEncodeAudio(const AudioFrame& audio_input) {
  // Avoid posting any new tasks if sending was already stopped in StopSend().
  rtc::CritScope cs(&encoder_queue_lock_);
  if (!encoder_queue_is_active_) {
    return;
  }
  std::unique_ptr<AudioFrame> audio_frame(new AudioFrame());
  // TODO(henrika): try to avoid copying by moving ownership of audio frame
  // either into pool of frames or into the task itself.
  audio_frame->CopyFrom(audio_input);
  // Profile time between when the audio frame is added to the task queue and
  // when the task is actually executed.
  audio_frame->UpdateProfileTimeStamp();
  encoder_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(
      new ProcessAndEncodeAudioTask(std::move(audio_frame), this)));
}

void Channel::ProcessAndEncodeAudio(const int16_t* audio_data,
                                    int sample_rate,
                                    size_t number_of_frames,
                                    size_t number_of_channels) {
  // Avoid posting as new task if sending was already stopped in StopSend().
  rtc::CritScope cs(&encoder_queue_lock_);
  if (!encoder_queue_is_active_) {
    return;
  }
  std::unique_ptr<AudioFrame> audio_frame(new AudioFrame());
  const auto props = GetEncoderProps();
  RTC_CHECK(props);
  audio_frame->sample_rate_hz_ = std::min(props->sample_rate_hz, sample_rate);
  audio_frame->num_channels_ =
      std::min(props->num_channels, number_of_channels);
  RemixAndResample(audio_data, number_of_frames, number_of_channels,
                   sample_rate, &input_resampler_, audio_frame.get());
  encoder_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(
      new ProcessAndEncodeAudioTask(std::move(audio_frame), this)));
}

void Channel::ProcessAndEncodeAudioOnTaskQueue(AudioFrame* audio_input) {
  RTC_DCHECK_RUN_ON(encoder_queue_);
  RTC_DCHECK_GT(audio_input->samples_per_channel_, 0);
  RTC_DCHECK_LE(audio_input->num_channels_, 2);

  // Measure time between when the audio frame is added to the task queue and
  // when the task is actually executed. Goal is to keep track of unwanted
  // extra latency added by the task queue.
  RTC_HISTOGRAM_COUNTS_10000("WebRTC.Audio.EncodingTaskQueueLatencyMs",
                             audio_input->ElapsedProfileTimeMs());

  bool is_muted = InputMute();
  AudioFrameOperations::Mute(audio_input, previous_frame_muted_, is_muted);

  if (_includeAudioLevelIndication) {
    size_t length =
        audio_input->samples_per_channel_ * audio_input->num_channels_;
    RTC_CHECK_LE(length, AudioFrame::kMaxDataSizeBytes);
    if (is_muted && previous_frame_muted_) {
      rms_level_.AnalyzeMuted(length);
    } else {
      rms_level_.Analyze(
          rtc::ArrayView<const int16_t>(audio_input->data(), length));
    }
  }
  previous_frame_muted_ = is_muted;

  // Add 10ms of raw (PCM) audio data to the encoder @ 32kHz.

  // The ACM resamples internally.
  audio_input->timestamp_ = _timeStamp;
  // This call will trigger AudioPacketizationCallback::SendData if encoding
  // is done and payload is ready for packetization and transmission.
  // Otherwise, it will return without invoking the callback.
  if (audio_coding_->Add10MsData(*audio_input) < 0) {
    RTC_LOG(LS_ERROR) << "ACM::Add10MsData() failed for channel " << _channelId;
    return;
  }

  _timeStamp += static_cast<uint32_t>(audio_input->samples_per_channel_);
}

void Channel::set_associate_send_channel(const ChannelOwner& channel) {
  RTC_DCHECK(!channel.channel() ||
             channel.channel()->ChannelId() != _channelId);
  rtc::CritScope lock(&assoc_send_channel_lock_);
  associate_send_channel_ = channel;
}

void Channel::DisassociateSendChannel(int channel_id) {
  rtc::CritScope lock(&assoc_send_channel_lock_);
  Channel* channel = associate_send_channel_.channel();
  if (channel && channel->ChannelId() == channel_id) {
    // If this channel is associated with a send channel of the specified
    // Channel ID, disassociate with it.
    ChannelOwner ref(NULL);
    associate_send_channel_ = ref;
  }
}

void Channel::SetRtcEventLog(RtcEventLog* event_log) {
  event_log_proxy_->SetEventLog(event_log);
}

void Channel::SetRtcpRttStats(RtcpRttStats* rtcp_rtt_stats) {
  rtcp_rtt_stats_proxy_->SetRtcpRttStats(rtcp_rtt_stats);
}

void Channel::UpdateOverheadForEncoder() {
  size_t overhead_per_packet =
      transport_overhead_per_packet_ + rtp_overhead_per_packet_;
  audio_coding_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* encoder) {
    if (*encoder) {
      (*encoder)->OnReceivedOverhead(overhead_per_packet);
    }
  });
}

void Channel::SetTransportOverhead(size_t transport_overhead_per_packet) {
  rtc::CritScope cs(&overhead_per_packet_lock_);
  transport_overhead_per_packet_ = transport_overhead_per_packet;
  UpdateOverheadForEncoder();
}

// TODO(solenberg): Make AudioSendStream an OverheadObserver instead.
void Channel::OnOverheadChanged(size_t overhead_bytes_per_packet) {
  rtc::CritScope cs(&overhead_per_packet_lock_);
  rtp_overhead_per_packet_ = overhead_bytes_per_packet;
  UpdateOverheadForEncoder();
}

int Channel::GetNetworkStatistics(NetworkStatistics& stats) {
  return audio_coding_->GetNetworkStatistics(&stats);
}

void Channel::GetDecodingCallStatistics(AudioDecodingCallStats* stats) const {
  audio_coding_->GetDecodingCallStatistics(stats);
}

ANAStats Channel::GetANAStatistics() const {
  return audio_coding_->GetANAStats();
}

uint32_t Channel::GetDelayEstimate() const {
  rtc::CritScope lock(&video_sync_lock_);
  return audio_coding_->FilteredCurrentDelayMs() + playout_delay_ms_;
}

int Channel::SetMinimumPlayoutDelay(int delayMs) {
  if ((delayMs < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delayMs > kVoiceEngineMaxMinPlayoutDelayMs)) {
    RTC_LOG(LS_ERROR) << "SetMinimumPlayoutDelay() invalid min delay";
    return -1;
  }
  if (audio_coding_->SetMinimumPlayoutDelay(delayMs) != 0) {
    RTC_LOG(LS_ERROR)
        << "SetMinimumPlayoutDelay() failed to set min playout delay";
    return -1;
  }
  return 0;
}

int Channel::GetPlayoutTimestamp(unsigned int& timestamp) {
  uint32_t playout_timestamp_rtp = 0;
  {
    rtc::CritScope lock(&video_sync_lock_);
    playout_timestamp_rtp = playout_timestamp_rtp_;
  }
  if (playout_timestamp_rtp == 0) {
    RTC_LOG(LS_ERROR) << "GetPlayoutTimestamp() failed to retrieve timestamp";
    return -1;
  }
  timestamp = playout_timestamp_rtp;
  return 0;
}

int Channel::GetRtpRtcp(RtpRtcp** rtpRtcpModule,
                        RtpReceiver** rtp_receiver) const {
  *rtpRtcpModule = _rtpRtcpModule.get();
  *rtp_receiver = rtp_receiver_.get();
  return 0;
}

void Channel::UpdatePlayoutTimestamp(bool rtcp) {
  jitter_buffer_playout_timestamp_ = audio_coding_->PlayoutTimestamp();

  if (!jitter_buffer_playout_timestamp_) {
    // This can happen if this channel has not received any RTP packets. In
    // this case, NetEq is not capable of computing a playout timestamp.
    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    RTC_LOG(LS_WARNING) << "Channel::UpdatePlayoutTimestamp() failed to read"
                        << " playout delay from the ADM";
    return;
  }

  RTC_DCHECK(jitter_buffer_playout_timestamp_);
  uint32_t playout_timestamp = *jitter_buffer_playout_timestamp_;

  // Remove the playout delay.
  playout_timestamp -= (delay_ms * (GetRtpTimestampRateHz() / 1000));

  {
    rtc::CritScope lock(&video_sync_lock_);
    if (!rtcp) {
      playout_timestamp_rtp_ = playout_timestamp;
    }
    playout_delay_ms_ = delay_ms;
  }
}

void Channel::RegisterReceiveCodecsToRTPModule() {
  // TODO(kwiberg): Iterate over the factory's supported codecs instead?
  const int nSupportedCodecs = AudioCodingModule::NumberOfCodecs();
  for (int idx = 0; idx < nSupportedCodecs; idx++) {
    CodecInst codec;
    if (audio_coding_->Codec(idx, &codec) == -1) {
      RTC_LOG(LS_WARNING) << "Unable to register codec #" << idx
                          << " for RTP/RTCP receiver.";
      continue;
    }
    const SdpAudioFormat format = CodecInstToSdp(codec);
    if (!decoder_factory_->IsSupportedDecoder(format) ||
        rtp_receiver_->RegisterReceivePayload(codec.pltype, format) == -1) {
      RTC_LOG(LS_WARNING) << "Unable to register " << format
                          << " for RTP/RTCP receiver.";
    }
  }
}

int Channel::SetSendRtpHeaderExtension(bool enable,
                                       RTPExtensionType type,
                                       unsigned char id) {
  int error = 0;
  _rtpRtcpModule->DeregisterSendRtpHeaderExtension(type);
  if (enable) {
    error = _rtpRtcpModule->RegisterSendRtpHeaderExtension(type, id);
  }
  return error;
}

int Channel::GetRtpTimestampRateHz() const {
  int sampleRate = audio_coding_->ReceiveSampleRate();
  // Default to the playout frequency if we've not gotten any packets yet.
  // TODO(ossu): Zero clockrate can only happen if we've added an external
  // decoder for a format we don't support internally. Remove once that way of
  // adding decoders is gone!
  return sampleRate != 0 ? sampleRate
             : audio_coding_->PlayoutFrequency();
}

int64_t Channel::GetRTT(bool allow_associate_channel) const {
  RtcpMode method = _rtpRtcpModule->RTCP();
  if (method == RtcpMode::kOff) {
    return 0;
  }
  std::vector<RTCPReportBlock> report_blocks;
  _rtpRtcpModule->RemoteRTCPStat(&report_blocks);

  int64_t rtt = 0;
  if (report_blocks.empty()) {
    if (allow_associate_channel) {
      rtc::CritScope lock(&assoc_send_channel_lock_);
      Channel* channel = associate_send_channel_.channel();
      // Tries to get RTT from an associated channel. This is important for
      // receive-only channels.
      if (channel) {
        // To prevent infinite recursion and deadlock, calling GetRTT of
        // associate channel should always use "false" for argument:
        // |allow_associate_channel|.
        rtt = channel->GetRTT(false);
      }
    }
    return rtt;
  }

  uint32_t remoteSSRC = rtp_receiver_->SSRC();
  std::vector<RTCPReportBlock>::const_iterator it = report_blocks.begin();
  for (; it != report_blocks.end(); ++it) {
    if (it->sender_ssrc == remoteSSRC)
      break;
  }
  if (it == report_blocks.end()) {
    // We have not received packets with SSRC matching the report blocks.
    // To calculate RTT we try with the SSRC of the first report block.
    // This is very important for send-only channels where we don't know
    // the SSRC of the other end.
    remoteSSRC = report_blocks[0].sender_ssrc;
  }

  int64_t avg_rtt = 0;
  int64_t max_rtt = 0;
  int64_t min_rtt = 0;
  if (_rtpRtcpModule->RTT(remoteSSRC, &rtt, &avg_rtt, &min_rtt, &max_rtt) !=
      0) {
    return 0;
  }
  return rtt;
}

}  // namespace voe
}  // namespace webrtc
