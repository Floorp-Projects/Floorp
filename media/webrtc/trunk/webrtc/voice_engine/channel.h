/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CHANNEL_H
#define WEBRTC_VOICE_ENGINE_CHANNEL_H

#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer_defines.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/file_player.h"
#include "webrtc/modules/utility/interface/file_recorder.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/voice_engine/dtmf_inband.h"
#include "webrtc/voice_engine/dtmf_inband_queue.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine/shared_data.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

#ifdef WEBRTC_DTMF_DETECTION
// TelephoneEventDetectionMethods, TelephoneEventObserver
#include "webrtc/voice_engine/include/voe_dtmf.h"
#endif

namespace webrtc {

class AudioDeviceModule;
class Config;
class CriticalSectionWrapper;
class FileWrapper;
class ProcessThread;
class ReceiveStatistics;
class RtpDump;
class RTPPayloadRegistry;
class RtpReceiver;
class RTPReceiverAudio;
class RtpRtcp;
class TelephoneEventHandler;
class VoEMediaProcess;
class VoERTCPObserver;
class VoERTPObserver;
class VoiceEngineObserver;

struct CallStatistics;
struct ReportBlock;
struct SenderInfo;

namespace voe {

class Statistics;
class StatisticsProxy;
class TransmitMixer;
class OutputMixer;


class Channel:
    public RtpData,
    public RtpFeedback,
    public RtcpFeedback,
    public FileCallback, // receiving notification from file player & recorder
    public Transport,
    public RtpAudioFeedback,
    public AudioPacketizationCallback, // receive encoded packets from the ACM
    public ACMVADCallback, // receive voice activity from the ACM
    public MixerParticipant // supplies output mixer with audio frames
{
public:
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

    int32_t SetNetEQPlayoutMode(NetEqModes mode);
    int32_t GetNetEQPlayoutMode(NetEqModes& mode);
    int32_t SetOnHoldStatus(bool enable, OnHoldModes mode);
    int32_t GetOnHoldStatus(bool& enabled, OnHoldModes& mode);
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
    int32_t SetAMREncFormat(AmrMode mode);
    int32_t SetAMRDecFormat(AmrMode mode);
    int32_t SetAMRWbEncFormat(AmrMode mode);
    int32_t SetAMRWbDecFormat(AmrMode mode);
    int32_t SetSendCNPayloadType(int type, PayloadFrequencies frequency);
    int32_t SetISACInitTargetRate(int rateBps, bool useFixedFrameSize);
    int32_t SetISACMaxRate(int rateBps);
    int32_t SetISACMaxPayloadSize(int sizeBytes);

    // VoE dual-streaming.
    int SetSecondarySendCodec(const CodecInst& codec, int red_payload_type);
    void RemoveSecondarySendCodec();
    int GetSecondarySendCodec(CodecInst* codec);

    // VoENetwork
    int32_t RegisterExternalTransport(Transport& transport);
    int32_t DeRegisterExternalTransport();
    int32_t ReceivedRTPPacket(const int8_t* data, int32_t length);
    int32_t ReceivedRTCPPacket(const int8_t* data, int32_t length);

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
    int ScaleLocalFilePlayout(float scale);
    int GetLocalPlayoutPosition(int& positionMs);
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
    int ScaleFileAsMicrophonePlayout(float scale);
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

    // VoECallReport
    void ResetDeadOrAliveCounters();
    int ResetRTCPStatistics();
    int GetRoundTripTimeSummary(StatVal& delaysMs) const;
    int GetDeadOrAliveCounters(int& countDead, int& countAlive) const;

    // VoENetEqStats
    int GetNetworkStatistics(NetworkStatistics& stats);
    void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const;

    // VoEVideoSync
    bool GetDelayEstimate(int* jitter_buffer_delay_ms,
                          int* playout_buffer_delay_ms,
                          int* avsync_offset_ms) const;
    int least_required_delay_ms() const { return least_required_delay_ms_; }
    int SetInitialPlayoutDelay(int delay_ms);
    int SetMinimumPlayoutDelay(int delayMs);
    void SetCurrentSyncOffset(int offsetMs) { _current_sync_offset = offsetMs; }
    int GetPlayoutTimestamp(unsigned int& timestamp);
    void UpdatePlayoutTimestamp(bool rtcp);
    int SetInitTimestamp(unsigned int timestamp);
    int SetInitSequenceNumber(short sequenceNumber);

    // VoEVideoSyncExtended
    int GetRtpRtcp(RtpRtcp** rtpRtcpModule, RtpReceiver** rtp_receiver) const;

    // VoEEncryption
    int RegisterExternalEncryption(Encryption& encryption);
    int DeRegisterExternalEncryption();

    // VoEDtmf
    int SendTelephoneEventOutband(unsigned char eventCode, int lengthMs,
                                  int attenuationDb, bool playDtmfEvent);
    int SendTelephoneEventInband(unsigned char eventCode, int lengthMs,
                                 int attenuationDb, bool playDtmfEvent);
    int SetDtmfPlayoutStatus(bool enable);
    bool DtmfPlayoutStatus() const;
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
    int RegisterRTPObserver(VoERTPObserver& observer);
    int DeRegisterRTPObserver();
    int RegisterRTCPObserver(VoERTCPObserver& observer);
    int DeRegisterRTCPObserver();
    int SetLocalSSRC(unsigned int ssrc);
    int GetLocalSSRC(unsigned int& ssrc);
    int GetRemoteSSRC(unsigned int& ssrc);
    int GetRemoteCSRCs(unsigned int arrCSRC[15]);
    int SetRTPAudioLevelIndicationStatus(bool enable, unsigned char ID);
    int GetRTPAudioLevelIndicationStatus(bool& enable, unsigned char& ID);
    int SetRTCPStatus(bool enable);
    int GetRTCPStatus(bool& enabled);
    int SetRTCP_CNAME(const char cName[256]);
    int GetRTCP_CNAME(char cName[256]);
    int GetRemoteRTCP_CNAME(char cName[256]);
    int GetRemoteRTCPReceiverInfo(uint32_t& NTPHigh, uint32_t& NTPLow,
                                  uint32_t& receivedPacketCount,
                                  uint64_t& receivedOctetCount,
                                  uint32_t& jitter,
                                  uint16_t& fractionLost,
                                  uint32_t& cumulativeLost,
                                  int32_t& rttMs);
    int SendApplicationDefinedRTCPPacket(unsigned char subType,
                                         unsigned int name, const char* data,
                                         unsigned short dataLengthInBytes);
    int GetRTPStatistics(unsigned int& averageJitterMs,
                         unsigned int& maxJitterMs,
                         unsigned int& discardedPackets,
                         unsigned int& cumulativeLost);
    int GetRemoteRTCPSenderInfo(SenderInfo* sender_info);
    int GetRemoteRTCPReportBlocks(std::vector<ReportBlock>* report_blocks);
    int GetRTPStatistics(CallStatistics& stats);
    int SetFECStatus(bool enable, int redPayloadtype);
    int GetFECStatus(bool& enabled, int& redPayloadtype);
    void SetNACKStatus(bool enable, int maxNumberOfPackets);
    int StartRTPDump(const char fileNameUTF8[1024], RTPDirections direction);
    int StopRTPDump(RTPDirections direction);
    bool RTPDumpIsActive(RTPDirections direction);
    int InsertExtraRTPPacket(unsigned char payloadType, bool markerBit,
                             const char* payloadData,
                             unsigned short payloadSize);
    uint32_t LastRemoteTimeStamp() { return _lastRemoteTimeStamp; }

    // From AudioPacketizationCallback in the ACM
    int32_t SendData(FrameType frameType,
                     uint8_t payloadType,
                     uint32_t timeStamp,
                     const uint8_t* payloadData,
                     uint16_t payloadSize,
                     const RTPFragmentationHeader* fragmentation);
    // From ACMVADCallback in the ACM
    int32_t InFrameType(int16_t frameType);

    int32_t OnRxVadDetected(int vadDecision);

    // From RtpData in the RTP/RTCP module
    int32_t OnReceivedPayloadData(const uint8_t* payloadData,
                                  uint16_t payloadSize,
                                  const WebRtcRTPHeader* rtpHeader);

    bool OnRecoveredPacket(const uint8_t* packet, int packet_length);

    // From RtpFeedback in the RTP/RTCP module
    int32_t OnInitializeDecoder(
            int32_t id,
            int8_t payloadType,
            const char payloadName[RTP_PAYLOAD_NAME_SIZE],
            int frequency,
            uint8_t channels,
            uint32_t rate);

    void OnPacketTimeout(int32_t id);

    void OnReceivedPacket(int32_t id, RtpRtcpPacketType packetType);

    void OnPeriodicDeadOrAlive(int32_t id,
                               RTPAliveType alive);

    void OnIncomingSSRCChanged(int32_t id,
                               uint32_t ssrc);

    void OnIncomingCSRCChanged(int32_t id,
                               uint32_t CSRC, bool added);

    void ResetStatistics(uint32_t ssrc);

    // From RtcpFeedback in the RTP/RTCP module
    void OnApplicationDataReceived(int32_t id,
                                   uint8_t subType,
                                   uint32_t name,
                                   uint16_t length,
                                   const uint8_t* data);

    // From RtpAudioFeedback in the RTP/RTCP module
    void OnReceivedTelephoneEvent(int32_t id,
                                  uint8_t event,
                                  bool endOfEvent);

    void OnPlayTelephoneEvent(int32_t id,
                              uint8_t event,
                              uint16_t lengthMs,
                              uint8_t volume);

    // From Transport (called by the RTP/RTCP module)
    int SendPacket(int /*channel*/, const void *data, int len);
    int SendRTCPPacket(int /*channel*/, const void *data, int len);

    // From MixerParticipant
    int32_t GetAudioFrame(int32_t id, AudioFrame& audioFrame);
    int32_t NeededFrequency(int32_t id);

    // From MonitorObserver
    void OnPeriodicProcess();

    // From FileCallback
    void PlayNotification(int32_t id,
                          uint32_t durationMs);
    void RecordNotification(int32_t id,
                            uint32_t durationMs);
    void PlayFileEnded(int32_t id);
    void RecordFileEnded(int32_t id);

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
        return _playing;
    }
    bool Sending() const
    {
        // A lock is needed because |_sending| is accessed by both
        // TransmitMixer::PrepareDemux() and StartSend()/StopSend(), which
        // are called by different threads.
        CriticalSectionScoped cs(&_callbackCritSect);
        return _sending;
    }
    bool Receiving() const
    {
        return _receiving;
    }
    bool ExternalTransport() const
    {
        return _externalTransport;
    }
    bool ExternalMixing() const
    {
        return _externalMixing;
    }
    bool OutputIsOnHold() const
    {
        return _outputIsOnHold;
    }
    bool InputIsOnHold() const
    {
        return _inputIsOnHold;
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

private:
    bool ReceivePacket(const uint8_t* packet, int packet_length,
                       const RTPHeader& header, bool in_order);
    bool HandleEncapsulation(const uint8_t* packet,
                             int packet_length,
                             const RTPHeader& header);
    bool IsPacketInOrder(const RTPHeader& header) const;
    bool IsPacketRetransmitted(const RTPHeader& header, bool in_order) const;
    int ResendPackets(const uint16_t* sequence_numbers, int length);
    int InsertInbandDtmfTone();
    int32_t MixOrReplaceAudioWithFile(int mixingFrequency);
    int32_t MixAudioWithFile(AudioFrame& audioFrame, int mixingFrequency);
    void UpdateDeadOrAliveCounters(bool alive);
    int32_t SendPacketRaw(const void *data, int len, bool RTCP);
    void UpdatePacketDelay(uint32_t timestamp,
                           uint16_t sequenceNumber);
    void RegisterReceiveCodecsToRTPModule();

    int SetRedPayloadType(int red_payload_type);

    CriticalSectionWrapper& _fileCritSect;
    CriticalSectionWrapper& _callbackCritSect;
    CriticalSectionWrapper& volume_settings_critsect_;
    uint32_t _instanceId;
    int32_t _channelId;

    scoped_ptr<RtpHeaderParser> rtp_header_parser_;
    scoped_ptr<RTPPayloadRegistry> rtp_payload_registry_;
    scoped_ptr<ReceiveStatistics> rtp_receive_statistics_;
    scoped_ptr<StatisticsProxy> statistics_proxy_;
    scoped_ptr<RtpReceiver> rtp_receiver_;
    TelephoneEventHandler* telephone_event_handler_;
    scoped_ptr<RtpRtcp> _rtpRtcpModule;
    scoped_ptr<AudioCodingModule> audio_coding_;
    RtpDump& _rtpDumpIn;
    RtpDump& _rtpDumpOut;
    AudioLevel _outputAudioLevel;
    bool _externalTransport;
    AudioFrame _audioFrame;
    scoped_array<int16_t> mono_recording_audio_;
    // Resampler is used when input data is stereo while codec is mono.
    PushResampler input_resampler_;
    uint8_t _audioLevel_dBov;
    FilePlayer* _inputFilePlayerPtr;
    FilePlayer* _outputFilePlayerPtr;
    FileRecorder* _outputFileRecorderPtr;
    int _inputFilePlayerId;
    int _outputFilePlayerId;
    int _outputFileRecorderId;
    bool _inputFilePlaying;
    bool _outputFilePlaying;
    bool _outputFileRecording;
    DtmfInbandQueue _inbandDtmfQueue;
    DtmfInband _inbandDtmfGenerator;
    bool _inputExternalMedia;
    bool _outputExternalMedia;
    VoEMediaProcess* _inputExternalMediaCallbackPtr;
    VoEMediaProcess* _outputExternalMediaCallbackPtr;
    uint8_t* _encryptionRTPBufferPtr;
    uint8_t* _decryptionRTPBufferPtr;
    uint8_t* _encryptionRTCPBufferPtr;
    uint8_t* _decryptionRTCPBufferPtr;
    uint32_t _timeStamp;
    uint8_t _sendTelephoneEventPayloadType;

    // Timestamp of the audio pulled from NetEq.
    uint32_t jitter_buffer_playout_timestamp_;
    uint32_t playout_timestamp_rtp_;
    uint32_t playout_timestamp_rtcp_;
    uint32_t playout_delay_ms_;
    uint32_t _numberOfDiscardedPackets;
    uint16_t send_sequence_number_;
    uint8_t restored_packet_[kVoiceEngineMaxIpPacketSizeBytes];

    // uses
    Statistics* _engineStatisticsPtr;
    OutputMixer* _outputMixerPtr;
    TransmitMixer* _transmitMixerPtr;
    ProcessThread* _moduleProcessThreadPtr;
    AudioDeviceModule* _audioDeviceModulePtr;
    VoiceEngineObserver* _voiceEngineObserverPtr; // owned by base
    CriticalSectionWrapper* _callbackCritSectPtr; // owned by base
    Transport* _transportPtr; // WebRtc socket or external transport
    Encryption* _encryptionPtr; // WebRtc SRTP or external encryption
    scoped_ptr<AudioProcessing> rtp_audioproc_;
    scoped_ptr<AudioProcessing> rx_audioproc_; // far end AudioProcessing
    VoERxVadCallback* _rxVadObserverPtr;
    int32_t _oldVadDecision;
    int32_t _sendFrameType; // Send data is voice, 1-voice, 0-otherwise
    VoERTPObserver* _rtpObserverPtr;
    VoERTCPObserver* _rtcpObserverPtr;
    // VoEBase
    bool _outputIsOnHold;
    bool _externalPlayout;
    bool _externalMixing;
    bool _inputIsOnHold;
    bool _playing;
    bool _sending;
    bool _receiving;
    bool _mixFileWithMicrophone;
    bool _rtpObserver;
    bool _rtcpObserver;
    // VoEVolumeControl
    bool _mute;
    float _panLeft;
    float _panRight;
    float _outputGain;
    // VoEEncryption
    bool _encrypting;
    bool _decrypting;
    // VoEDtmf
    bool _playOutbandDtmfEvent;
    bool _playInbandDtmfEvent;
    // VoeRTP_RTCP
    uint8_t _extraPayloadType;
    bool _insertExtraRTPPacket;
    bool _extraMarkerBit;
    uint32_t _lastLocalTimeStamp;
    uint32_t _lastRemoteTimeStamp;
    int8_t _lastPayloadType;
    bool _includeAudioLevelIndication;
    // VoENetwork
    bool _rtpPacketTimedOut;
    bool _rtpPacketTimeOutIsEnabled;
    uint32_t _rtpTimeOutSeconds;
    bool _connectionObserver;
    VoEConnectionObserver* _connectionObserverPtr;
    uint32_t _countAliveDetections;
    uint32_t _countDeadDetections;
    AudioFrame::SpeechType _outputSpeechType;
    // VoEVideoSync
    uint32_t _average_jitter_buffer_delay_us;
    int least_required_delay_ms_;
    uint32_t _previousTimestamp;
    uint16_t _recPacketDelayMs;
    int _current_sync_offset;
    // VoEAudioProcessing
    bool _RxVadDetection;
    bool _rxApmIsEnabled;
    bool _rxAgcIsEnabled;
    bool _rxNsIsEnabled;
    bool restored_packet_in_use_;
};

}  // namespace voe
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CHANNEL_H
