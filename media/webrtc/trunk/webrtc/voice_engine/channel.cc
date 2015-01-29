/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel.h"

#include "webrtc/base/timeutils.h"
#include "webrtc/common.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/utility/interface/audio_frame_operations.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/utility/interface/rtp_dump.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/statistics.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/utility.h"

#if defined(_WIN32)
#include <Qos.h>
#endif

namespace webrtc {
namespace voe {

// Extend the default RTCP statistics struct with max_jitter, defined as the
// maximum jitter value seen in an RTCP report block.
struct ChannelStatistics : public RtcpStatistics {
  ChannelStatistics() : rtcp(), max_jitter(0) {}

  RtcpStatistics rtcp;
  uint32_t max_jitter;
};

// Statistics callback, called at each generation of a new RTCP report block.
class StatisticsProxy : public RtcpStatisticsCallback {
 public:
  StatisticsProxy(uint32_t ssrc)
   : stats_lock_(CriticalSectionWrapper::CreateCriticalSection()),
     ssrc_(ssrc) {}
  virtual ~StatisticsProxy() {}

  virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                 uint32_t ssrc) OVERRIDE {
    if (ssrc != ssrc_)
      return;

    CriticalSectionScoped cs(stats_lock_.get());
    stats_.rtcp = statistics;
    if (statistics.jitter > stats_.max_jitter) {
      stats_.max_jitter = statistics.jitter;
    }
  }

  void ResetStatistics() {
    CriticalSectionScoped cs(stats_lock_.get());
    stats_ = ChannelStatistics();
  }

  ChannelStatistics GetStats() {
    CriticalSectionScoped cs(stats_lock_.get());
    return stats_;
  }

 private:
  // StatisticsUpdated calls are triggered from threads in the RTP module,
  // while GetStats calls can be triggered from the public voice engine API,
  // hence synchronization is needed.
  scoped_ptr<CriticalSectionWrapper> stats_lock_;
  const uint32_t ssrc_;
  ChannelStatistics stats_;
};

class VoEBitrateObserver : public BitrateObserver {
 public:
  explicit VoEBitrateObserver(Channel* owner)
      : owner_(owner) {}
  virtual ~VoEBitrateObserver() {}

  // Implements BitrateObserver.
  virtual void OnNetworkChanged(const uint32_t bitrate_bps,
                                const uint8_t fraction_lost,
                                const uint32_t rtt) OVERRIDE {
    // |fraction_lost| has a scale of 0 - 255.
    owner_->OnNetworkChanged(bitrate_bps, fraction_lost, rtt);
  }

 private:
  Channel* owner_;
};

int32_t
Channel::SendData(FrameType frameType,
                  uint8_t   payloadType,
                  uint32_t  timeStamp,
                  const uint8_t*  payloadData,
                  uint16_t  payloadSize,
                  const RTPFragmentationHeader* fragmentation)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendData(frameType=%u, payloadType=%u, timeStamp=%u,"
                 " payloadSize=%u, fragmentation=0x%x)",
                 frameType, payloadType, timeStamp, payloadSize, fragmentation);

    if (_includeAudioLevelIndication)
    {
        // Store current audio level in the RTP/RTCP module.
        // The level will be used in combination with voice-activity state
        // (frameType) to add an RTP header extension
        _rtpRtcpModule->SetAudioLevel(rms_level_.RMS());
    }

    // Push data from ACM to RTP/RTCP-module to deliver audio frame for
    // packetization.
    // This call will trigger Transport::SendPacket() from the RTP/RTCP module.
    if (_rtpRtcpModule->SendOutgoingData((FrameType&)frameType,
                                        payloadType,
                                        timeStamp,
                                        // Leaving the time when this frame was
                                        // received from the capture device as
                                        // undefined for voice for now.
                                        -1,
                                        payloadData,
                                        payloadSize,
                                        fragmentation) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "Channel::SendData() failed to send data to RTP/RTCP module");
        return -1;
    }

    _lastLocalTimeStamp = timeStamp;
    _lastPayloadType = payloadType;

    return 0;
}

int32_t
Channel::InFrameType(int16_t frameType)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::InFrameType(frameType=%d)", frameType);

    CriticalSectionScoped cs(&_callbackCritSect);
    // 1 indicates speech
    _sendFrameType = (frameType == 1) ? 1 : 0;
    return 0;
}

int32_t
Channel::OnRxVadDetected(int vadDecision)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::OnRxVadDetected(vadDecision=%d)", vadDecision);

    CriticalSectionScoped cs(&_callbackCritSect);
    if (_rxVadObserverPtr)
    {
        _rxVadObserverPtr->OnRxVad(_channelId, vadDecision);
    }

    return 0;
}

int
Channel::SendPacket(int channel, const void *data, int len)
{
    channel = VoEChannelId(channel);
    assert(channel == _channelId);

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendPacket(channel=%d, len=%d)", channel, len);

    CriticalSectionScoped cs(&_callbackCritSect);

    if (_transportPtr == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() failed to send RTP packet due to"
                     " invalid transport object");
        return -1;
    }

    uint8_t* bufferToSendPtr = (uint8_t*)data;
    int32_t bufferLength = len;

    // Dump the RTP packet to a file (if RTP dump is enabled).
    if (_rtpDumpOut.DumpPacket((const uint8_t*)data, len) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() RTP dump to output file failed");
    }

    int n = _transportPtr->SendPacket(channel, bufferToSendPtr,
                                      bufferLength);
    if (n < 0) {
      std::string transport_name =
          _externalTransport ? "external transport" : "WebRtc sockets";
      WEBRTC_TRACE(kTraceError, kTraceVoice,
                   VoEId(_instanceId,_channelId),
                   "Channel::SendPacket() RTP transmission using %s failed",
                   transport_name.c_str());
      return -1;
    }
    return n;
}

int
Channel::SendRTCPPacket(int channel, const void *data, int len)
{
    channel = VoEChannelId(channel);
    assert(channel == _channelId);

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SendRTCPPacket(channel=%d, len=%d)", channel, len);

    CriticalSectionScoped cs(&_callbackCritSect);
    if (_transportPtr == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendRTCPPacket() failed to send RTCP packet"
                     " due to invalid transport object");
        return -1;
    }

    uint8_t* bufferToSendPtr = (uint8_t*)data;
    int32_t bufferLength = len;

    // Dump the RTCP packet to a file (if RTP dump is enabled).
    if (_rtpDumpOut.DumpPacket((const uint8_t*)data, len) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::SendPacket() RTCP dump to output file failed");
    }

    int n = _transportPtr->SendRTCPPacket(channel,
                                          bufferToSendPtr,
                                          bufferLength);
    if (n < 0) {
      std::string transport_name =
          _externalTransport ? "external transport" : "WebRtc sockets";
      WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                   VoEId(_instanceId,_channelId),
                   "Channel::SendRTCPPacket() transmission using %s failed",
                   transport_name.c_str());
      return -1;
    }
    return n;
}

void
Channel::OnPlayTelephoneEvent(int32_t id,
                              uint8_t event,
                              uint16_t lengthMs,
                              uint8_t volume)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnPlayTelephoneEvent(id=%d, event=%u, lengthMs=%u,"
                 " volume=%u)", id, event, lengthMs, volume);

    if (!_playOutbandDtmfEvent || (event > 15))
    {
        // Ignore callback since feedback is disabled or event is not a
        // Dtmf tone event.
        return;
    }

    assert(_outputMixerPtr != NULL);

    // Start playing out the Dtmf tone (if playout is enabled).
    // Reduce length of tone with 80ms to the reduce risk of echo.
    _outputMixerPtr->PlayDtmfTone(event, lengthMs - 80, volume);
}

void
Channel::OnIncomingSSRCChanged(int32_t id, uint32_t ssrc)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnIncomingSSRCChanged(id=%d, SSRC=%d)",
                 id, ssrc);

    // Update ssrc so that NTP for AV sync can be updated.
    _rtpRtcpModule->SetRemoteSSRC(ssrc);
}

void Channel::OnIncomingCSRCChanged(int32_t id,
                                    uint32_t CSRC,
                                    bool added)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnIncomingCSRCChanged(id=%d, CSRC=%d, added=%d)",
                 id, CSRC, added);
}

void Channel::ResetStatistics(uint32_t ssrc) {
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(ssrc);
  if (statistician) {
    statistician->ResetStatistics();
  }
  statistics_proxy_->ResetStatistics();
}

void
Channel::OnApplicationDataReceived(int32_t id,
                                   uint8_t subType,
                                   uint32_t name,
                                   uint16_t length,
                                   const uint8_t* data)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnApplicationDataReceived(id=%d, subType=%u,"
                 " name=%u, length=%u)",
                 id, subType, name, length);

    int32_t channel = VoEChannelId(id);
    assert(channel == _channelId);

    if (_rtcpObserver)
    {
        CriticalSectionScoped cs(&_callbackCritSect);

        if (_rtcpObserverPtr)
        {
            _rtcpObserverPtr->OnApplicationDataReceived(channel,
                                                        subType,
                                                        name,
                                                        data,
                                                        length);
        }
    }
}

int32_t
Channel::OnInitializeDecoder(
    int32_t id,
    int8_t payloadType,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    int frequency,
    uint8_t channels,
    uint32_t rate)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnInitializeDecoder(id=%d, payloadType=%d, "
                 "payloadName=%s, frequency=%u, channels=%u, rate=%u)",
                 id, payloadType, payloadName, frequency, channels, rate);

    assert(VoEChannelId(id) == _channelId);

    CodecInst receiveCodec = {0};
    CodecInst dummyCodec = {0};

    receiveCodec.pltype = payloadType;
    receiveCodec.plfreq = frequency;
    receiveCodec.channels = channels;
    receiveCodec.rate = rate;
    strncpy(receiveCodec.plname, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);

    audio_coding_->Codec(payloadName, &dummyCodec, frequency, channels);
    receiveCodec.pacsize = dummyCodec.pacsize;

    // Register the new codec to the ACM
    if (audio_coding_->RegisterReceiveCodec(receiveCodec) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "Channel::OnInitializeDecoder() invalid codec ("
                     "pt=%d, name=%s) received - 1", payloadType, payloadName);
        _engineStatisticsPtr->SetLastError(VE_AUDIO_CODING_MODULE_ERROR);
        return -1;
    }

    return 0;
}

int32_t
Channel::OnReceivedPayloadData(const uint8_t* payloadData,
                               uint16_t payloadSize,
                               const WebRtcRTPHeader* rtpHeader)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::OnReceivedPayloadData(payloadSize=%d,"
                 " payloadType=%u, audioChannel=%u)",
                 payloadSize,
                 rtpHeader->header.payloadType,
                 rtpHeader->type.Audio.channel);

    if (!channel_state_.Get().playing)
    {
        // Avoid inserting into NetEQ when we are not playing. Count the
        // packet as discarded.
        WEBRTC_TRACE(kTraceStream, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "received packet is discarded since playing is not"
                     " activated");
        _numberOfDiscardedPackets++;
        return 0;
    }

    // Push the incoming payload (parsed and ready for decoding) into the ACM
    if (audio_coding_->IncomingPacket(payloadData,
                                      payloadSize,
                                      *rtpHeader) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceWarning,
            "Channel::OnReceivedPayloadData() unable to push data to the ACM");
        return -1;
    }

    // Update the packet delay.
    UpdatePacketDelay(rtpHeader->header.timestamp,
                      rtpHeader->header.sequenceNumber);

    uint16_t round_trip_time = 0;
    _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), &round_trip_time,
                        NULL, NULL, NULL);

    std::vector<uint16_t> nack_list = audio_coding_->GetNackList(
        round_trip_time);
    if (!nack_list.empty()) {
      // Can't use nack_list.data() since it's not supported by all
      // compilers.
      ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
    }
    return 0;
}

bool Channel::OnRecoveredPacket(const uint8_t* rtp_packet,
                                int rtp_packet_length) {
  RTPHeader header;
  if (!rtp_header_parser_->Parse(rtp_packet, rtp_packet_length, &header)) {
    WEBRTC_TRACE(kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "IncomingPacket invalid RTP header");
    return false;
  }
  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency < 0)
    return false;
  return ReceivePacket(rtp_packet, rtp_packet_length, header, false);
}

