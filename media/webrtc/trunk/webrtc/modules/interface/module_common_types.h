/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULE_COMMON_TYPES_H
#define MODULE_COMMON_TYPES_H

#include <cstring> // memcpy
#include <assert.h>

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

#ifdef _WIN32
    #pragma warning(disable:4351)       // remove warning "new behavior: elements of array
                                        // 'array' will be default initialized"
#endif

namespace webrtc {

struct RTPHeader
{
    bool           markerBit;
    WebRtc_UWord8  payloadType;
    WebRtc_UWord16 sequenceNumber;
    WebRtc_UWord32 timestamp;
    WebRtc_UWord32 ssrc;
    WebRtc_UWord8  numCSRCs;
    WebRtc_UWord32 arrOfCSRCs[kRtpCsrcSize];
    WebRtc_UWord8  paddingLength;
    WebRtc_UWord16 headerLength;
};

struct RTPHeaderExtension
{
    WebRtc_Word32  transmissionTimeOffset;
};

struct RTPAudioHeader
{
    WebRtc_UWord8  numEnergy;                         // number of valid entries in arrOfEnergy
    WebRtc_UWord8  arrOfEnergy[kRtpCsrcSize];   // one energy byte (0-9) per channel
    bool           isCNG;                             // is this CNG
    WebRtc_UWord8  channel;                           // number of channels 2 = stereo
};

enum {kNoPictureId = -1};
enum {kNoTl0PicIdx = -1};
enum {kNoTemporalIdx = -1};
enum {kNoKeyIdx = -1};
enum {kNoSimulcastIdx = 0};

struct RTPVideoHeaderVP8
{
    void InitRTPVideoHeaderVP8()
    {
        nonReference = false;
        pictureId = kNoPictureId;
        tl0PicIdx = kNoTl0PicIdx;
        temporalIdx = kNoTemporalIdx;
        layerSync = false;
        keyIdx = kNoKeyIdx;
        partitionId = 0;
        beginningOfPartition = false;
        frameWidth = 0;
        frameHeight = 0;
    }

    bool           nonReference;   // Frame is discardable.
    WebRtc_Word16  pictureId;      // Picture ID index, 15 bits;
                                   // kNoPictureId if PictureID does not exist.
    WebRtc_Word16  tl0PicIdx;      // TL0PIC_IDX, 8 bits;
                                   // kNoTl0PicIdx means no value provided.
    WebRtc_Word8   temporalIdx;    // Temporal layer index, or kNoTemporalIdx.
    bool           layerSync;      // This frame is a layer sync frame.
                                   // Disabled if temporalIdx == kNoTemporalIdx.
    int            keyIdx;         // 5 bits; kNoKeyIdx means not used.
    int            partitionId;    // VP8 partition ID
    bool           beginningOfPartition;  // True if this packet is the first
                                          // in a VP8 partition. Otherwise false
    int            frameWidth;     // Exists for key frames.
    int            frameHeight;    // Exists for key frames.
};
union RTPVideoTypeHeader
{
    RTPVideoHeaderVP8       VP8;
};

enum RTPVideoCodecTypes
{
    kRTPVideoGeneric  = 0,
    kRTPVideoVP8      = 8,
    kRTPVideoNoVideo  = 10,
    kRTPVideoFEC      = 11,
    kRTPVideoI420     = 12
};
struct RTPVideoHeader
{
    WebRtc_UWord16          width;                  // size
    WebRtc_UWord16          height;

    bool                    isFirstPacket;   // first packet in frame
    WebRtc_UWord8           simulcastIdx;    // Index if the simulcast encoder creating
                                             // this frame, 0 if not using simulcast.
    RTPVideoCodecTypes      codec;
    RTPVideoTypeHeader      codecHeader;
};
union RTPTypeHeader
{
    RTPAudioHeader  Audio;
    RTPVideoHeader  Video;
};

struct WebRtcRTPHeader
{
    RTPHeader       header;
    FrameType       frameType;
    RTPTypeHeader   type;
    RTPHeaderExtension extension;
};

class RTPFragmentationHeader
{
public:
    RTPFragmentationHeader() :
        fragmentationVectorSize(0),
        fragmentationOffset(NULL),
        fragmentationLength(NULL),
        fragmentationTimeDiff(NULL),
        fragmentationPlType(NULL)
    {};

