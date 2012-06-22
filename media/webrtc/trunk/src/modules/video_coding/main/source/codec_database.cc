/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "codec_database.h"

#include <assert.h>

#include "../../../../engine_configurations.h"
#include "internal_defines.h"
#include "trace.h"

#if defined(_WIN32)
    // VS 2005: Don't warn for default initialized arrays. See help for more info.
    // Don't warn for strncpy being unsecure.
    // switch statement contains 'default' but no 'case' labels
#pragma warning(disable:4351; disable:4996; disable:4065)
#endif

// Supported codecs
#ifdef  VIDEOCODEC_VP8
    #include "vp8.h"
#endif
#ifdef  VIDEOCODEC_I420
    #include "i420.h"
#endif

namespace webrtc
{

VCMDecoderMapItem::VCMDecoderMapItem(VideoCodec* settings,
                                     WebRtc_UWord32 numberOfCores,
                                     bool requireKeyFrame)
:
_settings(settings),
_numberOfCores(numberOfCores),
_requireKeyFrame(requireKeyFrame)
{
}

VCMExtDecoderMapItem::VCMExtDecoderMapItem(VideoDecoder* externalDecoderInstance,
                                           WebRtc_UWord8 payloadType,
                                           bool internalRenderTiming)
:
_payloadType(payloadType),
_externalDecoderInstance(externalDecoderInstance),
_internalRenderTiming(internalRenderTiming)
{
}

VCMCodecDataBase::VCMCodecDataBase(WebRtc_Word32 id):
_id(id),
_numberOfCores(0),
_maxPayloadSize(kDefaultPayloadSize),
_periodicKeyFrames(false),
_currentEncIsExternal(false),
_sendCodec(),
_receiveCodec(),
_externalPayloadType(0),
_externalEncoder(NULL),
_internalSource(false),
_ptrEncoder(NULL),
_ptrDecoder(NULL),
_currentDecIsExternal(false),
_decMap(),
_decExternalMap()
{
    //
}

VCMCodecDataBase::~VCMCodecDataBase()
{
    Reset();
}

WebRtc_Word32
VCMCodecDataBase::Reset()
{
    WebRtc_Word32 ret = ResetReceiver();
    if (ret < 0)
    {
        return ret;
    }
    ret = ResetSender();
    if (ret < 0)
    {
        return ret;
    }
   return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::ResetSender()
{
    DeleteEncoder();
    _periodicKeyFrames = false;
    return VCM_OK;
}

VCMGenericEncoder* VCMCodecDataBase::CreateEncoder(
    const VideoCodecType type) const {

    switch(type)
    {
#ifdef VIDEOCODEC_VP8
        case kVideoCodecVP8:
            return new VCMGenericEncoder(*(VP8Encoder::Create()));
#endif
#ifdef VIDEOCODEC_I420
        case kVideoCodecI420:
            return new VCMGenericEncoder(*(new I420Encoder));
#endif
        default:
            return NULL;
    }
}

void
VCMCodecDataBase::DeleteEncoder()
{
    if (_ptrEncoder)
    {
        _ptrEncoder->Release();
        if (!_currentEncIsExternal)
        {
            delete &_ptrEncoder->_encoder;
        }
        delete _ptrEncoder;
        _ptrEncoder = NULL;
    }
}

WebRtc_UWord8
VCMCodecDataBase::NumberOfCodecs()
{
    return VCM_NUM_VIDEO_CODECS_AVAILABLE;
}

WebRtc_Word32
VCMCodecDataBase::Codec(WebRtc_UWord8 listId, VideoCodec *settings)
{
    if (settings == NULL)
    {
        return VCM_PARAMETER_ERROR;
    }

    if (listId >= VCM_NUM_VIDEO_CODECS_AVAILABLE)
    {
        return VCM_PARAMETER_ERROR;
    }
    memset(settings, 0, sizeof(VideoCodec));
    switch (listId)
    {
#ifdef VIDEOCODEC_VP8
    case VCM_VP8_IDX:
        {
            strncpy(settings->plName, "VP8", 4);
            settings->codecType = kVideoCodecVP8;
            // 96 to 127 dynamic payload types for video codecs
            settings->plType = VCM_VP8_PAYLOAD_TYPE;
            settings->startBitrate = 100;
            settings->minBitrate = VCM_MIN_BITRATE;
            settings->maxBitrate = 0;
            settings->maxFramerate = VCM_DEFAULT_FRAME_RATE;
            settings->width = VCM_DEFAULT_CODEC_WIDTH;
            settings->height = VCM_DEFAULT_CODEC_HEIGHT;
            settings->numberOfSimulcastStreams = 0;
            settings->codecSpecific.VP8.resilience = kResilientStream;
            settings->codecSpecific.VP8.numberOfTemporalLayers = 1;
            settings->codecSpecific.VP8.denoisingOn = false;
            break;
        }
#endif
#ifdef VIDEOCODEC_I420
    case VCM_I420_IDX:
        {
            strncpy(settings->plName, "I420", 5);
            settings->codecType = kVideoCodecI420;
            // 96 to 127 dynamic payload types for video codecs
            settings->plType = VCM_I420_PAYLOAD_TYPE;
            // Bitrate needed for this size and framerate
            settings->startBitrate = 3*VCM_DEFAULT_CODEC_WIDTH*
                                       VCM_DEFAULT_CODEC_HEIGHT*8*
                                       VCM_DEFAULT_FRAME_RATE/1000/2;
            settings->maxBitrate = settings->startBitrate;
            settings->maxFramerate = VCM_DEFAULT_FRAME_RATE;
            settings->width = VCM_DEFAULT_CODEC_WIDTH;
            settings->height = VCM_DEFAULT_CODEC_HEIGHT;
            settings->minBitrate = VCM_MIN_BITRATE;
            settings->numberOfSimulcastStreams = 0;
            break;
        }
#endif
    default:
        {
            return VCM_PARAMETER_ERROR;
        }
    }

    return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::Codec(VideoCodecType codecType, VideoCodec* settings)
{
    for (int i = 0; i < VCMCodecDataBase::NumberOfCodecs(); i++)
    {
        const WebRtc_Word32 ret = VCMCodecDataBase::Codec(i, settings);
        if (ret != VCM_OK)
        {
            return ret;
        }
        if (codecType == settings->codecType)
        {
            return VCM_OK;
        }
    }
    return VCM_PARAMETER_ERROR;
}

// assuming only one registered encoder - since only one used, no need for more
WebRtc_Word32
VCMCodecDataBase::RegisterSendCodec(const VideoCodec* sendCodec,
                                    WebRtc_UWord32 numberOfCores,
                                    WebRtc_UWord32 maxPayloadSize)
 {
    if (sendCodec == NULL)
    {
        return VCM_UNINITIALIZED;
    }
    if (maxPayloadSize == 0)
    {
        maxPayloadSize = kDefaultPayloadSize;
    }
    if (numberOfCores > 32)
    {
        return VCM_PARAMETER_ERROR;
    }
    if (sendCodec->plType <= 0)
    {
        return VCM_PARAMETER_ERROR;
    }
    // Make sure the start bit rate is sane...
    if (sendCodec->startBitrate > 1000000)
    {
        return VCM_PARAMETER_ERROR;
    }
    if (sendCodec->codecType == kVideoCodecUnknown)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    _numberOfCores = numberOfCores;
    _maxPayloadSize = maxPayloadSize;

    memcpy(&_sendCodec, sendCodec, sizeof(VideoCodec));

    if (_sendCodec.maxBitrate == 0)
    {
        // max is one bit per pixel
        _sendCodec.maxBitrate = ((WebRtc_Word32)_sendCodec.height *
                                 (WebRtc_Word32)_sendCodec.width *
                                 (WebRtc_Word32)_sendCodec.maxFramerate) / 1000;
        if (_sendCodec.startBitrate > _sendCodec.maxBitrate)
        {
            // but if the customer tries to set a higher start bit rate we will increase
            // the max accordingly
            _sendCodec.maxBitrate = _sendCodec.startBitrate;
        }
    }

    return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::SendCodec(VideoCodec* currentSendCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideoCoding, VCMId(_id), "SendCodec");

    if(_ptrEncoder == NULL)
    {
        return VCM_UNINITIALIZED;
    }
    memcpy(currentSendCodec, &_sendCodec, sizeof(VideoCodec));
    return VCM_OK;
}

VideoCodecType
VCMCodecDataBase::SendCodec() const
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideoCoding, VCMId(_id),
            "SendCodec type");
    if (_ptrEncoder == NULL)
    {
        return kVideoCodecUnknown;
    }
    return _sendCodec.codecType;
}

WebRtc_Word32
VCMCodecDataBase::DeRegisterExternalEncoder(WebRtc_UWord8 payloadType, bool& wasSendCodec)
{
    wasSendCodec = false;
    if (_externalPayloadType != payloadType)
    {
        return VCM_PARAMETER_ERROR;
    }
    if (_sendCodec.plType == payloadType)
    {
        //De-register as send codec if needed
        DeleteEncoder();
        memset(&_sendCodec, 0, sizeof(VideoCodec));
        _currentEncIsExternal = false;
        wasSendCodec = true;
    }
    _externalPayloadType = 0;
    _externalEncoder = NULL;
    _internalSource = false;
    return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                          WebRtc_UWord8 payloadType,
                                          bool internalSource)
{
    // since only one encoder can be used at a given time,
    // only one external encoder can be registered/used
    _externalEncoder = externalEncoder;
    _externalPayloadType = payloadType;
    _internalSource = internalSource;

    return VCM_OK;
}

VCMGenericEncoder*
VCMCodecDataBase::SetEncoder(const VideoCodec* settings,
                             VCMEncodedFrameCallback* VCMencodedFrameCallback)

{
    // if encoder exists, will destroy it and create new one
    DeleteEncoder();

    if (settings->plType == _externalPayloadType)
    {
        // External encoder
        _ptrEncoder = new VCMGenericEncoder(*_externalEncoder, _internalSource);
        _currentEncIsExternal = true;
    }
    else
    {
        _ptrEncoder = CreateEncoder(settings->codecType);
        _currentEncIsExternal = false;
    }
    VCMencodedFrameCallback->SetPayloadType(settings->plType);

    if (_ptrEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Failed to create encoder: %s.",
                     settings->plName);
        return NULL;
    }
    if (_ptrEncoder->InitEncode(settings, _numberOfCores, _maxPayloadSize) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError,
                     webrtc::kTraceVideoCoding,
                     VCMId(_id),
                     "Failed to initialize encoder: %s.",
                     settings->plName);
        DeleteEncoder();
        return NULL;
    }
    else if (_ptrEncoder->RegisterEncodeCallback(VCMencodedFrameCallback) < 0)
    {
        DeleteEncoder();
        return NULL;
    }
    // Intentionally don't check return value since the encoder registration
    // shouldn't fail because the codec doesn't support changing the
    // periodic key frame setting.
    _ptrEncoder->SetPeriodicKeyFrames(_periodicKeyFrames);
    return _ptrEncoder;
}