int32_t Channel::GetAudioFrame(int32_t id, AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetAudioFrame(id=%d)", id);

    // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
    if (audio_coding_->PlayoutData10Ms(audioFrame.sample_rate_hz_,
                                       &audioFrame) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::GetAudioFrame() PlayoutData10Ms() failed!");
        // In all likelihood, the audio in this frame is garbage. We return an
        // error so that the audio mixer module doesn't add it to the mix. As
        // a result, it won't be played out and the actions skipped here are
        // irrelevant.
        return -1;
    }

    if (_RxVadDetection)
    {
        UpdateRxVadDetection(audioFrame);
    }

    // Convert module ID to internal VoE channel ID
    audioFrame.id_ = VoEChannelId(audioFrame.id_);
    // Store speech type for dead-or-alive detection
    _outputSpeechType = audioFrame.speech_type_;

    ChannelState::State state = channel_state_.Get();

    if (state.rx_apm_is_enabled) {
      int err = rx_audioproc_->ProcessStream(&audioFrame);
      if (err) {
        LOG(LS_ERROR) << "ProcessStream() error: " << err;
        assert(false);
      }
    }

    float output_gain = 1.0f;
    float left_pan =  1.0f;
    float right_pan =  1.0f;
    {
      CriticalSectionScoped cs(&volume_settings_critsect_);
      output_gain = _outputGain;
      left_pan = _panLeft;
      right_pan= _panRight;
    }

    // Output volume scaling
    if (output_gain < 0.99f || output_gain > 1.01f)
    {
        AudioFrameOperations::ScaleWithSat(output_gain, audioFrame);
    }

    // Scale left and/or right channel(s) if stereo and master balance is
    // active

    if (left_pan != 1.0f || right_pan != 1.0f)
    {
        if (audioFrame.num_channels_ == 1)
        {
            // Emulate stereo mode since panning is active.
            // The mono signal is copied to both left and right channels here.
            AudioFrameOperations::MonoToStereo(&audioFrame);
        }
        // For true stereo mode (when we are receiving a stereo signal), no
        // action is needed.

        // Do the panning operation (the audio frame contains stereo at this
        // stage)
        AudioFrameOperations::Scale(left_pan, right_pan, audioFrame);
    }

    // Mix decoded PCM output with file if file mixing is enabled
    if (state.output_file_playing)
    {
        MixAudioWithFile(audioFrame, audioFrame.sample_rate_hz_);
    }

    // External media
    if (_outputExternalMedia)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        const bool isStereo = (audioFrame.num_channels_ == 2);
        if (_outputExternalMediaCallbackPtr)
        {
            _outputExternalMediaCallbackPtr->Process(
                _channelId,
                kPlaybackPerChannel,
                (int16_t*)audioFrame.data_,
                audioFrame.samples_per_channel_,
                audioFrame.sample_rate_hz_,
                isStereo);
        }
    }

    // Record playout if enabled
    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFileRecording && _outputFileRecorderPtr)
        {
            _outputFileRecorderPtr->RecordAudioToFile(audioFrame);
        }
    }

    // Measure audio level (0-9)
    _outputAudioLevel.ComputeLevel(audioFrame);

    if (capture_start_rtp_time_stamp_ < 0 && audioFrame.timestamp_ != 0) {
      // The first frame with a valid rtp timestamp.
      capture_start_rtp_time_stamp_ = audioFrame.timestamp_;
    }

    if (capture_start_rtp_time_stamp_ >= 0) {
      // audioFrame.timestamp_ should be valid from now on.

      // Compute elapsed time.
      int64_t unwrap_timestamp =
          rtp_ts_wraparound_handler_->Unwrap(audioFrame.timestamp_);
      audioFrame.elapsed_time_ms_ =
          (unwrap_timestamp - capture_start_rtp_time_stamp_) /
          (GetPlayoutFrequency() / 1000);

      {
        CriticalSectionScoped lock(ts_stats_lock_.get());
        // Compute ntp time.
        audioFrame.ntp_time_ms_ = ntp_estimator_.Estimate(
            audioFrame.timestamp_);
        // |ntp_time_ms_| won't be valid until at least 2 RTCP SRs are received.
        if (audioFrame.ntp_time_ms_ > 0) {
          // Compute |capture_start_ntp_time_ms_| so that
          // |capture_start_ntp_time_ms_| + |elapsed_time_ms_| == |ntp_time_ms_|
          capture_start_ntp_time_ms_ =
              audioFrame.ntp_time_ms_ - audioFrame.elapsed_time_ms_;
        }
      }
    }

    return 0;
}

int32_t
Channel::NeededFrequency(int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::NeededFrequency(id=%d)", id);

    int highestNeeded = 0;

    // Determine highest needed receive frequency
    int32_t receiveFrequency = audio_coding_->ReceiveFrequency();

    // Return the bigger of playout and receive frequency in the ACM.
    if (audio_coding_->PlayoutFrequency() > receiveFrequency)
    {
        highestNeeded = audio_coding_->PlayoutFrequency();
    }
    else
    {
        highestNeeded = receiveFrequency;
    }

    // Special case, if we're playing a file on the playout side
    // we take that frequency into consideration as well
    // This is not needed on sending side, since the codec will
    // limit the spectrum anyway.
    if (channel_state_.Get().output_file_playing)
    {
        CriticalSectionScoped cs(&_fileCritSect);
        if (_outputFilePlayerPtr)
        {
            if(_outputFilePlayerPtr->Frequency()>highestNeeded)
            {
                highestNeeded=_outputFilePlayerPtr->Frequency();
            }
        }
    }

    return(highestNeeded);
}

int32_t
Channel::CreateChannel(Channel*& channel,
                       int32_t channelId,
                       uint32_t instanceId,
                       const Config& config)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(instanceId,channelId),
                 "Channel::CreateChannel(channelId=%d, instanceId=%d)",
        channelId, instanceId);

    channel = new Channel(channelId, instanceId, config);
    if (channel == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceVoice,
                     VoEId(instanceId,channelId),
                     "Channel::CreateChannel() unable to allocate memory for"
                     " channel");
        return -1;
    }
    return 0;
}

void
Channel::PlayNotification(int32_t id, uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PlayNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void
Channel::RecordNotification(int32_t id, uint32_t durationMs)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RecordNotification(id=%d, durationMs=%d)",
                 id, durationMs);

    // Not implement yet
}

void
Channel::PlayFileEnded(int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PlayFileEnded(id=%d)", id);

    if (id == _inputFilePlayerId)
    {
        channel_state_.SetInputFilePlaying(false);
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::PlayFileEnded() => input file player module is"
                     " shutdown");
    }
    else if (id == _outputFilePlayerId)
    {
        channel_state_.SetOutputFilePlaying(false);
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::PlayFileEnded() => output file player module is"
                     " shutdown");
    }
}

void
Channel::RecordFileEnded(int32_t id)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RecordFileEnded(id=%d)", id);

    assert(id == _outputFileRecorderId);

    CriticalSectionScoped cs(&_fileCritSect);

    _outputFileRecording = false;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::RecordFileEnded() => output file recorder module is"
                 " shutdown");
}

Channel::Channel(int32_t channelId,
                 uint32_t instanceId,
                 const Config& config) :
    _fileCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    volume_settings_critsect_(*CriticalSectionWrapper::CreateCriticalSection()),
    _instanceId(instanceId),
    _channelId(channelId),
    rtp_header_parser_(RtpHeaderParser::Create()),
    rtp_payload_registry_(
        new RTPPayloadRegistry(RTPPayloadStrategy::CreateStrategy(true))),
    rtp_receive_statistics_(ReceiveStatistics::Create(
        Clock::GetRealTimeClock())),
    rtp_receiver_(RtpReceiver::CreateAudioReceiver(
        VoEModuleId(instanceId, channelId), Clock::GetRealTimeClock(), this,
        this, this, rtp_payload_registry_.get())),
    telephone_event_handler_(rtp_receiver_->GetTelephoneEventHandler()),
    audio_coding_(AudioCodingModule::Create(
        VoEModuleId(instanceId, channelId))),
    _rtpDumpIn(*RtpDump::CreateRtpDump()),
    _rtpDumpOut(*RtpDump::CreateRtpDump()),
    _outputAudioLevel(),
    _externalTransport(false),
    _inputFilePlayerPtr(NULL),
    _outputFilePlayerPtr(NULL),
    _outputFileRecorderPtr(NULL),
    // Avoid conflict with other channels by adding 1024 - 1026,
    // won't use as much as 1024 channels.
    _inputFilePlayerId(VoEModuleId(instanceId, channelId) + 1024),
    _outputFilePlayerId(VoEModuleId(instanceId, channelId) + 1025),
    _outputFileRecorderId(VoEModuleId(instanceId, channelId) + 1026),
    _outputFileRecording(false),
    _inbandDtmfQueue(VoEModuleId(instanceId, channelId)),
    _inbandDtmfGenerator(VoEModuleId(instanceId, channelId)),
    _outputExternalMedia(false),
    _inputExternalMediaCallbackPtr(NULL),
    _outputExternalMediaCallbackPtr(NULL),
    _timeStamp(0), // This is just an offset, RTP module will add it's own random offset
    _sendTelephoneEventPayloadType(106),
    ntp_estimator_(Clock::GetRealTimeClock()),
    jitter_buffer_playout_timestamp_(0),
    playout_timestamp_rtp_(0),
    playout_timestamp_rtcp_(0),
    playout_delay_ms_(0),
    _numberOfDiscardedPackets(0),
    send_sequence_number_(0),
    ts_stats_lock_(CriticalSectionWrapper::CreateCriticalSection()),
    rtp_ts_wraparound_handler_(new rtc::TimestampWrapAroundHandler()),
    capture_start_rtp_time_stamp_(-1),
    capture_start_ntp_time_ms_(-1),
    _engineStatisticsPtr(NULL),
    _outputMixerPtr(NULL),
    _transmitMixerPtr(NULL),
    _moduleProcessThreadPtr(NULL),
    _audioDeviceModulePtr(NULL),
    _voiceEngineObserverPtr(NULL),
    _callbackCritSectPtr(NULL),
    _transportPtr(NULL),
    _rxVadObserverPtr(NULL),
    _oldVadDecision(-1),
    _sendFrameType(0),
    _rtcpObserverPtr(NULL),
    _externalMixing(false),
    _mixFileWithMicrophone(false),
    _rtcpObserver(false),
    _mute(false),
    _panLeft(1.0f),
    _panRight(1.0f),
    _outputGain(1.0f),
    _playOutbandDtmfEvent(false),
    _playInbandDtmfEvent(false),
    _lastLocalTimeStamp(0),
    _lastPayloadType(0),
    _includeAudioLevelIndication(false),
    _outputSpeechType(AudioFrame::kNormalSpeech),
    vie_network_(NULL),
    video_channel_(-1),
    _average_jitter_buffer_delay_us(0),
    least_required_delay_ms_(0),
    _previousTimestamp(0),
    _recPacketDelayMs(20),
    _current_sync_offset(0),
    _RxVadDetection(false),
    _rxAgcIsEnabled(false),
    _rxNsIsEnabled(false),
    restored_packet_in_use_(false),
    bitrate_controller_(
        BitrateController::CreateBitrateController(Clock::GetRealTimeClock(),
                                                   true)),
    rtcp_bandwidth_observer_(
        bitrate_controller_->CreateRtcpBandwidthObserver()),
    send_bitrate_observer_(new VoEBitrateObserver(this)),
    network_predictor_(new NetworkPredictor(Clock::GetRealTimeClock()))
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Channel() - ctor");
    _inbandDtmfQueue.ResetDtmf();
    _inbandDtmfGenerator.Init();
    _outputAudioLevel.Clear();

    RtpRtcp::Configuration configuration;
    configuration.id = VoEModuleId(instanceId, channelId);
    configuration.audio = true;
    configuration.outgoing_transport = this;
    configuration.rtcp_feedback = this;
    configuration.audio_messages = this;
    configuration.receive_statistics = rtp_receive_statistics_.get();
    configuration.bandwidth_callback = rtcp_bandwidth_observer_.get();

    _rtpRtcpModule.reset(RtpRtcp::CreateRtpRtcp(configuration));

    statistics_proxy_.reset(new StatisticsProxy(_rtpRtcpModule->SSRC()));
    rtp_receive_statistics_->RegisterRtcpStatisticsCallback(
        statistics_proxy_.get());

    Config audioproc_config;
    audioproc_config.Set<ExperimentalAgc>(new ExperimentalAgc(false));
    rx_audioproc_.reset(AudioProcessing::Create(audioproc_config));
}

Channel::~Channel()
{
    rtp_receive_statistics_->RegisterRtcpStatisticsCallback(NULL);
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::~Channel() - dtor");

    if (_outputExternalMedia)
    {
        DeRegisterExternalMediaProcessing(kPlaybackPerChannel);
    }
    if (channel_state_.Get().input_external_media)
    {
        DeRegisterExternalMediaProcessing(kRecordingPerChannel);
    }
    StopSend();
    StopPlayout();

    {
        CriticalSectionScoped cs(&_fileCritSect);
        if (_inputFilePlayerPtr)
        {
            _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
            _inputFilePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
            _inputFilePlayerPtr = NULL;
        }
        if (_outputFilePlayerPtr)
        {
            _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
            _outputFilePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
            _outputFilePlayerPtr = NULL;
        }
        if (_outputFileRecorderPtr)
        {
            _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
            _outputFileRecorderPtr->StopRecording();
            FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
            _outputFileRecorderPtr = NULL;
        }
    }

    // The order to safely shutdown modules in a channel is:
    // 1. De-register callbacks in modules
    // 2. De-register modules in process thread
    // 3. Destroy modules
    if (audio_coding_->RegisterTransportCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register transport callback"
                     " (Audio coding module)");
    }
    if (audio_coding_->RegisterVADCallback(NULL) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to de-register VAD callback"
                     " (Audio coding module)");
    }
    // De-register modules in process thread
    if (_moduleProcessThreadPtr->DeRegisterModule(_rtpRtcpModule.get()) == -1)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "~Channel() failed to deregister RTP/RTCP module");
    }
    // End of modules shutdown

    // Delete other objects
    if (vie_network_) {
      vie_network_->Release();
      vie_network_ = NULL;
    }
    RtpDump::DestroyRtpDump(&_rtpDumpIn);
    RtpDump::DestroyRtpDump(&_rtpDumpOut);
    delete &_callbackCritSect;
    delete &_fileCritSect;
    delete &volume_settings_critsect_;
}

int32_t
Channel::Init()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Init()");

    channel_state_.Reset();

    // --- Initial sanity

    if ((_engineStatisticsPtr == NULL) ||
        (_moduleProcessThreadPtr == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice,
                     VoEId(_instanceId,_channelId),
                     "Channel::Init() must call SetEngineInformation() first");
        return -1;
    }

    // --- Add modules to process thread (for periodic schedulation)

    const bool processThreadFail =
        ((_moduleProcessThreadPtr->RegisterModule(_rtpRtcpModule.get()) != 0) ||
        false);
    if (processThreadFail)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_INIT_CHANNEL, kTraceError,
            "Channel::Init() modules not registered");
        return -1;
    }
    // --- ACM initialization

    if ((audio_coding_->InitializeReceiver() == -1) ||
#ifdef WEBRTC_CODEC_AVT
        // out-of-band Dtmf tones are played out by default
        (audio_coding_->SetDtmfPlayoutStatus(true) == -1) ||
#endif
        (audio_coding_->InitializeSender() == -1))
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "Channel::Init() unable to initialize the ACM - 1");
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
    if (_rtpRtcpModule->SetRTCPStatus(kRtcpCompound) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "Channel::Init() RTP/RTCP module not initialized");
        return -1;
    }

     // --- Register all permanent callbacks
    const bool fail =
        (audio_coding_->RegisterTransportCallback(this) == -1) ||
        (audio_coding_->RegisterVADCallback(this) == -1);

    if (fail)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_INIT_CHANNEL, kTraceError,
            "Channel::Init() callbacks not registered");
        return -1;
    }

    // --- Register all supported codecs to the receiving side of the
    // RTP/RTCP module

    CodecInst codec;
    const uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    for (int idx = 0; idx < nSupportedCodecs; idx++)
    {
        // Open up the RTP/RTCP receiver for all supported codecs
        if ((audio_coding_->Codec(idx, &codec) == -1) ||
            (rtp_receiver_->RegisterReceivePayload(
                codec.plname,
                codec.pltype,
                codec.plfreq,
                codec.channels,
                (codec.rate < 0) ? 0 : codec.rate) == -1))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::Init() unable to register %s (%d/%d/%d/%d) "
                         "to RTP/RTCP receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }
        else
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                         VoEId(_instanceId,_channelId),
                         "Channel::Init() %s (%d/%d/%d/%d) has been added to "
                         "the RTP/RTCP receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }

        // Ensure that PCMU is used as default codec on the sending side
        if (!STR_CASE_CMP(codec.plname, "PCMU") && (codec.channels == 1))
        {
            SetSendCodec(codec);
        }

        // Register default PT for outband 'telephone-event'
        if (!STR_CASE_CMP(codec.plname, "telephone-event"))
        {
            if ((_rtpRtcpModule->RegisterSendPayload(codec) == -1) ||
                (audio_coding_->RegisterReceiveCodec(codec) == -1))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId,_channelId),
                             "Channel::Init() failed to register outband "
                             "'telephone-event' (%d/%d) correctly",
                             codec.pltype, codec.plfreq);
            }
        }

        if (!STR_CASE_CMP(codec.plname, "CN"))
        {
            if ((audio_coding_->RegisterSendCodec(codec) == -1) ||
                (audio_coding_->RegisterReceiveCodec(codec) == -1) ||
                (_rtpRtcpModule->RegisterSendPayload(codec) == -1))
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId,_channelId),
                             "Channel::Init() failed to register CN (%d/%d) "
                             "correctly - 1",
                             codec.pltype, codec.plfreq);
            }
        }
