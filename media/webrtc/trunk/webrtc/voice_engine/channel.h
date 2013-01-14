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

#include "audio_coding_module.h"
#include "audio_conference_mixer_defines.h"
#include "common_types.h"
#include "dtmf_inband.h"
#include "dtmf_inband_queue.h"
#include "file_player.h"
#include "file_recorder.h"
#include "level_indicator.h"
#include "resampler.h"
#include "rtp_rtcp.h"
#include "scoped_ptr.h"
#include "shared_data.h"
#include "voe_audio_processing.h"
#include "voe_network.h"
#include "voice_engine_defines.h"

#ifndef WEBRTC_EXTERNAL_TRANSPORT
#include "udp_transport.h"
#endif
#ifdef WEBRTC_SRTP
#include "SrtpModule.h"
#endif
#ifdef WEBRTC_DTMF_DETECTION
#include "voe_dtmf.h" // TelephoneEventDetectionMethods, TelephoneEventObserver
#endif

namespace webrtc
{
class CriticalSectionWrapper;
class ProcessThread;
class AudioDeviceModule;
class RtpRtcp;
class FileWrapper;
class RtpDump;
class VoiceEngineObserver;
class VoEMediaProcess;
class VoERTPObserver;
class VoERTCPObserver;

struct CallStatistics;
struct ReportBlock;
struct SenderInfo;

namespace voe
{
class Statistics;
class TransmitMixer;
class OutputMixer;


class Channel:
    public RtpData,
    public RtpFeedback,
    public RtcpFeedback,
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    public UdpTransportData, // receiving packet from sockets
#endif
    public FileCallback, // receiving notification from file player & recorder
    public Transport,
    public RtpAudioFeedback,
    public AudioPacketizationCallback, // receive encoded packets from the ACM
    public ACMVADCallback, // receive voice activity from the ACM
#ifdef WEBRTC_DTMF_DETECTION
    public AudioCodingFeedback, // inband Dtmf detection in the ACM
#endif
    public MixerParticipant // supplies output mixer with audio frames
{
public:
    enum {KNumSocketThreads = 1};
    enum {KNumberOfSocketBuffers = 8};
public:
    virtual ~Channel();
    static WebRtc_Word32 CreateChannel(Channel*& channel,
                                       const WebRtc_Word32 channelId,
                                       const WebRtc_UWord32 instanceId);
    Channel(const WebRtc_Word32 channelId, const WebRtc_UWord32 instanceId);
    WebRtc_Word32 Init();
    WebRtc_Word32 SetEngineInformation(
        Statistics& engineStatistics,
        OutputMixer& outputMixer,
        TransmitMixer& transmitMixer,
        ProcessThread& moduleProcessThread,
        AudioDeviceModule& audioDeviceModule,
        VoiceEngineObserver* voiceEngineObserver,
        CriticalSectionWrapper* callbackCritSect);
    WebRtc_Word32 UpdateLocalTimeStamp();

public:
    // API methods

    // VoEBase
    WebRtc_Word32 StartPlayout();
    WebRtc_Word32 StopPlayout();
    WebRtc_Word32 StartSend();
    WebRtc_Word32 StopSend();
    WebRtc_Word32 StartReceiving();
    WebRtc_Word32 StopReceiving();

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    WebRtc_Word32 SetLocalReceiver(const WebRtc_UWord16 rtpPort,
                                   const WebRtc_UWord16 rtcpPort,
                                   const char ipAddr[64],
                                   const char multicastIpAddr[64]);
    WebRtc_Word32 GetLocalReceiver(int& port, int& RTCPport, char ipAddr[]);
    WebRtc_Word32 SetSendDestination(const WebRtc_UWord16 rtpPort,
                                     const char ipAddr[64],
                                     const int sourcePort,
                                     const WebRtc_UWord16 rtcpPort);
    WebRtc_Word32 GetSendDestination(int& port, char ipAddr[64],
                                     int& sourcePort, int& RTCPport);
#endif
    WebRtc_Word32 SetNetEQPlayoutMode(NetEqModes mode);
    WebRtc_Word32 GetNetEQPlayoutMode(NetEqModes& mode);
    WebRtc_Word32 SetNetEQBGNMode(NetEqBgnModes mode);
    WebRtc_Word32 GetNetEQBGNMode(NetEqBgnModes& mode);
    WebRtc_Word32 SetOnHoldStatus(bool enable, OnHoldModes mode);
    WebRtc_Word32 GetOnHoldStatus(bool& enabled, OnHoldModes& mode);
    WebRtc_Word32 RegisterVoiceEngineObserver(VoiceEngineObserver& observer);
    WebRtc_Word32 DeRegisterVoiceEngineObserver();

    // VoECodec
    WebRtc_Word32 GetSendCodec(CodecInst& codec);
    WebRtc_Word32 GetRecCodec(CodecInst& codec);
    WebRtc_Word32 SetSendCodec(const CodecInst& codec);
    WebRtc_Word32 SetVADStatus(bool enableVAD, ACMVADMode mode,
                               bool disableDTX);
    WebRtc_Word32 GetVADStatus(bool& enabledVAD, ACMVADMode& mode,
                               bool& disabledDTX);
    WebRtc_Word32 SetRecPayloadType(const CodecInst& codec);
    WebRtc_Word32 GetRecPayloadType(CodecInst& codec);
    WebRtc_Word32 SetAMREncFormat(AmrMode mode);
    WebRtc_Word32 SetAMRDecFormat(AmrMode mode);
    WebRtc_Word32 SetAMRWbEncFormat(AmrMode mode);
    WebRtc_Word32 SetAMRWbDecFormat(AmrMode mode);
    WebRtc_Word32 SetSendCNPayloadType(int type, PayloadFrequencies frequency);
    WebRtc_Word32 SetISACInitTargetRate(int rateBps, bool useFixedFrameSize);
    WebRtc_Word32 SetISACMaxRate(int rateBps);
    WebRtc_Word32 SetISACMaxPayloadSize(int sizeBytes);

    // VoE dual-streaming.
    int SetSecondarySendCodec(const CodecInst& codec, int red_payload_type);
    void RemoveSecondarySendCodec();
    int GetSecondarySendCodec(CodecInst* codec);

    // VoENetwork
    WebRtc_Word32 RegisterExternalTransport(Transport& transport);
    WebRtc_Word32 DeRegisterExternalTransport();
    WebRtc_Word32 ReceivedRTPPacket(const WebRtc_Word8* data,
                                    WebRtc_Word32 length);
    WebRtc_Word32 ReceivedRTCPPacket(const WebRtc_Word8* data,
                                     WebRtc_Word32 length);
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    WebRtc_Word32 GetSourceInfo(int& rtpPort, int& rtcpPort, char ipAddr[64]);
    WebRtc_Word32 EnableIPv6();
    bool IPv6IsEnabled() const;
    WebRtc_Word32 SetSourceFilter(int rtpPort, int rtcpPort,
                                  const char ipAddr[64]);
    WebRtc_Word32 GetSourceFilter(int& rtpPort, int& rtcpPort, char ipAddr[64]);
    WebRtc_Word32 SetSendTOS(int DSCP, int priority, bool useSetSockopt);
    WebRtc_Word32 GetSendTOS(int &DSCP, int& priority, bool &useSetSockopt);
#if defined(_WIN32)
    WebRtc_Word32 SetSendGQoS(bool enable, int serviceType, int overrideDSCP);
    WebRtc_Word32 GetSendGQoS(bool &enabled, int &serviceType,
                              int &overrideDSCP);
#endif
#endif
    WebRtc_Word32 SetPacketTimeoutNotification(bool enable, int timeoutSeconds);
    WebRtc_Word32 GetPacketTimeoutNotification(bool& enabled,
                                               int& timeoutSeconds);
    WebRtc_Word32 RegisterDeadOrAliveObserver(VoEConnectionObserver& observer);
    WebRtc_Word32 DeRegisterDeadOrAliveObserver();
    WebRtc_Word32 SetPeriodicDeadOrAliveStatus(bool enable,
                                               int sampleTimeSeconds);
    WebRtc_Word32 GetPeriodicDeadOrAliveStatus(bool& enabled,
                                               int& sampleTimeSeconds);
    WebRtc_Word32 SendUDPPacket(const void* data, unsigned int length,
                                int& transmittedBytes, bool useRtcpSocket);

    // VoEFile
    int StartPlayingFileLocally(const char* fileName, const bool loop,
                                const FileFormats format,
                                const int startPosition,
                                const float volumeScaling,
                                const int stopPosition,
                                const CodecInst* codecInst);
    int StartPlayingFileLocally(InStream* stream, const FileFormats format,
                                const int startPosition,
                                const float volumeScaling,
                                const int stopPosition,
                                const CodecInst* codecInst);
    int StopPlayingFileLocally();
    int IsPlayingFileLocally() const;
    int RegisterFilePlayingToMixer();
    int ScaleLocalFilePlayout(const float scale);
    int GetLocalPlayoutPosition(int& positionMs);
    int StartPlayingFileAsMicrophone(const char* fileName, const bool loop,
                                     const FileFormats format,
                                     const int startPosition,
                                     const float volumeScaling,
                                     const int stopPosition,
                                     const CodecInst* codecInst);
    int StartPlayingFileAsMicrophone(InStream* stream,
                                     const FileFormats format,
                                     const int startPosition,
                                     const float volumeScaling,
                                     const int stopPosition,
                                     const CodecInst* codecInst);
    int StopPlayingFileAsMicrophone();
    int IsPlayingFileAsMicrophone() const;
    int ScaleFileAsMicrophonePlayout(const float scale);
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
    int GetSpeechOutputLevel(WebRtc_UWord32& level) const;
    int GetSpeechOutputLevelFullRange(WebRtc_UWord32& level) const;
    int SetMute(const bool enable);
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

    // VoEVideoSync
    int GetDelayEstimate(int& delayMs) const;
    int SetMinimumPlayoutDelay(int delayMs);
    int GetPlayoutTimestamp(unsigned int& timestamp);
    int SetInitTimestamp(unsigned int timestamp);
    int SetInitSequenceNumber(short sequenceNumber);

    // VoEVideoSyncExtended
    int GetRtpRtcp(RtpRtcp* &rtpRtcpModule) const;

    // VoEEncryption
#ifdef WEBRTC_SRTP
    int EnableSRTPSend(
            CipherTypes cipherType,
            int cipherKeyLength,
            AuthenticationTypes authType,
            int authKeyLength,
            int authTagLength,
            SecurityLevels level,
            const unsigned char key[kVoiceEngineMaxSrtpKeyLength],
            bool useForRTCP);
    int DisableSRTPSend();
    int EnableSRTPReceive(
            CipherTypes cipherType,
            int cipherKeyLength,
            AuthenticationTypes authType,
            int authKeyLength,
            int authTagLength,
            SecurityLevels level,
            const unsigned char key[kVoiceEngineMaxSrtpKeyLength],
            bool useForRTCP);
    int DisableSRTPReceive();
#endif
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
#ifdef WEBRTC_DTMF_DETECTION
    int RegisterTelephoneEventDetection(
            TelephoneEventDetectionMethods detectionMethod,
            VoETelephoneEventObserver& observer);
    int DeRegisterTelephoneEventDetection();
    int GetTelephoneEventDetectionStatus(
            bool& enabled,
            TelephoneEventDetectionMethods& detectionMethod);
#endif

    // VoEAudioProcessingImpl
    int UpdateRxVadDetection(AudioFrame& audioFrame);
    int RegisterRxVadObserver(VoERxVadCallback &observer);
    int DeRegisterRxVadObserver();
    int VoiceActivityIndicator(int &activity);
#ifdef WEBRTC_VOICE_ENGINE_AGC
    int SetRxAgcStatus(const bool enable, const AgcModes mode);
    int GetRxAgcStatus(bool& enabled, AgcModes& mode);
    int SetRxAgcConfig(const AgcConfig config);
    int GetRxAgcConfig(AgcConfig& config);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NR
    int SetRxNsStatus(const bool enable, const NsModes mode);
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
    int GetRemoteRTCPData(unsigned int& NTPHigh, unsigned int& NTPLow,
                          unsigned int& timestamp,
                          unsigned int& playoutTimestamp, unsigned int* jitter,
                          unsigned short* fractionLost);
    int SendApplicationDefinedRTCPPacket(const unsigned char subType,
                                         unsigned int name, const char* data,
                                         unsigned short dataLengthInBytes);
    int GetRTPStatistics(unsigned int& averageJitterMs,
                         unsigned int& maxJitterMs,
                         unsigned int& discardedPackets);
    int GetRemoteRTCPSenderInfo(SenderInfo* sender_info);
    int GetRemoteRTCPReportBlocks(std::vector<ReportBlock>* report_blocks);
    int GetRTPStatistics(CallStatistics& stats);
    int SetFECStatus(bool enable, int redPayloadtype);
    int GetFECStatus(bool& enabled, int& redPayloadtype);
    int StartRTPDump(const char fileNameUTF8[1024], RTPDirections direction);
    int StopRTPDump(RTPDirections direction);
    bool RTPDumpIsActive(RTPDirections direction);
    int InsertExtraRTPPacket(unsigned char payloadType, bool markerBit,
                             const char* payloadData,
                             unsigned short payloadSize);
    uint32_t LastRemoteTimeStamp() { return _lastRemoteTimeStamp; }

public:
    // From AudioPacketizationCallback in the ACM
    WebRtc_Word32 SendData(FrameType frameType,
                           WebRtc_UWord8 payloadType,
                           WebRtc_UWord32 timeStamp,
                           const WebRtc_UWord8* payloadData,
                           WebRtc_UWord16 payloadSize,
                           const RTPFragmentationHeader* fragmentation);
    // From ACMVADCallback in the ACM
    WebRtc_Word32 InFrameType(WebRtc_Word16 frameType);

#ifdef WEBRTC_DTMF_DETECTION
public: // From AudioCodingFeedback in the ACM
    int IncomingDtmf(const WebRtc_UWord8 digitDtmf, const bool end);
#endif

public:
    WebRtc_Word32 OnRxVadDetected(const int vadDecision);

public:
    // From RtpData in the RTP/RTCP module
    WebRtc_Word32 OnReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                        const WebRtc_UWord16 payloadSize,
                                        const WebRtcRTPHeader* rtpHeader);

public:
    // From RtpFeedback in the RTP/RTCP module
    WebRtc_Word32 OnInitializeDecoder(
            const WebRtc_Word32 id,
            const WebRtc_Word8 payloadType,
            const char payloadName[RTP_PAYLOAD_NAME_SIZE],
            const int frequency,
            const WebRtc_UWord8 channels,
            const WebRtc_UWord32 rate);

