/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VIDEO_SESSION_H_
#define VIDEO_SESSION_H_

#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

#include "MediaConduitInterface.h"
#include "MediaEngineWrapper.h"
#include "CodecStatistics.h"
#include "LoadManagerFactory.h"
#include "LoadManager.h"

// conflicts with #include of scoped_ptr.h
#undef FF
// Video Engine Includes
#include "webrtc/common_types.h"
#ifdef FF
#undef FF // Avoid name collision between scoped_ptr.h and nsCRTGlue.h.
#endif
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"

/** This file hosts several structures identifying different aspects
 * of a RTP Session.
 */

 using  webrtc::ViEBase;
 using  webrtc::ViENetwork;
 using  webrtc::ViECodec;
 using  webrtc::ViECapture;
 using  webrtc::ViERender;
 using  webrtc::ViEExternalCapture;
 using  webrtc::ViEExternalCodec;

namespace mozilla {

class WebrtcAudioConduit;

// Interface of external video encoder for WebRTC.
class WebrtcVideoEncoder:public VideoEncoder
                         ,public webrtc::VideoEncoder
{};

// Interface of external video decoder for WebRTC.
class WebrtcVideoDecoder:public VideoDecoder
                         ,public webrtc::VideoDecoder
{};

/**
 * Concrete class for Video session. Hooks up
 *  - media-source and target to external transport
 */
class WebrtcVideoConduit:public VideoSessionConduit
                         ,public webrtc::Transport
                         ,public webrtc::ExternalRenderer
{
public:
  //VoiceEngine defined constant for Payload Name Size.
  static const unsigned int CODEC_PLNAME_SIZE;

  /**
   * Set up A/V sync between this (incoming) VideoConduit and an audio conduit.
   */
  void SyncTo(WebrtcAudioConduit *aConduit);

  /**
   * Function to attach Renderer end-point for the Media-Video conduit.
   * @param aRenderer : Reference to the concrete Video renderer implementation
   * Note: Multiple invocations of this API shall remove an existing renderer
   * and attaches the new to the Conduit.
   */
  virtual MediaConduitErrorCode AttachRenderer(mozilla::RefPtr<VideoRenderer> aVideoRenderer) MOZ_OVERRIDE;
  virtual void DetachRenderer() MOZ_OVERRIDE;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VideoEngine for decoding
   */
  virtual MediaConduitErrorCode ReceivedRTPPacket(const void *data, int len) MOZ_OVERRIDE;

  /**
   * APIs used by the registered external transport to this Conduit to
   * feed in received RTP Frames to the VideoEngine for decoding
   */
  virtual MediaConduitErrorCode ReceivedRTCPPacket(const void *data, int len) MOZ_OVERRIDE;

  virtual MediaConduitErrorCode StopTransmitting() MOZ_OVERRIDE;
  virtual MediaConduitErrorCode StartTransmitting() MOZ_OVERRIDE;
  virtual MediaConduitErrorCode StopReceiving() MOZ_OVERRIDE;
  virtual MediaConduitErrorCode StartReceiving() MOZ_OVERRIDE;

   /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec for send
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        transmission sub-system on the engine.
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(const VideoCodecConfig* codecInfo) MOZ_OVERRIDE;

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
                               const std::vector<VideoCodecConfig* >& codecConfigList) MOZ_OVERRIDE;

  /**
   * Register Transport for this Conduit. RTP and RTCP frames from the VideoEngine
   * shall be passed to the registered transport for transporting externally.
   */
  virtual MediaConduitErrorCode SetTransmitterTransport(mozilla::RefPtr<TransportInterface> aTransport) MOZ_OVERRIDE;

  virtual MediaConduitErrorCode SetReceiverTransport(mozilla::RefPtr<TransportInterface> aTransport) MOZ_OVERRIDE;

  /**
   * Function to select and change the encoding resolution based on incoming frame size
   * and current available bandwidth.
   * @param width, height: dimensions of the frame
   */
  bool SelectSendResolution(unsigned short width,
                            unsigned short height);

  /**
   * Function to select and change the encoding frame rate based on incoming frame rate
   * and max-mbps setting.
   * @param framerate
   */
  bool SelectSendFrameRate(unsigned int framerate);