#ifdef WEBRTC_CODEC_RED
        // Register RED to the receiving side of the ACM.
        // We will not receive an OnInitializeDecoder() callback for RED.
        if (!STR_CASE_CMP(codec.plname, "RED"))
        {
            if (audio_coding_->RegisterReceiveCodec(codec) == -1)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                             VoEId(_instanceId,_channelId),
                             "Channel::Init() failed to register RED (%d/%d) "
                             "correctly",
                             codec.pltype, codec.plfreq);
            }
        }
#endif
    }

    if (rx_audioproc_->noise_suppression()->set_level(kDefaultNsMode) != 0) {
      LOG_FERR1(LS_ERROR, noise_suppression()->set_level, kDefaultNsMode);
      return -1;
    }
    if (rx_audioproc_->gain_control()->set_mode(kDefaultRxAgcMode) != 0) {
      LOG_FERR1(LS_ERROR, gain_control()->set_mode, kDefaultRxAgcMode);
      return -1;
    }

    return 0;
}

int32_t
Channel::SetEngineInformation(Statistics& engineStatistics,
                              OutputMixer& outputMixer,
                              voe::TransmitMixer& transmitMixer,
                              ProcessThread& moduleProcessThread,
                              AudioDeviceModule& audioDeviceModule,
                              VoiceEngineObserver* voiceEngineObserver,
                              CriticalSectionWrapper* callbackCritSect)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetEngineInformation()");
    _engineStatisticsPtr = &engineStatistics;
    _outputMixerPtr = &outputMixer;
    _transmitMixerPtr = &transmitMixer,
    _moduleProcessThreadPtr = &moduleProcessThread;
    _audioDeviceModulePtr = &audioDeviceModule;
    _voiceEngineObserverPtr = voiceEngineObserver;
    _callbackCritSectPtr = callbackCritSect;
    return 0;
}

int32_t
Channel::UpdateLocalTimeStamp()
{

    _timeStamp += _audioFrame.samples_per_channel_;
    return 0;
}

int32_t
Channel::StartPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayout()");
    if (channel_state_.Get().playing)
    {
        return 0;
    }

    if (!_externalMixing) {
        // Add participant as candidates for mixing.
        if (_outputMixerPtr->SetMixabilityStatus(*this, true) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
                "StartPlayout() failed to add participant to mixer");
            return -1;
        }
    }

    channel_state_.SetPlaying(true);
    if (RegisterFilePlayingToMixer() != 0)
        return -1;

    return 0;
}

int32_t
Channel::StopPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopPlayout()");
    if (!channel_state_.Get().playing)
    {
        return 0;
    }

    if (!_externalMixing) {
        // Remove participant as candidates for mixing
        if (_outputMixerPtr->SetMixabilityStatus(*this, false) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
                "StopPlayout() failed to remove participant from mixer");
            return -1;
        }
    }

    channel_state_.SetPlaying(false);
    _outputAudioLevel.Clear();

    return 0;
}

int32_t
Channel::StartSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartSend()");
    // Resume the previous sequence number which was reset by StopSend().
    // This needs to be done before |sending| is set to true.
    if (send_sequence_number_)
      SetInitSequenceNumber(send_sequence_number_);

    if (channel_state_.Get().sending)
    {
      return 0;
    }
    channel_state_.SetSending(true);

    if (_rtpRtcpModule->SetSendingStatus(true) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "StartSend() RTP/RTCP failed to start sending");
        CriticalSectionScoped cs(&_callbackCritSect);
        channel_state_.SetSending(false);
        return -1;
    }

    return 0;
}

int32_t
Channel::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopSend()");
    if (!channel_state_.Get().sending)
    {
      return 0;
    }
    channel_state_.SetSending(false);

    // Store the sequence number to be able to pick up the same sequence for
    // the next StartSend(). This is needed for restarting device, otherwise
    // it might cause libSRTP to complain about packets being replayed.
    // TODO(xians): Remove this workaround after RtpRtcpModule's refactoring
    // CL is landed. See issue
    // https://code.google.com/p/webrtc/issues/detail?id=2111 .
    send_sequence_number_ = _rtpRtcpModule->SequenceNumber();

    // Reset sending SSRC and sequence number and triggers direct transmission
    // of RTCP BYE
    if (_rtpRtcpModule->SetSendingStatus(false) == -1 ||
        _rtpRtcpModule->ResetSendDataCountersRTP() == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "StartSend() RTP/RTCP failed to stop sending");
    }

    return 0;
}

int32_t
Channel::StartReceiving()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartReceiving()");
    if (channel_state_.Get().receiving)
    {
        return 0;
    }
    channel_state_.SetReceiving(true);
    _numberOfDiscardedPackets = 0;
    return 0;
}

int32_t
Channel::StopReceiving()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopReceiving()");
    if (!channel_state_.Get().receiving)
    {
        return 0;
    }

    channel_state_.SetReceiving(false);
    return 0;
}

int32_t
Channel::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }
    _voiceEngineObserverPtr = &observer;
    return 0;
}

int32_t
Channel::DeRegisterVoiceEngineObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_voiceEngineObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterVoiceEngineObserver() observer already disabled");
        return 0;
    }
    _voiceEngineObserverPtr = NULL;
    return 0;
}

int32_t
Channel::GetSendCodec(CodecInst& codec)
{
    return (audio_coding_->SendCodec(&codec));
}

int32_t
Channel::GetRecCodec(CodecInst& codec)
{
    return (audio_coding_->ReceiveCodec(&codec));
}

int32_t
Channel::SetSendCodec(const CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendCodec()");

    if (audio_coding_->RegisterSendCodec(codec) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "SetSendCodec() failed to register codec to ACM");
        return -1;
    }

    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
        if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
        {
            WEBRTC_TRACE(
                    kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                    "SetSendCodec() failed to register codec to"
                    " RTP/RTCP module");
            return -1;
        }
    }

    if (_rtpRtcpModule->SetAudioPacketSize(codec.pacsize) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "SetSendCodec() failed to set audio packet size");
        return -1;
    }

    bitrate_controller_->SetBitrateObserver(send_bitrate_observer_.get(),
                                            codec.rate, 0, 0);

    return 0;
}

void
Channel::OnNetworkChanged(const uint32_t bitrate_bps,
                          const uint8_t fraction_lost,  // 0 - 255.
                          const uint32_t rtt) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
      "Channel::OnNetworkChanged(bitrate_bps=%d, fration_lost=%d, rtt=%d)",
      bitrate_bps, fraction_lost, rtt);
  // |fraction_lost| from BitrateObserver is short time observation of packet
  // loss rate from past. We use network predictor to make a more reasonable
  // loss rate estimation.
  network_predictor_->UpdatePacketLossRate(fraction_lost);
  uint8_t loss_rate = network_predictor_->GetLossRate();
  // Normalizes rate to 0 - 100.
  if (audio_coding_->SetPacketLossRate(100 * loss_rate / 255) != 0) {
    _engineStatisticsPtr->SetLastError(VE_AUDIO_CODING_MODULE_ERROR,
        kTraceError, "OnNetworkChanged() failed to set packet loss rate");
    assert(false);  // This should not happen.
  }
}

int32_t
Channel::SetVADStatus(bool enableVAD, ACMVADMode mode, bool disableDTX)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetVADStatus(mode=%d)", mode);
    // To disable VAD, DTX must be disabled too
    disableDTX = ((enableVAD == false) ? true : disableDTX);
    if (audio_coding_->SetVAD(!disableDTX, enableVAD, mode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetVADStatus() failed to set VAD");
        return -1;
    }
    return 0;
}

int32_t
Channel::GetVADStatus(bool& enabledVAD, ACMVADMode& mode, bool& disabledDTX)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetVADStatus");
    if (audio_coding_->VAD(&disabledDTX, &enabledVAD, &mode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "GetVADStatus() failed to get VAD status");
        return -1;
    }
    disabledDTX = !disabledDTX;
    return 0;
}

int32_t
Channel::SetRecPayloadType(const CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRecPayloadType()");

    if (channel_state_.Get().playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceError,
            "SetRecPayloadType() unable to set PT while playing");
        return -1;
    }
    if (channel_state_.Get().receiving)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_LISTENING, kTraceError,
            "SetRecPayloadType() unable to set PT while listening");
        return -1;
    }

    if (codec.pltype == -1)
    {
        // De-register the selected codec (RTP/RTCP module and ACM)

        int8_t pltype(-1);
        CodecInst rxCodec = codec;

        // Get payload type for the given codec
        rtp_payload_registry_->ReceivePayloadType(
            rxCodec.plname,
            rxCodec.plfreq,
            rxCodec.channels,
            (rxCodec.rate < 0) ? 0 : rxCodec.rate,
            &pltype);
        rxCodec.pltype = pltype;

        if (rtp_receiver_->DeRegisterReceivePayload(pltype) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                    VE_RTP_RTCP_MODULE_ERROR,
                    kTraceError,
                    "SetRecPayloadType() RTP/RTCP-module deregistration "
                    "failed");
            return -1;
        }
        if (audio_coding_->UnregisterReceiveCodec(rxCodec.pltype) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() ACM deregistration failed - 1");
            return -1;
        }
        return 0;
    }

    if (rtp_receiver_->RegisterReceivePayload(
        codec.plname,
        codec.pltype,
        codec.plfreq,
        codec.channels,
        (codec.rate < 0) ? 0 : codec.rate) != 0)
    {
        // First attempt to register failed => de-register and try again
        rtp_receiver_->DeRegisterReceivePayload(codec.pltype);
        if (rtp_receiver_->RegisterReceivePayload(
            codec.plname,
            codec.pltype,
            codec.plfreq,
            codec.channels,
            (codec.rate < 0) ? 0 : codec.rate) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() RTP/RTCP-module registration failed");
            return -1;
        }
    }
    if (audio_coding_->RegisterReceiveCodec(codec) != 0)
    {
        audio_coding_->UnregisterReceiveCodec(codec.pltype);
        if (audio_coding_->RegisterReceiveCodec(codec) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
                "SetRecPayloadType() ACM registration failed - 1");
            return -1;
        }
    }
    return 0;
}

int32_t
Channel::GetRecPayloadType(CodecInst& codec)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRecPayloadType()");
    int8_t payloadType(-1);
    if (rtp_payload_registry_->ReceivePayloadType(
        codec.plname,
        codec.plfreq,
        codec.channels,
        (codec.rate < 0) ? 0 : codec.rate,
        &payloadType) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceWarning,
            "GetRecPayloadType() failed to retrieve RX payload type");
        return -1;
    }
    codec.pltype = payloadType;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRecPayloadType() => pltype=%u", codec.pltype);
    return 0;
}

int32_t
Channel::SetSendCNPayloadType(int type, PayloadFrequencies frequency)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetSendCNPayloadType()");

    CodecInst codec;
    int32_t samplingFreqHz(-1);
    const int kMono = 1;
    if (frequency == kFreq32000Hz)
        samplingFreqHz = 32000;
    else if (frequency == kFreq16000Hz)
        samplingFreqHz = 16000;

    if (audio_coding_->Codec("CN", &codec, samplingFreqHz, kMono) == -1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetSendCNPayloadType() failed to retrieve default CN codec "
            "settings");
        return -1;
    }

    // Modify the payload type (must be set to dynamic range)
    codec.pltype = type;

    if (audio_coding_->RegisterSendCodec(codec) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetSendCNPayloadType() failed to register CN to ACM");
        return -1;
    }

    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
        if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetSendCNPayloadType() failed to register CN to RTP/RTCP "
                "module");
            return -1;
        }
    }
    return 0;
}

int Channel::SetOpusMaxPlaybackRate(int frequency_hz) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetOpusMaxPlaybackRate()");

  if (audio_coding_->SetOpusMaxPlaybackRate(frequency_hz) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetOpusMaxPlaybackRate() failed to set maximum playback rate");
    return -1;
  }
  return 0;
}

int32_t Channel::RegisterExternalTransport(Transport& transport)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::RegisterExternalTransport()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (_externalTransport)
    {
        _engineStatisticsPtr->SetLastError(VE_INVALID_OPERATION,
                                           kTraceError,
              "RegisterExternalTransport() external transport already enabled");
       return -1;
    }
    _externalTransport = true;
    _transportPtr = &transport;
    return 0;
}

int32_t
Channel::DeRegisterExternalTransport()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterExternalTransport()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_transportPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterExternalTransport() external transport already "
            "disabled");
        return 0;
    }
    _externalTransport = false;
    _transportPtr = NULL;
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "DeRegisterExternalTransport() all transport is disabled");
    return 0;
}