    ~RTPFragmentationHeader()
    {
        delete [] fragmentationOffset;
        delete [] fragmentationLength;
        delete [] fragmentationTimeDiff;
        delete [] fragmentationPlType;
    }

    void CopyFrom(const RTPFragmentationHeader& src)
    {
        if(this == &src)
        {
            return;
        }

        if(src.fragmentationVectorSize != fragmentationVectorSize)
        {
            // new size of vectors

            // delete old
            delete [] fragmentationOffset;
            fragmentationOffset = NULL;
            delete [] fragmentationLength;
            fragmentationLength = NULL;
            delete [] fragmentationTimeDiff;
            fragmentationTimeDiff = NULL;
            delete [] fragmentationPlType;
            fragmentationPlType = NULL;

            if(src.fragmentationVectorSize > 0)
            {
                // allocate new
                if(src.fragmentationOffset)
                {
                    fragmentationOffset = new WebRtc_UWord32[src.fragmentationVectorSize];
                }
                if(src.fragmentationLength)
                {
                    fragmentationLength = new WebRtc_UWord32[src.fragmentationVectorSize];
                }
                if(src.fragmentationTimeDiff)
                {
                    fragmentationTimeDiff = new WebRtc_UWord16[src.fragmentationVectorSize];
                }
                if(src.fragmentationPlType)
                {
                    fragmentationPlType = new WebRtc_UWord8[src.fragmentationVectorSize];
                }
            }
            // set new size
            fragmentationVectorSize =   src.fragmentationVectorSize;
        }

        if(src.fragmentationVectorSize > 0)
        {
            // copy values
            if(src.fragmentationOffset)
            {
                memcpy(fragmentationOffset, src.fragmentationOffset,
                       src.fragmentationVectorSize * sizeof(WebRtc_UWord32));
            }
            if(src.fragmentationLength)
            {
                memcpy(fragmentationLength, src.fragmentationLength,
                       src.fragmentationVectorSize * sizeof(WebRtc_UWord32));
            }
            if(src.fragmentationTimeDiff)
            {
                memcpy(fragmentationTimeDiff, src.fragmentationTimeDiff,
                       src.fragmentationVectorSize * sizeof(WebRtc_UWord16));
            }
            if(src.fragmentationPlType)
            {
                memcpy(fragmentationPlType, src.fragmentationPlType,
                       src.fragmentationVectorSize * sizeof(WebRtc_UWord8));
            }
        }
    }

    void VerifyAndAllocateFragmentationHeader(const WebRtc_UWord16 size)
    {
        if(fragmentationVectorSize < size)
        {
            WebRtc_UWord16 oldVectorSize = fragmentationVectorSize;
            {
                // offset
                WebRtc_UWord32* oldOffsets = fragmentationOffset;
                fragmentationOffset = new WebRtc_UWord32[size];
                memset(fragmentationOffset+oldVectorSize, 0,
                       sizeof(WebRtc_UWord32)*(size-oldVectorSize));
                // copy old values
                memcpy(fragmentationOffset,oldOffsets, sizeof(WebRtc_UWord32) * oldVectorSize);
                delete[] oldOffsets;
            }
            // length
            {
                WebRtc_UWord32* oldLengths = fragmentationLength;
                fragmentationLength = new WebRtc_UWord32[size];
                memset(fragmentationLength+oldVectorSize, 0,
                       sizeof(WebRtc_UWord32) * (size- oldVectorSize));
                memcpy(fragmentationLength, oldLengths,
                       sizeof(WebRtc_UWord32) * oldVectorSize);
                delete[] oldLengths;
            }
            // time diff
            {
                WebRtc_UWord16* oldTimeDiffs = fragmentationTimeDiff;
                fragmentationTimeDiff = new WebRtc_UWord16[size];
                memset(fragmentationTimeDiff+oldVectorSize, 0,
                       sizeof(WebRtc_UWord16) * (size- oldVectorSize));
                memcpy(fragmentationTimeDiff, oldTimeDiffs,
                       sizeof(WebRtc_UWord16) * oldVectorSize);
                delete[] oldTimeDiffs;
            }
            // payload type
            {
                WebRtc_UWord8* oldTimePlTypes = fragmentationPlType;
                fragmentationPlType = new WebRtc_UWord8[size];
                memset(fragmentationPlType+oldVectorSize, 0,
                       sizeof(WebRtc_UWord8) * (size- oldVectorSize));
                memcpy(fragmentationPlType, oldTimePlTypes,
                       sizeof(WebRtc_UWord8) * oldVectorSize);
                delete[] oldTimePlTypes;
            }
            fragmentationVectorSize = size;
        }
    }