  /**
   * Function to deliver a capture video frame for encoding and transport
   * @param video_frame: pointer to captured video-frame.
   * @param video_frame_length: size of the frame
   * @param width, height: dimensions of the frame
   * @param video_type: Type of the video frame - I420, RAW
   * @param captured_time: timestamp when the frame was captured.
   *                       if 0 timestamp is automatcally generated by the engine.
   *NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can be invoked
   *       This ensures the inserted video-frames can be transmitted by the conduit
   */
  virtual MediaConduitErrorCode SendVideoFrame(unsigned char* video_frame,
                                                unsigned int video_frame_length,
                                                unsigned short width,
                                                unsigned short height,
                                                VideoType video_type,
                                                uint64_t capture_time) MOZ_OVERRIDE;

  /**
   * Set an external encoder object |encoder| to the payload type |pltype|
   * for sender side codec.
   */
  virtual MediaConduitErrorCode SetExternalSendCodec(VideoCodecConfig* config,
                                                     VideoEncoder* encoder) MOZ_OVERRIDE;

  /**
   * Set an external decoder object |decoder| to the payload type |pltype|
   * for receiver side codec.
   */
  virtual MediaConduitErrorCode SetExternalRecvCodec(VideoCodecConfig* config,
                                                     VideoDecoder* decoder) MOZ_OVERRIDE;


  /**
   * Webrtc transport implementation to send and receive RTP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoEngine
   */
  virtual int SendPacket(int channel, const void *data, int len) MOZ_OVERRIDE;

  /**
   * Webrtc transport implementation to send and receive RTCP packet.
   * VideoConduit registers itself as ExternalTransport to the VideoEngine
   */
  virtual int SendRTCPPacket(int channel, const void *data, int len) MOZ_OVERRIDE;


  /**
   * Webrtc External Renderer Implementation APIs.
   * Raw I420 Frames are delivred to the VideoConduit by the VideoEngine
   */
  virtual int FrameSizeChange(unsigned int, unsigned int, unsigned int) MOZ_OVERRIDE;

  virtual int DeliverFrame(unsigned char*, int, uint32_t , int64_t,
                           int64_t, void *handle) MOZ_OVERRIDE;

  /**
   * Does DeliverFrame() support a null buffer and non-null handle
   * (video texture)?
   * B2G support it (when using HW video decoder with graphic buffer output).
   * XXX Investigate!  Especially for Android
   */
  virtual bool IsTextureSupported() MOZ_OVERRIDE {
#ifdef WEBRTC_GONK
    return true;
#else
    return false;
#endif
  }

  virtual uint64_t CodecPluginID() MOZ_OVERRIDE;

  unsigned short SendingWidth() MOZ_OVERRIDE {
    return mSendingWidth;
  }

  unsigned short SendingHeight() MOZ_OVERRIDE {
    return mSendingHeight;
  }

  unsigned int SendingMaxFs() MOZ_OVERRIDE {
    if(mCurSendCodecConfig) {
      return mCurSendCodecConfig->mMaxFrameSize;
    }
    return 0;
  }

  unsigned int SendingMaxFr() MOZ_OVERRIDE {
    if(mCurSendCodecConfig) {
      return mCurSendCodecConfig->mMaxFrameRate;
    }
    return 0;
  }

  WebrtcVideoConduit();
  virtual ~WebrtcVideoConduit();

  MediaConduitErrorCode Init();

  int GetChannel() { return mChannel; }
  webrtc::VideoEngine* GetVideoEngine() { return mVideoEngine; }
  bool GetLocalSSRC(unsigned int* ssrc) MOZ_OVERRIDE;
  bool SetLocalSSRC(unsigned int ssrc) MOZ_OVERRIDE;
  bool GetRemoteSSRC(unsigned int* ssrc) MOZ_OVERRIDE;
  bool SetLocalCNAME(const char* cname) MOZ_OVERRIDE;
  bool GetVideoEncoderStats(double* framerateMean,
                            double* framerateStdDev,
                            double* bitrateMean,
                            double* bitrateStdDev,
                            uint32_t* droppedFrames) MOZ_OVERRIDE;
  bool GetVideoDecoderStats(double* framerateMean,
                            double* framerateStdDev,
                            double* bitrateMean,
                            double* bitrateStdDev,
                            uint32_t* discardedPackets) MOZ_OVERRIDE;
  bool GetAVStats(int32_t* jitterBufferDelayMs,
                  int32_t* playoutBufferDelayMs,
                  int32_t* avSyncOffsetMs) MOZ_OVERRIDE;
  bool GetRTPStats(unsigned int* jitterMs, unsigned int* cumulativeLost) MOZ_OVERRIDE;
  bool GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                             uint32_t* jitterMs,
                             uint32_t* packetsReceived,
                             uint64_t* bytesReceived,
                             uint32_t* cumulativeLost,
                             int32_t* rttMs) MOZ_OVERRIDE;
  bool GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                           unsigned int* packetsSent,
                           uint64_t* bytesSent) MOZ_OVERRIDE;
  uint64_t MozVideoLatencyAvg();