int32_t Channel::ReceivedRTPPacket(const int8_t* data, int32_t length,
                                   const PacketTime& packet_time) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::ReceivedRTPPacket()");

  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false);

  // Dump the RTP packet to a file (if RTP dump is enabled).
  if (_rtpDumpIn.DumpPacket((const uint8_t*)data,
                            (uint16_t)length) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::SendPacket() RTP dump to input file failed");
  }
  const uint8_t* received_packet = reinterpret_cast<const uint8_t*>(data);
  RTPHeader header;
  if (!rtp_header_parser_->Parse(received_packet, length, &header)) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Incoming packet: invalid RTP header");
    return -1;
  }
  header.payload_type_frequency =
      rtp_payload_registry_->GetPayloadTypeFrequency(header.payloadType);
  if (header.payload_type_frequency < 0)
    return -1;
  bool in_order = IsPacketInOrder(header);
  rtp_receive_statistics_->IncomingPacket(header, length,
      IsPacketRetransmitted(header, in_order));
  rtp_payload_registry_->SetIncomingPayloadType(header);

  // Forward any packets to ViE bandwidth estimator, if enabled.
  {
    CriticalSectionScoped cs(&_callbackCritSect);
    if (vie_network_) {
      int64_t arrival_time_ms;
      if (packet_time.timestamp != -1) {
        arrival_time_ms = (packet_time.timestamp + 500) / 1000;
      } else {
        arrival_time_ms = TickTime::MillisecondTimestamp();
      }
      int payload_length = length - header.headerLength;
      vie_network_->ReceivedBWEPacket(video_channel_, arrival_time_ms,
                                      payload_length, header);
    }
  }

  return ReceivePacket(received_packet, length, header, in_order) ? 0 : -1;
}

bool Channel::ReceivePacket(const uint8_t* packet,
                            int packet_length,
                            const RTPHeader& header,
                            bool in_order) {
  if (rtp_payload_registry_->IsEncapsulated(header)) {
    return HandleEncapsulation(packet, packet_length, header);
  }
  const uint8_t* payload = packet + header.headerLength;
  int payload_length = packet_length - header.headerLength;
  assert(payload_length >= 0);
  PayloadUnion payload_specific;
  if (!rtp_payload_registry_->GetPayloadSpecifics(header.payloadType,
                                                  &payload_specific)) {
    return false;
  }
  return rtp_receiver_->IncomingRtpPacket(header, payload, payload_length,
                                          payload_specific, in_order);
}

bool Channel::HandleEncapsulation(const uint8_t* packet,
                                  int packet_length,
                                  const RTPHeader& header) {
  if (!rtp_payload_registry_->IsRtx(header))
    return false;

  // Remove the RTX header and parse the original RTP header.
  if (packet_length < header.headerLength)
    return false;
  if (packet_length > kVoiceEngineMaxIpPacketSizeBytes)
    return false;
  if (restored_packet_in_use_) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Multiple RTX headers detected, dropping packet");
    return false;
  }
  uint8_t* restored_packet_ptr = restored_packet_;
  if (!rtp_payload_registry_->RestoreOriginalPacket(
      &restored_packet_ptr, packet, &packet_length, rtp_receiver_->SSRC(),
      header)) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVoice, _channelId,
                 "Incoming RTX packet: invalid RTP header");
    return false;
  }
  restored_packet_in_use_ = true;
  bool ret = OnRecoveredPacket(restored_packet_ptr, packet_length);
  restored_packet_in_use_ = false;
  return ret;
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
  // Retransmissions are handled separately if RTX is enabled.
  if (rtp_payload_registry_->RtxEnabled())
    return false;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(header.ssrc);
  if (!statistician)
    return false;
  // Check if this is a retransmission.
  uint16_t min_rtt = 0;
  _rtpRtcpModule->RTT(rtp_receiver_->SSRC(), NULL, NULL, &min_rtt, NULL);
  return !in_order &&
      statistician->IsRetransmitOfOldPacket(header, min_rtt);
}

int32_t Channel::ReceivedRTCPPacket(const int8_t* data, int32_t length) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::ReceivedRTCPPacket()");
  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true);

  // Dump the RTCP packet to a file (if RTP dump is enabled).
  if (_rtpDumpIn.DumpPacket((const uint8_t*)data,
                            (uint16_t)length) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "Channel::SendPacket() RTCP dump to input file failed");
  }

  // Deliver RTCP packet to RTP/RTCP module for parsing
  if (_rtpRtcpModule->IncomingRtcpPacket((const uint8_t*)data,
                                         (uint16_t)length) == -1) {
    _engineStatisticsPtr->SetLastError(
        VE_SOCKET_TRANSPORT_MODULE_ERROR, kTraceWarning,
        "Channel::IncomingRTPPacket() RTCP packet is invalid");
  }

  {
    CriticalSectionScoped lock(ts_stats_lock_.get());
    uint16_t rtt = GetRTT();
    if (rtt == 0) {
      // Waiting for valid RTT.
      return 0;
    }
    uint32_t ntp_secs = 0;
    uint32_t ntp_frac = 0;
    uint32_t rtp_timestamp = 0;
    if (0 != _rtpRtcpModule->RemoteNTP(&ntp_secs, &ntp_frac, NULL, NULL,
                                       &rtp_timestamp)) {
      // Waiting for RTCP.
      return 0;
    }
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
  }
  return 0;
}

int Channel::StartPlayingFileLocally(const char* fileName,
                                     bool loop,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileLocally(fileNameUTF8[]=%s, loop=%d,"
                 " format=%d, volumeScaling=%5.3f, startPosition=%d, "
                 "stopPosition=%d)", fileName, loop, format, volumeScaling,
                 startPosition, stopPosition);

    if (channel_state_.Get().output_file_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceError,
            "StartPlayingFileLocally() is already playing");
        return -1;
    }

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFilePlayerPtr)
        {
            _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
            FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
            _outputFilePlayerPtr = NULL;
        }

        _outputFilePlayerPtr = FilePlayer::CreateFilePlayer(
            _outputFilePlayerId, (const FileFormats)format);

        if (_outputFilePlayerPtr == NULL)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "StartPlayingFileLocally() filePlayer format is not correct");
            return -1;
        }

        const uint32_t notificationTime(0);

        if (_outputFilePlayerPtr->StartPlayingFile(
                fileName,
                loop,
                startPosition,
                volumeScaling,
                notificationTime,
                stopPosition,
                (const CodecInst*)codecInst) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_BAD_FILE, kTraceError,
                "StartPlayingFile() failed to start file playout");
            _outputFilePlayerPtr->StopPlayingFile();
            FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
            _outputFilePlayerPtr = NULL;
            return -1;
        }
        _outputFilePlayerPtr->RegisterModuleFileCallback(this);
        channel_state_.SetOutputFilePlaying(true);
    }

    if (RegisterFilePlayingToMixer() != 0)
        return -1;

    return 0;
}

int Channel::StartPlayingFileLocally(InStream* stream,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileLocally(format=%d,"
                 " volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);

    if(stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileLocally() NULL as input stream");
        return -1;
    }


    if (channel_state_.Get().output_file_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceError,
            "StartPlayingFileLocally() is already playing");
        return -1;
    }

    {
      CriticalSectionScoped cs(&_fileCritSect);

      // Destroy the old instance
      if (_outputFilePlayerPtr)
      {
          _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
          FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
          _outputFilePlayerPtr = NULL;
      }

      // Create the instance
      _outputFilePlayerPtr = FilePlayer::CreateFilePlayer(
          _outputFilePlayerId,
          (const FileFormats)format);

      if (_outputFilePlayerPtr == NULL)
      {
          _engineStatisticsPtr->SetLastError(
              VE_INVALID_ARGUMENT, kTraceError,
              "StartPlayingFileLocally() filePlayer format isnot correct");
          return -1;
      }

      const uint32_t notificationTime(0);

      if (_outputFilePlayerPtr->StartPlayingFile(*stream, startPosition,
                                                 volumeScaling,
                                                 notificationTime,
                                                 stopPosition, codecInst) != 0)
      {
          _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                             "StartPlayingFile() failed to "
                                             "start file playout");
          _outputFilePlayerPtr->StopPlayingFile();
          FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
          _outputFilePlayerPtr = NULL;
          return -1;
      }
      _outputFilePlayerPtr->RegisterModuleFileCallback(this);
      channel_state_.SetOutputFilePlaying(true);
    }

    if (RegisterFilePlayingToMixer() != 0)
        return -1;

    return 0;
}

int Channel::StopPlayingFileLocally()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopPlayingFileLocally()");

    if (!channel_state_.Get().output_file_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopPlayingFileLocally() isnot playing");
        return 0;
    }

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFilePlayerPtr->StopPlayingFile() != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_STOP_RECORDING_FAILED, kTraceError,
                "StopPlayingFile() could not stop playing");
            return -1;
        }
        _outputFilePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
        _outputFilePlayerPtr = NULL;
        channel_state_.SetOutputFilePlaying(false);
    }
    // _fileCritSect cannot be taken while calling
    // SetAnonymousMixibilityStatus. Refer to comments in
    // StartPlayingFileLocally(const char* ...) for more details.
    if (_outputMixerPtr->SetAnonymousMixabilityStatus(*this, false) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
            "StopPlayingFile() failed to stop participant from playing as"
            "file in the mixer");
        return -1;
    }

    return 0;
}

int Channel::IsPlayingFileLocally() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::IsPlayingFileLocally()");

    return channel_state_.Get().output_file_playing;
}

int Channel::RegisterFilePlayingToMixer()
{
    // Return success for not registering for file playing to mixer if:
    // 1. playing file before playout is started on that channel.
    // 2. starting playout without file playing on that channel.
    if (!channel_state_.Get().playing ||
        !channel_state_.Get().output_file_playing)
    {
        return 0;
    }

    // |_fileCritSect| cannot be taken while calling
    // SetAnonymousMixabilityStatus() since as soon as the participant is added
    // frames can be pulled by the mixer. Since the frames are generated from
    // the file, _fileCritSect will be taken. This would result in a deadlock.
    if (_outputMixerPtr->SetAnonymousMixabilityStatus(*this, true) != 0)
    {
        channel_state_.SetOutputFilePlaying(false);
        CriticalSectionScoped cs(&_fileCritSect);
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CONF_MIX_MODULE_ERROR, kTraceError,
            "StartPlayingFile() failed to add participant as file to mixer");
        _outputFilePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_outputFilePlayerPtr);
        _outputFilePlayerPtr = NULL;
        return -1;
    }

    return 0;
}

int Channel::StartPlayingFileAsMicrophone(const char* fileName,
                                          bool loop,
                                          FileFormats format,
                                          int startPosition,
                                          float volumeScaling,
                                          int stopPosition,
                                          const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileAsMicrophone(fileNameUTF8[]=%s, "
                 "loop=%d, format=%d, volumeScaling=%5.3f, startPosition=%d, "
                 "stopPosition=%d)", fileName, loop, format, volumeScaling,
                 startPosition, stopPosition);

    CriticalSectionScoped cs(&_fileCritSect);

    if (channel_state_.Get().input_file_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() filePlayer is playing");
        return 0;
    }

    // Destroy the old instance
    if (_inputFilePlayerPtr)
    {
        _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
    }

    // Create the instance
    _inputFilePlayerPtr = FilePlayer::CreateFilePlayer(
        _inputFilePlayerId, (const FileFormats)format);

    if (_inputFilePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingFileAsMicrophone() filePlayer format isnot correct");
        return -1;
    }

    const uint32_t notificationTime(0);

    if (_inputFilePlayerPtr->StartPlayingFile(
        fileName,
        loop,
        startPosition,
        volumeScaling,
        notificationTime,
        stopPosition,
        (const CodecInst*)codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFile() failed to start file playout");
        _inputFilePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
        return -1;
    }
    _inputFilePlayerPtr->RegisterModuleFileCallback(this);
    channel_state_.SetInputFilePlaying(true);

    return 0;
}

int Channel::StartPlayingFileAsMicrophone(InStream* stream,
                                          FileFormats format,
                                          int startPosition,
                                          float volumeScaling,
                                          int stopPosition,
                                          const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartPlayingFileAsMicrophone(format=%d, "
                 "volumeScaling=%5.3f, startPosition=%d, stopPosition=%d)",
                 format, volumeScaling, startPosition, stopPosition);

    if(stream == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartPlayingFileAsMicrophone NULL as input stream");
        return -1;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    if (channel_state_.Get().input_file_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_PLAYING, kTraceWarning,
            "StartPlayingFileAsMicrophone() is playing");
        return 0;
    }

    // Destroy the old instance
    if (_inputFilePlayerPtr)
    {
        _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
    }

    // Create the instance
    _inputFilePlayerPtr = FilePlayer::CreateFilePlayer(
        _inputFilePlayerId, (const FileFormats)format);

    if (_inputFilePlayerPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartPlayingInputFile() filePlayer format isnot correct");
        return -1;
    }

    const uint32_t notificationTime(0);

    if (_inputFilePlayerPtr->StartPlayingFile(*stream, startPosition,
                                              volumeScaling, notificationTime,
                                              stopPosition, codecInst) != 0)
    {
        _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                           "StartPlayingFile() failed to start "
                                           "file playout");
        _inputFilePlayerPtr->StopPlayingFile();
        FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
        _inputFilePlayerPtr = NULL;
        return -1;
    }

    _inputFilePlayerPtr->RegisterModuleFileCallback(this);
    channel_state_.SetInputFilePlaying(true);

    return 0;
}

int Channel::StopPlayingFileAsMicrophone()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StopPlayingFileAsMicrophone()");

    CriticalSectionScoped cs(&_fileCritSect);

    if (!channel_state_.Get().input_file_playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "StopPlayingFileAsMicrophone() isnot playing");
        return 0;
    }

    if (_inputFilePlayerPtr->StopPlayingFile() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopPlayingFile() could not stop playing");
        return -1;
    }
    _inputFilePlayerPtr->RegisterModuleFileCallback(NULL);
    FilePlayer::DestroyFilePlayer(_inputFilePlayerPtr);
    _inputFilePlayerPtr = NULL;
    channel_state_.SetInputFilePlaying(false);

    return 0;
}

int Channel::IsPlayingFileAsMicrophone() const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::IsPlayingFileAsMicrophone()");
    return channel_state_.Get().input_file_playing;
}