    WebRtc_UWord16    fragmentationVectorSize;    // Number of fragmentations
    WebRtc_UWord32*   fragmentationOffset;        // Offset of pointer to data for each fragm.
    WebRtc_UWord32*   fragmentationLength;        // Data size for each fragmentation
    WebRtc_UWord16*   fragmentationTimeDiff;      // Timestamp difference relative "now" for
                                                  // each fragmentation
    WebRtc_UWord8*    fragmentationPlType;        // Payload type of each fragmentation

private:
    DISALLOW_COPY_AND_ASSIGN(RTPFragmentationHeader);
};

struct RTCPVoIPMetric
{
    // RFC 3611 4.7
    WebRtc_UWord8     lossRate;
    WebRtc_UWord8     discardRate;
    WebRtc_UWord8     burstDensity;
    WebRtc_UWord8     gapDensity;
    WebRtc_UWord16    burstDuration;
    WebRtc_UWord16    gapDuration;
    WebRtc_UWord16    roundTripDelay;
    WebRtc_UWord16    endSystemDelay;
    WebRtc_UWord8     signalLevel;
    WebRtc_UWord8     noiseLevel;
    WebRtc_UWord8     RERL;
    WebRtc_UWord8     Gmin;
    WebRtc_UWord8     Rfactor;
    WebRtc_UWord8     extRfactor;
    WebRtc_UWord8     MOSLQ;
    WebRtc_UWord8     MOSCQ;
    WebRtc_UWord8     RXconfig;
    WebRtc_UWord16    JBnominal;
    WebRtc_UWord16    JBmax;
    WebRtc_UWord16    JBabsMax;
};

// Types for the FEC packet masks. The type |kFecMaskRandom| is based on a
// random loss model. The type |kFecMaskBursty| is based on a bursty/consecutive
// loss model. The packet masks are defined in
// modules/rtp_rtcp/fec_private_tables_random(bursty).h
enum FecMaskType {
  kFecMaskRandom,
  kFecMaskBursty,
};

// Struct containing forward error correction settings.
struct FecProtectionParams {
  int fec_rate;
  bool use_uep_protection;
  int max_fec_frames;
  FecMaskType fec_mask_type;
};

// class describing a complete, or parts of an encoded frame.
class EncodedVideoData
{
public:
    EncodedVideoData() :
        payloadType(0),
        timeStamp(0),
        renderTimeMs(0),
        encodedWidth(0),
        encodedHeight(0),
        completeFrame(false),
        missingFrame(false),
        payloadData(NULL),
        payloadSize(0),
        bufferSize(0),
        fragmentationHeader(),
        frameType(kVideoFrameDelta),
        codec(kVideoCodecUnknown)
    {};