private:

  WebrtcVideoConduit(const WebrtcVideoConduit& other) = delete;
  void operator=(const WebrtcVideoConduit& other) = delete;

  //Local database of currently applied receive codecs
  typedef std::vector<VideoCodecConfig* > RecvCodecList;

  //Function to convert between WebRTC and Conduit codec structures
  void CodecConfigToWebRTCCodec(const VideoCodecConfig* codecInfo,
                                webrtc::VideoCodec& cinst);

  // Function to copy a codec structure to Conduit's database
  bool CopyCodecToDB(const VideoCodecConfig* codecInfo);

  // Functions to verify if the codec passed is already in
  // conduits database
  bool CheckCodecForMatch(const VideoCodecConfig* codecInfo) const;
  bool CheckCodecsForMatch(const VideoCodecConfig* curCodecConfig,
                           const VideoCodecConfig* codecInfo) const;

  //Checks the codec to be applied
  MediaConduitErrorCode ValidateCodecConfig(const VideoCodecConfig* codecInfo, bool send) const;

  //Utility function to dump recv codec database
  void DumpCodecDB() const;

  // Video Latency Test averaging filter
  void VideoLatencyUpdate(uint64_t new_sample);

  webrtc::VideoEngine* mVideoEngine;
  mozilla::ReentrantMonitor mTransportMonitor;
  mozilla::RefPtr<TransportInterface> mTransmitterTransport;
  mozilla::RefPtr<TransportInterface> mReceiverTransport;
  mozilla::RefPtr<VideoRenderer> mRenderer;

  ScopedCustomReleasePtr<webrtc::ViEBase> mPtrViEBase;
  ScopedCustomReleasePtr<webrtc::ViECapture> mPtrViECapture;
  ScopedCustomReleasePtr<webrtc::ViECodec> mPtrViECodec;
  ScopedCustomReleasePtr<webrtc::ViENetwork> mPtrViENetwork;
  ScopedCustomReleasePtr<webrtc::ViERender> mPtrViERender;
  ScopedCustomReleasePtr<webrtc::ViERTP_RTCP> mPtrRTP;
  ScopedCustomReleasePtr<webrtc::ViEExternalCodec> mPtrExtCodec;

  webrtc::ViEExternalCapture* mPtrExtCapture;

  // Engine state we are concerned with.
  mozilla::Atomic<bool> mEngineTransmitting; //If true ==> Transmit Sub-system is up and running
  mozilla::Atomic<bool> mEngineReceiving;    // if true ==> Receive Sus-sysmtem up and running

  int mChannel; // Video Channel for this conduit
  int mCapId;   // Capturer for this conduit
  RecvCodecList    mRecvCodecList;
  VideoCodecConfig* mCurSendCodecConfig;
  unsigned short mSendingWidth;
  unsigned short mSendingHeight;
  unsigned short mReceivingWidth;
  unsigned short mReceivingHeight;
  unsigned int   mSendingFramerate;
  unsigned short mNumReceivingStreams;
  bool mVideoLatencyTestEnable;
  uint64_t mVideoLatencyAvg;
  uint32_t mMinBitrate;
  uint32_t mStartBitrate;
  uint32_t mMaxBitrate;

  static const unsigned int sAlphaNum = 7;
  static const unsigned int sAlphaDen = 8;
  static const unsigned int sRoundingPadding = 1024;

  mozilla::RefPtr<WebrtcAudioConduit> mSyncedTo;

  nsAutoPtr<VideoCodecConfig> mExternalSendCodec;
  nsAutoPtr<VideoCodecConfig> mExternalRecvCodec;
  nsAutoPtr<VideoEncoder> mExternalSendCodecHandle;
  nsAutoPtr<VideoDecoder> mExternalRecvCodecHandle;

  // statistics object for video codec;
  nsAutoPtr<VideoCodecStatistics> mVideoCodecStat;

  nsAutoPtr<LoadManager> mLoadManager;
};

} // end namespace

#endif