int Channel::StartRecordingPlayout(const char* fileName,
                                   const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartRecordingPlayout(fileName=%s)", fileName);

    if (_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingPlayout() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec={100,"L16",16000,320,1,320000};

    if ((codecInst != NULL) &&
      ((codecInst->channels < 1) || (codecInst->channels > 2)))
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingPlayout() invalid compression");
        return(-1);
    }
    if(codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst=&dummyCodec;
    }
    else if((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    }
    else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_outputFileRecorderPtr)
    {
        _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
    }

    _outputFileRecorderPtr = FileRecorder::CreateFileRecorder(
        _outputFileRecorderId, (const FileFormats)format);
    if (_outputFileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingPlayout() fileRecorder format isnot correct");
        return -1;
    }

    if (_outputFileRecorderPtr->StartRecordingAudioFile(
        fileName, (const CodecInst&)*codecInst, notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRecordingAudioFile() failed to start file recording");
        _outputFileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
        return -1;
    }
    _outputFileRecorderPtr->RegisterModuleFileCallback(this);
    _outputFileRecording = true;

    return 0;
}

int Channel::StartRecordingPlayout(OutStream* stream,
                                   const CodecInst* codecInst)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::StartRecordingPlayout()");

    if (_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,-1),
                     "StartRecordingPlayout() is already recording");
        return 0;
    }

    FileFormats format;
    const uint32_t notificationTime(0); // Not supported in VoE
    CodecInst dummyCodec={100,"L16",16000,320,1,320000};

    if (codecInst != NULL && codecInst->channels != 1)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_ARGUMENT, kTraceError,
            "StartRecordingPlayout() invalid compression");
        return(-1);
    }
    if(codecInst == NULL)
    {
        format = kFileFormatPcm16kHzFile;
        codecInst=&dummyCodec;
    }
    else if((STR_CASE_CMP(codecInst->plname,"L16") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMU") == 0) ||
        (STR_CASE_CMP(codecInst->plname,"PCMA") == 0))
    {
        format = kFileFormatWavFile;
    }
    else
    {
        format = kFileFormatCompressedFile;
    }

    CriticalSectionScoped cs(&_fileCritSect);

    // Destroy the old instance
    if (_outputFileRecorderPtr)
    {
        _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
    }

    _outputFileRecorderPtr = FileRecorder::CreateFileRecorder(
        _outputFileRecorderId, (const FileFormats)format);
    if (_outputFileRecorderPtr == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRecordingPlayout() fileRecorder format isnot correct");
        return -1;
    }

    if (_outputFileRecorderPtr->StartRecordingAudioFile(*stream, *codecInst,
                                                        notificationTime) != 0)
    {
        _engineStatisticsPtr->SetLastError(VE_BAD_FILE, kTraceError,
                                           "StartRecordingPlayout() failed to "
                                           "start file recording");
        _outputFileRecorderPtr->StopRecording();
        FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
        _outputFileRecorderPtr = NULL;
        return -1;
    }

    _outputFileRecorderPtr->RegisterModuleFileCallback(this);
    _outputFileRecording = true;

    return 0;
}

int Channel::StopRecordingPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "Channel::StopRecordingPlayout()");

    if (!_outputFileRecording)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                     "StopRecordingPlayout() isnot recording");
        return -1;
    }


    CriticalSectionScoped cs(&_fileCritSect);

    if (_outputFileRecorderPtr->StopRecording() != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_STOP_RECORDING_FAILED, kTraceError,
            "StopRecording() could not stop recording");
        return(-1);
    }
    _outputFileRecorderPtr->RegisterModuleFileCallback(NULL);
    FileRecorder::DestroyFileRecorder(_outputFileRecorderPtr);
    _outputFileRecorderPtr = NULL;
    _outputFileRecording = false;

    return 0;
}

void
Channel::SetMixWithMicStatus(bool mix)
{
    CriticalSectionScoped cs(&_fileCritSect);
    _mixFileWithMicrophone=mix;
}

int
Channel::GetSpeechOutputLevel(uint32_t& level) const
{
    int8_t currentLevel = _outputAudioLevel.Level();
    level = static_cast<int32_t> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSpeechOutputLevel() => level=%u", level);
    return 0;
}

int
Channel::GetSpeechOutputLevelFullRange(uint32_t& level) const
{
    int16_t currentLevel = _outputAudioLevel.LevelFullRange();
    level = static_cast<int32_t> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSpeechOutputLevelFullRange() => level=%u", level);
    return 0;
}

int
Channel::SetMute(bool enable)
{
    CriticalSectionScoped cs(&volume_settings_critsect_);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetMute(enable=%d)", enable);
    _mute = enable;
    return 0;
}

bool
Channel::Mute() const
{
    CriticalSectionScoped cs(&volume_settings_critsect_);
    return _mute;
}

int
Channel::SetOutputVolumePan(float left, float right)
{
    CriticalSectionScoped cs(&volume_settings_critsect_);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetOutputVolumePan()");
    _panLeft = left;
    _panRight = right;
    return 0;
}

int
Channel::GetOutputVolumePan(float& left, float& right) const
{
    CriticalSectionScoped cs(&volume_settings_critsect_);
    left = _panLeft;
    right = _panRight;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetOutputVolumePan() => left=%3.2f, right=%3.2f", left, right);
    return 0;
}

int
Channel::SetChannelOutputVolumeScaling(float scaling)
{
    CriticalSectionScoped cs(&volume_settings_critsect_);
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetChannelOutputVolumeScaling()");
    _outputGain = scaling;
    return 0;
}

int
Channel::GetChannelOutputVolumeScaling(float& scaling) const
{
    CriticalSectionScoped cs(&volume_settings_critsect_);
    scaling = _outputGain;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetChannelOutputVolumeScaling() => scaling=%3.2f", scaling);
    return 0;
}

int Channel::SendTelephoneEventOutband(unsigned char eventCode,
                                       int lengthMs, int attenuationDb,
                                       bool playDtmfEvent)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendTelephoneEventOutband(..., playDtmfEvent=%d)",
               playDtmfEvent);

    _playOutbandDtmfEvent = playDtmfEvent;

    if (_rtpRtcpModule->SendTelephoneEventOutband(eventCode, lengthMs,
                                                 attenuationDb) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SEND_DTMF_FAILED,
            kTraceWarning,
            "SendTelephoneEventOutband() failed to send event");
        return -1;
    }
    return 0;
}

int Channel::SendTelephoneEventInband(unsigned char eventCode,
                                         int lengthMs,
                                         int attenuationDb,
                                         bool playDtmfEvent)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SendTelephoneEventInband(..., playDtmfEvent=%d)",
               playDtmfEvent);

    _playInbandDtmfEvent = playDtmfEvent;
    _inbandDtmfQueue.AddDtmf(eventCode, lengthMs, attenuationDb);

    return 0;
}

int
Channel::SetSendTelephoneEventPayloadType(unsigned char type)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetSendTelephoneEventPayloadType()");
    if (type > 127)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetSendTelephoneEventPayloadType() invalid type");
        return -1;
    }
    CodecInst codec = {};
    codec.plfreq = 8000;
    codec.pltype = type;
    memcpy(codec.plname, "telephone-event", 16);
    if (_rtpRtcpModule->RegisterSendPayload(codec) != 0)
    {
        _rtpRtcpModule->DeRegisterSendPayload(codec.pltype);
        if (_rtpRtcpModule->RegisterSendPayload(codec) != 0) {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "SetSendTelephoneEventPayloadType() failed to register send"
                "payload type");
            return -1;
        }
    }
    _sendTelephoneEventPayloadType = type;
    return 0;
}

int
Channel::GetSendTelephoneEventPayloadType(unsigned char& type)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetSendTelephoneEventPayloadType()");
    type = _sendTelephoneEventPayloadType;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetSendTelephoneEventPayloadType() => type=%u", type);
    return 0;
}

int
Channel::UpdateRxVadDetection(AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdateRxVadDetection()");

    int vadDecision = 1;

    vadDecision = (audioFrame.vad_activity_ == AudioFrame::kVadActive)? 1 : 0;

    if ((vadDecision != _oldVadDecision) && _rxVadObserverPtr)
    {
        OnRxVadDetected(vadDecision);
        _oldVadDecision = vadDecision;
    }

    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdateRxVadDetection() => vadDecision=%d",
                 vadDecision);
    return 0;
}

int
Channel::RegisterRxVadObserver(VoERxVadCallback &observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterRxVadObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_rxVadObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterRxVadObserver() observer already enabled");
        return -1;
    }
    _rxVadObserverPtr = &observer;
    _RxVadDetection = true;
    return 0;
}

int
Channel::DeRegisterRxVadObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterRxVadObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_rxVadObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterRxVadObserver() observer already disabled");
        return 0;
    }
    _rxVadObserverPtr = NULL;
    _RxVadDetection = false;
    return 0;
}

int
Channel::VoiceActivityIndicator(int &activity)
{
    activity = _sendFrameType;

    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::VoiceActivityIndicator(indicator=%d)", activity);
    return 0;
}

#ifdef WEBRTC_VOICE_ENGINE_AGC

int
Channel::SetRxAgcStatus(bool enable, AgcModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxAgcStatus(enable=%d, mode=%d)",
                 (int)enable, (int)mode);

    GainControl::Mode agcMode = kDefaultRxAgcMode;
    switch (mode)
    {
        case kAgcDefault:
            break;
        case kAgcUnchanged:
            agcMode = rx_audioproc_->gain_control()->mode();
            break;
        case kAgcFixedDigital:
            agcMode = GainControl::kFixedDigital;
            break;
        case kAgcAdaptiveDigital:
            agcMode =GainControl::kAdaptiveDigital;
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_ARGUMENT, kTraceError,
                "SetRxAgcStatus() invalid Agc mode");
            return -1;
    }

    if (rx_audioproc_->gain_control()->set_mode(agcMode) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Agc mode");
        return -1;
    }
    if (rx_audioproc_->gain_control()->Enable(enable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcStatus() failed to set Agc state");
        return -1;
    }

    _rxAgcIsEnabled = enable;
    channel_state_.SetRxApmIsEnabled(_rxAgcIsEnabled || _rxNsIsEnabled);

    return 0;
}

int
Channel::GetRxAgcStatus(bool& enabled, AgcModes& mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::GetRxAgcStatus(enable=?, mode=?)");

    bool enable = rx_audioproc_->gain_control()->is_enabled();
    GainControl::Mode agcMode =
        rx_audioproc_->gain_control()->mode();

    enabled = enable;

    switch (agcMode)
    {
        case GainControl::kFixedDigital:
            mode = kAgcFixedDigital;
            break;
        case GainControl::kAdaptiveDigital:
            mode = kAgcAdaptiveDigital;
            break;
        default:
            _engineStatisticsPtr->SetLastError(
                VE_APM_ERROR, kTraceError,
                "GetRxAgcStatus() invalid Agc mode");
            return -1;
    }

    return 0;
}

int
Channel::SetRxAgcConfig(AgcConfig config)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxAgcConfig()");

    if (rx_audioproc_->gain_control()->set_target_level_dbfs(
        config.targetLeveldBOv) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set target peak |level|"
            "(or envelope) of the Agc");
        return -1;
    }
    if (rx_audioproc_->gain_control()->set_compression_gain_db(
        config.digitalCompressionGaindB) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set the range in |gain| the"
            " digital compression stage may apply");
        return -1;
    }
    if (rx_audioproc_->gain_control()->enable_limiter(
        config.limiterEnable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxAgcConfig() failed to set hard limiter to the signal");
        return -1;
    }

    return 0;
}

int
Channel::GetRxAgcConfig(AgcConfig& config)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRxAgcConfig(config=%?)");

    config.targetLeveldBOv =
        rx_audioproc_->gain_control()->target_level_dbfs();
    config.digitalCompressionGaindB =
        rx_audioproc_->gain_control()->compression_gain_db();
    config.limiterEnable =
        rx_audioproc_->gain_control()->is_limiter_enabled();

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId), "GetRxAgcConfig() => "
                   "targetLeveldBOv=%u, digitalCompressionGaindB=%u,"
                   " limiterEnable=%d",
                   config.targetLeveldBOv,
                   config.digitalCompressionGaindB,
                   config.limiterEnable);

    return 0;
}

#endif // #ifdef WEBRTC_VOICE_ENGINE_AGC

#ifdef WEBRTC_VOICE_ENGINE_NR

int
Channel::SetRxNsStatus(bool enable, NsModes mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRxNsStatus(enable=%d, mode=%d)",
                 (int)enable, (int)mode);

    NoiseSuppression::Level nsLevel = kDefaultNsMode;
    switch (mode)
    {

        case kNsDefault:
            break;
        case kNsUnchanged:
            nsLevel = rx_audioproc_->noise_suppression()->level();
            break;
        case kNsConference:
            nsLevel = NoiseSuppression::kHigh;
            break;
        case kNsLowSuppression:
            nsLevel = NoiseSuppression::kLow;
            break;
        case kNsModerateSuppression:
            nsLevel = NoiseSuppression::kModerate;
            break;
        case kNsHighSuppression:
            nsLevel = NoiseSuppression::kHigh;
            break;
        case kNsVeryHighSuppression:
            nsLevel = NoiseSuppression::kVeryHigh;
            break;
    }

    if (rx_audioproc_->noise_suppression()->set_level(nsLevel)
        != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxNsStatus() failed to set NS level");
        return -1;
    }
    if (rx_audioproc_->noise_suppression()->Enable(enable) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetRxNsStatus() failed to set NS state");
        return -1;
    }

    _rxNsIsEnabled = enable;
    channel_state_.SetRxApmIsEnabled(_rxAgcIsEnabled || _rxNsIsEnabled);

    return 0;
}

int
Channel::GetRxNsStatus(bool& enabled, NsModes& mode)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRxNsStatus(enable=?, mode=?)");

    bool enable =
        rx_audioproc_->noise_suppression()->is_enabled();
    NoiseSuppression::Level ncLevel =
        rx_audioproc_->noise_suppression()->level();

    enabled = enable;

    switch (ncLevel)
    {
        case NoiseSuppression::kLow:
            mode = kNsLowSuppression;
            break;
        case NoiseSuppression::kModerate:
            mode = kNsModerateSuppression;
            break;
        case NoiseSuppression::kHigh:
            mode = kNsHighSuppression;
            break;
        case NoiseSuppression::kVeryHigh:
            mode = kNsVeryHighSuppression;
            break;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetRxNsStatus() => enabled=%d, mode=%d", enabled, mode);
    return 0;
}

#endif // #ifdef WEBRTC_VOICE_ENGINE_NR

