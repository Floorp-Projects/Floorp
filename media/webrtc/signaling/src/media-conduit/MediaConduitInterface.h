/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CONDUIT_ABSTRACTION_
#define MEDIA_CONDUIT_ABSTRACTION_

#include "nsISupportsImpl.h"
#include "nsXPCOM.h"
#include "nsDOMNavigationTiming.h"
#include "mozilla/RefPtr.h"
#include "CodecConfig.h"
#include "VideoTypes.h"
#include "MediaConduitErrors.h"

#include "ImageContainer.h"

#include "webrtc/common_types.h"
namespace webrtc {
class I420VideoFrame;
}

#include <vector>

namespace mozilla {
/**
 * Abstract Interface for transporting RTP packets - audio/vidoeo
 * The consumers of this interface are responsible for passing in
 * the RTPfied media packets
 */
class TransportInterface
{
protected:
  virtual ~TransportInterface() {}

public:
  /**
   * RTP Transport Function to be implemented by concrete transport implementation
   * @param data : RTP Packet (audio/video) to be transported
   * @param len  : Length of the media packet
   * @result     : NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult SendRtpPacket(const void* data, int len) = 0;

  /**
   * RTCP Transport Function to be implemented by concrete transport implementation
   * @param data : RTCP Packet to be transported
   * @param len  : Length of the RTCP packet
   * @result     : NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult SendRtcpPacket(const void* data, int len) = 0;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TransportInterface)
};

/**
 * This class wraps image object for VideoRenderer::RenderVideoFrame()
 * callback implementation to use for rendering.
 */
class ImageHandle
{
public:
  explicit ImageHandle(layers::Image* image) : mImage(image) {}

  const RefPtr<layers::Image>& GetImage() const { return mImage; }

private:
  RefPtr<layers::Image> mImage;
};

/**
 * 1. Abstract renderer for video data
 * 2. This class acts as abstract interface between the video-engine and
 *    video-engine agnostic renderer implementation.
 * 3. Concrete implementation of this interface is responsible for
 *    processing and/or rendering the obtained raw video frame to appropriate
 *    output , say, <video>
 */
class VideoRenderer
{
protected:
  virtual ~VideoRenderer() {}

public:
  /**
   * Callback Function reportng any change in the video-frame dimensions
   * @param width:  current width of the video @ decoder
   * @param height: current height of the video @ decoder
   * @param number_of_streams: number of participating video streams
   */
  virtual void FrameSizeChange(unsigned int width,
                               unsigned int height,
                               unsigned int number_of_streams) = 0;

  /**
   * Callback Function reporting decoded I420 frame for processing.
   * @param buffer: pointer to decoded video frame
   * @param buffer_size: size of the decoded frame
   * @param time_stamp: Decoder timestamp, typically 90KHz as per RTP
   * @render_time: Wall-clock time at the decoder for synchronization
   *                purposes in milliseconds
   * @handle: opaque handle for image object of decoded video frame.
   * NOTE: If decoded video frame is passed through buffer , it is the
   * responsibility of the concrete implementations of this class to own copy
   * of the frame if needed for time longer than scope of this callback.
   * Such implementations should be quick in processing the frames and return
   * immediately.
   * On the other hand, if decoded video frame is passed through handle, the
   * implementations should keep a reference to the (ref-counted) image object
   * inside until it's no longer needed.
   */
  virtual void RenderVideoFrame(const unsigned char* buffer,
                                size_t buffer_size,
                                uint32_t time_stamp,
                                int64_t render_time,
                                const ImageHandle& handle) = 0;
  virtual void RenderVideoFrame(const unsigned char* buffer,
                                size_t buffer_size,
                                uint32_t y_stride,
                                uint32_t cbcr_stride,
                                uint32_t time_stamp,
                                int64_t render_time,
                                const ImageHandle& handle) = 0;

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
class MediaSessionConduit
{
protected:
  virtual ~MediaSessionConduit() {}

public:
  enum Type { AUDIO, VIDEO } ;

