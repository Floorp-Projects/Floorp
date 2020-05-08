/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CONDUIT_ABSTRACTION_
#define MEDIA_CONDUIT_ABSTRACTION_

#include "nsISupportsImpl.h"
#include "nsXPCOM.h"
#include "nsDOMNavigationTiming.h"
#include "mozilla/RefPtr.h"
#include "mozilla/RefCounted.h"
#include "mozilla/UniquePtr.h"
#include "RtpSourceObserver.h"
#include "RtcpEventObserver.h"
#include "CodecConfig.h"
#include "VideoTypes.h"
#include "MediaConduitErrors.h"
#include "RTCStatsReport.h"

#include "ImageContainer.h"

#include "webrtc/call/call.h"
#include "webrtc/common_types.h"
#include "webrtc/common_types.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"

#include <vector>
#include <set>

namespace webrtc {
class VideoFrame;
}

namespace mozilla {

enum class MediaSessionConduitLocalDirection : int { kSend, kRecv };

class VideoSessionConduit;
class AudioSessionConduit;

using RtpExtList = std::vector<webrtc::RtpExtension>;

/**
 * Abstract Interface for transporting RTP packets - audio/vidoeo
 * The consumers of this interface are responsible for passing in
 * the RTPfied media packets
 */
class TransportInterface {
 protected:
  virtual ~TransportInterface() {}

 public:
  /**
   * RTP Transport Function to be implemented by concrete transport
   * implementation
   * @param data : RTP Packet (audio/video) to be transported
   * @param len  : Length of the media packet
   * @result     : NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult SendRtpPacket(const uint8_t* data, size_t len) = 0;

  /**
   * RTCP Transport Function to be implemented by concrete transport
   * implementation
   * @param data : RTCP Packet to be transported
   * @param len  : Length of the RTCP packet
   * @result     : NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult SendRtcpPacket(const uint8_t* data, size_t len) = 0;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TransportInterface)
};

/**
 * 1. Abstract renderer for video data
 * 2. This class acts as abstract interface between the video-engine and
 *    video-engine agnostic renderer implementation.
 * 3. Concrete implementation of this interface is responsible for
 *    processing and/or rendering the obtained raw video frame to appropriate
 *    output , say, <video>
 */
class VideoRenderer {
 protected:
  virtual ~VideoRenderer() {}

 public:
  /**
   * Callback Function reportng any change in the video-frame dimensions
   * @param width:  current width of the video @ decoder
   * @param height: current height of the video @ decoder
   */
  virtual void FrameSizeChange(unsigned int width, unsigned int height) = 0;

  /**
   * Callback Function reporting decoded frame for processing.
   * @param buffer: reference to decoded video frame
   * @param buffer_size: size of the decoded frame
   * @param time_stamp: Decoder timestamp, typically 90KHz as per RTP
   * @render_time: Wall-clock time at the decoder for synchronization
   *                purposes in milliseconds
   * NOTE: If decoded video frame is passed through buffer , it is the
   * responsibility of the concrete implementations of this class to own copy
   * of the frame if needed for time longer than scope of this callback.
   * Such implementations should be quick in processing the frames and return
   * immediately.
   */
  virtual void RenderVideoFrame(const webrtc::VideoFrameBuffer& buffer,
                                uint32_t time_stamp, int64_t render_time) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoRenderer)
};

/**
 * Generic Interface for representing Audio/Video Session
 * MediaSession conduit is identified by 2 main components
 * 1. Attached Transport Interface for inbound and outbound RTP transport
 * 2. Attached Renderer Interface for rendering media data off the network
 * This class hides specifics of Media-Engine implementation from the consumers
 * of this interface.
 * Also provides codec configuration API for the media sent and recevied
 */
class MediaSessionConduit {
 protected:
  virtual ~MediaSessionConduit() {}

 public:
  enum Type { AUDIO, VIDEO };

  static std::string LocalDirectionToString(
      const MediaSessionConduitLocalDirection aDirection) {
    return aDirection == MediaSessionConduitLocalDirection::kSend ? "send"
                                                                  : "receive";
  }

  virtual Type type() const = 0;

