/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_

#include "modules/video_coding/main/interface/video_coding.h"

#include <vector>

#include "modules/video_coding/main/source/codec_database.h"
#include "modules/video_coding/main/source/frame_buffer.h"
#include "modules/video_coding/main/source/generic_decoder.h"
#include "modules/video_coding/main/source/generic_encoder.h"
#include "modules/video_coding/main/source/jitter_buffer.h"
#include "modules/video_coding/main/source/media_optimization.h"
#include "modules/video_coding/main/source/receiver.h"
#include "modules/video_coding/main/source/tick_time_base.h"
#include "modules/video_coding/main/source/timing.h"
#include "system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc
{

class VCMProcessTimer
{
public:
    VCMProcessTimer(WebRtc_UWord32 periodMs, TickTimeBase* clock)
        : _clock(clock),
          _periodMs(periodMs),
          _latestMs(_clock->MillisecondTimestamp()) {}
    WebRtc_UWord32 Period() const;
    WebRtc_UWord32 TimeUntilProcess() const;
    void Processed();

private:
    TickTimeBase*         _clock;
    WebRtc_UWord32        _periodMs;
    WebRtc_Word64         _latestMs;
};

enum VCMKeyRequestMode
{
    kKeyOnError,    // Normal mode, request key frames on decoder error
    kKeyOnKeyLoss,  // Request key frames on decoder error and on packet loss
                    // in key frames.
    kKeyOnLoss,     // Request key frames on decoder error and on packet loss
                    // in any frame
};

class VideoCodingModuleImpl : public VideoCodingModule
{
public:
    VideoCodingModuleImpl(const WebRtc_Word32 id,
                          TickTimeBase* clock,
                          bool delete_clock_on_destroy);

    virtual ~VideoCodingModuleImpl();

    WebRtc_Word32 Id() const;

    //  Change the unique identifier of this object
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    // Returns the number of milliseconds until the module want a worker thread
    // to call Process
    virtual WebRtc_Word32 TimeUntilNextProcess();

    virtual WebRtc_Word32 Process();

    /*
    *   Sender
    */

    // Initialize send codec
    virtual WebRtc_Word32 InitializeSender();

    // Register the send codec to be used.
    virtual WebRtc_Word32 RegisterSendCodec(const VideoCodec* sendCodec,
                                            WebRtc_UWord32 numberOfCores,
                                            WebRtc_UWord32 maxPayloadSize);

    // Get current send codec
    virtual WebRtc_Word32 SendCodec(VideoCodec* currentSendCodec) const;

    // Get current send codec type
    virtual VideoCodecType SendCodec() const;

    // Register an external encoder object.
    virtual WebRtc_Word32 RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                                  WebRtc_UWord8 payloadType,
                                                  bool internalSource = false);

    // Get codec config parameters
    virtual WebRtc_Word32 CodecConfigParameters(WebRtc_UWord8* buffer,
                                                WebRtc_Word32 size);

    // Get encode bitrate
    virtual int Bitrate(unsigned int* bitrate) const;

    // Get encode frame rate
    virtual int FrameRate(unsigned int* framerate) const;

    // Set channel parameters
    virtual WebRtc_Word32 SetChannelParameters(
        WebRtc_UWord32 availableBandWidth,
        WebRtc_UWord8 lossRate,
        WebRtc_UWord32 rtt);

    // Set recieve channel parameters
    virtual WebRtc_Word32 SetReceiveChannelParameters(WebRtc_UWord32 rtt);

    // Register a transport callback which will be called to deliver the
    // encoded buffers
    virtual WebRtc_Word32 RegisterTransportCallback(
        VCMPacketizationCallback* transport);

    // Register a send statistics callback which will be called to deliver
    // information about the video stream produced by the encoder,
    // for instance the average frame rate and bit rate.
    virtual WebRtc_Word32 RegisterSendStatisticsCallback(
        VCMSendStatisticsCallback* sendStats);

    // Register a video quality settings callback which will be called when
    // frame rate/dimensions need to be updated for video quality optimization
    virtual WebRtc_Word32 RegisterVideoQMCallback(
        VCMQMSettingsCallback* videoQMSettings);

    // Register a video protection callback which will be called to deliver
    // the requested FEC rate and NACK status (on/off).
    virtual WebRtc_Word32 RegisterProtectionCallback(
        VCMProtectionCallback* protection);

    // Enable or disable a video protection method.
   virtual WebRtc_Word32 SetVideoProtection(VCMVideoProtection videoProtection,
                                            bool enable);

    // Add one raw video frame to the encoder, blocking.
    virtual WebRtc_Word32 AddVideoFrame(
        const I420VideoFrame& videoFrame,
        const VideoContentMetrics* _contentMetrics = NULL,
        const CodecSpecificInfo* codecSpecificInfo = NULL);

    virtual WebRtc_Word32 IntraFrameRequest(int stream_index);

    //Enable frame dropper
    virtual WebRtc_Word32 EnableFrameDropper(bool enable);

    // Sent frame counters
    virtual WebRtc_Word32 SentFrameCount(VCMFrameCount& frameCount) const;

    /*
    *   Receiver
    */

    // Initialize receiver, resets codec database etc
    virtual WebRtc_Word32 InitializeReceiver();

    // Register possible reveive codecs, can be called multiple times
    virtual WebRtc_Word32 RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                               WebRtc_Word32 numberOfCores,
                                               bool requireKeyFrame = false);

    // Register an externally defined decoder/render object.
    // Can be a decoder only or a decoder coupled with a renderer.
    virtual WebRtc_Word32 RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                                  WebRtc_UWord8 payloadType,
                                                  bool internalRenderTiming);

    // Register a receive callback. Will be called whenever there are a new
    // frame ready for rendering.
    virtual WebRtc_Word32 RegisterReceiveCallback(
        VCMReceiveCallback* receiveCallback);

    // Register a receive statistics callback which will be called to deliver
    // information about the video stream received by the receiving side of the
    // VCM, for instance the average frame rate and bit rate.
    virtual WebRtc_Word32 RegisterReceiveStatisticsCallback(
        VCMReceiveStatisticsCallback* receiveStats);

    // Register a frame type request callback.
    virtual WebRtc_Word32 RegisterFrameTypeCallback(
        VCMFrameTypeCallback* frameTypeCallback);

    // Register a frame storage callback.
    virtual WebRtc_Word32 RegisterFrameStorageCallback(
        VCMFrameStorageCallback* frameStorageCallback);

    // Nack callback
    virtual WebRtc_Word32 RegisterPacketRequestCallback(
        VCMPacketRequestCallback* callback);

    // Decode next frame, blocks for a maximum of maxWaitTimeMs milliseconds.
    // Should be called as often as possible to get the most out of the decoder.
    virtual WebRtc_Word32 Decode(WebRtc_UWord16 maxWaitTimeMs = 200);

    // Decode next dual frame, blocks for a maximum of maxWaitTimeMs
    // milliseconds.
    virtual WebRtc_Word32 DecodeDualFrame(WebRtc_UWord16 maxWaitTimeMs = 200);

    // Reset the decoder state
    virtual WebRtc_Word32 ResetDecoder();

    // Get current received codec
    virtual WebRtc_Word32 ReceiveCodec(VideoCodec* currentReceiveCodec) const;

    // Get current received codec type
    virtual VideoCodecType ReceiveCodec() const;

    // Incoming packet from network parsed and ready for decode, non blocking.
    virtual WebRtc_Word32 IncomingPacket(const WebRtc_UWord8* incomingPayload,
                                         WebRtc_UWord32 payloadLength,
                                         const WebRtcRTPHeader& rtpInfo);

    // A part of an encoded frame to be decoded.
    // Used in conjunction with VCMFrameStorageCallback.
    virtual WebRtc_Word32 DecodeFromStorage(
        const EncodedVideoData& frameFromStorage);

    // Minimum playout delay (Used for lip-sync). This is the minimum delay
    // required to sync with audio. Not included in  VideoCodingModule::Delay()
    // Defaults to 0 ms.
    virtual WebRtc_Word32 SetMinimumPlayoutDelay(
        WebRtc_UWord32 minPlayoutDelayMs);

    // The estimated delay caused by rendering
    virtual WebRtc_Word32 SetRenderDelay(WebRtc_UWord32 timeMS);

    // Current delay
    virtual WebRtc_Word32 Delay() const;

    // Received frame counters
    virtual WebRtc_Word32 ReceivedFrameCount(VCMFrameCount& frameCount) const;

    // Returns the number of packets discarded by the jitter buffer.
    virtual WebRtc_UWord32 DiscardedPackets() const;


    // Robustness APIs

    // Set the sender RTX/NACK mode.
    virtual int SetSenderNackMode(SenderNackMode mode);

    // Set the sender reference picture selection (RPS) mode.
    virtual int SetSenderReferenceSelection(bool enable);

    // Set the sender forward error correction (FEC) mode.
    virtual int SetSenderFEC(bool enable);

    // Set the key frame period, or disable periodic key frames (I-frames).
    virtual int SetSenderKeyFramePeriod(int periodMs);

    // Set the receiver robustness mode.
    virtual int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode,
                                          DecodeErrors errorMode);
    // Enables recording of debugging information.
    virtual int StartDebugRecording(const char* file_name_utf8);

    // Disables recording of debugging information.
    virtual int StopDebugRecording();