  virtual Type type() const = 0;

  /**
   * Function triggered on Incoming RTP packet from the remote
   * endpoint by the transport implementation.
   * @param data : RTP Packet (audio/video) to be processed
   * @param len  : Length of the media packet
   * Obtained packets are passed to the Media-Engine for further
   * processing , say, decoding
   */
  virtual MediaConduitErrorCode ReceivedRTPPacket(const void *data, int len) = 0;

  /**
   * Function triggered on Incoming RTCP packet from the remote
   * endpoint by the transport implementation.
   * @param data : RTCP Packet (audio/video) to be processed
   * @param len  : Length of the media packet
   * Obtained packets are passed to the Media-Engine for further
   * processing , say, decoding
   */
  virtual MediaConduitErrorCode ReceivedRTCPPacket(const void *data, int len) = 0;

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
  virtual MediaConduitErrorCode SetTransmitterTransport(RefPtr<TransportInterface> aTransport) = 0;

  /**
   * Function to attach receiver transport end-point of the Media conduit.
   * @param aTransport: Reference to the concrete teansport implementation
   * When nullptr, unsets the receiver transport endpoint.
   * Note: Multiple invocations of this call , replaces existing transport with
   * with the new one.
   * Note: This transport is used for RTCP.
   * Note: In the future, we should avoid using this for RTCP sender reports.
   */
  virtual MediaConduitErrorCode SetReceiverTransport(RefPtr<TransportInterface> aTransport) = 0;

  virtual bool SetLocalSSRC(unsigned int ssrc) = 0;
  virtual bool GetLocalSSRC(unsigned int* ssrc) = 0;
  virtual bool GetRemoteSSRC(unsigned int* ssrc) = 0;
  virtual bool SetLocalCNAME(const char* cname) = 0;

  /**
   * Functions returning stats needed by w3c stats model.
   */
  virtual bool GetVideoEncoderStats(double* framerateMean,
                                    double* framerateStdDev,
                                    double* bitrateMean,
                                    double* bitrateStdDev,
                                    uint32_t* droppedFrames) = 0;
  virtual bool GetVideoDecoderStats(double* framerateMean,
                                    double* framerateStdDev,
                                    double* bitrateMean,
                                    double* bitrateStdDev,
                                    uint32_t* discardedPackets) = 0;
  virtual bool GetAVStats(int32_t* jitterBufferDelayMs,
                          int32_t* playoutBufferDelayMs,
                          int32_t* avSyncOffsetMs) = 0;
  virtual bool GetRTPStats(unsigned int* jitterMs,
                           unsigned int* cumulativeLost) = 0;
  virtual bool GetRTCPReceiverReport(DOMHighResTimeStamp* timestamp,
                                     uint32_t* jitterMs,
                                     uint32_t* packetsReceived,
                                     uint64_t* bytesReceived,
                                     uint32_t* cumulativeLost,
                                     int32_t* rttMs) = 0;
  virtual bool GetRTCPSenderReport(DOMHighResTimeStamp* timestamp,
                                   unsigned int* packetsSent,
                                   uint64_t* bytesSent) = 0;

  virtual uint64_t CodecPluginID() = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSessionConduit)

};

// Abstract base classes for external encoder/decoder.
class CodecPluginID
{
public:
  virtual ~CodecPluginID() {}

  virtual uint64_t PluginID() const = 0;
};

class VideoEncoder : public CodecPluginID
{
public:
  virtual ~VideoEncoder() {}
};

class VideoDecoder : public CodecPluginID
{
public:
  virtual ~VideoDecoder() {}
};

/**
 * MediaSessionConduit for video
 * Refer to the comments on MediaSessionConduit above for overall
 * information
 */
class VideoSessionConduit : public MediaSessionConduit
{
public:
  /**
   * Factory function to create and initialize a Video Conduit Session
   * return: Concrete VideoSessionConduitObject or nullptr in the case
   *         of failure
   */
  static RefPtr<VideoSessionConduit> Create();