int
Channel::RegisterRTCPObserver(VoERTCPObserver& observer)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterRTCPObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (_rtcpObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "RegisterRTCPObserver() observer already enabled");
        return -1;
    }

    _rtcpObserverPtr = &observer;
    _rtcpObserver = true;

    return 0;
}

int
Channel::DeRegisterRTCPObserver()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::DeRegisterRTCPObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);

    if (!_rtcpObserverPtr)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceWarning,
            "DeRegisterRTCPObserver() observer already disabled");
        return 0;
    }

    _rtcpObserver = false;
    _rtcpObserverPtr = NULL;

    return 0;
}

int
Channel::SetLocalSSRC(unsigned int ssrc)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SetLocalSSRC()");
    if (channel_state_.Get().sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_ALREADY_SENDING, kTraceError,
            "SetLocalSSRC() already sending");
        return -1;
    }
    _rtpRtcpModule->SetSSRC(ssrc);
    return 0;
}

int
Channel::GetLocalSSRC(unsigned int& ssrc)
{
    ssrc = _rtpRtcpModule->SSRC();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetLocalSSRC() => ssrc=%lu", ssrc);
    return 0;
}

int
Channel::GetRemoteSSRC(unsigned int& ssrc)
{
    ssrc = rtp_receiver_->SSRC();
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetRemoteSSRC() => ssrc=%lu", ssrc);
    return 0;
}

int Channel::SetSendAudioLevelIndicationStatus(bool enable, unsigned char id) {
  _includeAudioLevelIndication = enable;
  return SetSendRtpHeaderExtension(enable, kRtpExtensionAudioLevel, id);
}

int Channel::SetReceiveAudioLevelIndicationStatus(bool enable,
                                                  unsigned char id) {
  rtp_header_parser_->DeregisterRtpHeaderExtension(
      kRtpExtensionAudioLevel);
  if (enable && !rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAudioLevel, id)) {
    return -1;
  }
  return 0;
}

int Channel::SetSendAbsoluteSenderTimeStatus(bool enable, unsigned char id) {
  return SetSendRtpHeaderExtension(enable, kRtpExtensionAbsoluteSendTime, id);
}

int Channel::SetReceiveAbsoluteSenderTimeStatus(bool enable, unsigned char id) {
  rtp_header_parser_->DeregisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime);
  if (enable && !rtp_header_parser_->RegisterRtpHeaderExtension(
      kRtpExtensionAbsoluteSendTime, id)) {
    return -1;
  }
  return 0;
}

int
Channel::SetRTCPStatus(bool enable)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetRTCPStatus()");
    if (_rtpRtcpModule->SetRTCPStatus(enable ?
        kRtcpCompound : kRtcpOff) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetRTCPStatus() failed to set RTCP status");
        return -1;
    }
    return 0;
}

int
Channel::GetRTCPStatus(bool& enabled)
{
    RTCPMethod method = _rtpRtcpModule->RTCP();
    enabled = (method != kRtcpOff);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId,_channelId),
                 "GetRTCPStatus() => enabled=%d", enabled);
    return 0;
}

int
Channel::SetRTCP_CNAME(const char cName[256])
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SetRTCP_CNAME()");
    if (_rtpRtcpModule->SetCNAME(cName) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetRTCP_CNAME() failed to set RTCP CNAME");
        return -1;
    }
    return 0;
}

int
Channel::GetRemoteRTCP_CNAME(char cName[256])
{
    if (cName == NULL)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "GetRemoteRTCP_CNAME() invalid CNAME input buffer");
        return -1;
    }
    char cname[RTCP_CNAME_SIZE];
    const uint32_t remoteSSRC = rtp_receiver_->SSRC();
    if (_rtpRtcpModule->RemoteCNAME(remoteSSRC, cname) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_CANNOT_RETRIEVE_CNAME, kTraceError,
            "GetRemoteRTCP_CNAME() failed to retrieve remote RTCP CNAME");
        return -1;
    }
    strcpy(cName, cname);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRemoteRTCP_CNAME() => cName=%s", cName);
    return 0;
}

int
Channel::GetRemoteRTCPReceiverInfo(
    uint32_t& NTPHigh,
    uint32_t& NTPLow,
    uint32_t& receivedPacketCount,
    uint64_t& receivedOctetCount,
    uint32_t& jitter,
    uint16_t& fractionLost,
    uint32_t& cumulativeLost,
    int32_t& rttMs)
{
  // Get all RTCP receiver report blocks that have been received on this
  // channel. If we receive RTP packets from a remote source we know the
  // remote SSRC and use the report block from him.
  // Otherwise use the first report block.
  std::vector<RTCPReportBlock> remote_stats;
  if (_rtpRtcpModule->RemoteRTCPStat(&remote_stats) != 0 ||
      remote_stats.empty()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                  VoEId(_instanceId, _channelId),
                  "GetRemoteRTCPReceiverInfo() failed to measure statistics due"
                  " to lack of received RTP and/or RTCP packets");
    return -1;
  }

  uint32_t remoteSSRC = rtp_receiver_->SSRC();
  std::vector<RTCPReportBlock>::const_iterator it = remote_stats.begin();
  for (; it != remote_stats.end(); ++it) {
    if (it->remoteSSRC == remoteSSRC)
      break;
  }

  if (it == remote_stats.end()) {
    // If we have not received any RTCP packets from this SSRC it probably
    // means that we have not received any RTP packets.
    // Use the first received report block instead.
    it = remote_stats.begin();
    remoteSSRC = it->remoteSSRC;
  }

  if (_rtpRtcpModule->GetReportBlockInfo(remoteSSRC,
                                         &NTPHigh,
                                         &NTPLow,
                                         &receivedPacketCount,
                                         &receivedOctetCount) != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRemoteRTCPReceiverInfo() failed to retrieve RTT from "
                 "the RTP/RTCP module");
    NTPHigh = 0;
    NTPLow = 0;
    receivedPacketCount = 0;
    receivedOctetCount = 0;
  }

  jitter = it->jitter;
  fractionLost = it->fractionLost;
  cumulativeLost = it->cumulativeLost;
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId, _channelId),
               "GetRemoteRTCPReceiverInfo() => jitter = %lu, "
               "fractionLost = %lu, cumulativeLost = %lu",
               jitter, fractionLost, cumulativeLost);

  uint16_t dummy;
  uint16_t rtt = 0;
  if (_rtpRtcpModule->RTT(remoteSSRC, &rtt, &dummy, &dummy, &dummy)
      != 0)
  {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() failed to retrieve RTT from "
                 "the RTP/RTCP module");
  }
  rttMs = rtt;
  return 0;
}

int
Channel::SendApplicationDefinedRTCPPacket(unsigned char subType,
                                             unsigned int name,
                                             const char* data,
                                             unsigned short dataLengthInBytes)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::SendApplicationDefinedRTCPPacket()");
    if (!channel_state_.Get().sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_NOT_SENDING, kTraceError,
            "SendApplicationDefinedRTCPPacket() not sending");
        return -1;
    }
    if (NULL == data)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SendApplicationDefinedRTCPPacket() invalid data value");
        return -1;
    }
    if (dataLengthInBytes % 4 != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SendApplicationDefinedRTCPPacket() invalid length value");
        return -1;
    }
    RTCPMethod status = _rtpRtcpModule->RTCP();
    if (status == kRtcpOff)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTCP_ERROR, kTraceError,
            "SendApplicationDefinedRTCPPacket() RTCP is disabled");
        return -1;
    }

    // Create and schedule the RTCP APP packet for transmission
    if (_rtpRtcpModule->SetRTCPApplicationSpecificData(
        subType,
        name,
        (const unsigned char*) data,
        dataLengthInBytes) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SEND_ERROR, kTraceError,
            "SendApplicationDefinedRTCPPacket() failed to send RTCP packet");
        return -1;
    }
    return 0;
}

int
Channel::GetRTPStatistics(
        unsigned int& averageJitterMs,
        unsigned int& maxJitterMs,
        unsigned int& discardedPackets,
        unsigned int& cumulativeLost)
{
    // The jitter statistics is updated for each received RTP packet and is
    // based on received packets.
    if (_rtpRtcpModule->RTCP() == kRtcpOff) {
      // If RTCP is off, there is no timed thread in the RTCP module regularly
      // generating new stats, trigger the update manually here instead.
      StreamStatistician* statistician =
          rtp_receive_statistics_->GetStatistician(rtp_receiver_->SSRC());
      if (statistician) {
        // Don't use returned statistics, use data from proxy instead so that
        // max jitter can be fetched atomically.
        RtcpStatistics s;
        statistician->GetStatistics(&s, true);
      }
    }

    ChannelStatistics stats = statistics_proxy_->GetStats();
    const int32_t playoutFrequency = audio_coding_->PlayoutFrequency();
    if (playoutFrequency > 0) {
      // Scale RTP statistics given the current playout frequency
      maxJitterMs = stats.max_jitter / (playoutFrequency / 1000);
      averageJitterMs = stats.rtcp.jitter / (playoutFrequency / 1000);
      cumulativeLost = stats.cumulative_lost;
    }

    discardedPackets = _numberOfDiscardedPackets;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId, _channelId),
               "GetRTPStatistics() => averageJitterMs = %lu, maxJitterMs = %lu,"
               " discardedPackets = %lu)",
               averageJitterMs, maxJitterMs, discardedPackets);
    return 0;
}

int Channel::GetRemoteRTCPReportBlocks(
    std::vector<ReportBlock>* report_blocks) {
  if (report_blocks == NULL) {
    _engineStatisticsPtr->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
      "GetRemoteRTCPReportBlock()s invalid report_blocks.");
    return -1;
  }

  // Get the report blocks from the latest received RTCP Sender or Receiver
  // Report. Each element in the vector contains the sender's SSRC and a
  // report block according to RFC 3550.
  std::vector<RTCPReportBlock> rtcp_report_blocks;
  if (_rtpRtcpModule->RemoteRTCPStat(&rtcp_report_blocks) != 0) {
    _engineStatisticsPtr->SetLastError(VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "GetRemoteRTCPReportBlocks() failed to read RTCP SR/RR report block.");
    return -1;
  }

  if (rtcp_report_blocks.empty())
    return 0;

  std::vector<RTCPReportBlock>::const_iterator it = rtcp_report_blocks.begin();
  for (; it != rtcp_report_blocks.end(); ++it) {
    ReportBlock report_block;
    report_block.sender_SSRC = it->remoteSSRC;
    report_block.source_SSRC = it->sourceSSRC;
    report_block.fraction_lost = it->fractionLost;
    report_block.cumulative_num_packets_lost = it->cumulativeLost;
    report_block.extended_highest_sequence_number = it->extendedHighSeqNum;
    report_block.interarrival_jitter = it->jitter;
    report_block.last_SR_timestamp = it->lastSR;
    report_block.delay_since_last_SR = it->delaySinceLastSR;
    report_blocks->push_back(report_block);
  }
  return 0;
}

int
Channel::GetRTPStatistics(CallStatistics& stats)
{
    // --- RtcpStatistics

    // The jitter statistics is updated for each received RTP packet and is
    // based on received packets.
    RtcpStatistics statistics;
    StreamStatistician* statistician =
        rtp_receive_statistics_->GetStatistician(rtp_receiver_->SSRC());
    if (!statistician || !statistician->GetStatistics(
        &statistics, _rtpRtcpModule->RTCP() == kRtcpOff)) {
      _engineStatisticsPtr->SetLastError(
          VE_CANNOT_RETRIEVE_RTP_STAT, kTraceWarning,
          "GetRTPStatistics() failed to read RTP statistics from the "
          "RTP/RTCP module");
    }

    stats.fractionLost = statistics.fraction_lost;
    stats.cumulativeLost = statistics.cumulative_lost;
    stats.extendedMax = statistics.extended_max_sequence_number;
    stats.jitterSamples = statistics.jitter;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => fractionLost=%lu, cumulativeLost=%lu,"
                 " extendedMax=%lu, jitterSamples=%li)",
                 stats.fractionLost, stats.cumulativeLost, stats.extendedMax,
                 stats.jitterSamples);

    // --- RTT
    stats.rttMs = GetRTT();

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => rttMs=%d", stats.rttMs);

    // --- Data counters

    uint32_t bytesSent(0);
    uint32_t packetsSent(0);
    uint32_t bytesReceived(0);
    uint32_t packetsReceived(0);

    if (statistician) {
      statistician->GetDataCounters(&bytesReceived, &packetsReceived);
    }

    if (_rtpRtcpModule->DataCountersRTP(&bytesSent,
                                        &packetsSent) != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                     VoEId(_instanceId, _channelId),
                     "GetRTPStatistics() failed to retrieve RTP datacounters =>"
                     " output will not be complete");
    }

    stats.bytesSent = bytesSent;
    stats.packetsSent = packetsSent;
    stats.bytesReceived = bytesReceived;
    stats.packetsReceived = packetsReceived;

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() => bytesSent=%d, packetsSent=%d,"
                 " bytesReceived=%d, packetsReceived=%d)",
                 stats.bytesSent, stats.packetsSent, stats.bytesReceived,
                 stats.packetsReceived);

    // --- Timestamps
    {
      CriticalSectionScoped lock(ts_stats_lock_.get());
      stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
    }
    return 0;
}

int Channel::SetREDStatus(bool enable, int redPayloadtype) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetREDStatus()");

  if (enable) {
    if (redPayloadtype < 0 || redPayloadtype > 127) {
      _engineStatisticsPtr->SetLastError(
          VE_PLTYPE_ERROR, kTraceError,
          "SetREDStatus() invalid RED payload type");
      return -1;
    }

    if (SetRedPayloadType(redPayloadtype) < 0) {
      _engineStatisticsPtr->SetLastError(
          VE_CODEC_ERROR, kTraceError,
          "SetSecondarySendCodec() Failed to register RED ACM");
      return -1;
    }
  }

  if (audio_coding_->SetREDStatus(enable) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetREDStatus() failed to set RED state in the ACM");
    return -1;
  }
  return 0;
}