WebRtc_Word32
VCMCodecDataBase::SetPeriodicKeyFrames(bool enable)
{
    _periodicKeyFrames = enable;
    if (_ptrEncoder != NULL)
    {
        return _ptrEncoder->SetPeriodicKeyFrames(_periodicKeyFrames);
    }
    return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                       WebRtc_UWord32 numberOfCores,
                                       bool requireKeyFrame)
{
    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideoCoding, VCMId(_id),
                 "Codec: %s, Payload type %d, Height %d, Width %d, Bitrate %d, Framerate %d.",
                 receiveCodec->plName, receiveCodec->plType,
                 receiveCodec->height, receiveCodec->width,
                 receiveCodec->startBitrate, receiveCodec->maxFramerate);

    // check if payload value already exists, if so  - erase old and insert new
    DeRegisterReceiveCodec(receiveCodec->plType);
    if (receiveCodec->codecType == kVideoCodecUnknown)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    VideoCodec* newReceiveCodec = new VideoCodec(*receiveCodec);
    _decMap[receiveCodec->plType] =
        new VCMDecoderMapItem(newReceiveCodec, numberOfCores, requireKeyFrame);

    return VCM_OK;
}

WebRtc_Word32 VCMCodecDataBase::DeRegisterReceiveCodec(
    WebRtc_UWord8 payloadType)
{
    DecoderMap::iterator it = _decMap.find(payloadType);
    if (it == _decMap.end())
    {
        return VCM_PARAMETER_ERROR;
    }
    VCMDecoderMapItem* decItem = (*it).second;
    delete decItem->_settings;
    delete decItem;
    _decMap.erase(it);
    if (_receiveCodec.plType == payloadType)
    {
        // This codec is currently in use.
        memset(&_receiveCodec, 0, sizeof(VideoCodec));
        _currentDecIsExternal = false;
    }
    return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::ResetReceiver()
{
    ReleaseDecoder(_ptrDecoder);
    _ptrDecoder = NULL;
    memset(&_receiveCodec, 0, sizeof(VideoCodec));
    DecoderMap::iterator it = _decMap.begin();
    while (it != _decMap.end()) {
        if ((*it).second->_settings != NULL)
        {
            delete (*it).second->_settings;
        }
        delete (*it).second;
        _decMap.erase(it);
        it = _decMap.begin();
    }
    ExternalDecoderMap::iterator exterit = _decExternalMap.begin();
    while (exterit != _decExternalMap.end()) {
        delete (*exterit).second;
        _decExternalMap.erase(exterit);
        exterit = _decExternalMap.begin();
    }

    _currentDecIsExternal = false;
    return VCM_OK;
}

WebRtc_Word32
VCMCodecDataBase::DeRegisterExternalDecoder(WebRtc_UWord8 payloadType)
{
    ExternalDecoderMap::iterator it = _decExternalMap.find(payloadType);
    if (it == _decExternalMap.end())
    {
        // Not found
        return VCM_PARAMETER_ERROR;
    }
    if (_receiveCodec.plType == payloadType)
    {
        // Release it if it was registered and in use
        ReleaseDecoder(_ptrDecoder);
        _ptrDecoder = NULL;
    }
    DeRegisterReceiveCodec(payloadType);
    delete (*it).second;
    _decExternalMap.erase(it);
    return VCM_OK;
}

// Add the external encoder object to the list of external decoders.
// Won't be registered as a receive codec until RegisterReceiveCodec is called.
WebRtc_Word32
VCMCodecDataBase::RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                          WebRtc_UWord8 payloadType,
                                          bool internalRenderTiming)
{
    // check if payload value already exists, if so  - erase old and insert new
    VCMExtDecoderMapItem* extDecoder = new VCMExtDecoderMapItem(externalDecoder,
                                                                payloadType,
                                                                internalRenderTiming);
    if (extDecoder == NULL)
    {
        return VCM_MEMORY;
    }
    DeRegisterExternalDecoder(payloadType);
    _decExternalMap[payloadType] = extDecoder;

    return VCM_OK;
}