  enum FrameRequestType
  {
    FrameRequestNone,
    FrameRequestFir,
    FrameRequestPli,
    FrameRequestUnknown
  };

  VideoSessionConduit() : mFrameRequestMethod(FrameRequestNone),
                          mUsingNackBasic(false),
                          mUsingTmmbr(false) {}

  virtual ~VideoSessionConduit() {}

  virtual Type type() const { return VIDEO; }

  /**
   * Function to attach Renderer end-point of the Media-Video conduit.
   * @param aRenderer : Reference to the concrete Video renderer implementation
   * Note: Multiple invocations of this API shall remove an existing renderer
   * and attaches the new to the Conduit.
   */
  virtual MediaConduitErrorCode AttachRenderer(RefPtr<VideoRenderer> aRenderer) = 0;
  virtual void DetachRenderer() = 0;

  /**
   * Function to deliver a capture video frame for encoding and transport
   * @param video_frame: pointer to captured video-frame.
   * @param video_frame_length: size of the frame
   * @param width, height: dimensions of the frame
   * @param video_type: Type of the video frame - I420, RAW
   * @param captured_time: timestamp when the frame was captured.
   *                       if 0 timestamp is automatcally generated
   * NOTE: ConfigureSendMediaCodec() MUST be called before this function can be invoked
   *       This ensures the inserted video-frames can be transmitted by the conduit
   */
  virtual MediaConduitErrorCode SendVideoFrame(unsigned char* video_frame,
                                               unsigned int video_frame_length,
                                               unsigned short width,
                                               unsigned short height,
                                               VideoType video_type,
                                               uint64_t capture_time) = 0;
  virtual MediaConduitErrorCode SendVideoFrame(webrtc::I420VideoFrame& frame) = 0;

  virtual MediaConduitErrorCode ConfigureCodecMode(webrtc::VideoCodecMode) = 0;
  /**
   * Function to configure send codec for the video session
   * @param sendSessionConfig: CodecConfiguration
   * @result: On Success, the video engine is configured with passed in codec for send
   *          On failure, video engine transmit functionality is disabled.
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        transmission sub-system on the engine
   *
   */
  virtual MediaConduitErrorCode ConfigureSendMediaCodec(const VideoCodecConfig* sendSessionConfig) = 0;

  /**
   * Function to configurelist of receive codecs for the video session
   * @param sendSessionConfig: CodecConfiguration
   * NOTE: This API can be invoked multiple time. Invoking this API may involve restarting
   *        reception sub-system on the engine
   *
   */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
                                const std::vector<VideoCodecConfig* >& recvCodecConfigList) = 0;

  /**
   * Set an external encoder
   * @param encoder
   * @result: on success, we will use the specified encoder
   */
  virtual MediaConduitErrorCode SetExternalSendCodec(VideoCodecConfig* config,
                                                     VideoEncoder* encoder) = 0;

  /**
   * Set an external decoder
   * @param decoder
   * @result: on success, we will use the specified decoder
   */
  virtual MediaConduitErrorCode SetExternalRecvCodec(VideoCodecConfig* config,
                                                     VideoDecoder* decoder) = 0;

  /**
   * These methods allow unit tests to double-check that the
   * max-fs and max-fr related settings are as expected.
   */
  virtual unsigned short SendingWidth() = 0;

  virtual unsigned short SendingHeight() = 0;

  virtual unsigned int SendingMaxFs() = 0;

  virtual unsigned int SendingMaxFr() = 0;

  /**
    * These methods allow unit tests to double-check that the
    * rtcp-fb settings are as expected.
    */
    FrameRequestType FrameRequestMethod() const {
      return mFrameRequestMethod;
    }

    bool UsingNackBasic() const {
      return mUsingNackBasic;
    }

