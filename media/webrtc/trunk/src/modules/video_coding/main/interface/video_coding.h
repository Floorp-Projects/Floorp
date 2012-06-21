/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_INTERFACE_VIDEO_CODING_H_
#define WEBRTC_MODULES_INTERFACE_VIDEO_CODING_H_

#include "modules/interface/module.h"
#include "modules/interface/module_common_types.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"

namespace webrtc
{

class TickTimeBase;
class VideoEncoder;
class VideoDecoder;
struct CodecSpecificInfo;

class VideoCodingModule : public Module
{
public:
    enum SenderNackMode {
        kNackNone,
        kNackAll,
        kNackSelective
    };

    enum ReceiverRobustness {
        kNone,
        kHardNack,
        kSoftNack,
        kDualDecoder,
        kReferenceSelection
    };

    enum DecodeErrors {
        kNoDecodeErrors,
        kAllowDecodeErrors
    };

    static VideoCodingModule* Create(const WebRtc_Word32 id);

    static VideoCodingModule* Create(const WebRtc_Word32 id,
                                     TickTimeBase* clock);

    static void Destroy(VideoCodingModule* module);

    // Get number of supported codecs
    //
    // Return value     : Number of supported codecs
    static WebRtc_UWord8 NumberOfCodecs();

    // Get supported codec settings with using id
    //
    // Input:
    //      - listId         : Id or index of the codec to look up
    //      - codec          : Memory where the codec settings will be stored
    //
    // Return value     : VCM_OK,              on success
    //                    VCM_PARAMETER_ERROR  if codec not supported or id too high
    static WebRtc_Word32 Codec(const WebRtc_UWord8 listId, VideoCodec* codec);

    // Get supported codec settings using codec type
    //
    // Input:
    //      - codecType      : The codec type to get settings for
    //      - codec          : Memory where the codec settings will be stored
    //
    // Return value     : VCM_OK,              on success
    //                    VCM_PARAMETER_ERROR  if codec not supported
    static WebRtc_Word32 Codec(VideoCodecType codecType, VideoCodec* codec);

    /*
    *   Sender
    */

    // Any encoder-related state of VCM will be initialized to the
    // same state as when the VCM was created. This will not interrupt
    // or effect decoding functionality of VCM. VCM will lose all the
    // encoding-related settings by calling this function.
    // For instance, a send codec has to be registered again.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 InitializeSender() = 0;

    // Registers a codec to be used for encoding. Calling this
    // API multiple times overwrites any previously registered codecs.
    //
    // Input:
    //      - sendCodec      : Settings for the codec to be registered.
    //      - numberOfCores  : The number of cores the codec is allowed
    //                         to use.
    //      - maxPayloadSize : The maximum size each payload is allowed
    //                                to have. Usually MTU - overhead.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterSendCodec(const VideoCodec* sendCodec,
                                            WebRtc_UWord32 numberOfCores,
                                            WebRtc_UWord32 maxPayloadSize) = 0;

    // API to get the current send codec in use.
    //
    // Input:
    //      - currentSendCodec : Address where the sendCodec will be written.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 SendCodec(VideoCodec* currentSendCodec) const = 0;

    // API to get the current send codec type
    //
    // Return value      : Codec type, on success.
    //                     kVideoCodecUnknown, on error or if no send codec is set
    virtual VideoCodecType SendCodec() const = 0;