    void OnPacketTimeout(const WebRtc_Word32 id);

    void OnReceivedPacket(const WebRtc_Word32 id,
                          const RtpRtcpPacketType packetType);

    void OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                               const RTPAliveType alive);

    void OnIncomingSSRCChanged(const WebRtc_Word32 id,
                               const WebRtc_UWord32 SSRC);

    void OnIncomingCSRCChanged(const WebRtc_Word32 id,
                               const WebRtc_UWord32 CSRC, const bool added);

public:
    // From RtcpFeedback in the RTP/RTCP module
    void OnApplicationDataReceived(const WebRtc_Word32 id,
                                   const WebRtc_UWord8 subType,
                                   const WebRtc_UWord32 name,
                                   const WebRtc_UWord16 length,
                                   const WebRtc_UWord8* data);

public:
    // From RtpAudioFeedback in the RTP/RTCP module
    void OnReceivedTelephoneEvent(const WebRtc_Word32 id,
                                  const WebRtc_UWord8 event,
                                  const bool endOfEvent);

    void OnPlayTelephoneEvent(const WebRtc_Word32 id,
                              const WebRtc_UWord8 event,
                              const WebRtc_UWord16 lengthMs,
                              const WebRtc_UWord8 volume);

public:
    // From UdpTransportData in the Socket Transport module
    void IncomingRTPPacket(const WebRtc_Word8* incomingRtpPacket,
                           const WebRtc_Word32 rtpPacketLength,
                           const char* fromIP,
                           const WebRtc_UWord16 fromPort);