    EncodedVideoData(const EncodedVideoData& data)
    {
        payloadType         = data.payloadType;
        timeStamp           = data.timeStamp;
        renderTimeMs        = data.renderTimeMs;
        encodedWidth        = data.encodedWidth;
        encodedHeight       = data.encodedHeight;
        completeFrame       = data.completeFrame;
        missingFrame        = data.missingFrame;
        payloadSize         = data.payloadSize;
        fragmentationHeader.CopyFrom(data.fragmentationHeader);
        frameType           = data.frameType;
        codec               = data.codec;
        if (data.payloadSize > 0)
        {
            payloadData = new WebRtc_UWord8[data.payloadSize];
            memcpy(payloadData, data.payloadData, data.payloadSize);
        }
        else
        {
            payloadData = NULL;
        }
    }


    ~EncodedVideoData()
    {
        delete [] payloadData;
    };

    EncodedVideoData& operator=(const EncodedVideoData& data)
    {
        if (this == &data)
        {
            return *this;
        }
        payloadType         = data.payloadType;
        timeStamp           = data.timeStamp;
        renderTimeMs        = data.renderTimeMs;
        encodedWidth        = data.encodedWidth;
        encodedHeight       = data.encodedHeight;
        completeFrame       = data.completeFrame;
        missingFrame        = data.missingFrame;
        payloadSize         = data.payloadSize;
        fragmentationHeader.CopyFrom(data.fragmentationHeader);
        frameType           = data.frameType;
        codec               = data.codec;
        if (data.payloadSize > 0)
        {
            delete [] payloadData;
            payloadData = new WebRtc_UWord8[data.payloadSize];
            memcpy(payloadData, data.payloadData, data.payloadSize);
            bufferSize = data.payloadSize;
        }
        return *this;
    };
    void VerifyAndAllocate( const WebRtc_UWord32 size)
    {
        if (bufferSize < size)
        {
            WebRtc_UWord8* oldPayload = payloadData;
            payloadData = new WebRtc_UWord8[size];
            memcpy(payloadData, oldPayload, sizeof(WebRtc_UWord8) * payloadSize);

            bufferSize = size;
            delete[] oldPayload;
        }
    }

    WebRtc_UWord8               payloadType;
    WebRtc_UWord32              timeStamp;
    WebRtc_Word64               renderTimeMs;
    WebRtc_UWord32              encodedWidth;
    WebRtc_UWord32              encodedHeight;
    bool                        completeFrame;
    bool                        missingFrame;
    WebRtc_UWord8*              payloadData;
    WebRtc_UWord32              payloadSize;
    WebRtc_UWord32              bufferSize;
    RTPFragmentationHeader      fragmentationHeader;
    FrameType                   frameType;
    VideoCodecType              codec;
};

struct VideoContentMetrics {
  VideoContentMetrics()
      : motion_magnitude(0.0f),
        spatial_pred_err(0.0f),
        spatial_pred_err_h(0.0f),
        spatial_pred_err_v(0.0f) {
  }

  void Reset() {
    motion_magnitude = 0.0f;
    spatial_pred_err = 0.0f;
    spatial_pred_err_h = 0.0f;
    spatial_pred_err_v = 0.0f;
  }
  float motion_magnitude;
  float spatial_pred_err;
  float spatial_pred_err_h;
  float spatial_pred_err_v;
};

/*************************************************
 *
 * VideoFrame class
 *
 * The VideoFrame class allows storing and
 * handling of video frames.
 *
 *
 *************************************************/
class VideoFrame
{
public:
    VideoFrame();
    ~VideoFrame();
    /**
    * Verifies that current allocated buffer size is larger than or equal to the input size.
    * If the current buffer size is smaller, a new allocation is made and the old buffer data
    * is copied to the new buffer.
    * Buffer size is updated to minimumSize.
    */
    WebRtc_Word32 VerifyAndAllocate(const WebRtc_UWord32 minimumSize);
    /**
    *    Update length of data buffer in frame. Function verifies that new length is less or
    *    equal to allocated size.
    */
    WebRtc_Word32 SetLength(const WebRtc_UWord32 newLength);
    /*
    *    Swap buffer and size data
    */
    WebRtc_Word32 Swap(WebRtc_UWord8*& newMemory,
                       WebRtc_UWord32& newLength,
                       WebRtc_UWord32& newSize);
    /*
    *    Swap buffer and size data
    */
    WebRtc_Word32 SwapFrame(VideoFrame& videoFrame);
    /**
    *    Copy buffer: If newLength is bigger than allocated size, a new buffer of size length
    *    is allocated.
    */
    WebRtc_Word32 CopyFrame(const VideoFrame& videoFrame);
    /**
    *    Copy buffer: If newLength is bigger than allocated size, a new buffer of size length
    *    is allocated.
    */
    WebRtc_Word32 CopyFrame(WebRtc_UWord32 length, const WebRtc_UWord8* sourceBuffer);
    /**
    *    Delete VideoFrame and resets members to zero
    */
    void Free();
    /**
    *   Set frame timestamp (90kHz)
    */
    void SetTimeStamp(const WebRtc_UWord32 timeStamp) {_timeStamp = timeStamp;}
    /**
    *   Get pointer to frame buffer
    */
    WebRtc_UWord8*    Buffer() const {return _buffer;}