    bool UsingTmmbr() const {
      return mUsingTmmbr;
    }
   protected:
     /* RTCP feedback settings, for unit testing purposes */
     FrameRequestType mFrameRequestMethod;
     bool mUsingNackBasic;
     bool mUsingTmmbr;
};

/**
 * MediaSessionConduit for audio
 * Refer to the comments on MediaSessionConduit above for overall
 * information
 */
class AudioSessionConduit : public MediaSessionConduit
{
public:

   /**
    * Factory function to create and initialize an Audio Conduit Session
    * return: Concrete AudioSessionConduitObject or nullptr in the case
    *         of failure
    */
  static RefPtr<AudioSessionConduit> Create();

  virtual ~AudioSessionConduit() {}

  virtual Type type() const { return AUDIO; }


  /**
   * Function to deliver externally captured audio sample for encoding and transport
   * @param audioData [in]: Pointer to array containing a frame of audio
   * @param lengthSamples [in]: Length of audio frame in samples in multiple of 10 milliseconds
  *                             Ex: Frame length is 160, 320, 440 for 16, 32, 44 kHz sampling rates
                                    respectively.
                                    audioData[] is lengthSamples in size
                                    say, for 16kz sampling rate, audioData[] should contain 160
                                    samples of 16-bits each for a 10m audio frame.
   * @param samplingFreqHz [in]: Frequency/rate of the sampling in Hz ( 16000, 32000 ...)
   * @param capture_delay [in]:  Approx Delay from recording until it is delivered to VoiceEngine
                                 in milliseconds.
   * NOTE: ConfigureSendMediaCodec() SHOULD be called before this function can be invoked
   *       This ensures the inserted audio-samples can be transmitted by the conduit
   *
   */
  virtual MediaConduitErrorCode SendAudioFrame(const int16_t audioData[],
                                                int32_t lengthSamples,
                                                int32_t samplingFreqHz,
                                                int32_t capture_delay) = 0;

  /**
   * Function to grab a decoded audio-sample from the media engine for rendering
   * / playoutof length 10 milliseconds.
   *
   * @param speechData [in]: Pointer to a array to which a 10ms frame of audio will be copied
   * @param samplingFreqHz [in]: Frequency of the sampling for playback in Hertz (16000, 32000,..)
   * @param capture_delay [in]: Estimated Time between reading of the samples to rendering/playback
   * @param lengthSamples [out]: Will contain length of the audio frame in samples at return.
                                 Ex: A value of 160 implies 160 samples each of 16-bits was copied
                                     into speechData
   * NOTE: This function should be invoked every 10 milliseconds for the best
   *          peformance
   * NOTE: ConfigureRecvMediaCodec() SHOULD be called before this function can be invoked
   *       This ensures the decoded samples are ready for reading.
   *
   */
  virtual MediaConduitErrorCode GetAudioFrame(int16_t speechData[],
                                              int32_t samplingFreqHz,
                                              int32_t capture_delay,
                                              int& lengthSamples) = 0;

   /**
    * Function to configure send codec for the audio session
    * @param sendSessionConfig: CodecConfiguration
    * NOTE: See VideoConduit for more information
    */

  virtual MediaConduitErrorCode ConfigureSendMediaCodec(const AudioCodecConfig* sendCodecConfig) = 0;

   /**
    * Function to configure list of receive codecs for the audio session
    * @param sendSessionConfig: CodecConfiguration
    * NOTE: See VideoConduit for more information
    */
  virtual MediaConduitErrorCode ConfigureRecvMediaCodecs(
                                const std::vector<AudioCodecConfig* >& recvCodecConfigList) = 0;
   /**
    * Function to enable the audio level extension
    * @param enabled: enable extension
    * @param id: id to be used for this rtp header extension
    * NOTE: See AudioConduit for more information
    */
  virtual MediaConduitErrorCode EnableAudioLevelExtension(bool enabled, uint8_t id) = 0;

};
}
#endif