bool
VCMCodecDataBase::DecoderRegistered() const
{
    return !_decMap.empty();
}

WebRtc_Word32
VCMCodecDataBase::ReceiveCodec(VideoCodec* currentReceiveCodec) const
{
    if (_ptrDecoder == NULL)
    {
        return VCM_NO_FRAME_DECODED;
    }
    memcpy(currentReceiveCodec, &_receiveCodec, sizeof(VideoCodec));
    return VCM_OK;
}

VideoCodecType
VCMCodecDataBase::ReceiveCodec() const
{
    if (_ptrDecoder == NULL)
    {
        return kVideoCodecUnknown;
    }
    return _receiveCodec.codecType;
}

VCMGenericDecoder*
VCMCodecDataBase::SetDecoder(WebRtc_UWord8 payloadType,
                             VCMDecodedFrameCallback& callback)
{
    if (payloadType == _receiveCodec.plType || payloadType == 0)
    {
        return _ptrDecoder;
    }
    // check for exisitng decoder, if exists - delete
    if (_ptrDecoder)
    {
        ReleaseDecoder(_ptrDecoder);
        _ptrDecoder = NULL;
        memset(&_receiveCodec, 0, sizeof(VideoCodec));
    }
    _ptrDecoder = CreateAndInitDecoder(payloadType, _receiveCodec,
                                       _currentDecIsExternal);
    if (_ptrDecoder == NULL)
    {
        return NULL;
    }
    if (_ptrDecoder->RegisterDecodeCompleteCallback(&callback) < 0)
    {
        ReleaseDecoder(_ptrDecoder);
        _ptrDecoder = NULL;
        memset(&_receiveCodec, 0, sizeof(VideoCodec));
        return NULL;
    }
    return _ptrDecoder;
}

