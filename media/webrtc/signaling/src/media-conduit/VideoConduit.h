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
#include "RtpPacketQueue.h"
#include "runnable_utils.h"

// conflicts with #include of scoped_ptr.h
#undef FF
// Video Engine Includes
#include "api/video_codecs/video_encoder_factory.h"
#include "webrtc/call/call.h"
#include "webrtc/common_types.h"
#ifdef FF
#  undef FF  // Avoid name collision between scoped_ptr.h and nsCRTGlue.h.
#endif
#include "webrtc/api/video_codecs/video_decoder.h"
#include "webrtc/api/video_codecs/video_encoder.h"
#include "webrtc/api/video_codecs/sdp_video_format.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videoadapter.h"
#include "webrtc/media/base/videobroadcaster.h"
#include <functional>
#include <memory>
/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */

namespace mozilla {

// Convert (SI) kilobits/sec to (SI) bits/sec
#define KBPS(kbps) kbps * 1000

const int kViEMinCodecBitrate_bps = KBPS(30);
const unsigned int kVideoMtu = 1200;
const int kQpMax = 56;

template <typename T>
T MinIgnoreZero(const T& a, const T& b);

class VideoStreamFactory;
class WebrtcAudioConduit;
class nsThread;

// Interface of external video encoder for WebRTC.
class WebrtcVideoEncoder : public VideoEncoder, public webrtc::VideoEncoder {};

// Interface of external video decoder for WebRTC.
class WebrtcVideoDecoder : public VideoDecoder, public webrtc::VideoDecoder {};

/**
 * Concrete class for Video session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcVideoConduit
    : public VideoSessionConduit,
      public webrtc::Transport,
      public webrtc::VideoEncoderFactory,
      public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  // VoiceEngine defined constant for Payload Name Size.
  static const unsigned int CODEC_PLNAME_SIZE;

  MediaConduitErrorCode SetLocalRTPExtensions(
      MediaSessionConduitLocalDirection aDirection,
      const RtpExtList& aExtensions) override;

  /**
   * Function to attach Renderer end-point for the Media-Video conduit.
   * @param aRenderer : Reference to the concrete mozilla Video renderer
   * implementation Note: Multiple invocations of this API shall remove an
   * existing renderer and attaches the new to the Conduit.
   */
  MediaConduitErrorCode AttachRenderer(
      RefPtr<mozilla::VideoRenderer> aVideoRenderer) override;
  void DetachRenderer() override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VideoEngine for decoding
   */
  MediaConduitErrorCode ReceivedRTPPacket(const void* data, int len,
                                          uint32_t ssrc) override;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTCP Frames to the VideoEngine for decoding
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
   * Function to configure sending codec mode for different content
   */
  MediaConduitErrorCode ConfigureCodecMode(webrtc::VideoCodecMode) override;

  /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec
   *          for send
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   * restarting transmission sub-system on the engine.
   */
  MediaConduitErrorCode ConfigureSendMediaCodec(
      const VideoCodecConfig* codecInfo) override;

  /**
   * Function to configure list of receive codecs for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec
   *          for send
   *          Also the playout is enabled.
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve
   * restarting transmission sub-system on the engine.
   */
  MediaConduitErrorCode ConfigureRecvMediaCodecs(
      const std::vector<UniquePtr<VideoCodecConfig>>& codecConfigList) override;

  /**
   * Register Transport for this Conduit. RTP and RTCP frames from the
   * VideoEngine shall be passed to the registered transport for transporting
   * externally.
   */
  MediaConduitErrorCode SetTransmitterTransport(
      RefPtr<TransportInterface> aTransport) override;

  MediaConduitErrorCode SetReceiverTransport(
      RefPtr<TransportInterface> aTransport) override;

  /**
   * Function to select and change the encoding resolution based on incoming
   * frame size and current available bandwidth.
   * @param width, height: dimensions of the frame
   * @param frame: optional frame to submit for encoding after reconfig
   */
  void SelectSendResolution(unsigned short width, unsigned short height);