  /**
   * Function triggered on Incoming RTP packet from the remote
   * endpoint by the transport implementation.
   * @param data : RTP Packet (audio/video) to be processed
   * @param len  : Length of the media packet
   * Obtained packets are passed to the Media-Engine for further
   * processing , say, decoding
   */
  virtual MediaConduitErrorCode ReceivedRTPPacket(
      const void* data, int len, webrtc::RTPHeader& header) = 0;

  /**
   * Function triggered on Incoming RTCP packet from the remote
   * endpoint by the transport implementation.
   * @param data : RTCP Packet (audio/video) to be processed
   * @param len  : Length of the media packet
   * Obtained packets are passed to the Media-Engine for further
   * processing , say, decoding
   */
  virtual MediaConduitErrorCode ReceivedRTCPPacket(const void* data,
                                                   int len) = 0;

  virtual Maybe<DOMHighResTimeStamp> LastRtcpReceived() const = 0;
  virtual DOMHighResTimeStamp GetNow() const = 0;

  virtual MediaConduitErrorCode StopTransmitting() = 0;
  virtual MediaConduitErrorCode StartTransmitting() = 0;
  virtual MediaConduitErrorCode StopReceiving() = 0;
  virtual MediaConduitErrorCode StartReceiving() = 0;

  /**
   * Function to attach transmitter transport end-point of the Media conduit.
   * @param aTransport: Reference to the concrete teansport implementation
   * When nullptr, unsets the transmitter transport endpoint.
   * Note: Multiple invocations of this call , replaces existing transport with
   * with the new one.
   * Note: This transport is used for RTP, and RTCP if no receiver transport is
   * set. In the future, we should ensure that RTCP sender reports use this
   * regardless of whether the receiver transport is set.
   */
  virtual MediaConduitErrorCode SetTransmitterTransport(
      RefPtr<TransportInterface> aTransport) = 0;

  /**
   * Function to attach receiver transport end-point of the Media conduit.
   * @param aTransport: Reference to the concrete teansport implementation
   * When nullptr, unsets the receiver transport endpoint.
   * Note: Multiple invocations of this call , replaces existing transport with
   * with the new one.
   * Note: This transport is used for RTCP.
   * Note: In the future, we should avoid using this for RTCP sender reports.
   */
  virtual MediaConduitErrorCode SetReceiverTransport(
      RefPtr<TransportInterface> aTransport) = 0;

  /* Sets the local SSRCs
   * @return true iff the local ssrcs == aSSRCs upon return
   * Note: this is an ordered list and {a,b,c} != {b,a,c}
   */
  virtual bool SetLocalSSRCs(const std::vector<uint32_t>& aSSRCs,
                             const std::vector<uint32_t>& aRtxSSRCs) = 0;
  virtual std::vector<uint32_t> GetLocalSSRCs() = 0;

  /**
   * Adds negotiated RTP header extensions to the the conduit. Unknown
   * extensions are ignored.
   * @param aDirection the local direction to set the RTP header extensions for
   * @param aExtensions the RTP header extensions to set
   * @return if all extensions were set it returns a success code,
   *         if an extension fails to set it may immediately return an error
   *         code
   * TODO webrtc.org 64 update: make return type void again
   */
  virtual MediaConduitErrorCode SetLocalRTPExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& aExtensions) = 0;

  virtual bool GetRemoteSSRC(uint32_t* ssrc) = 0;
  virtual bool SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) = 0;
  virtual bool UnsetRemoteSSRC(uint32_t ssrc) = 0;
  virtual bool SetLocalCNAME(const char* cname) = 0;

  virtual bool SetLocalMID(const std::string& mid) = 0;

  virtual void SetSyncGroup(const std::string& group) = 0;

  /**
   * Functions returning stats needed by w3c stats model.
   */

  virtual bool GetSendPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) = 0;

  virtual bool GetRecvPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) = 0;

  virtual bool GetRTPReceiverStats(unsigned int* jitterMs,
                                   unsigned int* cumulativeLost) = 0;
  virtual bool GetRTCPReceiverReport(uint32_t* jitterMs,
                                     uint32_t* packetsReceived,
                                     uint64_t* bytesReceived,
                                     uint32_t* cumulativeLost,
                                     Maybe<double>* aOutRttMs) = 0;
  virtual bool GetRTCPSenderReport(unsigned int* packetsSent,
                                   uint64_t* bytesSent) = 0;

  virtual uint64_t CodecPluginID() = 0;

  virtual void SetPCHandle(const std::string& aPCHandle) = 0;

  virtual MediaConduitErrorCode DeliverPacket(const void* data, int len) = 0;

  virtual void DeleteStreams() = 0;

  virtual Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() = 0;

  virtual void SetRtcpEventObserver(RtcpEventObserver* observer) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSessionConduit)
};