int
Channel::GetREDStatus(bool& enabled, int& redPayloadtype)
{
    enabled = audio_coding_->REDStatus();
    if (enabled)
    {
        int8_t payloadType(0);
        if (_rtpRtcpModule->SendREDPayloadType(payloadType) != 0)
        {
            _engineStatisticsPtr->SetLastError(
                VE_RTP_RTCP_MODULE_ERROR, kTraceError,
                "GetREDStatus() failed to retrieve RED PT from RTP/RTCP "
                "module");
            return -1;
        }
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                   VoEId(_instanceId, _channelId),
                   "GetREDStatus() => enabled=%d, redPayloadtype=%d",
                   enabled, redPayloadtype);
        return 0;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetREDStatus() => enabled=%d", enabled);
    return 0;
}

int Channel::SetCodecFECStatus(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
               "Channel::SetCodecFECStatus()");

  if (audio_coding_->SetCodecFEC(enable) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetCodecFECStatus() failed to set FEC state");
    return -1;
  }
  return 0;
}

bool Channel::GetCodecFECStatus() {
  bool enabled = audio_coding_->CodecFEC();
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId, _channelId),
               "GetCodecFECStatus() => enabled=%d", enabled);
  return enabled;
}

void Channel::SetNACKStatus(bool enable, int maxNumberOfPackets) {
  // None of these functions can fail.
  _rtpRtcpModule->SetStorePacketsStatus(enable, maxNumberOfPackets);
  rtp_receive_statistics_->SetMaxReorderingThreshold(maxNumberOfPackets);
  rtp_receiver_->SetNACKStatus(enable ? kNackRtcp : kNackOff);
  if (enable)
    audio_coding_->EnableNack(maxNumberOfPackets);
  else
    audio_coding_->DisableNack();
}

// Called when we are missing one or more packets.
int Channel::ResendPackets(const uint16_t* sequence_numbers, int length) {
  return _rtpRtcpModule->SendNACK(sequence_numbers, length);
}

int
Channel::StartRTPDump(const char fileNameUTF8[1024],
                      RTPDirections direction)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::StartRTPDump()");
    if ((direction != kRtpIncoming) && (direction != kRtpOutgoing))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StartRTPDump() invalid RTP direction");
        return -1;
    }
    RtpDump* rtpDumpPtr = (direction == kRtpIncoming) ?
        &_rtpDumpIn : &_rtpDumpOut;
    if (rtpDumpPtr == NULL)
    {
        assert(false);
        return -1;
    }
    if (rtpDumpPtr->IsActive())
    {
        rtpDumpPtr->Stop();
    }
    if (rtpDumpPtr->Start(fileNameUTF8) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_BAD_FILE, kTraceError,
            "StartRTPDump() failed to create file");
        return -1;
    }
    return 0;
}

int
Channel::StopRTPDump(RTPDirections direction)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, _channelId),
                 "Channel::StopRTPDump()");
    if ((direction != kRtpIncoming) && (direction != kRtpOutgoing))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "StopRTPDump() invalid RTP direction");
        return -1;
    }
    RtpDump* rtpDumpPtr = (direction == kRtpIncoming) ?
        &_rtpDumpIn : &_rtpDumpOut;
    if (rtpDumpPtr == NULL)
    {
        assert(false);
        return -1;
    }
    if (!rtpDumpPtr->IsActive())
    {
        return 0;
    }
    return rtpDumpPtr->Stop();
}

bool
Channel::RTPDumpIsActive(RTPDirections direction)
{
    if ((direction != kRtpIncoming) &&
        (direction != kRtpOutgoing))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "RTPDumpIsActive() invalid RTP direction");
        return false;
    }
    RtpDump* rtpDumpPtr = (direction == kRtpIncoming) ?
        &_rtpDumpIn : &_rtpDumpOut;
    return rtpDumpPtr->IsActive();
}

void Channel::SetVideoEngineBWETarget(ViENetwork* vie_network,
                                      int video_channel) {
  CriticalSectionScoped cs(&_callbackCritSect);
  if (vie_network_) {
    vie_network_->Release();
    vie_network_ = NULL;
  }
  video_channel_ = -1;

  if (vie_network != NULL && video_channel != -1) {
    vie_network_ = vie_network;
    video_channel_ = video_channel;
  }
}

uint32_t
Channel::Demultiplex(const AudioFrame& audioFrame)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::Demultiplex()");
    _audioFrame.CopyFrom(audioFrame);
    _audioFrame.id_ = _channelId;
    return 0;
}

void Channel::Demultiplex(const int16_t* audio_data,
                          int sample_rate,
                          int number_of_frames,
                          int number_of_channels) {
  CodecInst codec;
  GetSendCodec(codec);

  if (!mono_recording_audio_.get()) {
    // Temporary space for DownConvertToCodecFormat.
    mono_recording_audio_.reset(new int16_t[kMaxMonoDataSizeSamples]);
  }
  DownConvertToCodecFormat(audio_data,
                           number_of_frames,
                           number_of_channels,
                           sample_rate,
                           codec.channels,
                           codec.plfreq,
                           mono_recording_audio_.get(),
                           &input_resampler_,
                           &_audioFrame);
}

uint32_t
Channel::PrepareEncodeAndSend(int mixingFrequency)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::PrepareEncodeAndSend()");

    if (_audioFrame.samples_per_channel_ == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::PrepareEncodeAndSend() invalid audio frame");
        return 0xFFFFFFFF;
    }

    if (channel_state_.Get().input_file_playing)
    {
        MixOrReplaceAudioWithFile(mixingFrequency);
    }

    bool is_muted = Mute();  // Cache locally as Mute() takes a lock.
    if (is_muted) {
      AudioFrameOperations::Mute(_audioFrame);
    }

    if (channel_state_.Get().input_external_media)
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        const bool isStereo = (_audioFrame.num_channels_ == 2);
        if (_inputExternalMediaCallbackPtr)
        {
            _inputExternalMediaCallbackPtr->Process(
                _channelId,
                kRecordingPerChannel,
               (int16_t*)_audioFrame.data_,
                _audioFrame.samples_per_channel_,
                _audioFrame.sample_rate_hz_,
                isStereo);
        }
    }

    InsertInbandDtmfTone();

    if (_includeAudioLevelIndication) {
      int length = _audioFrame.samples_per_channel_ * _audioFrame.num_channels_;
      if (is_muted) {
        rms_level_.ProcessMuted(length);
      } else {
        rms_level_.Process(_audioFrame.data_, length);
      }
    }

    return 0;
}

uint32_t
Channel::EncodeAndSend()
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::EncodeAndSend()");

    assert(_audioFrame.num_channels_ <= 2);
    if (_audioFrame.samples_per_channel_ == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::EncodeAndSend() invalid audio frame");
        return 0xFFFFFFFF;
    }

    _audioFrame.id_ = _channelId;

    // --- Add 10ms of raw (PCM) audio data to the encoder @ 32kHz.

    // The ACM resamples internally.
    _audioFrame.timestamp_ = _timeStamp;
    if (audio_coding_->Add10MsData((AudioFrame&)_audioFrame) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,_channelId),
                     "Channel::EncodeAndSend() ACM encoding failed");
        return 0xFFFFFFFF;
    }

    _timeStamp += _audioFrame.samples_per_channel_;

    // --- Encode if complete frame is ready

    // This call will trigger AudioPacketizationCallback::SendData if encoding
    // is done and payload is ready for packetization and transmission.
    return audio_coding_->Process();
}

int Channel::RegisterExternalMediaProcessing(
    ProcessingTypes type,
    VoEMediaProcess& processObject)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (kPlaybackPerChannel == type)
    {
        if (_outputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceError,
                "Channel::RegisterExternalMediaProcessing() "
                "output external media already enabled");
            return -1;
        }
        _outputExternalMediaCallbackPtr = &processObject;
        _outputExternalMedia = true;
    }
    else if (kRecordingPerChannel == type)
    {
        if (_inputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceError,
                "Channel::RegisterExternalMediaProcessing() "
                "output external media already enabled");
            return -1;
        }
        _inputExternalMediaCallbackPtr = &processObject;
        channel_state_.SetInputExternalMedia(true);
    }
    return 0;
}

int Channel::DeRegisterExternalMediaProcessing(ProcessingTypes type)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::DeRegisterExternalMediaProcessing()");

    CriticalSectionScoped cs(&_callbackCritSect);

    if (kPlaybackPerChannel == type)
    {
        if (!_outputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceWarning,
                "Channel::DeRegisterExternalMediaProcessing() "
                "output external media already disabled");
            return 0;
        }
        _outputExternalMedia = false;
        _outputExternalMediaCallbackPtr = NULL;
    }
    else if (kRecordingPerChannel == type)
    {
        if (!_inputExternalMediaCallbackPtr)
        {
            _engineStatisticsPtr->SetLastError(
                VE_INVALID_OPERATION, kTraceWarning,
                "Channel::DeRegisterExternalMediaProcessing() "
                "input external media already disabled");
            return 0;
        }
        channel_state_.SetInputExternalMedia(false);
        _inputExternalMediaCallbackPtr = NULL;
    }

    return 0;
}

int Channel::SetExternalMixing(bool enabled) {
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetExternalMixing(enabled=%d)", enabled);

    if (channel_state_.Get().playing)
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_OPERATION, kTraceError,
            "Channel::SetExternalMixing() "
            "external mixing cannot be changed while playing.");
        return -1;
    }

    _externalMixing = enabled;

    return 0;
}

int
Channel::GetNetworkStatistics(NetworkStatistics& stats)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetNetworkStatistics()");
    ACMNetworkStatistics acm_stats;
    int return_value = audio_coding_->NetworkStatistics(&acm_stats);
    if (return_value >= 0) {
      memcpy(&stats, &acm_stats, sizeof(NetworkStatistics));
    }
    return return_value;
}

void Channel::GetDecodingCallStatistics(AudioDecodingCallStats* stats) const {
  audio_coding_->GetDecodingCallStatistics(stats);
}

bool Channel::GetDelayEstimate(int* jitter_buffer_delay_ms,
                               int* playout_buffer_delay_ms,
                               int* avsync_offset_ms) const {
  if (_average_jitter_buffer_delay_us == 0) {
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetDelayEstimate() no valid estimate.");
    return false;
  }
  *jitter_buffer_delay_ms = (_average_jitter_buffer_delay_us + 500) / 1000 +
      _recPacketDelayMs;
  *playout_buffer_delay_ms = playout_delay_ms_;
  *avsync_offset_ms = _current_sync_offset;
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::GetDelayEstimate()");
  return true;
}

int Channel::SetInitialPlayoutDelay(int delay_ms)
{
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetInitialPlayoutDelay()");
  if ((delay_ms < kVoiceEngineMinMinPlayoutDelayMs) ||
      (delay_ms > kVoiceEngineMaxMinPlayoutDelayMs))
  {
    _engineStatisticsPtr->SetLastError(
        VE_INVALID_ARGUMENT, kTraceError,
        "SetInitialPlayoutDelay() invalid min delay");
    return -1;
  }
  if (audio_coding_->SetInitialPlayoutDelay(delay_ms) != 0)
  {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetInitialPlayoutDelay() failed to set min playout delay");
    return -1;
  }
  return 0;
}


int
Channel::SetMinimumPlayoutDelay(int delayMs)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetMinimumPlayoutDelay()");
    if ((delayMs < kVoiceEngineMinMinPlayoutDelayMs) ||
        (delayMs > kVoiceEngineMaxMinPlayoutDelayMs))
    {
        _engineStatisticsPtr->SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetMinimumPlayoutDelay() invalid min delay");
        return -1;
    }
    if (audio_coding_->SetMinimumPlayoutDelay(delayMs) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
            "SetMinimumPlayoutDelay() failed to set min playout delay");
        return -1;
    }
    return 0;
}

void Channel::UpdatePlayoutTimestamp(bool rtcp) {
  uint32_t playout_timestamp = 0;

  if (audio_coding_->PlayoutTimestamp(&playout_timestamp) == -1)  {
    // This can happen if this channel has not been received any RTP packet. In
    // this case, NetEq is not capable of computing playout timestamp.
    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::UpdatePlayoutTimestamp() failed to read playout"
                 " delay from the ADM");
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceError,
        "UpdatePlayoutTimestamp() failed to retrieve playout delay");
    return;
  }

  jitter_buffer_playout_timestamp_ = playout_timestamp;

  // Remove the playout delay.
  playout_timestamp -= (delay_ms * (GetPlayoutFrequency() / 1000));

  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::UpdatePlayoutTimestamp() => playoutTimestamp = %lu",
               playout_timestamp);

  if (rtcp) {
    playout_timestamp_rtcp_ = playout_timestamp;
  } else {
    playout_timestamp_rtp_ = playout_timestamp;
  }
  playout_delay_ms_ = delay_ms;
}

int Channel::GetPlayoutTimestamp(unsigned int& timestamp) {
  WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::GetPlayoutTimestamp()");
  if (playout_timestamp_rtp_ == 0)  {
    _engineStatisticsPtr->SetLastError(
        VE_CANNOT_RETRIEVE_VALUE, kTraceError,
        "GetPlayoutTimestamp() failed to retrieve timestamp");
    return -1;
  }
  timestamp = playout_timestamp_rtp_;
  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
               VoEId(_instanceId,_channelId),
               "GetPlayoutTimestamp() => timestamp=%u", timestamp);
  return 0;
}

int
Channel::SetInitTimestamp(unsigned int timestamp)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::SetInitTimestamp()");
    if (channel_state_.Get().sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SENDING, kTraceError, "SetInitTimestamp() already sending");
        return -1;
    }
    if (_rtpRtcpModule->SetStartTimestamp(timestamp) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetInitTimestamp() failed to set timestamp");
        return -1;
    }
    return 0;
}

int
Channel::SetInitSequenceNumber(short sequenceNumber)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::SetInitSequenceNumber()");
    if (channel_state_.Get().sending)
    {
        _engineStatisticsPtr->SetLastError(
            VE_SENDING, kTraceError,
            "SetInitSequenceNumber() already sending");
        return -1;
    }
    if (_rtpRtcpModule->SetSequenceNumber(sequenceNumber) != 0)
    {
        _engineStatisticsPtr->SetLastError(
            VE_RTP_RTCP_MODULE_ERROR, kTraceError,
            "SetInitSequenceNumber() failed to set sequence number");
        return -1;
    }
    return 0;
}

int
Channel::GetRtpRtcp(RtpRtcp** rtpRtcpModule, RtpReceiver** rtp_receiver) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::GetRtpRtcp()");
    *rtpRtcpModule = _rtpRtcpModule.get();
    *rtp_receiver = rtp_receiver_.get();
    return 0;
}

