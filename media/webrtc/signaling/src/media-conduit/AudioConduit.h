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
#include "RtpPacketQueue.h"

// Audio Engine Includes
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/channel_proxy.h"

/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */
namespace mozilla {
// Helper function

DOMHighResTimeStamp NTPtoDOMHighResTimeStamp(uint32_t ntpHigh, uint32_t ntpLow);

/**
 * Concrete class for Audio session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcAudioConduit : public AudioSessionConduit,
                           public webrtc::Transport,
                           public webrtc::RtpPacketObserver {
 public:
  // VoiceEngine defined constant for Payload Name Size.
  static const unsigned int CODEC_PLNAME_SIZE;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VoiceEngine for decoding
   */
  MediaConduitErrorCode ReceivedRTPPacket(const void* data, int len,
                                          uint32_t ssrc) override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTCP Frames to the VoiceEngine for decoding
   */
  MediaConduitErrorCode ReceivedRTCPPacket(const void* data, int len) override;

  MediaConduitErrorCode StopTransmitting() override;
  MediaConduitErrorCode StartTransmitting() override;
  MediaConduitErrorCode StopReceiving() override;
  MediaConduitErrorCode StartReceiving() override;

  MediaConduitErrorCode StopTransmittingLocked();
  MediaConduitErrorCode StartTransmittingLocked();
  MediaConduitErrorCode StopReceivingLocked();
  MediaConduitErrorCode StartReceivingLocked();