// Wrap the webrtc.org Call class adding mozilla add/ref support.
class WebRtcCallWrapper : public RefCounted<WebRtcCallWrapper> {
 public:
  typedef webrtc::Call::Config Config;

  static RefPtr<WebRtcCallWrapper> Create(
      const dom::RTCStatsTimestampMaker& aTimestampMaker) {
    return new WebRtcCallWrapper(aTimestampMaker);
  }

  static RefPtr<WebRtcCallWrapper> Create(UniquePtr<webrtc::Call>&& aCall) {
    return new WebRtcCallWrapper(std::move(aCall));
  }

  // Don't allow copying/assigning.
  WebRtcCallWrapper(const WebRtcCallWrapper&) = delete;
  void operator=(const WebRtcCallWrapper&) = delete;

  webrtc::Call* Call() const { return mCall.get(); }

  virtual ~WebRtcCallWrapper() {
    if (mCall->voice_engine()) {
      webrtc::VoiceEngine* voice_engine = mCall->voice_engine();
      mCall.reset(nullptr);  // Force it to release the voice engine reference
      // Delete() must be after all refs are released
      webrtc::VoiceEngine::Delete(voice_engine);
    } else {
      // Must ensure it's destroyed *before* the EventLog!
      mCall.reset(nullptr);
    }
  }

  bool UnsetRemoteSSRC(uint32_t ssrc) {
    for (auto conduit : mConduits) {
      if (!conduit->UnsetRemoteSSRC(ssrc)) {
        return false;
      }
    }

    return true;
  }

  void RegisterConduit(MediaSessionConduit* conduit) {
    mConduits.insert(conduit);
  }

  void UnregisterConduit(MediaSessionConduit* conduit) {
    mConduits.erase(conduit);
  }

  DOMHighResTimeStamp GetNow() const { return mTimestampMaker.GetNow(); }

  const dom::RTCStatsTimestampMaker& GetTimestampMaker() const {
    return mTimestampMaker;
  }

  MOZ_DECLARE_REFCOUNTED_TYPENAME(WebRtcCallWrapper)

  rtc::scoped_refptr<webrtc::AudioDecoderFactory> mDecoderFactory;

 private:
  explicit WebRtcCallWrapper(const dom::RTCStatsTimestampMaker& aTimestampMaker)
      : mTimestampMaker(aTimestampMaker) {
    auto voice_engine = webrtc::VoiceEngine::Create();
    mDecoderFactory = webrtc::CreateBuiltinAudioDecoderFactory();

    webrtc::AudioState::Config audio_state_config;
    audio_state_config.voice_engine = voice_engine;
    audio_state_config.audio_mixer = webrtc::AudioMixerImpl::Create();
    audio_state_config.audio_processing = webrtc::AudioProcessing::Create();
    mFakeAudioDeviceModule.reset(new webrtc::FakeAudioDeviceModule());
    auto voe_base = webrtc::VoEBase::GetInterface(voice_engine);
    voe_base->Init(mFakeAudioDeviceModule.get(),
                   audio_state_config.audio_processing.get(), mDecoderFactory);
    voe_base->Release();
    auto audio_state = webrtc::AudioState::Create(audio_state_config);
    webrtc::Call::Config config(&mEventLog);
    config.audio_state = audio_state;
    mCall.reset(webrtc::Call::Create(config));
  }

  explicit WebRtcCallWrapper(UniquePtr<webrtc::Call>&& aCall) {
    MOZ_ASSERT(aCall);
    mCall = std::move(aCall);
  }

  UniquePtr<webrtc::Call> mCall;
  UniquePtr<webrtc::FakeAudioDeviceModule> mFakeAudioDeviceModule;
  webrtc::RtcEventLogNullImpl mEventLog;
  // Allows conduits to know about one another, to avoid remote SSRC
  // collisions.
  std::set<MediaSessionConduit*> mConduits;
  dom::RTCStatsTimestampMaker mTimestampMaker;
};

