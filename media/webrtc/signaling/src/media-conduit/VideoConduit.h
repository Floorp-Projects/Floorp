/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VIDEO_SESSION_H_
#define VIDEO_SESSION_H_

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"

#include "MediaConduitInterface.h"
#include "MediaEngineWrapper.h"
#include "RunningStat.h"
#include "runnable_utils.h"

// conflicts with #include of scoped_ptr.h
#undef FF
// Video Engine Includes
#include "webrtc/call.h"
#include "webrtc/common_types.h"
#ifdef FF
#undef FF // Avoid name collision between scoped_ptr.h and nsCRTGlue.h.
#endif
#include "webrtc/video_decoder.h"
#include "webrtc/video_encoder.h"
#include "webrtc/config.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videoadapter.h"
#include "webrtc/media/base/videobroadcaster.h"
#include <functional>
#include <memory>
/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */

namespace mozilla {

const unsigned int kVideoMtu = 1200;
const int kQpMax = 56;

class WebrtcAudioConduit;
class nsThread;

// Interface of external video encoder for WebRTC.
class WebrtcVideoEncoder : public VideoEncoder
                         , public webrtc::VideoEncoder
{
};

// Interface of external video decoder for WebRTC.
class WebrtcVideoDecoder : public VideoDecoder
                         , public webrtc::VideoDecoder
{
};

/**
 * Concrete class for Video session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcVideoConduit : public VideoSessionConduit
                         , public webrtc::Transport
                         , public rtc::VideoSinkInterface<webrtc::VideoFrame>
                         , public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
public:

  /* Default minimum bitrate for video streams. */
  static const uint32_t kDefaultMinBitrate_bps;
  /* Default start a.k.a. target bitrate for video streams. */
  static const uint32_t kDefaultStartBitrate_bps;
  /* Default maximum bitrate for video streams. */
  static const uint32_t kDefaultMaxBitrate_bps;

  //VoiceEngine defined constant for Payload Name Size.
  static const unsigned int CODEC_PLNAME_SIZE;

  MediaConduitErrorCode
  SetLocalRTPExtensions(MediaSessionConduitLocalDirection aDirection,
                        const RtpExtList& aExtensions) override;

  /**
   * Set up A/V sync between this (incoming) VideoConduit and an audio conduit.
   */
  void SyncTo(WebrtcAudioConduit *aConduit);

  /**
   * Function to attach Renderer end-point for the Media-Video conduit.
   * @param aRenderer : Reference to the concrete mozilla Video renderer implementation
   * Note: Multiple invocations of this API shall remove an existing renderer
   * and attaches the new to the Conduit.
   */
  virtual MediaConduitErrorCode AttachRenderer(RefPtr<mozilla::VideoRenderer> aVideoRenderer) override;
  virtual void DetachRenderer() override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VideoEngine for decoding
   */
  virtual MediaConduitErrorCode ReceivedRTPPacket(const void* data, int len, uint32_t ssrc) override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VideoEngine for decoding
   */
  virtual MediaConduitErrorCode ReceivedRTCPPacket(const void* data, int len) override;

  virtual MediaConduitErrorCode StopTransmitting() override;
  virtual MediaConduitErrorCode StartTransmitting() override;
  virtual MediaConduitErrorCode StopReceiving() override;
  virtual MediaConduitErrorCode StartReceiving() override;

  /**
   * Function to configure sending codec mode for different content
   */
  virtual MediaConduitErrorCode ConfigureCodecMode(webrtc::VideoCodecMode) override;

   /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec for send
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        transmission sub-system on the engine.
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(const VideoCodecConfig* codecInfo) override;

