/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef AUDIO_SESSION_H_
#define AUDIO_SESSION_H_

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"

#include "MediaConduitInterface.h"
#include "MediaEngineWrapper.h"
#include "RtpSourceObserver.h"

// Audio Engine Includes
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/channel_proxy.h"

//Some WebRTC types for short notations
 using webrtc::VoEBase;
 using webrtc::VoENetwork;
 using webrtc::VoECodec;
 using webrtc::VoEExternalMedia;
 using webrtc::VoEAudioProcessing;
 using webrtc::VoEVideoSync;
 using webrtc::VoERTP_RTCP;
/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */
namespace mozilla {
// Helper function

DOMHighResTimeStamp
NTPtoDOMHighResTimeStamp(uint32_t ntpHigh, uint32_t ntpLow);

/**
 * Concrete class for Audio session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcAudioConduit: public AudioSessionConduit
                        , public webrtc::Transport
                        , public webrtc::RtpPacketObserver
{
public:
  //VoiceEngine defined constant for Payload Name Size.
  static const unsigned int CODEC_PLNAME_SIZE;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VoiceEngine for decoding
   */
  virtual MediaConduitErrorCode ReceivedRTPPacket(const void *data, int len, uint32_t ssrc) override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTCP Frames to the VoiceEngine for decoding
   */
  virtual MediaConduitErrorCode ReceivedRTCPPacket(const void *data, int len) override;

  virtual MediaConduitErrorCode StopTransmitting() override;
  virtual MediaConduitErrorCode StartTransmitting() override;
  virtual MediaConduitErrorCode StopReceiving() override;
  virtual MediaConduitErrorCode StartReceiving() override;

