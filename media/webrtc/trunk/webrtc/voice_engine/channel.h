/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CHANNEL_H_
#define WEBRTC_VOICE_ENGINE_CHANNEL_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer_defines.h"
#include "webrtc/modules/audio_processing/rms_level.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/rtp_rtcp/interface/remote_ntp_time_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/file_player.h"
#include "webrtc/modules/utility/interface/file_recorder.h"
#include "webrtc/voice_engine/dtmf_inband.h"
#include "webrtc/voice_engine/dtmf_inband_queue.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine/network_predictor.h"
#include "webrtc/voice_engine/shared_data.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

#ifdef WEBRTC_DTMF_DETECTION
// TelephoneEventDetectionMethods, TelephoneEventObserver
#include "webrtc/voice_engine/include/voe_dtmf.h"
#endif

namespace rtc {

class TimestampWrapAroundHandler;
}

namespace webrtc {

class AudioDeviceModule;
class Config;
class CriticalSectionWrapper;
class FileWrapper;
class ProcessThread;
class ReceiveStatistics;
class RemoteNtpTimeEstimator;
class RtpDump;
class RTPPayloadRegistry;
class RtpReceiver;
class RTPReceiverAudio;
class RtpRtcp;
class TelephoneEventHandler;
class ViENetwork;
class VoEMediaProcess;
class VoERTPObserver;
class VoiceEngineObserver;

struct CallStatistics;
struct ReportBlock;
struct SenderInfo;

namespace voe {

class OutputMixer;
class Statistics;
class StatisticsProxy;
class TransmitMixer;
class VoERtcpObserver;

// Helper class to simplify locking scheme for members that are accessed from
// multiple threads.
// Example: a member can be set on thread T1 and read by an internal audio
// thread T2. Accessing the member via this class ensures that we are
// safe and also avoid TSan v2 warnings.
class ChannelState {
 public:
    struct State {
        State() : rx_apm_is_enabled(false),
                  input_external_media(false),
                  output_file_playing(false),
                  input_file_playing(false),
                  playing(false),
                  sending(false),
                  receiving(false) {}

        bool rx_apm_is_enabled;
        bool input_external_media;
        bool output_file_playing;
        bool input_file_playing;
        bool playing;
        bool sending;
        bool receiving;
    };

    ChannelState() : lock_(CriticalSectionWrapper::CreateCriticalSection()) {
    }
    virtual ~ChannelState() {}

    void Reset() {
        CriticalSectionScoped lock(lock_.get());
        state_ = State();
    }

    State Get() const {
        CriticalSectionScoped lock(lock_.get());
        return state_;
    }

    void SetRxApmIsEnabled(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.rx_apm_is_enabled = enable;
    }

    void SetInputExternalMedia(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.input_external_media = enable;
    }

    void SetOutputFilePlaying(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.output_file_playing = enable;
    }

    void SetInputFilePlaying(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.input_file_playing = enable;
    }

    void SetPlaying(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.playing = enable;
    }

    void SetSending(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.sending = enable;
    }