    void IncomingRTCPPacket(const WebRtc_Word8* incomingRtcpPacket,
                            const WebRtc_Word32 rtcpPacketLength,
                            const char* fromIP,
                            const WebRtc_UWord16 fromPort);

public:
    // From Transport (called by the RTP/RTCP module)
    int SendPacket(int /*channel*/, const void *data, int len);
    int SendRTCPPacket(int /*channel*/, const void *data, int len);

public:
    // From MixerParticipant
    WebRtc_Word32 GetAudioFrame(const WebRtc_Word32 id,
                                AudioFrame& audioFrame);
    WebRtc_Word32 NeededFrequency(const WebRtc_Word32 id);

public:
    // From MonitorObserver
    void OnPeriodicProcess();

public:
    // From FileCallback
    void PlayNotification(const WebRtc_Word32 id,
                          const WebRtc_UWord32 durationMs);
    void RecordNotification(const WebRtc_Word32 id,
                            const WebRtc_UWord32 durationMs);
    void PlayFileEnded(const WebRtc_Word32 id);
    void RecordFileEnded(const WebRtc_Word32 id);

public:
    WebRtc_UWord32 InstanceId() const
    {
        return _instanceId;
    }
    WebRtc_Word32 ChannelId() const
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
    WebRtc_Word8 OutputEnergyLevel() const
    {
        return _outputAudioLevel.Level();
    }
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    bool SendSocketsInitialized() const
    {
        return _socketTransportModule.SendSocketsInitialized();
    }
    bool ReceiveSocketsInitialized() const
    {
        return _socketTransportModule.ReceiveSocketsInitialized();
    }
#endif
    WebRtc_UWord32 Demultiplex(const AudioFrame& audioFrame);
    WebRtc_UWord32 PrepareEncodeAndSend(int mixingFrequency);
    WebRtc_UWord32 EncodeAndSend();

private:

    int GetCodec(int idx, CodecInst& codec);

    int InsertInbandDtmfTone();
    WebRtc_Word32
            MixOrReplaceAudioWithFile(const int mixingFrequency);
    WebRtc_Word32 MixAudioWithFile(AudioFrame& audioFrame,
                                   const int mixingFrequency);
    WebRtc_Word32 GetPlayoutTimeStamp(WebRtc_UWord32& playoutTimestamp);
    void UpdateDeadOrAliveCounters(bool alive);
    WebRtc_Word32 SendPacketRaw(const void *data, int len, bool RTCP);
    WebRtc_Word32 UpdatePacketDelay(const WebRtc_UWord32 timestamp,
                                    const WebRtc_UWord16 sequenceNumber);
    void RegisterReceiveCodecsToRTPModule();
    int ApmProcessRx(AudioFrame& audioFrame);

    int SetRedPayloadType(int red_payload_type);
private:
    CriticalSectionWrapper& _fileCritSect;
    CriticalSectionWrapper& _callbackCritSect;
    WebRtc_UWord32 _instanceId;
    WebRtc_Word32 _channelId;

private:
    scoped_ptr<RtpRtcp> _rtpRtcpModule;
    AudioCodingModule& _audioCodingModule;
#ifndef WEBRTC_EXTERNAL_TRANSPORT
    WebRtc_UWord8 _numSocketThreads;
    UdpTransport& _socketTransportModule;
#endif
#ifdef WEBRTC_SRTP
    SrtpModule& _srtpModule;
#endif
    RtpDump& _rtpDumpIn;
    RtpDump& _rtpDumpOut;
private:
    AudioLevel _outputAudioLevel;
    bool _externalTransport;
    AudioFrame _audioFrame;
    WebRtc_UWord8 _audioLevel_dBov;
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
    WebRtc_UWord8* _encryptionRTPBufferPtr;
    WebRtc_UWord8* _decryptionRTPBufferPtr;
    WebRtc_UWord8* _encryptionRTCPBufferPtr;
    WebRtc_UWord8* _decryptionRTCPBufferPtr;
    WebRtc_UWord32 _timeStamp;
    WebRtc_UWord8 _sendTelephoneEventPayloadType;
    WebRtc_UWord32 _playoutTimeStampRTP;
    WebRtc_UWord32 _playoutTimeStampRTCP;
    WebRtc_UWord32 _numberOfDiscardedPackets;
private:
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
    scoped_ptr<AudioProcessing> _rtpAudioProc;
    AudioProcessing* _rxAudioProcessingModulePtr; // far end AudioProcessing
#ifdef WEBRTC_DTMF_DETECTION
    VoETelephoneEventObserver* _telephoneEventDetectionPtr;
#endif
    VoERxVadCallback* _rxVadObserverPtr;
    WebRtc_Word32 _oldVadDecision;
    WebRtc_Word32 _sendFrameType; // Send data is voice, 1-voice, 0-otherwise
    VoERTPObserver* _rtpObserverPtr;
    VoERTCPObserver* _rtcpObserverPtr;
private:
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
    bool _inbandTelephoneEventDetection;
    bool _outOfBandTelephoneEventDetecion;
    // VoeRTP_RTCP
    WebRtc_UWord8 _extraPayloadType;
    bool _insertExtraRTPPacket;
    bool _extraMarkerBit;
    WebRtc_UWord32 _lastLocalTimeStamp;
    uint32_t _lastRemoteTimeStamp;
    WebRtc_Word8 _lastPayloadType;
    bool _includeAudioLevelIndication;
    // VoENetwork
    bool _rtpPacketTimedOut;
    bool _rtpPacketTimeOutIsEnabled;
    WebRtc_UWord32 _rtpTimeOutSeconds;
    bool _connectionObserver;
    VoEConnectionObserver* _connectionObserverPtr;
    WebRtc_UWord32 _countAliveDetections;
    WebRtc_UWord32 _countDeadDetections;
    AudioFrame::SpeechType _outputSpeechType;
    // VoEVideoSync
    WebRtc_UWord32 _averageDelayMs;
    WebRtc_UWord16 _previousSequenceNumber;
    WebRtc_UWord32 _previousTimestamp;
    WebRtc_UWord16 _recPacketDelayMs;
    // VoEAudioProcessing
    bool _RxVadDetection;
    bool _rxApmIsEnabled;
    bool _rxAgcIsEnabled;
    bool _rxNsIsEnabled;
};

} // namespace voe

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CHANNEL_H