    // Register an external encoder object. This can not be used together with
    // external decoder callbacks.
    //
    // Input:
    //      - externalEncoder : Encoder object to be used for encoding frames inserted
    //                          with the AddVideoFrame API.
    //      - payloadType     : The payload type bound which this encoder is bound to.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                                  WebRtc_UWord8 payloadType,
                                                  bool internalSource = false) = 0;

    // API to get codec config parameters to be sent out-of-band to a receiver.
    //
    // Input:
    //      - buffer          : Memory where the codec config parameters should be written.
    //      - size            : Size of the memory available.
    //
    // Return value      : Number of bytes written, on success.
    //                     < 0,                     on error.
    virtual WebRtc_Word32 CodecConfigParameters(WebRtc_UWord8* buffer, WebRtc_Word32 size) = 0;

    // API to get currently configured encoder target bitrate in kbit/s.
    //
    // Return value      : 0,   on success.
    //                     < 0, on error.
    virtual int Bitrate(unsigned int* bitrate) const = 0;

    // API to get currently configured encoder target frame rate.
    //
    // Return value      : 0,   on success.
    //                     < 0, on error.
    virtual int FrameRate(unsigned int* framerate) const = 0;

    // Sets the parameters describing the send channel. These parameters are inputs to the
    // Media Optimization inside the VCM and also specifies the target bit rate for the
    // encoder. Bit rate used by NACK should already be compensated for by the user.
    //
    // Input:
    //      - availableBandWidth    : Band width available for the VCM in kbit/s.
    //      - lossRate              : Fractions of lost packets the past second.
    //                                (loss rate in percent = 100 * packetLoss / 255)
    //      - rtt                   : Current round-trip time in ms.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 SetChannelParameters(WebRtc_UWord32 availableBandWidth,
                                               WebRtc_UWord8 lossRate,
                                               WebRtc_UWord32 rtt) = 0;

    // Sets the parameters describing the receive channel. These parameters are inputs to the
    // Media Optimization inside the VCM.
    //
    // Input:
    //      - rtt                   : Current round-trip time in ms.
    //                                with the most amount available bandwidth in a conference
    //                                scenario
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 SetReceiveChannelParameters(WebRtc_UWord32 rtt) = 0;

    // Register a transport callback which will be called to deliver the encoded data and
    // side information.
    //
    // Input:
    //      - transport  : The callback object to register.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterTransportCallback(VCMPacketizationCallback* transport) = 0;

    // Register video output information callback which will be called to deliver information
    // about the video stream produced by the encoder, for instance the average frame rate and
    // bit rate.
    //
    // Input:
    //      - outputInformation  : The callback object to register.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterSendStatisticsCallback(
                                     VCMSendStatisticsCallback* sendStats) = 0;

    // Register a video quality settings callback which will be called when
    // frame rate/dimensions need to be updated for video quality optimization
    //
    // Input:
    //		- videoQMSettings  : The callback object to register.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error
    virtual WebRtc_Word32 RegisterVideoQMCallback(VCMQMSettingsCallback* videoQMSettings) = 0;

    // Register a video protection callback which will be called to deliver
    // the requested FEC rate and NACK status (on/off).
    //
    // Input:
    //      - protection  : The callback object to register.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterProtectionCallback(VCMProtectionCallback* protection) = 0;

    // Enable or disable a video protection method.
    //
    // Input:
    //      - videoProtection  : The method to enable or disable.
    //      - enable           : True if the method should be enabled, false if
    //                           it should be disabled.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 SetVideoProtection(VCMVideoProtection videoProtection,
                                             bool enable) = 0;

    // Add one raw video frame to the encoder. This function does all the necessary
    // processing, then decides what frame type to encode, or if the frame should be
    // dropped. If the frame should be encoded it passes the frame to the encoder
    // before it returns.
    //
    // Input:
    //      - videoFrame        : Video frame to encode.
    //      - codecSpecificInfo : Extra codec information, e.g., pre-parsed in-band signaling.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 AddVideoFrame(
        const VideoFrame& videoFrame,
        const VideoContentMetrics* contentMetrics = NULL,
        const CodecSpecificInfo* codecSpecificInfo = NULL) = 0;

    // Next frame encoded should be an intra frame (keyframe).
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 IntraFrameRequest() = 0;

    // Frame Dropper enable. Can be used to disable the frame dropping when the encoder
    // over-uses its bit rate. This API is designed to be used when the encoded frames
    // are supposed to be stored to an AVI file, or when the I420 codec is used and the
    // target bit rate shouldn't affect the frame rate.
    //
    // Input:
    //      - enable            : True to enable the setting, false to disable it.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 EnableFrameDropper(bool enable) = 0;

    // Sent frame counters
    virtual WebRtc_Word32 SentFrameCount(VCMFrameCount& frameCount) const = 0;

    /*
    *   Receiver
    */

    // The receiver state of the VCM will be initialized to the
    // same state as when the VCM was created. This will not interrupt
    // or effect the send side functionality of VCM. VCM will lose all the
    // decoding-related settings by calling this function. All frames
    // inside the jitter buffer are flushed and the delay is reset.
    // For instance, a receive codec has to be registered again.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 InitializeReceiver() = 0;

    // Register possible receive codecs, can be called multiple times for different codecs.
    // The module will automatically switch between registered codecs depending on the
    // payload type of incoming frames. The actual decoder will be created when needed.
    //
    // Input:
    //      - receiveCodec      : Settings for the codec to be registered.
    //      - numberOfCores     : Number of CPU cores that the decoder is allowed to use.
    //      - requireKeyFrame   : Set this to true if you don't want any delta frames
    //                            to be decoded until the first key frame has been decoded.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                               WebRtc_Word32 numberOfCores,
                                               bool requireKeyFrame = false) = 0;

    // Register an externally defined decoder/renderer object. Can be a decoder only or a
    // decoder coupled with a renderer. Note that RegisterReceiveCodec must be called to
    // be used for decoding incoming streams.
    //
    // Input:
    //      - externalDecoder        : The external decoder/renderer object.
    //      - payloadType            : The payload type which this decoder should be
    //                                 registered to.
    //      - internalRenderTiming   : True if the internal renderer (if any) of the decoder
    //                                 object can make sure to render at a given time in ms.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                                  WebRtc_UWord8 payloadType,
                                                  bool internalRenderTiming) = 0;

    // Register a receive callback. Will be called whenever there is a new frame ready
    // for rendering.
    //
    // Input:
    //      - receiveCallback        : The callback object to be used by the module when a
    //                                 frame is ready for rendering.
    //                                 De-register with a NULL pointer.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterReceiveCallback(VCMReceiveCallback* receiveCallback) = 0;

    // Register a receive statistics callback which will be called to deliver information
    // about the video stream received by the receiving side of the VCM, for instance the
    // average frame rate and bit rate.
    //
    // Input:
    //      - receiveStats  : The callback object to register.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterReceiveStatisticsCallback(
                               VCMReceiveStatisticsCallback* receiveStats) = 0;

    // Register a frame type request callback. This callback will be called when the
    // module needs to request specific frame types from the send side.
    //
    // Input:
    //      - frameTypeCallback      : The callback object to be used by the module when
    //                                 requesting a specific type of frame from the send side.
    //                                 De-register with a NULL pointer.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 RegisterFrameTypeCallback(
                                  VCMFrameTypeCallback* frameTypeCallback) = 0;

    // Register a frame storage callback. This callback will be called right before an
    // encoded frame is given to the decoder. Useful for recording the incoming video sequence.
    //
    // Input:
    //      - frameStorageCallback    : The callback object used by the module
    //                                  to store a received encoded frame.
    //
    // Return value     : VCM_OK, on success.
    //                    < 0,         on error.
    virtual WebRtc_Word32 RegisterFrameStorageCallback(
                             VCMFrameStorageCallback* frameStorageCallback) = 0;

    // Registers a callback which is called whenever the receive side of the VCM
    // encounters holes in the packet sequence and needs packets to be retransmitted.
    //
    // Input:
    //              - callback      : The callback to be registered in the VCM.
    //
    // Return value     : VCM_OK,     on success.
    //                    <0,              on error.
    virtual WebRtc_Word32 RegisterPacketRequestCallback(
                                        VCMPacketRequestCallback* callback) = 0;

    // Waits for the next frame in the jitter buffer to become complete
    // (waits no longer than maxWaitTimeMs), then passes it to the decoder for decoding.
    // Should be called as often as possible to get the most out of the decoder.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 Decode(WebRtc_UWord16 maxWaitTimeMs = 200) = 0;

    // Waits for the next frame in the dual jitter buffer to become complete
    // (waits no longer than maxWaitTimeMs), then passes it to the dual decoder
    // for decoding. This will never trigger a render callback. Should be
    // called frequently, and as long as it returns 1 it should be called again
    // as soon as possible.
    //
    // Return value      : 1,           if a frame was decoded
    //                     0,           if no frame was decoded
    //                     < 0,         on error.
    virtual WebRtc_Word32 DecodeDualFrame(WebRtc_UWord16 maxWaitTimeMs = 200) = 0;

    // Decodes a frame and sets an appropriate render time in ms relative to the system time.
    // Should be used in conjunction with VCMFrameStorageCallback.
    //
    // Input:
    //      - frameFromStorage      : Encoded frame read from file or received through
    //                                the VCMFrameStorageCallback callback.
    //
    // Return value:        : VCM_OK, on success
    //                        < 0,         on error
    virtual WebRtc_Word32 DecodeFromStorage(const EncodedVideoData& frameFromStorage) = 0;

    // Reset the decoder state to the initial state.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 ResetDecoder() = 0;

    // API to get the codec which is currently used for decoding by the module.
    //
    // Input:
    //      - currentReceiveCodec      : Settings for the codec to be registered.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 ReceiveCodec(VideoCodec* currentReceiveCodec) const = 0;

    // API to get the codec type currently used for decoding by the module.
    //
    // Return value      : codecy type,            on success.
    //                     kVideoCodecUnknown, on error or if no receive codec is registered
    virtual VideoCodecType ReceiveCodec() const = 0;

    // Insert a parsed packet into the receiver side of the module. Will be placed in the
    // jitter buffer waiting for the frame to become complete. Returns as soon as the packet
    // has been placed in the jitter buffer.
    //
    // Input:
    //      - incomingPayload      : Payload of the packet.
    //      - payloadLength        : Length of the payload.
    //      - rtpInfo              : The parsed header.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 IncomingPacket(const WebRtc_UWord8* incomingPayload,
                                       WebRtc_UWord32 payloadLength,
                                       const WebRtcRTPHeader& rtpInfo) = 0;

    // Minimum playout delay (Used for lip-sync). This is the minimum delay required
    // to sync with audio. Not included in  VideoCodingModule::Delay()
    // Defaults to 0 ms.
    //
    // Input:
    //      - minPlayoutDelayMs   : Additional delay in ms.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 SetMinimumPlayoutDelay(WebRtc_UWord32 minPlayoutDelayMs) = 0;

    // Set the time required by the renderer to render a frame.
    //
    // Input:
    //      - timeMS        : The time in ms required by the renderer to render a frame.
    //
    // Return value      : VCM_OK, on success.
    //                     < 0,         on error.
    virtual WebRtc_Word32 SetRenderDelay(WebRtc_UWord32 timeMS) = 0;

    // The total delay desired by the VCM. Can be less than the minimum
    // delay set with SetMinimumPlayoutDelay.
    //
    // Return value      : Total delay in ms, on success.
    //                     < 0,               on error.
    virtual WebRtc_Word32 Delay() const = 0;

    // Get the received frame counters. Keeps track of the number of each frame type
    // received since the start of the call.
    //
    // Output:
    //      - frameCount      : Struct to be filled with the number of frames received.
    //
    // Return value           : VCM_OK,        on success.
    //                          <0,                 on error.
    virtual WebRtc_Word32 ReceivedFrameCount(VCMFrameCount& frameCount) const = 0;

    // Returns the number of packets discarded by the jitter buffer due to being
    // too late. This can include duplicated packets which arrived after the
    // frame was sent to the decoder. Therefore packets which were prematurely
    // NACKed will be counted.
    virtual WebRtc_UWord32 DiscardedPackets() const = 0;


    // Robustness APIs

    // Set the sender RTX/NACK mode.
    // Input:
    //      - mode       : the selected NACK mode.
    //
    // Return value      : VCM_OK, on success;
    //                     < 0, on error.
    virtual int SetSenderNackMode(SenderNackMode mode) = 0;

    // Set the sender reference picture selection (RPS) mode.
    // Input:
    //      - enable     : true or false, for enable and disable, respectively.
    //
    // Return value      : VCM_OK, on success;
    //                     < 0, on error.
    virtual int SetSenderReferenceSelection(bool enable) = 0;

    // Set the sender forward error correction (FEC) mode.
    // Input:
    //      - enable     : true or false, for enable and disable, respectively.
    //
    // Return value      : VCM_OK, on success;
    //                     < 0, on error.
    virtual int SetSenderFEC(bool enable) = 0;

    // Set the key frame period, or disable periodic key frames (I-frames).
    // Input:
    //      - periodMs   : period in ms; <= 0 to disable periodic key frames.
    //
    // Return value      : VCM_OK, on success;
    //                     < 0, on error.
    virtual int SetSenderKeyFramePeriod(int periodMs) = 0;

    // Set the receiver robustness mode. The mode decides how the receiver
    // responds to losses in the stream. The type of counter-measure (soft or
    // hard NACK, dual decoder, RPS, etc.) is selected through the
    // robustnessMode parameter. The errorMode parameter decides if it is
    // allowed to display frames corrupted by losses. Note that not all
    // combinations of the two parameters are feasible. An error will be
    // returned for invalid combinations.
    // Input:
    //      - robustnessMode : selected robustness mode.
    //      - errorMode      : selected error mode.
    //
    // Return value      : VCM_OK, on success;
    //                     < 0, on error.
    virtual int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                          DecodeErrors errorMode) = 0;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_INTERFACE_VIDEO_CODING_H_
