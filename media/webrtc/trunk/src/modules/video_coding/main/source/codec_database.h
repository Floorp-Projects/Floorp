/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODEC_DATABASE_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODEC_DATABASE_H_

#include <map>

#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "modules/video_coding/main/source/generic_decoder.h"
#include "modules/video_coding/main/source/generic_encoder.h"
#include "typedefs.h"

namespace webrtc
{

enum VCMCodecDBProperties
{
    kDefaultPayloadSize = 1440
};

class VCMDecoderMapItem {
public:
    VCMDecoderMapItem(VideoCodec* settings,
                      WebRtc_UWord32 numberOfCores,
                      bool requireKeyFrame);

    VideoCodec*     _settings;
    WebRtc_UWord32  _numberOfCores;
    bool            _requireKeyFrame;
};

class VCMExtDecoderMapItem {
public:
    VCMExtDecoderMapItem(VideoDecoder* externalDecoderInstance,
                         WebRtc_UWord8 payloadType,
                         bool internalRenderTiming);

    WebRtc_UWord8   _payloadType;
    VideoDecoder*   _externalDecoderInstance;
    bool            _internalRenderTiming;
};

/*******************************/
/* VCMCodecDataBase class      */
/*******************************/
class VCMCodecDataBase
{
public:
    VCMCodecDataBase(WebRtc_Word32 id);
    ~VCMCodecDataBase();
    /**
    * Release codecdatabase - release all memory for both send and receive side
    */
    WebRtc_Word32 Reset();
    /**
    * Sender Side
    */
    /**
    * Returns the number of supported codecs (or -1 in case of error).
    */
    static WebRtc_UWord8 NumberOfCodecs();
    /**
    * Get supported codecs with ID
    * Input Values:
    *       listnr    : Requested codec id number
    *       codec_inst: Pointer to the struct in which the returned codec information is copied
    * Return Values: 0 if successful, otherwise
    */
    static WebRtc_Word32 Codec(WebRtc_UWord8 listId, VideoCodec* settings);
    static WebRtc_Word32 Codec(VideoCodecType codecType, VideoCodec* settings);
    /**
    * Reset Sender side
    */
    WebRtc_Word32 ResetSender();
    /**
    * Setting the sender side codec and initiaiting the desired codec given the VideoCodec
    * struct.
    * Return Value:	0 if the codec and the settings are supported, otherwise
    */
    WebRtc_Word32 RegisterSendCodec(const VideoCodec* sendCodec,
                                  WebRtc_UWord32 numberOfCores,
                                  WebRtc_UWord32 maxPayloadSize);
    /**
    * Get current send side codec. Relevant for internal codecs only.
    */
    WebRtc_Word32 SendCodec(VideoCodec* currentSendCodec) const;
    /**
    * Get current send side codec type. Relevant for internal codecs only.
    */
    VideoCodecType SendCodec() const;
    /**
    * Register external encoder - current assumption - if one is registered then it will also
    * be used, and therefore it is also initialized
    * Return value: A pointer to the encoder on success, or null, in case of an error.
    */
    WebRtc_Word32 DeRegisterExternalEncoder(WebRtc_UWord8 payloadType, bool& wasSendCodec);
    WebRtc_Word32 RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                        WebRtc_UWord8 payloadType,
                                        bool internalSource);
    /**
    * Returns a encoder given a payloadname - to be used with internal encoders only.
    * Special cases:
    *	 Encoder exists -  If payload matches, returns existing one, otherwise,
    *	 deletes existing one and creates new one.
    *	 No match found / Error - returns NULL.
    */
    VCMGenericEncoder* SetEncoder(const VideoCodec* settings,
                                  VCMEncodedFrameCallback* VCMencodedFrameCallback);

    WebRtc_Word32 SetPeriodicKeyFrames(bool enable);

    bool InternalSource() const;

    /*
    * Receiver Side
    */
    WebRtc_Word32 ResetReceiver();
    /**
    * Register external decoder/render object
    */
    WebRtc_Word32 DeRegisterExternalDecoder(WebRtc_UWord8 payloadType);
    WebRtc_Word32 RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                        WebRtc_UWord8 payloadType,
                                        bool internalRenderTiming);

    bool DecoderRegistered() const;
    /**
    * Register recieve codec
    */
    WebRtc_Word32 RegisterReceiveCodec(const VideoCodec* receiveCodec,
                                     WebRtc_UWord32 numberOfCores,
                                     bool requireKeyFrame);
    WebRtc_Word32 DeRegisterReceiveCodec(WebRtc_UWord8 payloadType);
    /**
    * Get current receive side codec. Relevant for internal codecs only.
    */
    WebRtc_Word32 ReceiveCodec(VideoCodec* currentReceiveCodec) const;
    /**
    * Get current receive side codec type. Relevant for internal codecs only.
    */
    VideoCodecType ReceiveCodec() const;
    /**
    * Returns a decoder given which matches a payload type.
    * Special cases:
    *	 Decoder exists -  If payload matches, returns existing one, otherwise, deletes
    *	 existing one, and creates new one.
    *	 No match found / Error - returns NULL.
    */
    VCMGenericDecoder* SetDecoder(WebRtc_UWord8 payloadType, VCMDecodedFrameCallback& callback);

    VCMGenericDecoder* CreateAndInitDecoder(WebRtc_UWord8 payloadType,
                                            VideoCodec& newCodec,
                                            bool &external) const;

    VCMGenericDecoder* CreateDecoderCopy() const;

    void ReleaseDecoder(VCMGenericDecoder* decoder) const;

    void CopyDecoder(const VCMGenericDecoder& decoder);

    bool RenderTiming() const;

protected:
    /**
    * Create an internal Encoder given a codec type
    */
    VCMGenericEncoder* CreateEncoder(const VideoCodecType type) const;

    void DeleteEncoder();
    /*
    * Create an internal Decoder given a codec type
    */
    VCMGenericDecoder* CreateDecoder(VideoCodecType type) const;

    VCMDecoderMapItem* FindDecoderItem(WebRtc_UWord8 payloadType) const;

    VCMExtDecoderMapItem* FindExternalDecoderItem(WebRtc_UWord8 payloadType) const;

private:
    typedef std::map<uint8_t, VCMDecoderMapItem*> DecoderMap;
    typedef std::map<uint8_t, VCMExtDecoderMapItem*> ExternalDecoderMap;
    WebRtc_Word32 _id;
    WebRtc_UWord32 _numberOfCores;
    WebRtc_UWord32 _maxPayloadSize;
    bool _periodicKeyFrames;
    bool _currentEncIsExternal;
    VideoCodec _sendCodec;
    VideoCodec _receiveCodec;
    WebRtc_UWord8 _externalPayloadType;
    VideoEncoder* _externalEncoder;
    bool _internalSource;
    VCMGenericEncoder* _ptrEncoder;
    VCMGenericDecoder* _ptrDecoder;
    bool _currentDecIsExternal;
    DecoderMap _decMap;
    ExternalDecoderMap _decExternalMap;
}; // end of VCMCodecDataBase class definition

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CODEC_DATABASE_H_
