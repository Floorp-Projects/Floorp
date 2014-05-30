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
#include <list>

#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;

struct AVISTREAMHEADER
{
    AVISTREAMHEADER();
    uint32_t fcc;
    uint32_t cb;
    uint32_t fccType;
    uint32_t fccHandler;
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    struct
    {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } rcFrame;
};

struct BITMAPINFOHEADER
{
    BITMAPINFOHEADER();
    uint32_t biSize;
    uint32_t biWidth;
    uint32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    uint32_t biXPelsPerMeter;
    uint32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};

struct WAVEFORMATEX
{
    WAVEFORMATEX();
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
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

    int32_t Open(AVIStreamType streamType, const char* fileName,
                 bool loop = false);

    int32_t CreateVideoStream(const AVISTREAMHEADER& videoStreamHeader,
                              const BITMAPINFOHEADER& bitMapInfoHeader,
                              const uint8_t* codecConfigParams,
                              int32_t codecConfigParamsLength);

    int32_t CreateAudioStream(const AVISTREAMHEADER& audioStreamHeader,
                              const WAVEFORMATEX& waveFormatHeader);
    int32_t Create(const char* fileName);

    int32_t WriteAudio(const uint8_t* data, int32_t length);
    int32_t WriteVideo(const uint8_t* data, int32_t length);

    int32_t GetVideoStreamInfo(AVISTREAMHEADER& videoStreamHeader,
                               BITMAPINFOHEADER& bitmapInfo,
                               char* codecConfigParameters,
                               int32_t& configLength);

    int32_t GetDuration(int32_t& durationMs);

    int32_t GetAudioStreamInfo(WAVEFORMATEX& waveHeader);

    int32_t ReadAudio(uint8_t* data, int32_t& length);
    int32_t ReadVideo(uint8_t* data, int32_t& length);

    int32_t Close();

    static uint32_t MakeFourCc(uint8_t ch0, uint8_t ch1, uint8_t ch2,
                               uint8_t ch3);

private:
    enum AVIFileMode
    {
        NotSet,
        Read,
        Write
    };

    struct AVIINDEXENTRY
    {
        AVIINDEXENTRY(uint32_t inckid, uint32_t indwFlags,
                      uint32_t indwChunkOffset,
                      uint32_t indwChunkLength);
        uint32_t ckid;
        uint32_t dwFlags;
        uint32_t dwChunkOffset;
        uint32_t dwChunkLength;
    };

    int32_t PrepareDataChunkHeaders();

    int32_t ReadMoviSubChunk(uint8_t* data, int32_t& length, uint32_t tag1,
                             uint32_t tag2 = 0);

    int32_t WriteRIFF();
    int32_t WriteHeaders();
    int32_t WriteAVIMainHeader();
    int32_t WriteAVIStreamHeaders();
    int32_t WriteAVIVideoStreamHeaders();
    int32_t WriteAVIVideoStreamHeaderChunks();
    int32_t WriteAVIAudioStreamHeaders();
    int32_t WriteAVIAudioStreamHeaderChunks();

    int32_t WriteMoviStart();

    size_t PutByte(uint8_t byte);
    size_t PutLE16(uint16_t word);
    size_t PutLE32(uint32_t word);
    size_t PutBuffer(const uint8_t* str, size_t size);
    size_t PutBufferZ(const char* str);
    long PutLE32LengthFromCurrent(long startPos);
    void PutLE32AtPos(long pos, uint32_t word);

    size_t GetByte(uint8_t& word);
    size_t GetLE16(uint16_t& word);
    size_t GetLE32(uint32_t& word);
    size_t GetBuffer(uint8_t* str, size_t size);

    void CloseRead();
    void CloseWrite();

    void ResetMembers();
    void ResetComplexMembers();

    int32_t ReadRIFF();
    int32_t ReadHeaders();
    int32_t ReadAVIMainHeader();
    int32_t ReadAVIVideoStreamHeader(int32_t endpos);
    int32_t ReadAVIAudioStreamHeader(int32_t endpos);

    uint32_t StreamAndTwoCharCodeToTag(int32_t streamNum,
                                       const char* twoCharCode);

    void ClearIndexList();
    void AddChunkToIndexList(uint32_t inChunkId, uint32_t inFlags,
                             uint32_t inOffset,  uint32_t inSize);

    void WriteIndex();

private:
    typedef std::list<AVIINDEXENTRY*> IndexList;
    struct AVIMAINHEADER
    {
        AVIMAINHEADER();
        uint32_t fcc;
        uint32_t cb;
        uint32_t dwMicroSecPerFrame;
        uint32_t dwMaxBytesPerSec;
        uint32_t dwPaddingGranularity;
        uint32_t dwFlags;
        uint32_t dwTotalFrames;
        uint32_t dwInitialFrames;
        uint32_t dwStreams;
        uint32_t dwSuggestedBufferSize;
        uint32_t dwWidth;
        uint32_t dwHeight;
        uint32_t dwReserved[4];
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

    int8_t _videoConfigParameters[CODEC_CONFIG_LENGTH];
    int32_t _videoConfigLength;
    int8_t _videoStreamName[STREAM_NAME_LENGTH];
    int8_t _audioConfigParameters[CODEC_CONFIG_LENGTH];
    int8_t _audioStreamName[STREAM_NAME_LENGTH];

    AVIStream _videoStream;
    AVIStream _audioStream;

    int32_t _nrStreams;
    int32_t _aviLength;
    int32_t _dataLength;
    size_t        _bytesRead;
    size_t        _dataStartByte;
    int32_t _framesRead;
    int32_t _videoFrames;
    int32_t _audioFrames;

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
    int32_t _moviListOffset;

    bool _writeAudioStream;
    bool _writeVideoStream;

    AVIFileMode _aviMode;
    uint8_t* _videoCodecConfigParams;
    int32_t _videoCodecConfigParamsLength;

    uint32_t _videoStreamDataChunkPrefix;
    uint32_t _audioStreamDataChunkPrefix;
    bool _created;

    IndexList _indexList;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_MEDIA_FILE_SOURCE_AVI_FILE_H_