// Abstract base classes for external encoder/decoder.
class CodecPluginID {
 public:
  virtual ~CodecPluginID() {}

  virtual uint64_t PluginID() const = 0;
};

class VideoEncoder : public CodecPluginID {
 public:
  virtual ~VideoEncoder() {}
};

class VideoDecoder : public CodecPluginID {
 public:
  virtual ~VideoDecoder() {}
};

/**
 * MediaSessionConduit for video
 * Refer to the comments on MediaSessionConduit above for overall
 * information
 */
class VideoSessionConduit : public MediaSessionConduit {
 public:
  /**
   * Factory function to create and initialize a Video Conduit Session
   * @param  webrtc::Call instance shared by paired audio and video
   *         media conduits
   * @result Concrete VideoSessionConduitObject or nullptr in the case
   *         of failure
   */
  static RefPtr<VideoSessionConduit> Create(
      RefPtr<WebRtcCallWrapper> aCall,
      nsCOMPtr<nsISerialEventTarget> aStsThread);

  enum FrameRequestType {
    FrameRequestNone,
    FrameRequestFir,
    FrameRequestPli,
    FrameRequestUnknown
  };

  VideoSessionConduit()
      : mFrameRequestMethod(FrameRequestNone),
        mUsingNackBasic(false),
        mUsingTmmbr(false),
        mUsingFEC(false) {}

  virtual ~VideoSessionConduit() {}

  Type type() const override { return VIDEO; }

  /**
   * Function to attach Renderer end-point of the Media-Video conduit.
   * @param aRenderer : Reference to the concrete Video renderer implementation
   * Note: Multiple invocations of this API shall remove an existing renderer
   * and attaches the new to the Conduit.
   */
  virtual MediaConduitErrorCode AttachRenderer(
      RefPtr<mozilla::VideoRenderer> aRenderer) = 0;
  virtual void DetachRenderer() = 0;

  virtual void DisableSsrcChanges() = 0;

  bool SetRemoteSSRC(uint32_t ssrc, uint32_t rtxSsrc) override = 0;
  bool UnsetRemoteSSRC(uint32_t ssrc) override = 0;

  /**
   * Function to deliver a capture video frame for encoding and transport.
   * If the frame's timestamp is 0, it will be automatcally generated.
   *
   * NOTE: ConfigureSendMediaCodec() must be called before this function can
   *       be invoked. This ensures the inserted video-frames can be
   *       transmitted by the conduit.
   */
  virtual MediaConduitErrorCode SendVideoFrame(
      const webrtc::VideoFrame& frame) = 0;

  virtual MediaConduitErrorCode ConfigureCodecMode(webrtc::VideoCodecMode) = 0;

  /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec
   *          for send. On failure, video engine transmit functionality is
   *          disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   *       restarting transmission sub-system on the engine
   *
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(
      const VideoCodecConfig* sendSessionConfig) = 0;

  /**
   * Function to configurelist of receive codecs for the video session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   *       restarting reception sub-system on the engine
   *
   */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<VideoCodecConfig>>& recvCodecConfigList) = 0;

  /**
   * These methods allow unit tests to double-check that the
   * rtcp-fb settings are as expected.
   */
  FrameRequestType FrameRequestMethod() const { return mFrameRequestMethod; }

  bool UsingNackBasic() const { return mUsingNackBasic; }

  bool UsingTmmbr() const { return mUsingTmmbr; }

  bool UsingFEC() const { return mUsingFEC; }

  virtual bool GetVideoEncoderStats(double* framerateMean,
                                    double* framerateStdDev,
                                    double* bitrateMean, double* bitrateStdDev,
                                    uint32_t* droppedFrames,
                                    uint32_t* framesEncoded,
                                    Maybe<uint64_t>* qpSum) = 0;
  virtual bool GetVideoDecoderStats(double* framerateMean,
                                    double* framerateStdDev,
                                    double* bitrateMean, double* bitrateStdDev,
                                    uint32_t* discardedPackets,
                                    uint32_t* framesDecoded) = 0;

  virtual void RecordTelemetry() const = 0;

 protected:
  /* RTCP feedback settings, for unit testing purposes */
  FrameRequestType mFrameRequestMethod;
  bool mUsingNackBasic;
  bool mUsingTmmbr;
  bool mUsingFEC;
};