    WebRtc_UWord8*&   Buffer() {return _buffer;}

    /**
    *   Get allocated buffer size
    */
    WebRtc_UWord32    Size() const {return _bufferSize;}
    /**
    *   Get frame length
    */
    WebRtc_UWord32    Length() const {return _bufferLength;}
    /**
    *   Get frame timestamp (90kHz)
    */
    WebRtc_UWord32    TimeStamp() const {return _timeStamp;}
    /**
    *   Get frame width
    */
    WebRtc_UWord32    Width() const {return _width;}
    /**
    *   Get frame height
    */
    WebRtc_UWord32    Height() const {return _height;}
    /**
    *   Set frame width
    */
    void   SetWidth(const WebRtc_UWord32 width)  {_width = width;}
    /**
    *   Set frame height
    */
    void  SetHeight(const WebRtc_UWord32 height) {_height = height;}
    /**
    *   Set render time in miliseconds
    */
    void SetRenderTime(const WebRtc_Word64 renderTimeMs) {_renderTimeMs = renderTimeMs;}
    /**
    *  Get render time in miliseconds
    */
    WebRtc_Word64    RenderTimeMs() const {return _renderTimeMs;}

private:
    void Set(WebRtc_UWord8* buffer,
             WebRtc_UWord32 size,
             WebRtc_UWord32 length,
             WebRtc_UWord32 timeStamp);