VCMGenericDecoder*
VCMCodecDataBase::CreateAndInitDecoder(WebRtc_UWord8 payloadType,
                                       VideoCodec& newCodec,
                                       bool &external) const
{
    VCMDecoderMapItem* decoderItem = FindDecoderItem(payloadType);
    if (decoderItem == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(_id),
                     "Unknown payload type: %u", payloadType);
        return NULL;
    }
    VCMGenericDecoder* ptrDecoder = NULL;
    VCMExtDecoderMapItem* externalDecItem = FindExternalDecoderItem(
        payloadType);
    if (externalDecItem != NULL)
    {
        // External codec
        ptrDecoder = new VCMGenericDecoder(
            *externalDecItem->_externalDecoderInstance,
            _id,
            true);
        external = true;
    }
    else
    {
        // create decoder
        ptrDecoder = CreateDecoder(decoderItem->_settings->codecType);
        external = false;
    }
    if (ptrDecoder == NULL)
    {
        return NULL;
    }

    if (ptrDecoder->InitDecode(decoderItem->_settings,
                               decoderItem->_numberOfCores,
                               decoderItem->_requireKeyFrame) < 0)
    {
        ReleaseDecoder(ptrDecoder);
        return NULL;
    }
    memcpy(&newCodec, decoderItem->_settings, sizeof(VideoCodec));
    return ptrDecoder;
}