  /**
   * Function to configure send codec for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the audio engine is configured with passed in codec
   * for send On failure, audio engine transmit functionality is disabled. NOTE:
   * This API can be invoked multiple time. Invoking this API may involve
   * restarting transmission sub-system on the engine.
   */
  MediaConduitErrorCode ConfigureSendMediaCodec(
      const AudioCodecConfig* codecConfig) override;
  /**
   * Function to configure list of receive codecs for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the audio engine is configured with passed in codec
   * for send Also the playout is enabled. On failure, audio engine transmit
   * functionality is disabled. NOTE: This API can be invoked multiple time.
   * Invoking this API may involve restarting transmission sub-system on the
   * engine.
   */
  MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<AudioCodecConfig>>& codecConfigList) override;

  MediaConduitErrorCode SetLocalRTPExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& extensions) override;

  /**
   * Register External Transport to this Conduit. RTP and RTCP frames from the
   * VoiceEngine shall be passed to the registered transport for transporting
   * externally.
   */
  MediaConduitErrorCode SetTransmitterTransport(
      RefPtr<TransportInterface> aTransport) override;

  MediaConduitErrorCode SetReceiverTransport(
      RefPtr<TransportInterface> aTransport) override;

  /**
   * Function to deliver externally captured audio sample for encoding and
   * transport
   * @param audioData [in]: Pointer to array containing a frame of audio
   * @param lengthSamples [in]: Length of audio frame in samples in multiple of
   *                             10 milliseconds
   *                             Ex: Frame length is 160, 320, 440 for 16, 32,
   *                             44 kHz sampling rates respectively.
   *                             audioData[] should be of lengthSamples in
   *                             size say, for 16kz sampling rate,
   *                             audioData[] should contain 160 samples of
   *                             16-bits each for a 10m audio frame.
   * @param samplingFreqHz [in]: Frequency/rate of the sampling in Hz
   *                             ( 16000, 32000 ...)
   * @param capture_delay [in]:  Approx Delay from recording until it is
   *                             delivered to VoiceEngine in milliseconds.
   * NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can
   * be invoked. This ensures the inserted audio-samples can be transmitted by
   * the conduit
   */
  MediaConduitErrorCode SendAudioFrame(const int16_t speechData[],
                                       int32_t lengthSamples,
                                       int32_t samplingFreqHz,
                                       uint32_t channels,
                                       int32_t capture_time) override;

  /**
   * Function to grab a decoded audio-sample from the media engine for
   * rendering / playoutof length 10 milliseconds.
   *
   * @param speechData [in]: Pointer to a array to which a 10ms frame of audio
   *                         will be copied
   * @param samplingFreqHz [in]: Frequency of the sampling for playback in
   *                             Hertz (16000, 32000,..)
   * @param capture_delay [in]: Estimated Time between reading of the samples
   *                            to rendering/playback
   * @param lengthSamples [in]: Contain maximum length of speechData array.
   * @param lengthSamples [out]: Will contain length of the audio frame in
   *                             samples at return.
   *                             Ex: A value of 160 implies 160 samples each of
   *                             16-bits was copied into speechData
   * NOTE: This function should be invoked every 10 milliseconds for the best
   * peformance
   * NOTE: ConfigureRecvMediaCodec() SHOULD be called before this function can
   * be invoked
   * This ensures the decoded samples are ready for reading and playout is
   * enabled.
   */
  MediaConduitErrorCode GetAudioFrame(int16_t speechData[],
                                      int32_t samplingFreqHz,
                                      int32_t capture_delay,
                                      int& lengthSamples) override;

  /**
   * Webrtc transport implementation to send and receive RTP packet.
   * AudioConduit registers itself as ExternalTransport to the VoiceEngine
   */
  bool SendRtp(const uint8_t* data, size_t len,
               const webrtc::PacketOptions& options) override;

  /**
   * Webrtc transport implementation to send and receive RTCP packet.
   * AudioConduit registers itself as ExternalTransport to the VoiceEngine
   */
  bool SendRtcp(const uint8_t* data, size_t len) override;

  uint64_t CodecPluginID() override { return 0; }
  void SetPCHandle(const std::string& aPCHandle) override {}
  MediaConduitErrorCode DeliverPacket(const void* data, int len) override;

  void DeleteStreams() override {}

  WebrtcAudioConduit(RefPtr<WebRtcCallWrapper> aCall,
                     nsCOMPtr<nsIEventTarget> aStsThread)
      : mFakeAudioDevice(new webrtc::FakeAudioDeviceModule()),
        mTransportMonitor("WebrtcAudioConduit"),
        mTransmitterTransport(nullptr),
        mReceiverTransport(nullptr),
        mCall(aCall),
        mRecvStreamConfig(),
        mRecvStream(nullptr),
        mSendStreamConfig(
            this)  // 'this' is stored but not  dereferenced in the constructor.
        ,
        mSendStream(nullptr),
        mRecvSSRC(0),
        mEngineTransmitting(false),
        mEngineReceiving(false),
        mRecvChannel(-1),
        mSendChannel(-1),
        mDtmfEnabled(false),
        mMutex("WebrtcAudioConduit::mMutex"),
        mCaptureDelay(150),
        mStsThread(aStsThread) {}

  virtual ~WebrtcAudioConduit();

  virtual MediaConduitErrorCode Init();

  int GetRecvChannel() { return mRecvChannel; }
  webrtc::VoiceEngine* GetVoiceEngine() {
    return mCall->Call()->voice_engine();
  }

  /* Set Local SSRC list.
   * Note: Until the refactor of the VoE into the call API is complete
   *   this list should contain only a single ssrc.
   */
  bool SetLocalSSRCs(const std::vector<unsigned int>& aSSRCs) override;
  std::vector<unsigned int> GetLocalSSRCs() override;
  bool SetRemoteSSRC(unsigned int ssrc) override;
  bool UnsetRemoteSSRC(uint32_t ssrc) override { return true; }
  bool GetRemoteSSRC(unsigned int* ssrc) override;
  bool SetLocalCNAME(const char* cname) override;
  bool SetLocalMID(const std::string& mid) override;

  void SetSyncGroup(const std::string& group) override;

  bool GetSendPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) override;

  bool GetRecvPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) override;

  bool GetRTPReceiverStats(unsigned int* jitterMs,
                           unsigned int* cumulativeLost) override;
  bool GetRTCPReceiverReport(uint32_t* jitterMs, uint32_t* packetsReceived,
                             uint64_t* bytesReceived, uint32_t* cumulativeLost,
                             int32_t* rttMs) override;
  bool GetRTCPSenderReport(unsigned int* packetsSent,
                           uint64_t* bytesSent) override;

  bool SetDtmfPayloadType(unsigned char type, int freq) override;

  bool InsertDTMFTone(int channel, int eventCode, bool outOfBand, int lengthMs,
                      int attenuationDb) override;

  void GetRtpSources(const int64_t aTimeNow,
                     nsTArray<dom::RTCRtpSourceEntry>& outSources) override;

  void OnRtpPacket(const webrtc::WebRtcRTPHeader* aRtpHeader,
                   const int64_t aTimestamp, const uint32_t aJitter) override;

  // test-only: inserts fake CSRCs and audio level data
  void InsertAudioLevelForContributingSource(uint32_t aSource,
                                             int64_t aTimestamp, bool aHasLevel,
                                             uint8_t aLevel);

  bool IsSamplingFreqSupported(int freq) const override;

 protected:
  // These are protected so they can be accessed by unit tests
  std::unique_ptr<webrtc::voe::ChannelProxy> mRecvChannelProxy = nullptr;
  std::unique_ptr<webrtc::voe::ChannelProxy> mSendChannelProxy = nullptr;

 private:
  WebrtcAudioConduit(const WebrtcAudioConduit& other) = delete;
  void operator=(const WebrtcAudioConduit& other) = delete;

  // Function to convert between WebRTC and Conduit codec structures
  bool CodecConfigToWebRTCCodec(const AudioCodecConfig* codecInfo,
                                webrtc::AudioSendStream::Config& config);

  // Generate block size in sample lenght for a given sampling frequency
  unsigned int GetNum10msSamplesForFrequency(int samplingFreqHz) const;

  // Checks the codec to be applied
  MediaConduitErrorCode ValidateCodecConfig(const AudioCodecConfig* codecInfo,
                                            bool send);

  MediaConduitErrorCode CreateSendStream();
  void DeleteSendStream();
  MediaConduitErrorCode CreateRecvStream();
  void DeleteRecvStream();

  bool RecreateSendStreamIfExists();
  bool RecreateRecvStreamIfExists();

  MediaConduitErrorCode CreateChannels();
  virtual void DeleteChannels();

  UniquePtr<webrtc::FakeAudioDeviceModule> mFakeAudioDevice;
  mozilla::ReentrantMonitor mTransportMonitor;
  RefPtr<TransportInterface> mTransmitterTransport;
  RefPtr<TransportInterface> mReceiverTransport;
  ScopedCustomReleasePtr<webrtc::VoEBase> mPtrVoEBase;

  const RefPtr<WebRtcCallWrapper> mCall;
  webrtc::AudioReceiveStream::Config mRecvStreamConfig;
  webrtc::AudioReceiveStream* mRecvStream;
  webrtc::AudioSendStream::Config mSendStreamConfig;
  webrtc::AudioSendStream* mSendStream;

  // accessed on creation, and when receiving packets
  Atomic<uint32_t> mRecvSSRC;  // this can change during a stream!
  RtpPacketQueue mRtpPacketQueue;

  // engine states of our interets
  mozilla::Atomic<bool>
      mEngineTransmitting;  // If true => VoiceEngine Send-subsystem is up
  mozilla::Atomic<bool>
      mEngineReceiving;  // If true => VoiceEngine Receive-subsystem is up
                         // and playout is enabled
  // Keep track of each inserted RTP block and the time it was inserted
  // so we can estimate the clock time for a specific TimeStamp coming out
  // (for when we send data to MediaStreamTracks).  Blocks are aged out as
  // needed.
  struct Processing {
    TimeStamp mTimeStamp;
    uint32_t mRTPTimeStamp;  // RTP timestamps received
  };
  AutoTArray<Processing, 8> mProcessing;

  int mRecvChannel;
  int mSendChannel;
  bool mDtmfEnabled;

  Mutex mMutex;
  nsAutoPtr<AudioCodecConfig> mCurSendCodecConfig;

  // Current "capture" delay (really output plus input delay)
  int32_t mCaptureDelay;

  webrtc::AudioFrame mAudioFrame;  // for output pulls

  RtpSourceObserver mRtpSourceObserver;

  // Socket transport service thread. Any thread.
  const nsCOMPtr<nsIEventTarget> mStsThread;
};

}  // namespace mozilla

#endif