  /**
   * Function to configure send codec for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the audio engine is configured with passed in codec for send
   *          On failure, audio engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        transmission sub-system on the engine.
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(const AudioCodecConfig* codecConfig) override;
  /**
   * Function to configure list of receive codecs for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the audio engine is configured with passed in codec for send
   *          Also the playout is enabled.
   *          On failure, audio engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        transmission sub-system on the engine.
   */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
    const std::vector<AudioCodecConfig* >& codecConfigList) override;

  MediaConduitErrorCode
  SetLocalRTPExtensions(MediaSessionConduitLocalDirection aDirection,
                        const RtpExtList& extensions) override;

  /**
   * Register External Transport to this Conduit. RTP and RTCP frames from the VoiceEngine
   * shall be passed to the registered transport for transporting externally.
   */
  virtual MediaConduitErrorCode SetTransmitterTransport(RefPtr<TransportInterface> aTransport) override;

  virtual MediaConduitErrorCode SetReceiverTransport(RefPtr<TransportInterface> aTransport) override;

  /**
   * Function to deliver externally captured audio sample for encoding and transport
   * @param audioData [in]: Pointer to array containing a frame of audio
   * @param lengthSamples [in]: Length of audio frame in samples in multiple of 10 milliseconds
   *                             Ex: Frame length is 160, 320, 440 for 16, 32, 44 kHz sampling rates
                                    respectively.
                                    audioData[] should be of lengthSamples in size
                                    say, for 16kz sampling rate, audioData[] should contain 160
                                    samples of 16-bits each for a 10m audio frame.
   * @param samplingFreqHz [in]: Frequency/rate of the sampling in Hz ( 16000, 32000 ...)
   * @param capture_delay [in]:  Approx Delay from recording until it is delivered to VoiceEngine
                                 in milliseconds.
   * NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can be invoked
   *       This ensures the inserted audio-samples can be transmitted by the conduit
   *
   */
  virtual MediaConduitErrorCode SendAudioFrame(const int16_t speechData[],
                                               int32_t lengthSamples,
                                               int32_t samplingFreqHz,
                                               uint32_t channels,
                                               int32_t capture_time) override;

  /**
   * Function to grab a decoded audio-sample from the media engine for rendering
   * / playoutof length 10 milliseconds.
   *
   * @param speechData [in]: Pointer to a array to which a 10ms frame of audio will be copied
   * @param samplingFreqHz [in]: Frequency of the sampling for playback in Hertz (16000, 32000,..)
   * @param capture_delay [in]: Estimated Time between reading of the samples to rendering/playback
   * @param lengthSamples [in]: Contain maximum length of speechData array.
   * @param lengthSamples [out]: Will contain length of the audio frame in samples at return.
                                 Ex: A value of 160 implies 160 samples each of 16-bits was copied
                                     into speechData
   * NOTE: This function should be invoked every 10 milliseconds for the best
   *          peformance
   * NOTE: ConfigureRecvMediaCodec() SHOULD be called before this function can be invoked
   *       This ensures the decoded samples are ready for reading and playout is enabled.
   *
   */
   virtual MediaConduitErrorCode GetAudioFrame(int16_t speechData[],
                                              int32_t samplingFreqHz,
                                              int32_t capture_delay,
                                              int& lengthSamples) override;


  /**
   * Webrtc transport implementation to send and receive RTP packet.
   * AudioConduit registers itself as ExternalTransport to the VoiceEngine
   */
   virtual bool SendRtp(const uint8_t* data,
                        size_t len,
                        const webrtc::PacketOptions& options) override;

  /**
   * Webrtc transport implementation to send and receive RTCP packet.
   * AudioConduit registers itself as ExternalTransport to the VoiceEngine
   */
  virtual bool SendRtcp(const uint8_t *data,
                        size_t len) override;

  virtual uint64_t CodecPluginID() override { return 0; }
  virtual void SetPCHandle(const std::string& aPCHandle) override {}

  virtual void DeleteStreams() override {}

  explicit WebrtcAudioConduit():
                      mVoiceEngine(nullptr),
                      mFakeAudioDevice(new webrtc::FakeAudioDeviceModule()),
                      mTransportMonitor("WebrtcAudioConduit"),
                      mTransmitterTransport(nullptr),
                      mReceiverTransport(nullptr),
                      mEngineTransmitting(false),
                      mEngineReceiving(false),
                      mChannel(-1),
                      mDtmfEnabled(false),
                      mCodecMutex("AudioConduit codec db"),
                      mCaptureDelay(150),
                      mLastTimestamp(0),
                      mSamples(0),
                      mLastSyncLog(0)
  {
  }

  virtual ~WebrtcAudioConduit();

  MediaConduitErrorCode Init();

  int GetChannel() { return mChannel; }
  webrtc::VoiceEngine* GetVoiceEngine() { return mVoiceEngine; }

  /* Set Local SSRC list.
   * Note: Until the refactor of the VoE into the call API is complete
   *   this list should contain only a single ssrc.
   */
  bool SetLocalSSRCs(const std::vector<unsigned int>& aSSRCs) override;
  std::vector<unsigned int> GetLocalSSRCs() const override;
  bool SetRemoteSSRC(unsigned int ssrc) override
  {
    return false;
  }
  bool GetRemoteSSRC(unsigned int* ssrc) override;
  bool SetLocalCNAME(const char* cname) override;
  bool SetLocalMID(const std::string& mid) override;

  bool GetSendPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) override;

  bool GetRecvPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) override;

  bool GetVideoEncoderStats(double* framerateMean,
                            double* framerateStdDev,
                            double* bitrateMean,
                            double* bitrateStdDev,
                            uint32_t* droppedFrames,
                            uint32_t* framesEncoded) override
  {
    return false;
  }
  bool GetVideoDecoderStats(double* framerateMean,
                            double* framerateStdDev,
                            double* bitrateMean,
                            double* bitrateStdDev,
                            uint32_t* discardedPackets,
                            uint32_t* framesDecoded) override
  {
    return false;
  }
  bool GetAVStats(int32_t* jitterBufferDelayMs,
                  int32_t* playoutBufferDelayMs,
                  int32_t* avSyncOffsetMs) override;
  bool GetRTPStats(unsigned int* jitterMs, unsigned int* cumulativeLost) override;
  bool GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                             uint32_t* jitterMs,
                             uint32_t* packetsReceived,
                             uint64_t* bytesReceived,
                             uint32_t *cumulativeLost,
                             int32_t* rttMs) override;
  bool GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                           unsigned int* packetsSent,
                           uint64_t* bytesSent) override;

  bool SetDtmfPayloadType(unsigned char type, int freq) override;

  bool InsertDTMFTone(int channel, int eventCode, bool outOfBand,
                      int lengthMs, int attenuationDb) override;

  void GetRtpSources(const int64_t aTimeNow,
                     nsTArray<dom::RTCRtpSourceEntry>& outSources) override;

  void OnRtpPacket(const webrtc::WebRtcRTPHeader* aRtpHeader,
                   const int64_t aTimestamp,
                   const uint32_t aJitter) override;

  // test-only: inserts fake CSRCs and audio level data
  void InsertAudioLevelForContributingSource(uint32_t aSource,
                                             int64_t aTimestamp,
                                             bool aHasLevel,
                                             uint8_t aLevel);

  bool IsSamplingFreqSupported(int freq) const override;