/**
 * MediaSessionConduit for audio
 * Refer to the comments on MediaSessionConduit above for overall
 * information
 */
class AudioSessionConduit : public MediaSessionConduit {
 public:
  /**
   * Factory function to create and initialize an Audio Conduit Session
   * @param  webrtc::Call instance shared by paired audio and video
   *         media conduits
   * @result Concrete AudioSessionConduitObject or nullptr in the case
   *         of failure
   */
  static RefPtr<AudioSessionConduit> Create(
      RefPtr<WebRtcCallWrapper> aCall,
      nsCOMPtr<nsISerialEventTarget> aStsThread);

  virtual ~AudioSessionConduit() {}

  Type type() const override { return AUDIO; }

  Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() override {
    return Nothing();
  }

  /**
   * Function to deliver externally captured audio sample for encoding and
   * transport
   * @param audioData [in]: Pointer to array containing a frame of audio
   * @param lengthSamples [in]: Length of audio frame in samples in multiple of
   *                            10 milliseconds
   *                            Ex: Frame length is 160, 320, 440 for 16, 32,
   *                                44 kHz sampling rates respectively.
   *                                audioData[] is lengthSamples in size
   *                                say, for 16kz sampling rate, audioData[]
   *                                should contain 160 samples of 16-bits each
   *                                for a 10m audio frame.
   * @param samplingFreqHz [in]: Frequency/rate of the sampling in Hz ( 16000,
   *                             32000 ...)
   * @param capture_delay [in]:  Approx Delay from recording until it is
   *                             delivered to VoiceEngine in milliseconds.
   * NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can
   * be invoked. This ensures the inserted audio-samples can be transmitted by
   * the conduit.
   *
   */
  virtual MediaConduitErrorCode SendAudioFrame(const int16_t audioData[],
                                               int32_t lengthSamples,
                                               int32_t samplingFreqHz,
                                               uint32_t channels,
                                               int32_t capture_delay) = 0;

  /**
   * Function to grab a decoded audio-sample from the media engine for rendering
   * / playoutof length 10 milliseconds.
   *
   * @param speechData [in]: Pointer to a array to which a 10ms frame of audio
   *                         will be copied
   * @param samplingFreqHz [in]: Frequency of the sampling for playback in
   *                             Hertz (16000, 32000,..)
   * @param capture_delay [in]: Estimated Time between reading of the samples
   *                            to rendering/playback
   * @param numChannels [out]: Number of channels in the audio frame,
   *                           guaranteed to be non-zero.
   * @param lengthSamples [out]: Will contain length of the audio frame in
   *                             samples at return.
   *                             Ex: A value of 160 implies 160 samples each of
   *                                 16-bits was copied into speechData
   * NOTE: This function should be invoked every 10 milliseconds for the best
   * peformance
   * NOTE: ConfigureRecvMediaCodec() SHOULD be called before this function can
   * be invoked. This ensures the decoded samples are ready for reading.
   *
   */
  virtual MediaConduitErrorCode GetAudioFrame(int16_t speechData[],
                                              int32_t samplingFreqHz,
                                              int32_t capture_delay,
                                              size_t& numChannels,
                                              size_t& lengthSamples) = 0;

  /**
   * Checks if given sampling frequency is supported
   * @param freq: Sampling rate (in Hz) to check
   */
  virtual bool IsSamplingFreqSupported(int freq) const = 0;

  /**
   * Function to configure send codec for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: See VideoConduit for more information
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(
      const AudioCodecConfig* sendCodecConfig) = 0;

  /**
   * Function to configure list of receive codecs for the audio session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: See VideoConduit for more information
   */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<AudioCodecConfig>>& recvCodecConfigList) = 0;

  virtual bool SetDtmfPayloadType(unsigned char type, int freq) = 0;

  virtual bool InsertDTMFTone(int channel, int eventCode, bool outOfBand,
                              int lengthMs, int attenuationDb) = 0;

  virtual void GetRtpSources(nsTArray<dom::RTCRtpSourceEntry>& outSources) = 0;
};
}  // namespace mozilla
#endif
