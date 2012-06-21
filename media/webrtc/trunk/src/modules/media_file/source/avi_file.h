/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Class for reading (x)or writing to an AVI file.
// Note: the class cannot be used for reading and writing at the same time.
#ifndef WEBRTC_MODULES_MEDIA_FILE_SOURCE_AVI_FILE_H_
#define WEBRTC_MODULES_MEDIA_FILE_SOURCE_AVI_FILE_H_

#include <stdio.h>

#include "typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
class ListWrapper;

struct AVISTREAMHEADER
{
    AVISTREAMHEADER();
    WebRtc_UWord32 fcc;
    WebRtc_UWord32 cb;
    WebRtc_UWord32 fccType;
    WebRtc_UWord32 fccHandler;
    WebRtc_UWord32 dwFlags;
    WebRtc_UWord16 wPriority;
    WebRtc_UWord16 wLanguage;
    WebRtc_UWord32 dwInitialFrames;
    WebRtc_UWord32 dwScale;
    WebRtc_UWord32 dwRate;
    WebRtc_UWord32 dwStart;
    WebRtc_UWord32 dwLength;
    WebRtc_UWord32 dwSuggestedBufferSize;
    WebRtc_UWord32 dwQuality;
    WebRtc_UWord32 dwSampleSize;
    struct
    {
        WebRtc_Word16 left;
        WebRtc_Word16 top;
        WebRtc_Word16 right;
        WebRtc_Word16 bottom;
    } rcFrame;
};

struct BITMAPINFOHEADER
{
    BITMAPINFOHEADER();
    WebRtc_UWord32 biSize;
    WebRtc_UWord32 biWidth;
    WebRtc_UWord32 biHeight;
    WebRtc_UWord16 biPlanes;
    WebRtc_UWord16 biBitCount;
    WebRtc_UWord32 biCompression;
    WebRtc_UWord32 biSizeImage;
    WebRtc_UWord32 biXPelsPerMeter;
    WebRtc_UWord32 biYPelsPerMeter;
    WebRtc_UWord32 biClrUsed;
    WebRtc_UWord32 biClrImportant;
};

struct WAVEFORMATEX
{
    WAVEFORMATEX();
    WebRtc_UWord16 wFormatTag;
    WebRtc_UWord16 nChannels;
    WebRtc_UWord32 nSamplesPerSec;
    WebRtc_UWord32 nAvgBytesPerSec;
    WebRtc_UWord16 nBlockAlign;
    WebRtc_UWord16 wBitsPerSample;
    WebRtc_UWord16 cbSize;
};

class AviFile
{
public:
    enum AVIStreamType
    {
        AVI_AUDIO = 0,
        AVI_VIDEO = 1
    };

    // Unsigned, for comparison with must-be-unsigned types.
    static const unsigned int CODEC_CONFIG_LENGTH = 64;
    static const unsigned int STREAM_NAME_LENGTH  = 32;

    AviFile();
    ~AviFile();

    WebRtc_Word32 Open(AVIStreamType streamType, const char* fileName,
                       bool loop = false);

    WebRtc_Word32 CreateVideoStream(const AVISTREAMHEADER& videoStreamHeader,
                                    const BITMAPINFOHEADER& bitMapInfoHeader,
                                    const WebRtc_UWord8* codecConfigParams,
                                    WebRtc_Word32 codecConfigParamsLength);

    WebRtc_Word32 CreateAudioStream(const AVISTREAMHEADER& audioStreamHeader,
                                    const WAVEFORMATEX& waveFormatHeader);
    WebRtc_Word32 Create(const char* fileName);

    WebRtc_Word32 WriteAudio(const WebRtc_UWord8* data, WebRtc_Word32 length);
    WebRtc_Word32 WriteVideo(const WebRtc_UWord8* data, WebRtc_Word32 length);

    WebRtc_Word32 GetVideoStreamInfo(AVISTREAMHEADER& videoStreamHeader,
                                     BITMAPINFOHEADER& bitmapInfo,
                                     char* codecConfigParameters,
                                     WebRtc_Word32& configLength);

    WebRtc_Word32 GetDuration(WebRtc_Word32& durationMs);

    WebRtc_Word32 GetAudioStreamInfo(WAVEFORMATEX& waveHeader);

    WebRtc_Word32 ReadAudio(WebRtc_UWord8* data, WebRtc_Word32& length);
    WebRtc_Word32 ReadVideo(WebRtc_UWord8* data, WebRtc_Word32& length);

    WebRtc_Word32 Close();

    static WebRtc_UWord32 MakeFourCc(WebRtc_UWord8 ch0, WebRtc_UWord8 ch1,
                                     WebRtc_UWord8 ch2, WebRtc_UWord8 ch3);

private:
    enum AVIFileMode
    {
        NotSet,
        Read,
        Write
    };

    struct AVIINDEXENTRY
    {
        AVIINDEXENTRY(WebRtc_UWord32 inckid, WebRtc_UWord32 indwFlags,
                      WebRtc_UWord32 indwChunkOffset,
                      WebRtc_UWord32 indwChunkLength);
        WebRtc_UWord32 ckid;
        WebRtc_UWord32 dwFlags;
        WebRtc_UWord32 dwChunkOffset;
        WebRtc_UWord32 dwChunkLength;
    };

    WebRtc_Word32 PrepareDataChunkHeaders();