    WebRtc_UWord8*          _buffer;          // Pointer to frame buffer
    WebRtc_UWord32          _bufferSize;      // Allocated buffer size
    WebRtc_UWord32          _bufferLength;    // Length (in bytes) of buffer
    WebRtc_UWord32          _timeStamp;       // Timestamp of frame (90kHz)
    WebRtc_UWord32          _width;
    WebRtc_UWord32          _height;
    WebRtc_Word64           _renderTimeMs;
}; // end of VideoFrame class declaration

// inline implementation of VideoFrame class:
inline
VideoFrame::VideoFrame():
    _buffer(0),
    _bufferSize(0),
    _bufferLength(0),
    _timeStamp(0),
    _width(0),
    _height(0),
    _renderTimeMs(0)
{
    //
}
inline
VideoFrame::~VideoFrame()
{
    if(_buffer)
    {
        delete [] _buffer;
        _buffer = NULL;
    }
}


inline
WebRtc_Word32
VideoFrame::VerifyAndAllocate(const WebRtc_UWord32 minimumSize)
{
    if (minimumSize < 1)
    {
        return -1;
    }
    if(minimumSize > _bufferSize)
    {
        // create buffer of sufficient size
        WebRtc_UWord8* newBufferBuffer = new WebRtc_UWord8[minimumSize];
        if(_buffer)
        {
            // copy old data
            memcpy(newBufferBuffer, _buffer, _bufferSize);
            delete [] _buffer;
        }
        else
        {
            memset(newBufferBuffer, 0, minimumSize * sizeof(WebRtc_UWord8));
        }
        _buffer = newBufferBuffer;
        _bufferSize = minimumSize;
    }
    return 0;
}

inline
WebRtc_Word32
VideoFrame::SetLength(const WebRtc_UWord32 newLength)
{
    if (newLength >_bufferSize )
    { // can't accomodate new value
        return -1;
    }
     _bufferLength = newLength;
     return 0;
}

inline
WebRtc_Word32
VideoFrame::SwapFrame(VideoFrame& videoFrame)
{
    WebRtc_UWord32 tmpTimeStamp  = _timeStamp;
    WebRtc_UWord32 tmpWidth      = _width;
    WebRtc_UWord32 tmpHeight     = _height;
    WebRtc_Word64  tmpRenderTime = _renderTimeMs;

    _timeStamp = videoFrame._timeStamp;
    _width = videoFrame._width;
    _height = videoFrame._height;
    _renderTimeMs = videoFrame._renderTimeMs;

    videoFrame._timeStamp = tmpTimeStamp;
    videoFrame._width = tmpWidth;
    videoFrame._height = tmpHeight;
    videoFrame._renderTimeMs = tmpRenderTime;

    return Swap(videoFrame._buffer, videoFrame._bufferLength, videoFrame._bufferSize);
}

inline
WebRtc_Word32
VideoFrame::Swap(WebRtc_UWord8*& newMemory, WebRtc_UWord32& newLength, WebRtc_UWord32& newSize)
{
    WebRtc_UWord8* tmpBuffer = _buffer;
    WebRtc_UWord32 tmpLength = _bufferLength;
    WebRtc_UWord32 tmpSize = _bufferSize;
    _buffer = newMemory;
    _bufferLength = newLength;
    _bufferSize = newSize;
    newMemory = tmpBuffer;
    newLength = tmpLength;
    newSize = tmpSize;
    return 0;
}

inline
WebRtc_Word32
VideoFrame::CopyFrame(WebRtc_UWord32 length, const WebRtc_UWord8* sourceBuffer)
{
    if (length > _bufferSize)
    {
        WebRtc_Word32 ret = VerifyAndAllocate(length);
        if (ret < 0)
        {
            return ret;
        }
    }
     memcpy(_buffer, sourceBuffer, length);
    _bufferLength = length;
    return 0;
}

inline
WebRtc_Word32
VideoFrame::CopyFrame(const VideoFrame& videoFrame)
{
    if(CopyFrame(videoFrame.Length(), videoFrame.Buffer()) != 0)
    {
        return -1;
    }
    _timeStamp = videoFrame._timeStamp;
    _width = videoFrame._width;
    _height = videoFrame._height;
    _renderTimeMs = videoFrame._renderTimeMs;
    return 0;
}

inline
void
VideoFrame::Free()
{
    _timeStamp = 0;
    _bufferLength = 0;
    _bufferSize = 0;
    _height = 0;
    _width = 0;
    _renderTimeMs = 0;

    if(_buffer)
    {
        delete [] _buffer;
        _buffer = NULL;
    }
}


/* This class holds up to 60 ms of super-wideband (32 kHz) stereo audio. It
 * allows for adding and subtracting frames while keeping track of the resulting
 * states.
 *
 * Notes
 * - The total number of samples in |data_| is
 *   samples_per_channel_ * num_channels_
 *
 * - Stereo data is interleaved starting with the left channel.
 *
 * - The +operator assume that you would never add exactly opposite frames when
 *   deciding the resulting state. To do this use the -operator.
 */
class AudioFrame
{
public:
    enum { kMaxDataSizeSamples = 3840 };  // stereo, 32 kHz, 60ms (2*32*60)

    enum VADActivity
    {
        kVadActive  = 0,
        kVadPassive = 1,
        kVadUnknown = 2
    };
    enum SpeechType
    {
        kNormalSpeech = 0,
        kPLC          = 1,
        kCNG          = 2,
        kPLCCNG       = 3,
        kUndefined    = 4
    };

    AudioFrame();
    virtual ~AudioFrame();