  /**
   * Function to deliver a capture video frame for encoding and transport.
   * If the frame's timestamp is 0, it will be automatically generated.
   *
   * NOTE: ConfigureSendMediaCodec() must be called before this function can
   *       be invoked. This ensures the inserted video-frames can be
   *       transmitted by the conduit.
   */
  MediaConduitErrorCode SendVideoFrame(
      const webrtc::VideoFrame& frame) override;

  /**
   * webrtc::Transport method implementation
   * ---------------------------------------
   * Webrtc transport implementation to send and receive RTP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoStream
   */
  bool SendRtp(const uint8_t* packet, size_t length,
               const webrtc::PacketOptions& options) override;

  /**
   * webrtc::Transport method implementation
   * ---------------------------------------
   * Webrtc transport implementation to send and receive RTCP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoEngine
   */
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  /*
   * webrtc:VideoSinkInterface implementation
   * -------------------------------
   */
  void OnFrame(const webrtc::VideoFrame& frame) override;

  /*
   * webrtc:VideoSourceInterface implementation
   * -------------------------------
   */
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  void OnSinkWantsChanged(const rtc::VideoSinkWants& wants);

  uint64_t CodecPluginID() override;

  void SetPCHandle(const std::string& aPCHandle) override {
    MOZ_ASSERT(NS_IsMainThread());
    mPCHandle = aPCHandle;
  }

  void DeleteStreams() override;

  bool Denoising() const { return mDenoising; }

  uint8_t SpatialLayers() const { return mSpatialLayers; }

  uint8_t TemporalLayers() const { return mTemporalLayers; }