private:
  WebrtcAudioConduit(const WebrtcAudioConduit& other) = delete;
  void operator=(const WebrtcAudioConduit& other) = delete;

  //Local database of currently applied receive codecs
  typedef std::vector<AudioCodecConfig* > RecvCodecList;

  //Function to convert between WebRTC and Conduit codec structures
  bool CodecConfigToWebRTCCodec(const AudioCodecConfig* codecInfo,
                                webrtc::CodecInst& cinst);

  //Generate block size in sample lenght for a given sampling frequency
  unsigned int GetNum10msSamplesForFrequency(int samplingFreqHz) const;

  // Function to copy a codec structure to Conduit's database
  bool CopyCodecToDB(const AudioCodecConfig* codecInfo);

  // Functions to verify if the codec passed is already in
  // conduits database
  bool CheckCodecForMatch(const AudioCodecConfig* codecInfo) const;
  bool CheckCodecsForMatch(const AudioCodecConfig* curCodecConfig,
                           const AudioCodecConfig* codecInfo) const;
  //Checks the codec to be applied
  MediaConduitErrorCode ValidateCodecConfig(const AudioCodecConfig* codecInfo, bool send);

  //Utility function to dump recv codec database
  void DumpCodecDB() const;

  webrtc::VoiceEngine* mVoiceEngine;
  UniquePtr<webrtc::FakeAudioDeviceModule> mFakeAudioDevice;
  mozilla::ReentrantMonitor mTransportMonitor;
  RefPtr<TransportInterface> mTransmitterTransport;
  RefPtr<TransportInterface> mReceiverTransport;
  ScopedCustomReleasePtr<webrtc::VoENetwork>   mPtrVoENetwork;
  ScopedCustomReleasePtr<webrtc::VoEBase>      mPtrVoEBase;
  ScopedCustomReleasePtr<webrtc::VoECodec>     mPtrVoECodec;
  ScopedCustomReleasePtr<webrtc::VoEExternalMedia> mPtrVoEXmedia;
  ScopedCustomReleasePtr<webrtc::VoEAudioProcessing> mPtrVoEProcessing;
  ScopedCustomReleasePtr<webrtc::VoEVideoSync> mPtrVoEVideoSync;
  ScopedCustomReleasePtr<webrtc::VoERTP_RTCP>  mPtrVoERTP_RTCP;
  ScopedCustomReleasePtr<webrtc::VoERTP_RTCP>  mPtrRTP;
  //engine states of our interets
  mozilla::Atomic<bool> mEngineTransmitting; // If true => VoiceEngine Send-subsystem is up
  mozilla::Atomic<bool> mEngineReceiving;    // If true => VoiceEngine Receive-subsystem is up
                            // and playout is enabled
  // Keep track of each inserted RTP block and the time it was inserted
  // so we can estimate the clock time for a specific TimeStamp coming out
  // (for when we send data to MediaStreamTracks).  Blocks are aged out as needed.
  struct Processing {
    TimeStamp mTimeStamp;
    uint32_t mRTPTimeStamp; // RTP timestamps received
  };
  AutoTArray<Processing,8> mProcessing;

  int mChannel;
  std::unique_ptr<webrtc::voe::ChannelProxy> mChannelProxy;
  bool mDtmfEnabled;
  RecvCodecList    mRecvCodecList;

  Mutex mCodecMutex; // protects mCurSendCodecConfig
  nsAutoPtr<AudioCodecConfig> mCurSendCodecConfig;

  // Current "capture" delay (really output plus input delay)
  int32_t mCaptureDelay;

  uint32_t mLastTimestamp;

  webrtc::AudioFrame mAudioFrame; // for output pulls

  uint32_t mSamples;
  uint32_t mLastSyncLog;

  RtpSourceObserver mRtpSourceObserver;
};

} // end namespace

#endif