    WebRtc_Word32 ReadMoviSubChunk(WebRtc_UWord8* data, WebRtc_Word32& length,
                                   WebRtc_UWord32 tag1,
                                   WebRtc_UWord32 tag2 = 0);

    WebRtc_Word32 WriteRIFF();
    WebRtc_Word32 WriteHeaders();
    WebRtc_Word32 WriteAVIMainHeader();
    WebRtc_Word32 WriteAVIStreamHeaders();
    WebRtc_Word32 WriteAVIVideoStreamHeaders();
    WebRtc_Word32 WriteAVIVideoStreamHeaderChunks();
    WebRtc_Word32 WriteAVIAudioStreamHeaders();
    WebRtc_Word32 WriteAVIAudioStreamHeaderChunks();

    WebRtc_Word32 WriteMoviStart();

    size_t PutByte(WebRtc_UWord8 byte);
    size_t PutLE16(WebRtc_UWord16 word);
    size_t PutLE32(WebRtc_UWord32 word);
    size_t PutBuffer(const WebRtc_UWord8* str, size_t size);
    size_t PutBufferZ(const char* str);
    long PutLE32LengthFromCurrent(long startPos);
    void PutLE32AtPos(long pos, WebRtc_UWord32 word);

    size_t GetByte(WebRtc_UWord8& word);
    size_t GetLE16(WebRtc_UWord16& word);
    size_t GetLE32(WebRtc_UWord32& word);
    size_t GetBuffer(WebRtc_UWord8* str, size_t size);

    void CloseRead();
    void CloseWrite();

    void ResetMembers();
    void ResetComplexMembers();

    WebRtc_Word32 ReadRIFF();
    WebRtc_Word32 ReadHeaders();
    WebRtc_Word32 ReadAVIMainHeader();
    WebRtc_Word32 ReadAVIVideoStreamHeader(WebRtc_Word32 endpos);
    WebRtc_Word32 ReadAVIAudioStreamHeader(WebRtc_Word32 endpos);

    WebRtc_UWord32 StreamAndTwoCharCodeToTag(WebRtc_Word32 streamNum,
                                             const char* twoCharCode);

    void ClearIndexList();
    void AddChunkToIndexList(WebRtc_UWord32 inChunkId, WebRtc_UWord32 inFlags,
                             WebRtc_UWord32 inOffset,  WebRtc_UWord32 inSize);

    void WriteIndex();

private:
    struct AVIMAINHEADER
    {
        AVIMAINHEADER();
        WebRtc_UWord32 fcc;
        WebRtc_UWord32 cb;
        WebRtc_UWord32 dwMicroSecPerFrame;
        WebRtc_UWord32 dwMaxBytesPerSec;
        WebRtc_UWord32 dwPaddingGranularity;
        WebRtc_UWord32 dwFlags;
        WebRtc_UWord32 dwTotalFrames;
        WebRtc_UWord32 dwInitialFrames;
        WebRtc_UWord32 dwStreams;
        WebRtc_UWord32 dwSuggestedBufferSize;
        WebRtc_UWord32 dwWidth;
        WebRtc_UWord32 dwHeight;
        WebRtc_UWord32 dwReserved[4];
    };

    struct AVIStream
    {
        AVIStreamType streamType;
        int           streamNumber;
    };

    CriticalSectionWrapper* _crit;
    FILE*            _aviFile;
    AVIMAINHEADER    _aviHeader;
    AVISTREAMHEADER  _videoStreamHeader;
    AVISTREAMHEADER  _audioStreamHeader;
    BITMAPINFOHEADER _videoFormatHeader;
    WAVEFORMATEX     _audioFormatHeader;

    WebRtc_Word8 _videoConfigParameters[CODEC_CONFIG_LENGTH];
    WebRtc_Word32 _videoConfigLength;
    WebRtc_Word8 _videoStreamName[STREAM_NAME_LENGTH];
    WebRtc_Word32 _videoStreamNameLength;
    WebRtc_Word8 _audioConfigParameters[CODEC_CONFIG_LENGTH];
    WebRtc_Word8 _audioStreamName[STREAM_NAME_LENGTH];

    AVIStream _videoStream;
    AVIStream _audioStream;

    WebRtc_Word32 _nrStreams;
    WebRtc_Word32 _aviLength;
    WebRtc_Word32 _dataLength;
    size_t        _bytesRead;
    size_t        _dataStartByte;
    WebRtc_Word32 _framesRead;
    WebRtc_Word32 _videoFrames;
    WebRtc_Word32 _audioFrames;

    bool _reading;
    AVIStreamType _openedAs;
    bool _loop;
    bool _writing;

    size_t _bytesWritten;

    size_t _riffSizeMark;
    size_t _moviSizeMark;
    size_t _totNumFramesMark;
    size_t _videoStreamLengthMark;
    size_t _audioStreamLengthMark;
    WebRtc_Word32 _moviListOffset;

    bool _writeAudioStream;
    bool _writeVideoStream;

    AVIFileMode _aviMode;
    WebRtc_UWord8* _videoCodecConfigParams;
    WebRtc_Word32 _videoCodecConfigParamsLength;

    WebRtc_UWord32 _videoStreamDataChunkPrefix;
    WebRtc_UWord32 _audioStreamDataChunkPrefix;
    bool _created;

    ListWrapper* _indexList; // Elements are of type AVIINDEXENTRY.
};
} // namespace webrtc

#endif // WEBRTC_MODULES_MEDIA_FILE_SOURCE_AVI_FILE_H_
