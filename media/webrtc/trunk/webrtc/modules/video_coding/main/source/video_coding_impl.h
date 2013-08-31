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

#include "webrtc/modules/video_coding/main/source/codec_database.h"
#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/generic_decoder.h"
#include "webrtc/modules/video_coding/main/source/generic_encoder.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/media_optimization.h"
#include "webrtc/modules/video_coding/main/source/receiver.h"
#include "webrtc/modules/video_coding/main/source/timing.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc
{

class VCMProcessTimer
{
public:
    VCMProcessTimer(uint32_t periodMs, Clock* clock)
        : _clock(clock),
          _periodMs(periodMs),
          _latestMs(_clock->TimeInMilliseconds()) {}
    uint32_t Period() const;
    uint32_t TimeUntilProcess() const;
    void Processed();

private:
    Clock*                _clock;
    uint32_t        _periodMs;
    int64_t         _latestMs;
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
    VideoCodingModuleImpl(const int32_t id, Clock* clock,
                          EventFactory* event_factory, bool owns_event_factory);

    virtual ~VideoCodingModuleImpl();

    int32_t Id() const;

    //  Change the unique identifier of this object
    virtual int32_t ChangeUniqueId(const int32_t id);

    // Returns the number of milliseconds until the module want a worker thread
    // to call Process
    virtual int32_t TimeUntilNextProcess();

    virtual int32_t Process();

    /*
    *   Sender
    */

    // Initialize send codec
    virtual int32_t InitializeSender();

    // Register the send codec to be used.
    virtual int32_t RegisterSendCodec(const VideoCodec* sendCodec,
                                            uint32_t numberOfCores,
                                            uint32_t maxPayloadSize);

    // Get current send codec
    virtual int32_t SendCodec(VideoCodec* currentSendCodec) const;

    // Get current send codec type
    virtual VideoCodecType SendCodec() const;

    // Register an external encoder object.
    virtual int32_t RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                                  uint8_t payloadType,
                                                  bool internalSource = false);

    // Get codec config parameters
    virtual int32_t CodecConfigParameters(uint8_t* buffer,
                                                int32_t size);

    // Get encode bitrate
    virtual int Bitrate(unsigned int* bitrate) const;

    // Get encode frame rate
    virtual int FrameRate(unsigned int* framerate) const;

    // Set channel parameters
    virtual int32_t SetChannelParameters(
        uint32_t target_bitrate,  // bits/s.
        uint8_t lossRate,
        uint32_t rtt);

    // Set recieve channel parameters
    virtual int32_t SetReceiveChannelParameters(uint32_t rtt);

    // Register a transport callback which will be called to deliver the
    // encoded buffers
    virtual int32_t RegisterTransportCallback(
        VCMPacketizationCallback* transport);

    // Register a send statistics callback which will be called to deliver
    // information about the video stream produced by the encoder,
    // for instance the average frame rate and bit rate.
    virtual int32_t RegisterSendStatisticsCallback(
        VCMSendStatisticsCallback* sendStats);

    // Register a video quality settings callback which will be called when
    // frame rate/dimensions need to be updated for video quality optimization
    virtual int32_t RegisterVideoQMCallback(
        VCMQMSettingsCallback* videoQMSettings);

    // Register a video protection callback which will be called to deliver
    // the requested FEC rate and NACK status (on/off).
    virtual int32_t RegisterProtectionCallback(
        VCMProtectionCallback* protection);

    // Enable or disable a video protection method.
   virtual int32_t SetVideoProtection(VCMVideoProtection videoProtection,
                                            bool enable);

    // Add one raw video frame to the encoder, blocking.
    virtual int32_t AddVideoFrame(
        const I420VideoFrame& videoFrame,
        const VideoContentMetrics* _contentMetrics = NULL,
        const CodecSpecificInfo* codecSpecificInfo = NULL);

    virtual int32_t IntraFrameRequest(int stream_index);

    //Enable frame dropper
    virtual int32_t EnableFrameDropper(bool enable);

    // Sent frame counters
    virtual int32_t SentFrameCount(VCMFrameCount& frameCount) const;

    /*
    *   Receiver
    */

    // Initialize receiver, resets codec database etc
    virtual int32_t InitializeReceiver();

    // Register possible reveive codecs, can be called multiple times
    virtual int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                               int32_t numberOfCores,
                                               bool requireKeyFrame = false);

    // Register an externally defined decoder/render object.
    // Can be a decoder only or a decoder coupled with a renderer.
    virtual int32_t RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                                  uint8_t payloadType,
                                                  bool internalRenderTiming);

    // Register a receive callback. Will be called whenever there are a new
    // frame ready for rendering.
    virtual int32_t RegisterReceiveCallback(
        VCMReceiveCallback* receiveCallback);

    // Register a receive statistics callback which will be called to deliver
    // information about the video stream received by the receiving side of the
    // VCM, for instance the average frame rate and bit rate.
    virtual int32_t RegisterReceiveStatisticsCallback(
        VCMReceiveStatisticsCallback* receiveStats);

    // Register a frame type request callback.
    virtual int32_t RegisterFrameTypeCallback(
        VCMFrameTypeCallback* frameTypeCallback);

    // Register a frame storage callback.
    virtual int32_t RegisterFrameStorageCallback(
        VCMFrameStorageCallback* frameStorageCallback);

    // Nack callback
    virtual int32_t RegisterPacketRequestCallback(
        VCMPacketRequestCallback* callback);

    // Render buffer size callback.
    virtual int RegisterRenderBufferSizeCallback(
        VCMRenderBufferSizeCallback* callback);

    // Decode next frame, blocks for a maximum of maxWaitTimeMs milliseconds.
    // Should be called as often as possible to get the most out of the decoder.
    virtual int32_t Decode(uint16_t maxWaitTimeMs = 200);

    // Decode next dual frame, blocks for a maximum of maxWaitTimeMs
    // milliseconds.
    virtual int32_t DecodeDualFrame(uint16_t maxWaitTimeMs = 200);

    // Reset the decoder state
    virtual int32_t ResetDecoder();

    // Get current received codec
    virtual int32_t ReceiveCodec(VideoCodec* currentReceiveCodec) const;

    // Get current received codec type
    virtual VideoCodecType ReceiveCodec() const;

    // Incoming packet from network parsed and ready for decode, non blocking.
    virtual int32_t IncomingPacket(const uint8_t* incomingPayload,
                                         uint32_t payloadLength,
                                         const WebRtcRTPHeader& rtpInfo);

    // A part of an encoded frame to be decoded.
    // Used in conjunction with VCMFrameStorageCallback.
    virtual int32_t DecodeFromStorage(
        const EncodedVideoData& frameFromStorage);

    // Minimum playout delay (Used for lip-sync). This is the minimum delay
    // required to sync with audio. Not included in  VideoCodingModule::Delay()
    // Defaults to 0 ms.
    virtual int32_t SetMinimumPlayoutDelay(
        uint32_t minPlayoutDelayMs);

    // The estimated delay caused by rendering
    virtual int32_t SetRenderDelay(uint32_t timeMS);

    // Current delay
    virtual int32_t Delay() const;

    // Received frame counters
    virtual int32_t ReceivedFrameCount(VCMFrameCount& frameCount) const;

    // Returns the number of packets discarded by the jitter buffer.
    virtual uint32_t DiscardedPackets() const;


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

    virtual void SetNackSettings(size_t max_nack_list_size,
                                 int max_packet_age_to_nack,
                                 int max_incomplete_time_ms);

    // Set the video delay for the receiver (default = 0).
    virtual int SetMinReceiverDelay(int desired_delay_ms);

    // Enables recording of debugging information.
    virtual int StartDebugRecording(const char* file_name_utf8);

    // Disables recording of debugging information.
    virtual int StopDebugRecording();