  /**
   * Function to configure list of receive codecs for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec for send
   *          Also the playout is enabled.
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        transmission sub-system on the engine.
   */
   virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
       const std::vector<VideoCodecConfig* >& codecConfigList) override;

  /**
   * Register Transport for this Conduit. RTP and RTCP frames from the VideoEngine
   * shall be passed to the registered transport for transporting externally.
   */
  virtual MediaConduitErrorCode SetTransmitterTransport(RefPtr<TransportInterface> aTransport) override;

  virtual MediaConduitErrorCode SetReceiverTransport(RefPtr<TransportInterface> aTransport) override;

  /**
   * Function to set the encoding bitrate limits based on incoming frame size and rate
   * @param width, height: dimensions of the frame
   * @param cap: user-enforced max bitrate, or 0
   * @param aLastFramerateTenths: holds the current input framerate
   * @param aVideoStream stream to apply bitrates to
   */
  void SelectBitrates(unsigned short width,
                      unsigned short height,
                      int cap,
                      int32_t aLastFramerateTenths,
                      webrtc::VideoStream& aVideoStream);

  /**
   * Function to select and change the encoding resolution based on incoming frame size
   * and current available bandwidth.
   * @param width, height: dimensions of the frame
   * @param frame: optional frame to submit for encoding after reconfig
   */
  void SelectSendResolution(unsigned short width,
                            unsigned short height);

  /**
   * Function to select and change the encoding frame rate based on incoming frame rate
   * and max-mbps setting.
   * @param current framerate
   * @result new framerate
   */
  unsigned int SelectSendFrameRate(const VideoCodecConfig* codecConfig,
                                   unsigned int old_framerate,
                                   unsigned short sending_width,
                                   unsigned short sending_height) const;

  /**
   * Function to deliver a capture video frame for encoding and transport.
   * If the frame's timestamp is 0, it will be automatically generated.
   *
   * NOTE: ConfigureSendMediaCodec() must be called before this function can
   *       be invoked. This ensures the inserted video-frames can be
   *       transmitted by the conduit.
   */
  virtual MediaConduitErrorCode SendVideoFrame(
    const webrtc::VideoFrame& frame) override;

  /**
   * webrtc::Transport method implementation
   * ---------------------------------------
   * Webrtc transport implementation to send and receive RTP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoStream
   */
  virtual bool SendRtp(const uint8_t* packet, size_t length,
                       const webrtc::PacketOptions& options) override;

  /**
   * webrtc::Transport method implementation
   * ---------------------------------------
   * Webrtc transport implementation to send and receive RTCP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoEngine
   */
  virtual bool SendRtcp(const uint8_t* packet, size_t length) override;


  /*
   * webrtc:VideoSinkInterface implementation
   * -------------------------------
   */
  virtual void OnFrame(const webrtc::VideoFrame& frame) override;

  /*
   * webrtc:VideoSourceInterface implementation
   * -------------------------------
   */
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  void OnSinkWantsChanged(const rtc::VideoSinkWants& wants);

  virtual uint64_t CodecPluginID() override;

  virtual void SetPCHandle(const std::string& aPCHandle) override {
    mPCHandle = aPCHandle;
  }

  virtual void DeleteStreams() override;

  unsigned int SendingMaxFs() override {
    if(mCurSendCodecConfig) {
      return mCurSendCodecConfig->mEncodingConstraints.maxFs;
    }
    return 0;
  }

  unsigned int SendingMaxFr() override {
    if(mCurSendCodecConfig) {
      return mCurSendCodecConfig->mEncodingConstraints.maxFps;
    }
    return 0;
  }

  bool Denoising() const {
    return mDenoising;
  }

  uint8_t SpatialLayers() const {
    return mSpatialLayers;
  }

  uint8_t TemporalLayers() const {
    return mTemporalLayers;
  }

  webrtc::VideoCodecMode CodecMode() const {
    return mCodecMode;
  }

  WebrtcVideoConduit(RefPtr<WebRtcCallWrapper> aCall,
                     UniquePtr<cricket::VideoAdapter>&& aVideoAdapter);
  virtual ~WebrtcVideoConduit();

  MediaConduitErrorCode InitMain();
  virtual MediaConduitErrorCode Init();

  std::vector<unsigned int> GetLocalSSRCs() const override;
  bool SetLocalSSRCs(const std::vector<unsigned int> & ssrcs) override;
  bool GetRemoteSSRC(unsigned int* ssrc) override;
  bool SetRemoteSSRC(unsigned int ssrc) override;
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
                            uint32_t* framesEncoded) override;
  bool GetVideoDecoderStats(double* framerateMean,
                            double* framerateStdDev,
                            double* bitrateMean,
                            double* bitrateStdDev,
                            uint32_t* discardedPackets,
                            uint32_t* framesDecoded) override;
  bool GetAVStats(int32_t* jitterBufferDelayMs,
                  int32_t* playoutBufferDelayMs,
                  int32_t* avSyncOffsetMs) override;
  bool GetRTPStats(unsigned int* jitterMs, unsigned int* cumulativeLost) override;
  bool GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                             uint32_t* jitterMs,
                             uint32_t* packetsReceived,
                             uint64_t* bytesReceived,
                             uint32_t* cumulativeLost,
                             int32_t* rttMs) override;
  bool GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                           unsigned int* packetsSent,
                           uint64_t* bytesSent) override;
  uint64_t MozVideoLatencyAvg();

  void DisableSsrcChanges() override {
    mAllowSsrcChange = false;
  }