// TODO(andrew): refactor Mix functions here and in transmit_mixer.cc to use
// a shared helper.
int32_t
Channel::MixOrReplaceAudioWithFile(int mixingFrequency)
{
    scoped_ptr<int16_t[]> fileBuffer(new int16_t[640]);
    int fileSamples(0);

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_inputFilePlayerPtr == NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixOrReplaceAudioWithFile() fileplayer"
                             " doesnt exist");
            return -1;
        }

        if (_inputFilePlayerPtr->Get10msAudioFromFile(fileBuffer.get(),
                                                      fileSamples,
                                                      mixingFrequency) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixOrReplaceAudioWithFile() file mixing "
                         "failed");
            return -1;
        }
        if (fileSamples == 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixOrReplaceAudioWithFile() file is ended");
            return 0;
        }
    }

    assert(_audioFrame.samples_per_channel_ == fileSamples);

    if (_mixFileWithMicrophone)
    {
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        MixWithSat(_audioFrame.data_,
                   _audioFrame.num_channels_,
                   fileBuffer.get(),
                   1,
                   fileSamples);
    }
    else
    {
        // Replace ACM audio with file.
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        _audioFrame.UpdateFrame(_channelId,
                                0xFFFFFFFF,
                                fileBuffer.get(),
                                fileSamples,
                                mixingFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadUnknown,
                                1);

    }
    return 0;
}

int32_t
Channel::MixAudioWithFile(AudioFrame& audioFrame,
                          int mixingFrequency)
{
    assert(mixingFrequency <= 48000);

    scoped_ptr<int16_t[]> fileBuffer(new int16_t[960]);
    int fileSamples(0);

    {
        CriticalSectionScoped cs(&_fileCritSect);

        if (_outputFilePlayerPtr == NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixAudioWithFile() file mixing failed");
            return -1;
        }

        // We should get the frequency we ask for.
        if (_outputFilePlayerPtr->Get10msAudioFromFile(fileBuffer.get(),
                                                       fileSamples,
                                                       mixingFrequency) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::MixAudioWithFile() file mixing failed");
            return -1;
        }
    }

    if (audioFrame.samples_per_channel_ == fileSamples)
    {
        // Currently file stream is always mono.
        // TODO(xians): Change the code when FilePlayer supports real stereo.
        MixWithSat(audioFrame.data_,
                   audioFrame.num_channels_,
                   fileBuffer.get(),
                   1,
                   fileSamples);
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId,_channelId),
            "Channel::MixAudioWithFile() samples_per_channel_(%d) != "
            "fileSamples(%d)",
            audioFrame.samples_per_channel_, fileSamples);
        return -1;
    }

    return 0;
}

int
Channel::InsertInbandDtmfTone()
{
    // Check if we should start a new tone.
    if (_inbandDtmfQueue.PendingDtmf() &&
        !_inbandDtmfGenerator.IsAddingTone() &&
        _inbandDtmfGenerator.DelaySinceLastTone() >
        kMinTelephoneEventSeparationMs)
    {
        int8_t eventCode(0);
        uint16_t lengthMs(0);
        uint8_t attenuationDb(0);

        eventCode = _inbandDtmfQueue.NextDtmf(&lengthMs, &attenuationDb);
        _inbandDtmfGenerator.AddTone(eventCode, lengthMs, attenuationDb);
        if (_playInbandDtmfEvent)
        {
            // Add tone to output mixer using a reduced length to minimize
            // risk of echo.
            _outputMixerPtr->PlayDtmfTone(eventCode, lengthMs - 80,
                                          attenuationDb);
        }
    }

    if (_inbandDtmfGenerator.IsAddingTone())
    {
        uint16_t frequency(0);
        _inbandDtmfGenerator.GetSampleRate(frequency);

        if (frequency != _audioFrame.sample_rate_hz_)
        {
            // Update sample rate of Dtmf tone since the mixing frequency
            // has changed.
            _inbandDtmfGenerator.SetSampleRate(
                (uint16_t) (_audioFrame.sample_rate_hz_));
            // Reset the tone to be added taking the new sample rate into
            // account.
            _inbandDtmfGenerator.ResetTone();
        }

        int16_t toneBuffer[320];
        uint16_t toneSamples(0);
        // Get 10ms tone segment and set time since last tone to zero
        if (_inbandDtmfGenerator.Get10msTone(toneBuffer, toneSamples) == -1)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                       VoEId(_instanceId, _channelId),
                       "Channel::EncodeAndSend() inserting Dtmf failed");
            return -1;
        }

        // Replace mixed audio with DTMF tone.
        for (int sample = 0;
            sample < _audioFrame.samples_per_channel_;
            sample++)
        {
            for (int channel = 0;
                channel < _audioFrame.num_channels_;
                channel++)
            {
                const int index = sample * _audioFrame.num_channels_ + channel;
                _audioFrame.data_[index] = toneBuffer[sample];
            }
        }

        assert(_audioFrame.samples_per_channel_ == toneSamples);
    } else
    {
        // Add 10ms to "delay-since-last-tone" counter
        _inbandDtmfGenerator.UpdateDelaySinceLastTone();
    }
    return 0;
}

int32_t
Channel::SendPacketRaw(const void *data, int len, bool RTCP)
{
    CriticalSectionScoped cs(&_callbackCritSect);
    if (_transportPtr == NULL)
    {
        return -1;
    }
    if (!RTCP)
    {
        return _transportPtr->SendPacket(_channelId, data, len);
    }
    else
    {
        return _transportPtr->SendRTCPPacket(_channelId, data, len);
    }
}

// Called for incoming RTP packets after successful RTP header parsing.
void Channel::UpdatePacketDelay(uint32_t rtp_timestamp,
                                uint16_t sequence_number) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_instanceId,_channelId),
               "Channel::UpdatePacketDelay(timestamp=%lu, sequenceNumber=%u)",
               rtp_timestamp, sequence_number);

  // Get frequency of last received payload
  int rtp_receive_frequency = GetPlayoutFrequency();

  // Update the least required delay.
  least_required_delay_ms_ = audio_coding_->LeastRequiredDelayMs();

  // |jitter_buffer_playout_timestamp_| updated in UpdatePlayoutTimestamp for
  // every incoming packet.
  uint32_t timestamp_diff_ms = (rtp_timestamp -
      jitter_buffer_playout_timestamp_) / (rtp_receive_frequency / 1000);
  if (!IsNewerTimestamp(rtp_timestamp, jitter_buffer_playout_timestamp_) ||
      timestamp_diff_ms > (2 * kVoiceEngineMaxMinPlayoutDelayMs)) {
    // If |jitter_buffer_playout_timestamp_| is newer than the incoming RTP
    // timestamp, the resulting difference is negative, but is set to zero.
    // This can happen when a network glitch causes a packet to arrive late,
    // and during long comfort noise periods with clock drift.
    timestamp_diff_ms = 0;
  }

  uint16_t packet_delay_ms = (rtp_timestamp - _previousTimestamp) /
      (rtp_receive_frequency / 1000);

  _previousTimestamp = rtp_timestamp;

  if (timestamp_diff_ms == 0) return;

  if (packet_delay_ms >= 10 && packet_delay_ms <= 60) {
    _recPacketDelayMs = packet_delay_ms;
  }

  if (_average_jitter_buffer_delay_us == 0) {
    _average_jitter_buffer_delay_us = timestamp_diff_ms * 1000;
    return;
  }

  // Filter average delay value using exponential filter (alpha is
  // 7/8). We derive 1000 *_average_jitter_buffer_delay_us here (reduces
  // risk of rounding error) and compensate for it in GetDelayEstimate()
  // later.
  _average_jitter_buffer_delay_us = (_average_jitter_buffer_delay_us * 7 +
      1000 * timestamp_diff_ms + 500) / 8;
}

void
Channel::RegisterReceiveCodecsToRTPModule()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId,_channelId),
                 "Channel::RegisterReceiveCodecsToRTPModule()");


    CodecInst codec;
    const uint8_t nSupportedCodecs = AudioCodingModule::NumberOfCodecs();

    for (int idx = 0; idx < nSupportedCodecs; idx++)
    {
        // Open up the RTP/RTCP receiver for all supported codecs
        if ((audio_coding_->Codec(idx, &codec) == -1) ||
            (rtp_receiver_->RegisterReceivePayload(
                codec.plname,
                codec.pltype,
                codec.plfreq,
                codec.channels,
                (codec.rate < 0) ? 0 : codec.rate) == -1))
        {
            WEBRTC_TRACE(
                         kTraceWarning,
                         kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::RegisterReceiveCodecsToRTPModule() unable"
                         " to register %s (%d/%d/%d/%d) to RTP/RTCP receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }
        else
        {
            WEBRTC_TRACE(
                         kTraceInfo,
                         kTraceVoice,
                         VoEId(_instanceId, _channelId),
                         "Channel::RegisterReceiveCodecsToRTPModule() %s "
                         "(%d/%d/%d/%d) has been added to the RTP/RTCP "
                         "receiver",
                         codec.plname, codec.pltype, codec.plfreq,
                         codec.channels, codec.rate);
        }
    }
}

int Channel::SetSecondarySendCodec(const CodecInst& codec,
                                   int red_payload_type) {
  // Sanity check for payload type.
  if (red_payload_type < 0 || red_payload_type > 127) {
    _engineStatisticsPtr->SetLastError(
        VE_PLTYPE_ERROR, kTraceError,
        "SetRedPayloadType() invalid RED payload type");
    return -1;
  }

  if (SetRedPayloadType(red_payload_type) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register RED ACM");
    return -1;
  }
  if (audio_coding_->RegisterSecondarySendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetSecondarySendCodec() Failed to register secondary send codec in "
        "ACM");
    return -1;
  }

  return 0;
}

void Channel::RemoveSecondarySendCodec() {
  audio_coding_->UnregisterSecondarySendCodec();
}

int Channel::GetSecondarySendCodec(CodecInst* codec) {
  if (audio_coding_->SecondarySendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "GetSecondarySendCodec() Failed to get secondary sent codec from ACM");
    return -1;
  }
  return 0;
}

// Assuming this method is called with valid payload type.
int Channel::SetRedPayloadType(int red_payload_type) {
  CodecInst codec;
  bool found_red = false;

  // Get default RED settings from the ACM database
  const int num_codecs = AudioCodingModule::NumberOfCodecs();
  for (int idx = 0; idx < num_codecs; idx++) {
    audio_coding_->Codec(idx, &codec);
    if (!STR_CASE_CMP(codec.plname, "RED")) {
      found_red = true;
      break;
    }
  }

  if (!found_red) {
    _engineStatisticsPtr->SetLastError(
        VE_CODEC_ERROR, kTraceError,
        "SetRedPayloadType() RED is not supported");
    return -1;
  }

  codec.pltype = red_payload_type;
  if (audio_coding_->RegisterSendCodec(codec) < 0) {
    _engineStatisticsPtr->SetLastError(
        VE_AUDIO_CODING_MODULE_ERROR, kTraceError,
        "SetRedPayloadType() RED registration in ACM module failed");
    return -1;
  }

  if (_rtpRtcpModule->SetSendREDPayloadType(red_payload_type) != 0) {
    _engineStatisticsPtr->SetLastError(
        VE_RTP_RTCP_MODULE_ERROR, kTraceError,
        "SetRedPayloadType() RED registration in RTP/RTCP module failed");
    return -1;
  }
  return 0;
}

int Channel::SetSendRtpHeaderExtension(bool enable, RTPExtensionType type,
                                       unsigned char id) {
  int error = 0;
  _rtpRtcpModule->DeregisterSendRtpHeaderExtension(type);
  if (enable) {
    error = _rtpRtcpModule->RegisterSendRtpHeaderExtension(type, id);
  }
  return error;
}

int32_t Channel::GetPlayoutFrequency() {
  int32_t playout_frequency = audio_coding_->PlayoutFrequency();
  CodecInst current_recive_codec;
  if (audio_coding_->ReceiveCodec(&current_recive_codec) == 0) {
    if (STR_CASE_CMP("G722", current_recive_codec.plname) == 0) {
      // Even though the actual sampling rate for G.722 audio is
      // 16,000 Hz, the RTP clock rate for the G722 payload format is
      // 8,000 Hz because that value was erroneously assigned in
      // RFC 1890 and must remain unchanged for backward compatibility.
      playout_frequency = 8000;
    } else if (STR_CASE_CMP("opus", current_recive_codec.plname) == 0) {
      // We are resampling Opus internally to 32,000 Hz until all our
      // DSP routines can operate at 48,000 Hz, but the RTP clock
      // rate for the Opus payload format is standardized to 48,000 Hz,
      // because that is the maximum supported decoding sampling rate.
      playout_frequency = 48000;
    }
  }
  return playout_frequency;
}

int Channel::GetRTT() const {
  RTCPMethod method = _rtpRtcpModule->RTCP();
  if (method == kRtcpOff) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() RTCP is disabled => valid RTT "
                 "measurements cannot be retrieved");
    return 0;
  }
  std::vector<RTCPReportBlock> report_blocks;
  _rtpRtcpModule->RemoteRTCPStat(&report_blocks);
  if (report_blocks.empty()) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() failed to measure RTT since no "
                 "RTCP packets have been received yet");
    return 0;
  }

  uint32_t remoteSSRC = rtp_receiver_->SSRC();
  std::vector<RTCPReportBlock>::const_iterator it = report_blocks.begin();
  for (; it != report_blocks.end(); ++it) {
    if (it->remoteSSRC == remoteSSRC)
      break;
  }
  if (it == report_blocks.end()) {
    // We have not received packets with SSRC matching the report blocks.
    // To calculate RTT we try with the SSRC of the first report block.
    // This is very important for send-only channels where we don't know
    // the SSRC of the other end.
    remoteSSRC = report_blocks[0].remoteSSRC;
  }
  uint16_t rtt = 0;
  uint16_t avg_rtt = 0;
  uint16_t max_rtt= 0;
  uint16_t min_rtt = 0;
  if (_rtpRtcpModule->RTT(remoteSSRC, &rtt, &avg_rtt, &min_rtt, &max_rtt)
      != 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVoice,
                 VoEId(_instanceId, _channelId),
                 "GetRTPStatistics() failed to retrieve RTT from "
                 "the RTP/RTCP module");
    return 0;
  }
  return static_cast<int>(rtt);
}

}  // namespace voe
}  // namespace webrtc
