/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_GENERIC_DECODER_H_
#define WEBRTC_MODULES_VIDEO_CODING_GENERIC_DECODER_H_

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/timestamp_map.h"
#include "webrtc/modules/video_coding/main/source/timing.h"

namespace webrtc
{

class VCMReceiveCallback;

enum { kDecoderFrameMemoryLength = 30 };

struct VCMFrameInformation
{
    int64_t     renderTimeMs;
    int64_t     decodeStartTimeMs;
    void*             userData;
    VideoRotation rotation;
};

class VCMDecodedFrameCallback : public DecodedImageCallback
{
public:
    VCMDecodedFrameCallback(VCMTiming& timing, Clock* clock);
    virtual ~VCMDecodedFrameCallback();
    void SetUserReceiveCallback(VCMReceiveCallback* receiveCallback);
    VCMReceiveCallback* UserReceiveCallback();

    virtual int32_t Decoded(I420VideoFrame& decodedImage);
    virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId);
    virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId);

    uint64_t LastReceivedPictureID() const;

    int32_t Map(uint32_t timestamp, VCMFrameInformation* frameInfo);
    int32_t Pop(uint32_t timestamp);

private:
    // Protect |_receiveCallback| and |_timestampMap|.
    CriticalSectionWrapper* _critSect;
    Clock* _clock;
    VCMReceiveCallback* _receiveCallback;  // Guarded by |_critSect|.
    VCMTiming& _timing;
    VCMTimestampMap _timestampMap;  // Guarded by |_critSect|.
    uint64_t _lastReceivedPictureID;
};


class VCMGenericDecoder
{
    friend class VCMCodecDataBase;
public:
    VCMGenericDecoder(VideoDecoder& decoder, bool isExternal = false);
    ~VCMGenericDecoder();

    /**
    *	Initialize the decoder with the information from the VideoCodec
    */
    int32_t InitDecode(const VideoCodec* settings,
                             int32_t numberOfCores);

    /**
    *	Decode to a raw I420 frame,
    *
    *	inputVideoBuffer	reference to encoded video frame
    */
    int32_t Decode(const VCMEncodedFrame& inputFrame, int64_t nowMs);

    /**
    *	Free the decoder memory
    */
    int32_t Release();

    /**
    *	Reset the decoder state, prepare for a new call
    */
    int32_t Reset();

    /**
    *	Codec configuration data sent out-of-band, i.e. in SIP call setup
    *
    *	buffer pointer to the configuration data
    *	size the size of the configuration data in bytes
    */
    int32_t SetCodecConfigParameters(const uint8_t* /*buffer*/,
                                           int32_t /*size*/);

    /**
    * Set decode callback. Deregistering while decoding is illegal.
    */
    int32_t RegisterDecodeCompleteCallback(VCMDecodedFrameCallback* callback);

    bool External() const;

private:
    VCMDecodedFrameCallback*    _callback;
    VCMFrameInformation         _frameInfos[kDecoderFrameMemoryLength];
    uint32_t                    _nextFrameInfoIdx;
    VideoDecoder&               _decoder;
    VideoCodecType              _codecType;
    bool                        _isExternal;
    bool                        _keyFrameDecoded;
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_GENERIC_DECODER_H_