private:
  // Don't allow copying/assigning.
  WebrtcVideoConduit(const WebrtcVideoConduit&) = delete;
  void operator=(const WebrtcVideoConduit&) = delete;

  /** Shared statistics for receive and transmit video streams
   */
  class StreamStatistics {
  public:
    void Update(const double aFrameRate, const double aBitrate);
    /**
     * Returns gathered stream statistics
     * @param aOutFrMean: mean framerate
     * @param aOutFrStdDev: standard deviation of framerate
     * @param aOutBrMean: mean bitrate
     * @param aOutBrStdDev: standard deviation of bitrate
     */
    bool GetVideoStreamStats(double& aOutFrMean,
        double& aOutFrStdDev,
        double& aOutBrMean,
        double& aOutBrStdDev) const;
  private:
    RunningStat mFrameRate;
    RunningStat mBitrate;
  };

  /**
   * Statistics for sending streams
   */
  class SendStreamStatistics : public StreamStatistics {
  public:
    /**
     * Returns the calculate number of dropped frames
     * @param aOutDroppedFrames: the number of dropped frames
     */
    void DroppedFrames(uint32_t& aOutDroppedFrames) const;
    /**
     * Returns the number of frames that have been encoded so far
     */
    uint32_t FramesEncoded() const {
      return mFramesEncoded;
    }
    void Update(const webrtc::VideoSendStream::Stats& aStats);
    /**
     * Call once for every frame delivered for encoding
     */
    void FrameDeliveredToEncoder() { ++mFramesDeliveredToEncoder; }
  private:
    uint32_t mDroppedFrames = 0;
    uint32_t mFramesEncoded = 0;
    mozilla::Atomic<int32_t> mFramesDeliveredToEncoder;
  };

  /** Statistics for receiving streams
   */
  class ReceiveStreamStatistics : public StreamStatistics {
  public:
    /**
     * Returns the number of discarded packets
     * @param aOutDiscPackets: number of discarded packets
     */
    void DiscardedPackets(uint32_t& aOutDiscPackets) const;
   /**
    * Returns the number of frames decoded
    * @param aOutDiscPackets: number of frames decoded
    */
    void FramesDecoded(uint32_t& aFramesDecoded) const;
    void Update(const webrtc::VideoReceiveStream::Stats& aStats);
  private:
    uint32_t mDiscardedPackets = 0;
    uint32_t mFramesDecoded = 0;
  };
  /*
   * Stores encoder configuration information and produces
   * a VideoEncoderConfig from it.
   */
  class VideoStreamFactory;

  class VideoEncoderConfigBuilder {
  public:
    /**
     * Stores extended data for Simulcast Streams
     */
    class SimulcastStreamConfig {
    public:
      int jsMaxBitrate; // user-controlled max bitrate
      double jsScaleDownBy=1.0; // user-controlled downscale
    };
    void SetEncoderSpecificSettings(rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings> aSettings);
    void SetVideoStreamFactory(rtc::scoped_refptr<WebrtcVideoConduit::VideoStreamFactory> aFactory);
    void SetMinTransmitBitrateBps(int aXmitMinBps);
    void SetContentType(webrtc::VideoEncoderConfig::ContentType aContentType);
    void SetResolutionDivisor(unsigned char aDivisor);
    void SetMaxEncodings(size_t aMaxStreams);
    void AddStream(webrtc::VideoStream aStream);
    void AddStream(webrtc::VideoStream aStream,const SimulcastStreamConfig& aSimulcastConfig);
    size_t StreamCount() const;
    void ClearStreams();
    void ForEachStream(
      const std::function<void(webrtc::VideoStream&, SimulcastStreamConfig&, const size_t index)> && f);
    webrtc::VideoEncoderConfig CopyConfig() const { return mConfig.Copy(); }
    size_t NumberOfStreams() const { return mConfig.number_of_streams; }

  private:
    webrtc::VideoEncoderConfig mConfig;
    std::vector<SimulcastStreamConfig> mSimulcastStreams;
  };

  //Function to convert between WebRTC and Conduit codec structures
  void CodecConfigToWebRTCCodec(const VideoCodecConfig* codecInfo,
                                webrtc::VideoCodec& cinst);

  //Checks the codec to be applied
  MediaConduitErrorCode ValidateCodecConfig(const VideoCodecConfig* codecInfo);

  //Utility function to dump recv codec database
  void DumpCodecDB() const;

  bool CodecsDifferent(const nsTArray<UniquePtr<VideoCodecConfig>>& a,
                       const nsTArray<UniquePtr<VideoCodecConfig>>& b);

  // Factory class for VideoStreams... vie_encoder.cc will call this to reconfigure.
  // We need to give it access to the conduit to make it's decisions
  class VideoStreamFactory : public webrtc::VideoEncoderConfig::VideoStreamFactoryInterface
  {
  public:
    VideoStreamFactory(const std::string& aCodecName,
                       WebrtcVideoConduit *aConduit)
      : mCodecName(aCodecName),
        mConduit(aConduit) {}

  private:
    std::vector<webrtc::VideoStream>
      CreateEncoderStreams(int width, int height,
                           const webrtc::VideoEncoderConfig& config) override;
    std::string mCodecName;
    // this is owned by the conduit!
    WebrtcVideoConduit* mConduit;
  };

  // Video Latency Test averaging filter
  void VideoLatencyUpdate(uint64_t new_sample);

  MediaConduitErrorCode CreateSendStream();
  void DeleteSendStream();
  MediaConduitErrorCode CreateRecvStream();
  void DeleteRecvStream();

  webrtc::VideoDecoder* CreateDecoder(webrtc::VideoCodecType aType);
  webrtc::VideoEncoder* CreateEncoder(webrtc::VideoCodecType aType,
                                      bool enable_simulcast);

  MediaConduitErrorCode DeliverPacket(const void *data, int len);

  bool RequiresNewSendStream(const VideoCodecConfig& newConfig) const;

  mozilla::ReentrantMonitor mTransportMonitor;
  RefPtr<TransportInterface> mTransmitterTransport;
  RefPtr<TransportInterface> mReceiverTransport;
  RefPtr<mozilla::VideoRenderer> mRenderer;

  // Frame adapter - handle sinks that we feed data to, and handle resolution
  // changes needed for them.
  UniquePtr<cricket::VideoAdapter> mVideoAdapter;
  rtc::VideoBroadcaster mVideoBroadcaster;

  // Engine state we are concerned with.
  mozilla::Atomic<bool> mEngineTransmitting; // If true ==> Transmit Subsystem is up and running
  mozilla::Atomic<bool> mEngineReceiving;    // if true ==> Receive Subsystem up and running

  int mCapId;   // Capturer for this conduit
  //Local database of currently applied receive codecs
  nsTArray<UniquePtr<VideoCodecConfig>> mRecvCodecList;

  // protects mCurSendCodecConfig, mVideoSend/RecvStreamStats, mSend/RecvStreams, mSendPacketCounts, mRecvPacketCounts
  Mutex mCodecMutex;
  nsAutoPtr<VideoCodecConfig> mCurSendCodecConfig;
  SendStreamStatistics mSendStreamStats;
  ReceiveStreamStatistics mRecvStreamStats;
  webrtc::RtcpPacketTypeCounter mSendPacketCounts;
  webrtc::RtcpPacketTypeCounter mRecvPacketCounts;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete these:
  webrtc::VideoReceiveStream* mRecvStream;
  webrtc::VideoSendStream* mSendStream;

  unsigned short mLastWidth;
  unsigned short mLastHeight;
  unsigned short mSendingWidth;
  unsigned short mSendingHeight;
  unsigned short mReceivingWidth;
  unsigned short mReceivingHeight;
  unsigned int   mSendingFramerate;
  // scaled by *10 because Atomic<double/float> isn't supported
  mozilla::Atomic<int32_t, mozilla::Relaxed> mLastFramerateTenths;
  unsigned short mNumReceivingStreams;
  bool mVideoLatencyTestEnable;
  uint64_t mVideoLatencyAvg;
  // all in bps!
  int mMinBitrate;
  int mStartBitrate;
  int mPrefMaxBitrate;
  int mNegotiatedMaxBitrate;
  int mMinBitrateEstimate;
  bool mDenoising;
  bool mLockScaling; // for tests that care about output resolution
  uint8_t mSpatialLayers;
  uint8_t mTemporalLayers;

  rtc::VideoSinkWants mLastSinkWanted;

  static const unsigned int sAlphaNum = 7;
  static const unsigned int sAlphaDen = 8;
  static const unsigned int sRoundingPadding = 1024;

  RefPtr<WebrtcAudioConduit> mSyncedTo;

  webrtc::VideoCodecMode mCodecMode;

  // WEBRTC.ORG Call API
  RefPtr<WebRtcCallWrapper> mCall;

  webrtc::VideoSendStream::Config mSendStreamConfig;
  VideoEncoderConfigBuilder mEncoderConfig;

  webrtc::VideoReceiveStream::Config mRecvStreamConfig;

  // Are SSRC changes without signaling allowed or not
  bool mAllowSsrcChange;
  bool mWaitingForInitialSsrc;

  // accessed on creation, and when receiving packets
  uint32_t mRecvSSRC; // this can change during a stream!

  // The runnable to set the SSRC is in-flight; queue packets until it's done.
  bool mRecvSSRCSetInProgress;
  struct QueuedPacket {
    int mLen;
    uint8_t mData[1];
  };
  nsTArray<UniquePtr<QueuedPacket>> mQueuedPackets;

  // The lifetime of these codecs are maintained by the VideoConduit instance.
  // They are passed to the webrtc::VideoSendStream or VideoReceiveStream,
  // on construction.
  nsAutoPtr<webrtc::VideoEncoder> mEncoder; // only one encoder for now
  std::vector<std::unique_ptr<webrtc::VideoDecoder>> mDecoders;
  WebrtcVideoEncoder* mSendCodecPlugin;
  WebrtcVideoDecoder* mRecvCodecPlugin;

  nsCOMPtr<nsITimer> mVideoStatsTimer;

  std::string mPCHandle;
};
} // end namespace

#endif