    int UpdateFrame(
        int id,
        uint32_t timestamp,
        const int16_t* data,
        int samples_per_channel,
        int sample_rate_hz,
        SpeechType speech_type,
        VADActivity vad_activity,
        int num_channels = 1,
        uint32_t energy = -1);

    AudioFrame& Append(const AudioFrame& rhs);

    void Mute();

    AudioFrame& operator=(const AudioFrame& rhs);
    AudioFrame& operator>>=(const int rhs);
    AudioFrame& operator+=(const AudioFrame& rhs);
    AudioFrame& operator-=(const AudioFrame& rhs);

    int id_;
    uint32_t timestamp_;
    int16_t data_[kMaxDataSizeSamples];
    int samples_per_channel_;
    int sample_rate_hz_;
    int num_channels_;
    SpeechType speech_type_;
    VADActivity vad_activity_;
    uint32_t energy_;
};

inline
AudioFrame::AudioFrame()
    :
    id_(-1),
    timestamp_(0),
    data_(),
    samples_per_channel_(0),
    sample_rate_hz_(0),
    num_channels_(1),
    speech_type_(kUndefined),
    vad_activity_(kVadUnknown),
    energy_(0xffffffff)
{
}

inline
AudioFrame::~AudioFrame()
{
}

inline
int
AudioFrame::UpdateFrame(
    int id,
    uint32_t timestamp,
    const int16_t* data,
    int samples_per_channel,
    int sample_rate_hz,
    SpeechType speech_type,
    VADActivity vad_activity,
    int num_channels,
    uint32_t energy)
{
    id_            = id;
    timestamp_     = timestamp;
    sample_rate_hz_ = sample_rate_hz;
    speech_type_    = speech_type;
    vad_activity_   = vad_activity;
    num_channels_  = num_channels;
    energy_        = energy;

    if((samples_per_channel > kMaxDataSizeSamples) ||
        (num_channels > 2) || (num_channels < 1))
    {
        samples_per_channel_ = 0;
        return -1;
    }
    samples_per_channel_ = samples_per_channel;
    if(data != NULL)
    {
        memcpy(data_, data, sizeof(int16_t) *
            samples_per_channel * num_channels_);
    }
    else
    {
        memset(data_,0,sizeof(int16_t) *
            samples_per_channel * num_channels_);
    }
    return 0;
}

inline
void
AudioFrame::Mute()
{
  memset(data_, 0, samples_per_channel_ * num_channels_ * sizeof(int16_t));
}

inline
AudioFrame&
AudioFrame::operator=(const AudioFrame& rhs)
{
    // Sanity Check
    if((rhs.samples_per_channel_ > kMaxDataSizeSamples) ||
        (rhs.num_channels_ > 2) ||
        (rhs.num_channels_ < 1))
    {
        return *this;
    }
    if(this == &rhs)
    {
        return *this;
    }
    id_               = rhs.id_;
    timestamp_        = rhs.timestamp_;
    sample_rate_hz_    = rhs.sample_rate_hz_;
    speech_type_       = rhs.speech_type_;
    vad_activity_      = rhs.vad_activity_;
    num_channels_     = rhs.num_channels_;
    energy_           = rhs.energy_;

    samples_per_channel_ = rhs.samples_per_channel_;
    memcpy(data_, rhs.data_,
        sizeof(int16_t) * rhs.samples_per_channel_ * num_channels_);

    return *this;
}

inline
AudioFrame&
AudioFrame::operator>>=(const int rhs)
{
    assert((num_channels_ > 0) && (num_channels_ < 3));
    if((num_channels_ > 2) ||
        (num_channels_ < 1))
    {
        return *this;
    }
    for(int i = 0; i < samples_per_channel_ * num_channels_; i++)
    {
        data_[i] = static_cast<int16_t>(data_[i] >> rhs);
    }
    return *this;
}

inline
AudioFrame&
AudioFrame::Append(const AudioFrame& rhs)
{
    // Sanity check
    assert((num_channels_ > 0) && (num_channels_ < 3));
    if((num_channels_ > 2) ||
        (num_channels_ < 1))
    {
        return *this;
    }
    if(num_channels_ != rhs.num_channels_)
    {
        return *this;
    }
    if((vad_activity_ == kVadActive) ||
        rhs.vad_activity_ == kVadActive)
    {
        vad_activity_ = kVadActive;
    }
    else if((vad_activity_ == kVadUnknown) ||
        rhs.vad_activity_ == kVadUnknown)
    {
        vad_activity_ = kVadUnknown;
    }
    if(speech_type_ != rhs.speech_type_)
    {
        speech_type_ = kUndefined;
    }

    int offset = samples_per_channel_ * num_channels_;
    for(int i = 0;
        i < rhs.samples_per_channel_ * rhs.num_channels_;
        i++)
    {
        data_[offset+i] = rhs.data_[i];
    }
    samples_per_channel_ += rhs.samples_per_channel_;
    return *this;
}

// merge vectors
inline
AudioFrame&
AudioFrame::operator+=(const AudioFrame& rhs)
{
    // Sanity check
    assert((num_channels_ > 0) && (num_channels_ < 3));
    if((num_channels_ > 2) ||
        (num_channels_ < 1))
    {
        return *this;
    }
    if(num_channels_ != rhs.num_channels_)
    {
        return *this;
    }
    bool noPrevData = false;
    if(samples_per_channel_ != rhs.samples_per_channel_)
    {
        if(samples_per_channel_ == 0)
        {
            // special case we have no data to start with
            samples_per_channel_ = rhs.samples_per_channel_;
            noPrevData = true;
        } else
        {
          return *this;
        }
    }

    if((vad_activity_ == kVadActive) ||
        rhs.vad_activity_ == kVadActive)
    {
        vad_activity_ = kVadActive;
    }
    else if((vad_activity_ == kVadUnknown) ||
        rhs.vad_activity_ == kVadUnknown)
    {
        vad_activity_ = kVadUnknown;
    }

    if(speech_type_ != rhs.speech_type_)
    {
        speech_type_ = kUndefined;
    }

    if(noPrevData)
    {
        memcpy(data_, rhs.data_,
          sizeof(int16_t) * rhs.samples_per_channel_ * num_channels_);
    } else
    {
      // IMPROVEMENT this can be done very fast in assembly
      for(int i = 0; i < samples_per_channel_ * num_channels_; i++)
      {
          int32_t wrapGuard = static_cast<int32_t>(data_[i]) +
              static_cast<int32_t>(rhs.data_[i]);
          if(wrapGuard < -32768)
          {
              data_[i] = -32768;
          }else if(wrapGuard > 32767)
          {
              data_[i] = 32767;
          }else
          {
              data_[i] = (int16_t)wrapGuard;
          }
      }
    }
    energy_ = 0xffffffff;
    return *this;
}

inline
AudioFrame&
AudioFrame::operator-=(const AudioFrame& rhs)
{
    // Sanity check
    assert((num_channels_ > 0) && (num_channels_ < 3));
    if((num_channels_ > 2)||
        (num_channels_ < 1))
    {
        return *this;
    }
    if((samples_per_channel_ != rhs.samples_per_channel_) ||
        (num_channels_ != rhs.num_channels_))
    {
        return *this;
    }
    if((vad_activity_ != kVadPassive) ||
        rhs.vad_activity_ != kVadPassive)
    {
        vad_activity_ = kVadUnknown;
    }
    speech_type_ = kUndefined;

    for(int i = 0; i < samples_per_channel_ * num_channels_; i++)
    {
        int32_t wrapGuard = static_cast<int32_t>(data_[i]) -
            static_cast<int32_t>(rhs.data_[i]);
        if(wrapGuard < -32768)
        {
            data_[i] = -32768;
        }
        else if(wrapGuard > 32767)
        {
            data_[i] = 32767;
        }
        else
        {
            data_[i] = (int16_t)wrapGuard;
        }
    }
    energy_ = 0xffffffff;
    return *this;
}

} // namespace webrtc

#endif // MODULE_COMMON_TYPES_H