protected:
    int32_t Decode(const webrtc::VCMEncodedFrame& frame);
    int32_t RequestKeyFrame();
    int32_t RequestSliceLossIndication(
        const uint64_t pictureID) const;
    int32_t NackList(uint16_t* nackList, uint16_t& size);

private:
    int32_t                       _id;
    Clock*                              clock_;
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
    VCMRenderBufferSizeCallback*        render_buffer_callback_;
    VCMGenericDecoder*                  _decoder;
    VCMGenericDecoder*                  _dualDecoder;
#ifdef DEBUG_DECODER_BIT_STREAM
    FILE*                               _bitStreamBeforeDecoder;
#endif
    VCMFrameBuffer                      _frameFromFile;
    VCMKeyRequestMode                   _keyRequestMode;
    bool                                _scheduleKeyRequest;
    size_t                              max_nack_list_size_;

    CriticalSectionWrapper*             _sendCritSect; // Critical section for send side
    VCMGenericEncoder*                  _encoder;
    VCMEncodedFrameCallback             _encodedFrameCallback;
    std::vector<FrameType>              _nextFrameTypes;
    media_optimization::VCMMediaOptimization _mediaOpt;
    VideoCodecType                      _sendCodecType;
    VCMSendStatisticsCallback*          _sendStatsCallback;
    FILE*                               _encoderInputFile;
    VCMCodecDataBase                    _codecDataBase;
    VCMProcessTimer                     _receiveStatsTimer;
    VCMProcessTimer                     _sendStatsTimer;
    VCMProcessTimer                     _retransmissionTimer;
    VCMProcessTimer                     _keyRequestTimer;
    EventFactory*                       event_factory_;
    bool                                owns_event_factory_;
    bool                                frame_dropper_enabled_;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CODING_VIDEO_CODING_IMPL_H_