protected:
    WebRtc_Word32 Decode(const webrtc::VCMEncodedFrame& frame);
    WebRtc_Word32 RequestKeyFrame();
    WebRtc_Word32 RequestSliceLossIndication(
        const WebRtc_UWord64 pictureID) const;
    WebRtc_Word32 NackList(WebRtc_UWord16* nackList, WebRtc_UWord16& size);

private:
    WebRtc_Word32                       _id;
    TickTimeBase*                       clock_;
    bool                                delete_clock_on_destroy_;
    CriticalSectionWrapper*             _receiveCritSect;
    bool                                _receiverInited;
    VCMTiming                           _timing;
    VCMTiming                           _dualTiming;
    VCMReceiver                         _receiver;
    VCMReceiver                         _dualReceiver;
    VCMDecodedFrameCallback             _decodedFrameCallback;
    VCMDecodedFrameCallback             _dualDecodedFrameCallback;
    VCMFrameTypeCallback*               _frameTypeCallback;
    VCMFrameStorageCallback*            _frameStorageCallback;
    VCMReceiveStatisticsCallback*       _receiveStatsCallback;
    VCMPacketRequestCallback*           _packetRequestCallback;
    VCMGenericDecoder*                  _decoder;
    VCMGenericDecoder*                  _dualDecoder;
#ifdef DEBUG_DECODER_BIT_STREAM
    FILE*                               _bitStreamBeforeDecoder;
#endif
    VCMFrameBuffer                      _frameFromFile;
    VCMKeyRequestMode                   _keyRequestMode;
    bool                                _scheduleKeyRequest;

    CriticalSectionWrapper*             _sendCritSect; // Critical section for send side
    VCMGenericEncoder*                  _encoder;
    VCMEncodedFrameCallback             _encodedFrameCallback;
    std::vector<FrameType>              _nextFrameTypes;
    VCMMediaOptimization                _mediaOpt;
    VideoCodecType                      _sendCodecType;
    VCMSendStatisticsCallback*          _sendStatsCallback;
    FILE*                               _encoderInputFile;
    VCMCodecDataBase                    _codecDataBase;
    VCMProcessTimer                     _receiveStatsTimer;
    VCMProcessTimer                     _sendStatsTimer;
    VCMProcessTimer                     _retransmissionTimer;
    VCMProcessTimer                     _keyRequestTimer;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
