/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CONDUIT_ABSTRACTION_
#define MEDIA_CONDUIT_ABSTRACTION_

#include "nsISupportsImpl.h"
#include "nsXPCOM.h"
#include "mozilla/RefPtr.h"
#include "CodecConfig.h"
#include "VideoTypes.h"
#include "MediaConduitErrors.h"

#include <vector>

namespace mozilla {
/**
 * Abstract Interface for transporting RTP packets - audio/vidoeo
 * The consumers of this interface are responsible for passing in
 * the RTPfied media packets
 */
class TransportInterface
{
public:
  virtual ~TransportInterface() {}

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
 * 1. Abstract renderer for video data
 * 2. This class acts as abstract interface between the video-engine and
 *    video-engine agnostic renderer implementation.
 * 3. Concrete implementation of this interface is responsible for
 *    processing and/or rendering the obtained raw video frame to appropriate
 *    output , say, <video>
 */
class VideoRenderer
{
 public:
  virtual ~VideoRenderer() {}

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
   * NOTE: It is the responsibility of the concrete implementations of this
   * class to own copy of the frame if needed for time longer than scope of
   * this callback.
   * Such implementations should be quick in processing the frames and return
   * immediately.
   */
  virtual void RenderVideoFrame(const unsigned char* buffer,
                                unsigned int buffer_size,
                                uint32_t time_stamp,
                                int64_t render_time) = 0;

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
public:
  enum Type { AUDIO, VIDEO } ;

  virtual ~MediaSessionConduit() {}

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


  /**
   * Function to attach Transport end-point of the Media conduit.
   * @param aTransport: Reference to the concrete teansport implementation
   * Note: Multiple invocations of this call , replaces existing transport with
   * with the new one.
   */
  virtual MediaConduitErrorCode AttachTransport(RefPtr<TransportInterface> aTransport) = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaSessionConduit)

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
   * return: Concrete VideoSessionConduitObject or NULL in the case
   *         of failure
   */
  static RefPtr<VideoSessionConduit> Create();

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
    * Factory function to create and initialize a Video Conduit Session
    * return: Concrete VideoSessionConduitObject or NULL in the case
    *         of failure
    */
  static mozilla::RefPtr<AudioSessionConduit> Create(AudioSessionConduit *aOther);

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

};


}

#endif