VCMGenericDecoder*
VCMCodecDataBase::CreateDecoderCopy() const
{
    if (_ptrDecoder == NULL)
    {
        return NULL;
    }
    VideoDecoder* decoderCopy = _ptrDecoder->_decoder.Copy();
    if (decoderCopy == NULL)
    {
        return NULL;
    }
    return new VCMGenericDecoder(*decoderCopy, _id, _ptrDecoder->External());
}

void
VCMCodecDataBase::CopyDecoder(const VCMGenericDecoder& decoder)
{
    VideoDecoder* decoderCopy = decoder._decoder.Copy();
    if (decoderCopy != NULL)
    {
        VCMDecodedFrameCallback* cb = _ptrDecoder->_callback;
        ReleaseDecoder(_ptrDecoder);
        _ptrDecoder = new VCMGenericDecoder(*decoderCopy, _id,
                                            decoder.External());
        if (cb && _ptrDecoder->RegisterDecodeCompleteCallback(cb))
        {
            assert(false);
        }
    }
}

bool
VCMCodecDataBase::RenderTiming() const
{
    bool renderTiming = true;
    if (_currentDecIsExternal)
    {
        VCMExtDecoderMapItem* extItem = FindExternalDecoderItem(_receiveCodec.plType);
        renderTiming = extItem->_internalRenderTiming;
    }
    return renderTiming;
}

void
VCMCodecDataBase::ReleaseDecoder(VCMGenericDecoder* decoder) const
{
    if (decoder != NULL)
    {
        assert(&decoder->_decoder != NULL);
        decoder->Release();
        if (!decoder->External())
        {
            delete &decoder->_decoder;
        }
        delete decoder;
    }
}

VCMDecoderMapItem*
VCMCodecDataBase::FindDecoderItem(WebRtc_UWord8 payloadType) const
{
    DecoderMap::const_iterator it = _decMap.find(payloadType);
    if (it != _decMap.end())
    {
        return (*it).second;
    }
    return NULL;
}

VCMExtDecoderMapItem*
VCMCodecDataBase::FindExternalDecoderItem(WebRtc_UWord8 payloadType) const
{
    ExternalDecoderMap::const_iterator it = _decExternalMap.find(payloadType);
    if (it != _decExternalMap.end())
    {
        return (*it).second;
    }
    return NULL;
}

VCMGenericDecoder*
VCMCodecDataBase::CreateDecoder(VideoCodecType type) const
{
    switch(type)
    {
#ifdef VIDEOCODEC_VP8
    case kVideoCodecVP8:
        return new VCMGenericDecoder(*(VP8Decoder::Create()), _id);
#endif
#ifdef VIDEOCODEC_I420
    case kVideoCodecI420:
         return new VCMGenericDecoder(*(new I420Decoder), _id);
#endif
    default:
        return NULL;
    }
}
}