    void SetReceiving(bool enable) {
        CriticalSectionScoped lock(lock_.get());
        state_.receiving = enable;
    }

private:
 rtc::scoped_ptr<CriticalSectionWrapper> lock_;
    State state_;
};

class Channel:
    public RtpData,
    public RtpFeedback,
    public FileCallback, // receiving notification from file player & recorder
    public Transport,
    public RtpAudioFeedback,
    public AudioPacketizationCallback, // receive encoded packets from the ACM
    public ACMVADCallback, // receive voice activity from the ACM
    public MixerParticipant // supplies output mixer with audio frames
{
public:
    friend class VoERtcpObserver;

    enum {KNumSocketThreads = 1};
    enum {KNumberOfSocketBuffers = 8};
    virtual ~Channel();
    static int32_t CreateChannel(Channel*& channel,
                                 int32_t channelId,
                                 uint32_t instanceId,
                                 const Config& config);
    Channel(int32_t channelId, uint32_t instanceId, const Config& config);
    int32_t Init();
    int32_t SetEngineInformation(
        Statistics& engineStatistics,
        OutputMixer& outputMixer,
        TransmitMixer& transmitMixer,
        ProcessThread& moduleProcessThread,
        AudioDeviceModule& audioDeviceModule,
        VoiceEngineObserver* voiceEngineObserver,
        CriticalSectionWrapper* callbackCritSect);
    int32_t UpdateLocalTimeStamp();

    // API methods

    // VoEBase
    int32_t StartPlayout();
    int32_t StopPlayout();
    int32_t StartSend();
    int32_t StopSend();
    int32_t StartReceiving();
    int32_t StopReceiving();

    int32_t RegisterVoiceEngineObserver(VoiceEngineObserver& observer);
    int32_t DeRegisterVoiceEngineObserver();

    // VoECodec
    int32_t GetSendCodec(CodecInst& codec);
    int32_t GetRecCodec(CodecInst& codec);
    int32_t SetSendCodec(const CodecInst& codec);
    int32_t SetVADStatus(bool enableVAD, ACMVADMode mode, bool disableDTX);
    int32_t GetVADStatus(bool& enabledVAD, ACMVADMode& mode, bool& disabledDTX);
    int32_t SetRecPayloadType(const CodecInst& codec);
    int32_t GetRecPayloadType(CodecInst& codec);
    int32_t SetSendCNPayloadType(int type, PayloadFrequencies frequency);
    int SetOpusMaxPlaybackRate(int frequency_hz);
    int SetOpusDtx(bool enable_dtx);

    // VoENetwork
    int32_t RegisterExternalTransport(Transport& transport);
    int32_t DeRegisterExternalTransport();
    int32_t ReceivedRTPPacket(const int8_t* data, size_t length,
                              const PacketTime& packet_time);
    int32_t ReceivedRTCPPacket(const int8_t* data, size_t length);

    // VoEFile
    int StartPlayingFileLocally(const char* fileName, bool loop,
                                FileFormats format,
                                int startPosition,
                                float volumeScaling,
                                int stopPosition,
                                const CodecInst* codecInst);
    int StartPlayingFileLocally(InStream* stream, FileFormats format,
                                int startPosition,
                                float volumeScaling,
                                int stopPosition,
                                const CodecInst* codecInst);
    int StopPlayingFileLocally();
    int IsPlayingFileLocally() const;
    int RegisterFilePlayingToMixer();
    int StartPlayingFileAsMicrophone(const char* fileName, bool loop,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst);
    int StartPlayingFileAsMicrophone(InStream* stream,
                                     FileFormats format,
                                     int startPosition,
                                     float volumeScaling,
                                     int stopPosition,
                                     const CodecInst* codecInst);
    int StopPlayingFileAsMicrophone();
    int IsPlayingFileAsMicrophone() const;
    int StartRecordingPlayout(const char* fileName, const CodecInst* codecInst);
    int StartRecordingPlayout(OutStream* stream, const CodecInst* codecInst);
    int StopRecordingPlayout();

    void SetMixWithMicStatus(bool mix);

    // VoEExternalMediaProcessing
    int RegisterExternalMediaProcessing(ProcessingTypes type,
                                        VoEMediaProcess& processObject);
    int DeRegisterExternalMediaProcessing(ProcessingTypes type);
    int SetExternalMixing(bool enabled);

    // VoEVolumeControl
    int GetSpeechOutputLevel(uint32_t& level) const;
    int GetSpeechOutputLevelFullRange(uint32_t& level) const;
    int SetMute(bool enable);
    bool Mute() const;
    int SetOutputVolumePan(float left, float right);
    int GetOutputVolumePan(float& left, float& right) const;
    int SetChannelOutputVolumeScaling(float scaling);
    int GetChannelOutputVolumeScaling(float& scaling) const;

    // VoENetEqStats
    int GetNetworkStatistics(NetworkStatistics& stats);
    void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const;

    // VoEVideoSync
    bool GetDelayEstimate(int* jitter_buffer_delay_ms,
                          int* playout_buffer_delay_ms) const;
    int least_required_delay_ms() const { return least_required_delay_ms_; }
    int SetInitialPlayoutDelay(int delay_ms);
    int SetMinimumPlayoutDelay(int delayMs);
    int GetPlayoutTimestamp(unsigned int& timestamp);
    void UpdatePlayoutTimestamp(bool rtcp);
    int SetInitTimestamp(unsigned int timestamp);
    int SetInitSequenceNumber(short sequenceNumber);

    // VoEVideoSyncExtended
    int GetRtpRtcp(RtpRtcp** rtpRtcpModule, RtpReceiver** rtp_receiver) const;

    // VoEDtmf
    int SendTelephoneEventOutband(unsigned char eventCode, int lengthMs,
                                  int attenuationDb, bool playDtmfEvent);
    int SendTelephoneEventInband(unsigned char eventCode, int lengthMs,
                                 int attenuationDb, bool playDtmfEvent);
    int SetSendTelephoneEventPayloadType(unsigned char type);
    int GetSendTelephoneEventPayloadType(unsigned char& type);

    // VoEAudioProcessingImpl
    int UpdateRxVadDetection(AudioFrame& audioFrame);
    int RegisterRxVadObserver(VoERxVadCallback &observer);
    int DeRegisterRxVadObserver();
    int VoiceActivityIndicator(int &activity);
#ifdef WEBRTC_VOICE_ENGINE_AGC
    int SetRxAgcStatus(bool enable, AgcModes mode);
    int GetRxAgcStatus(bool& enabled, AgcModes& mode);
    int SetRxAgcConfig(AgcConfig config);
    int GetRxAgcConfig(AgcConfig& config);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NR
    int SetRxNsStatus(bool enable, NsModes mode);
    int GetRxNsStatus(bool& enabled, NsModes& mode);
#endif

    // VoERTP_RTCP
    int SetLocalSSRC(unsigned int ssrc);
    int GetLocalSSRC(unsigned int& ssrc);
    int GetRemoteSSRC(unsigned int& ssrc);
    int SetSendAudioLevelIndicationStatus(bool enable, unsigned char id);
    int SetReceiveAudioLevelIndicationStatus(bool enable, unsigned char id);
    int SetSendAbsoluteSenderTimeStatus(bool enable, unsigned char id);
    int SetReceiveAbsoluteSenderTimeStatus(bool enable, unsigned char id);
    void SetRTCPStatus(bool enable);
    int GetRTCPStatus(bool& enabled);
    int SetRTCP_CNAME(const char cName[256]);
    int GetRemoteRTCP_CNAME(char cName[256]);
    int GetRemoteRTCPData(unsigned int& NTPHigh, unsigned int& NTPLow,
                          unsigned int& timestamp,
                          unsigned int& playoutTimestamp, unsigned int* jitter,
                          unsigned short* fractionLost);
    int SendApplicationDefinedRTCPPacket(unsigned char subType,
                                         unsigned int name, const char* data,
                                         unsigned short dataLengthInBytes);
    int GetRTPStatistics(unsigned int& averageJitterMs,
                         unsigned int& maxJitterMs,
                         unsigned int& discardedPackets);
    int GetRemoteRTCPReportBlocks(std::vector<ReportBlock>* report_blocks);
    int GetRTPStatistics(CallStatistics& stats);
    int SetREDStatus(bool enable, int redPayloadtype);
    int GetREDStatus(bool& enabled, int& redPayloadtype);
    int SetCodecFECStatus(bool enable);
    bool GetCodecFECStatus();
    void SetNACKStatus(bool enable, int maxNumberOfPackets);
    int StartRTPDump(const char fileNameUTF8[1024], RTPDirections direction);
    int StopRTPDump(RTPDirections direction);
    bool RTPDumpIsActive(RTPDirections direction);
    // Takes ownership of the ViENetwork.
    void SetVideoEngineBWETarget(ViENetwork* vie_network, int video_channel);

    // From AudioPacketizationCallback in the ACM
    int32_t SendData(FrameType frameType,
                     uint8_t payloadType,
                     uint32_t timeStamp,
                     const uint8_t* payloadData,
                     size_t payloadSize,
                     const RTPFragmentationHeader* fragmentation) override;

    // From ACMVADCallback in the ACM
    int32_t InFrameType(FrameType frame_type) override;

    int32_t OnRxVadDetected(int vadDecision);

    // From RtpData in the RTP/RTCP module
    int32_t OnReceivedPayloadData(const uint8_t* payloadData,
                                  size_t payloadSize,
                                  const WebRtcRTPHeader* rtpHeader) override;
    bool OnRecoveredPacket(const uint8_t* packet,
                           size_t packet_length) override;

    // From RtpFeedback in the RTP/RTCP module
    int32_t OnInitializeDecoder(int32_t id,
                                int8_t payloadType,
                                const char payloadName[RTP_PAYLOAD_NAME_SIZE],
                                int frequency,
                                uint8_t channels,
                                uint32_t rate) override;
    void OnIncomingSSRCChanged(int32_t id, uint32_t ssrc) override;
    void OnIncomingCSRCChanged(int32_t id, uint32_t CSRC, bool added) override;
    void ResetStatistics(uint32_t ssrc) override;

    // From RtpAudioFeedback in the RTP/RTCP module
    void OnPlayTelephoneEvent(int32_t id,
                              uint8_t event,
                              uint16_t lengthMs,
                              uint8_t volume) override;

    // From Transport (called by the RTP/RTCP module)
    int SendPacket(int /*channel*/, const void* data, size_t len) override;
    int SendRTCPPacket(int /*channel*/, const void* data, size_t len) override;

    // From MixerParticipant
    int32_t GetAudioFrame(int32_t id, AudioFrame& audioFrame) override;
    int32_t NeededFrequency(int32_t id) override;

    // From FileCallback
    void PlayNotification(int32_t id, uint32_t durationMs) override;
    void RecordNotification(int32_t id, uint32_t durationMs) override;
    void PlayFileEnded(int32_t id) override;
    void RecordFileEnded(int32_t id) override;

    uint32_t InstanceId() const
    {
        return _instanceId;
    }
    int32_t ChannelId() const
    {
        return _channelId;
    }
    bool Playing() const
    {
        return channel_state_.Get().playing;
    }
    bool Sending() const
    {
        return channel_state_.Get().sending;
    }
    bool Receiving() const
    {
        return channel_state_.Get().receiving;
    }
    bool ExternalTransport() const
    {
        CriticalSectionScoped cs(&_callbackCritSect);
        return _externalTransport;
    }
    bool ExternalMixing() const
    {
        return _externalMixing;
    }
    RtpRtcp* RtpRtcpModulePtr() const
    {
        return _rtpRtcpModule.get();
    }
    int8_t OutputEnergyLevel() const
    {
        return _outputAudioLevel.Level();
    }
    uint32_t Demultiplex(const AudioFrame& audioFrame);
    // Demultiplex the data to the channel's |_audioFrame|. The difference
    // between this method and the overloaded method above is that |audio_data|
    // does not go through transmit_mixer and APM.
    void Demultiplex(const int16_t* audio_data,
                     int sample_rate,
                     int number_of_frames,
                     int number_of_channels);
    uint32_t PrepareEncodeAndSend(int mixingFrequency);
    uint32_t EncodeAndSend();

protected:
    void OnIncomingFractionLoss(int fraction_lost);

private:
    bool ReceivePacket(const uint8_t* packet, size_t packet_length,
                       const RTPHeader& header, bool in_order);
    bool HandleRtxPacket(const uint8_t* packet,
                         size_t packet_length,
                         const RTPHeader& header);
    bool IsPacketInOrder(const RTPHeader& header) const;
    bool IsPacketRetransmitted(const RTPHeader& header, bool in_order) const;
    int ResendPackets(const uint16_t* sequence_numbers, int length);
    int InsertInbandDtmfTone();
    int32_t MixOrReplaceAudioWithFile(int mixingFrequency);
    int32_t MixAudioWithFile(AudioFrame& audioFrame, int mixingFrequency);
    int32_t SendPacketRaw(const void *data, size_t len, bool RTCP);
    void UpdatePacketDelay(uint32_t timestamp,
                           uint16_t sequenceNumber);
    void RegisterReceiveCodecsToRTPModule();

    int SetRedPayloadType(int red_payload_type);
    int SetSendRtpHeaderExtension(bool enable, RTPExtensionType type,
                                  unsigned char id);

    int32_t GetPlayoutFrequency();
    int64_t GetRTT() const;

    CriticalSectionWrapper& _fileCritSect;
    CriticalSectionWrapper& _callbackCritSect;
    CriticalSectionWrapper& volume_settings_critsect_;
    uint32_t _instanceId;
    int32_t _channelId;

    ChannelState channel_state_;

    rtc::scoped_ptr<RtpHeaderParser> rtp_header_parser_;
    rtc::scoped_ptr<RTPPayloadRegistry> rtp_payload_registry_;
    rtc::scoped_ptr<ReceiveStatistics> rtp_receive_statistics_;
    rtc::scoped_ptr<StatisticsProxy> statistics_proxy_;
    rtc::scoped_ptr<RtpReceiver> rtp_receiver_;
    TelephoneEventHandler* telephone_event_handler_;
    rtc::scoped_ptr<RtpRtcp> _rtpRtcpModule;
    rtc::scoped_ptr<AudioCodingModule> audio_coding_;
    RtpDump& _rtpDumpIn;
    RtpDump& _rtpDumpOut;
    AudioLevel _outputAudioLevel;
    bool _externalTransport;
    AudioFrame _audioFrame;
    rtc::scoped_ptr<int16_t[]> mono_recording_audio_;
    // Downsamples to the codec rate if necessary.
    PushResampler<int16_t> input_resampler_;
    FilePlayer* _inputFilePlayerPtr;
    FilePlayer* _outputFilePlayerPtr;
    FileRecorder* _outputFileRecorderPtr;
    int _inputFilePlayerId;
    int _outputFilePlayerId;
    int _outputFileRecorderId;
    bool _outputFileRecording;
    DtmfInbandQueue _inbandDtmfQueue;
    DtmfInband _inbandDtmfGenerator;
    bool _outputExternalMedia;
    VoEMediaProcess* _inputExternalMediaCallbackPtr;
    VoEMediaProcess* _outputExternalMediaCallbackPtr;
    uint32_t _timeStamp;
    uint8_t _sendTelephoneEventPayloadType;

    RemoteNtpTimeEstimator ntp_estimator_ GUARDED_BY(ts_stats_lock_);

    // Timestamp of the audio pulled from NetEq.
    uint32_t jitter_buffer_playout_timestamp_;
    uint32_t playout_timestamp_rtp_;
    uint32_t playout_timestamp_rtcp_;
    uint32_t playout_delay_ms_;
    uint32_t _numberOfDiscardedPackets;
    uint16_t send_sequence_number_;
    uint8_t restored_packet_[kVoiceEngineMaxIpPacketSizeBytes];

    rtc::scoped_ptr<CriticalSectionWrapper> ts_stats_lock_;

    rtc::scoped_ptr<rtc::TimestampWrapAroundHandler> rtp_ts_wraparound_handler_;
    // The rtp timestamp of the first played out audio frame.
    int64_t capture_start_rtp_time_stamp_;
    // The capture ntp time (in local timebase) of the first played out audio
    // frame.
    int64_t capture_start_ntp_time_ms_ GUARDED_BY(ts_stats_lock_);

    // uses
    Statistics* _engineStatisticsPtr;
    OutputMixer* _outputMixerPtr;
    TransmitMixer* _transmitMixerPtr;
    ProcessThread* _moduleProcessThreadPtr;
    AudioDeviceModule* _audioDeviceModulePtr;
    VoiceEngineObserver* _voiceEngineObserverPtr; // owned by base
    CriticalSectionWrapper* _callbackCritSectPtr; // owned by base
    Transport* _transportPtr; // WebRtc socket or external transport
    RMSLevel rms_level_;
    rtc::scoped_ptr<AudioProcessing> rx_audioproc_;  // far end AudioProcessing
    VoERxVadCallback* _rxVadObserverPtr;
    int32_t _oldVadDecision;
    int32_t _sendFrameType; // Send data is voice, 1-voice, 0-otherwise
    // VoEBase
    bool _externalMixing;
    bool _mixFileWithMicrophone;
    // VoEVolumeControl
    bool _mute;
    float _panLeft;
    float _panRight;
    float _outputGain;
    // VoEDtmf
    bool _playOutbandDtmfEvent;
    bool _playInbandDtmfEvent;
    // VoeRTP_RTCP
    uint32_t _lastLocalTimeStamp;
    int8_t _lastPayloadType;
    bool _includeAudioLevelIndication;
    // VoENetwork
    AudioFrame::SpeechType _outputSpeechType;
    ViENetwork* vie_network_;
    int video_channel_;
    // VoEVideoSync
    uint32_t _average_jitter_buffer_delay_us;
    int least_required_delay_ms_;
    uint32_t _previousTimestamp;
    uint16_t _recPacketDelayMs;
    // VoEAudioProcessing
    bool _RxVadDetection;
    bool _rxAgcIsEnabled;
    bool _rxNsIsEnabled;
    bool restored_packet_in_use_;
    // RtcpBandwidthObserver
    rtc::scoped_ptr<VoERtcpObserver> rtcp_observer_;
    rtc::scoped_ptr<NetworkPredictor> network_predictor_;
};

}  // namespace voe
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CHANNEL_H_