  webrtc::VideoCodecMode CodecMode() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mCodecMode;
  }

  WebrtcVideoConduit(RefPtr<WebRtcCallWrapper> aCall,
                     nsCOMPtr<nsIEventTarget> aStsThread);
  virtual ~WebrtcVideoConduit();

  MediaConduitErrorCode InitMain();
  virtual MediaConduitErrorCode Init();

  std::vector<unsigned int> GetLocalSSRCs() override;
  bool SetLocalSSRCs(const std::vector<unsigned int>& ssrcs) override;
  bool GetRemoteSSRC(unsigned int* ssrc) override;
  bool SetRemoteSSRC(unsigned int ssrc) override;
  bool UnsetRemoteSSRC(uint32_t ssrc) override;
  bool SetLocalCNAME(const char* cname) override;
  bool SetLocalMID(const std::string& mid) override;

  void SetSyncGroup(const std::string& group) override;

  bool GetRemoteSSRCLocked(unsigned int* ssrc);
  bool SetRemoteSSRCLocked(unsigned int ssrc);

  bool GetSendPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) override;

  bool GetRecvPacketTypeStats(
      webrtc::RtcpPacketTypeCounter* aPacketCounts) override;

  void PollStats();
  void UpdateVideoStatsTimer();
  bool GetVideoEncoderStats(double* framerateMean, double* framerateStdDev,
                            double* bitrateMean, double* bitrateStdDev,
                            uint32_t* droppedFrames, uint32_t* framesEncoded,
                            Maybe<uint64_t>* qpSum) override;
  bool GetVideoDecoderStats(double* framerateMean, double* framerateStdDev,
                            double* bitrateMean, double* bitrateStdDev,
                            uint32_t* discardedPackets,
                            uint32_t* framesDecoded) override;
  bool GetRTPReceiverStats(unsigned int* jitterMs,
                           unsigned int* cumulativeLost) override;
  bool GetRTCPReceiverReport(uint32_t* jitterMs, uint32_t* packetsReceived,
                             uint64_t* bytesReceived, uint32_t* cumulativeLost,
                             Maybe<double>* aOutRttSec) override;
  bool GetRTCPSenderReport(unsigned int* packetsSent,
                           uint64_t* bytesSent) override;
  uint64_t MozVideoLatencyAvg();

  void DisableSsrcChanges() override {
    ASSERT_ON_THREAD(mStsThread);
    mAllowSsrcChange = false;
  }

  Maybe<RefPtr<VideoSessionConduit>> AsVideoSessionConduit() override {
    return Some(RefPtr<VideoSessionConduit>(this));
  }

  void RecordTelemetry() const override {
    ASSERT_ON_THREAD(mStsThread);
    mSendStreamStats.RecordTelemetry();
    mRecvStreamStats.RecordTelemetry();
  }

 private:
  // Don't allow copying/assigning.
  WebrtcVideoConduit(const WebrtcVideoConduit&) = delete;
  void operator=(const WebrtcVideoConduit&) = delete;

  /**
   * Statistics for the Call associated with this VideoConduit.
   * Single threaded.
   */
  class CallStatistics {
   public:
    explicit CallStatistics(nsCOMPtr<nsIEventTarget> aStatsThread)
        : mStatsThread(aStatsThread) {}
    void Update(const webrtc::Call::Stats& aStats);
    Maybe<DOMHighResTimeStamp> RttSec() const;

   protected:
    const nsCOMPtr<nsIEventTarget> mStatsThread;

   private:
    Maybe<DOMHighResTimeStamp> mRttSec = Nothing();
  };

  /**
   * Shared statistics for receive and transmit video streams.
   * Single threaded.
   */
  class StreamStatistics {
   public:
    explicit StreamStatistics(nsCOMPtr<nsIEventTarget> aStatsThread)
        : mStatsThread(aStatsThread) {}
    void Update(const double aFrameRate, const double aBitrate,
                const webrtc::RtcpPacketTypeCounter& aPacketCounts);
    /**
     * Returns gathered stream statistics
     * @param aOutFrMean: mean framerate
     * @param aOutFrStdDev: standard deviation of framerate
     * @param aOutBrMean: mean bitrate
     * @param aOutBrStdDev: standard deviation of bitrate
     */
    bool GetVideoStreamStats(double& aOutFrMean, double& aOutFrStdDev,
                             double& aOutBrMean, double& aOutBrStdDev) const;

    /**
     * Accumulates video quality telemetry
     */
    void RecordTelemetry() const;
    const webrtc::RtcpPacketTypeCounter& PacketCounts() const;
    bool Active() const;
    void SetActive(bool aActive);
    virtual bool IsSend() const { return false; };

   protected:
    const nsCOMPtr<nsIEventTarget> mStatsThread;

   private:
    bool mActive = false;
    RunningStat mFrameRate;
    RunningStat mBitRate;
    webrtc::RtcpPacketTypeCounter mPacketCounts;
  };

  /**
   * Statistics for sending streams. Single threaded.
   */
  class SendStreamStatistics : public StreamStatistics {
   public:
    explicit SendStreamStatistics(nsCOMPtr<nsIEventTarget> aStatsThread)
        : StreamStatistics(
              std::forward<nsCOMPtr<nsIEventTarget>>(aStatsThread)) {}
    /**
     * Returns the calculate number of dropped frames
     */
    uint32_t DroppedFrames() const;
    /**
     * Returns the number of frames that have been encoded so far
     */
    uint32_t FramesEncoded() const;
    void Update(const webrtc::VideoSendStream::Stats& aStats,
                uint32_t aConfiguredSsrc);
    /**
     * Call once for every frame delivered for encoding
     */
    void FrameDeliveredToEncoder();

    bool SsrcFound() const;
    uint32_t JitterMs() const;
    uint32_t PacketsLost() const;
    uint64_t BytesReceived() const;
    uint32_t PacketsReceived() const;
    Maybe<uint64_t> QpSum() const;
    bool IsSend() const override { return true; };

   private:
    uint32_t mDroppedFrames = 0;
    uint32_t mFramesEncoded = 0;
    int32_t mFramesDeliveredToEncoder = 0;

    bool mSsrcFound = false;
    uint32_t mJitterMs = 0;
    uint32_t mPacketsLost = 0;
    uint64_t mBytesReceived = 0;
    uint32_t mPacketsReceived = 0;
    Maybe<uint64_t> mQpSum;
  };

  /**
   * Statistics for receiving streams. Single threaded.
   */
  class ReceiveStreamStatistics : public StreamStatistics {
   public:
    explicit ReceiveStreamStatistics(nsCOMPtr<nsIEventTarget> aStatsThread)
        : StreamStatistics(
              std::forward<nsCOMPtr<nsIEventTarget>>(aStatsThread)) {}
    uint32_t BytesSent() const;
    /**
     * Returns the number of discarded packets
     */
    uint32_t DiscardedPackets() const;
    /**
     * Returns the number of frames decoded
     */
    uint32_t FramesDecoded() const;
    uint32_t JitterMs() const;
    uint32_t PacketsLost() const;
    uint32_t PacketsSent() const;
    uint32_t Ssrc() const;
    void Update(const webrtc::VideoReceiveStream::Stats& aStats);

   private:
    uint32_t mBytesSent = 0;
    uint32_t mDiscardedPackets = 0;
    uint32_t mFramesDecoded = 0;
    uint32_t mJitterMs = 0;
    uint32_t mPacketsLost = 0;
    uint32_t mPacketsSent = 0;
    uint32_t mSsrc = 0;
  };

  // Utility function to dump recv codec database
  void DumpCodecDB() const;

  // Video Latency Test averaging filter
  void VideoLatencyUpdate(uint64_t new_sample);

  MediaConduitErrorCode CreateSendStream();
  void DeleteSendStream();
  MediaConduitErrorCode CreateRecvStream();
  void DeleteRecvStream();

  std::unique_ptr<webrtc::VideoDecoder> CreateDecoder(
      webrtc::VideoCodecType aType);
  std::unique_ptr<webrtc::VideoEncoder> CreateEncoder(
      webrtc::VideoCodecType aType);

  // webrtc::VideoEncoderFactory
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

  CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override;

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;

  MediaConduitErrorCode DeliverPacket(const void* data, int len) override;

  bool RequiresNewSendStream(const VideoCodecConfig& newConfig) const;

  mozilla::ReentrantMonitor mTransportMonitor;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mTransmitterTransport;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<TransportInterface> mReceiverTransport;

  // Accessed on any thread under mTransportMonitor.
  RefPtr<mozilla::VideoRenderer> mRenderer;

  // Accessed on any thread under mTransportMonitor.
  unsigned short mReceivingWidth = 0;

  // Accessed on any thread under mTransportMonitor.
  unsigned short mReceivingHeight = 0;

  // Socket transport service thread that runs stats queries against us. Any
  // thread.
  const nsCOMPtr<nsIEventTarget> mStsThread;

  Mutex mMutex;

  // Adapter handling resolution constraints from signaling and sinks.
  // Written only on main thread. Guarded by mMutex, except for reads on main.
  UniquePtr<cricket::VideoAdapter> mVideoAdapter;

  // Our own record of the sinks added to mVideoBroadcaster so we can support
  // dispatching updates to sinks from off-main-thread. Main thread only.
  AutoTArray<rtc::VideoSinkInterface<webrtc::VideoFrame>*, 1> mRegisteredSinks;

  // Broadcaster that distributes our frames to all registered sinks.
  // Sinks can only be added, updated and removed on main thread.
  // Frames can be passed in on any thread.
  rtc::VideoBroadcaster mVideoBroadcaster;

  // Buffer pool used for scaling frames.
  // Accessed on the frame-feeding thread only.
  webrtc::I420BufferPool mBufferPool;

  // Engine state we are concerned with. Written on main thread and read
  // anywhere.
  mozilla::Atomic<bool>
      mEngineTransmitting;  // If true ==> Transmit Subsystem is up and running
  mozilla::Atomic<bool>
      mEngineReceiving;  // if true ==> Receive Subsystem up and running

  // Local database of currently applied receive codecs. Main thread only.
  nsTArray<UniquePtr<VideoCodecConfig>> mRecvCodecList;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  nsAutoPtr<VideoCodecConfig> mCurSendCodecConfig;

  // Bookkeeping of send stream stats. Sts thread only.
  SendStreamStatistics mSendStreamStats;

  // Bookkeeping of send stream stats. Sts thread only.
  ReceiveStreamStatistics mRecvStreamStats;

  // Bookkeeping of call stats. Sts thread only.
  CallStatistics mCallStats;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete this.
  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::VideoReceiveStream* mRecvStream = nullptr;

  // Must call webrtc::Call::DestroyVideoReceive/SendStream to delete this.
  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::VideoSendStream* mSendStream = nullptr;

  // Written on the frame feeding thread.
  // Guarded by mMutex, except for reads on the frame feeding thread.
  unsigned short mLastWidth = 0;

  // Written on the frame feeding thread.
  // Guarded by mMutex, except for reads on the frame feeding thread.
  unsigned short mLastHeight = 0;

  // Accessed under mMutex.
  unsigned int mSendingFramerate;

  // Written on main thread at creation,
  // then written or read on any thread under mTransportMonitor.
  bool mVideoLatencyTestEnable = false;

  // Accessed from any thread under mTransportMonitor.
  uint64_t mVideoLatencyAvg = 0;

  // All in bps.
  // All written on main thread and guarded by mMutex, except for reads on main.
  int mMinBitrate = 0;
  int mStartBitrate = 0;
  int mPrefMaxBitrate = 0;
  int mNegotiatedMaxBitrate = 0;
  int mMinBitrateEstimate = 0;

  // Set to true to force denoising on.
  // Written at creation, then read anywhere.
  bool mDenoising = false;

  // Set to true to ignore sink wants (scaling due to bwe and cpu usage).
  // Written at creation, then read anywhere.
  bool mLockScaling = false;

  // Written at creation, then read anywhere.
  uint8_t mSpatialLayers = 1;

  // Written at creation, then read anywhere.
  uint8_t mTemporalLayers = 1;

  static const unsigned int sAlphaNum = 7;
  static const unsigned int sAlphaDen = 8;
  static const unsigned int sRoundingPadding = 1024;

  // Main thread only.
  RefPtr<WebrtcAudioConduit> mSyncedTo;

  // Main thread only.
  webrtc::VideoCodecMode mActiveCodecMode;
  webrtc::VideoCodecMode mCodecMode;

  // WEBRTC.ORG Call API
  // Const so can be accessed on any thread. Most methods are called on
  // main thread, though Receiver() is called on STS. This seems fine.
  const RefPtr<WebRtcCallWrapper> mCall;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  webrtc::VideoSendStream::Config mSendStreamConfig;

  // Main thread only.
  webrtc::VideoEncoderConfig mEncoderConfig;

  // Written only on main thread. Guarded by mMutex, except for reads on main.
  // Calls can happen on any thread.
  RefPtr<rtc::RefCountedObject<VideoStreamFactory>> mVideoStreamFactory;

  // Main thread only.
  webrtc::VideoReceiveStream::Config mRecvStreamConfig;

  // Are SSRC changes without signaling allowed or not
  // Accessed only on mStsThread.
  bool mAllowSsrcChange = true;

  // Accessed only on mStsThread.
  bool mWaitingForInitialSsrc = true;

  // Accessed during configuration/signaling (main),
  // and when receiving packets (sts).
  Atomic<uint32_t> mRecvSSRC;  // this can change during a stream!

  // Accessed only on mStsThread.
  RtpPacketQueue mRtpPacketQueue;

  // The lifetime of these codecs are maintained by the VideoConduit instance.
  // They are passed to the webrtc::VideoSendStream or VideoReceiveStream,
  // on construction.
  std::unique_ptr<webrtc::VideoEncoder> mEncoder;  // only one encoder for now
  std::vector<std::unique_ptr<webrtc::VideoDecoder>> mDecoders;
  // Main thread only
  uint64_t mSendCodecPluginID = 0;
  // Main thread only
  uint64_t mRecvCodecPluginID = 0;

  // Timer that updates video stats periodically. Main thread only.
  nsCOMPtr<nsITimer> mVideoStatsTimer;
  // True if mVideoStatsTimer is running. Main thread only.
  bool mVideoStatsTimerActive = false;

  // Main thread only
  std::string mPCHandle;
};
}  // namespace mozilla

#endif
