/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/media_file/source/media_file_utility.h"

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    #include "avi_file.h"
#endif

namespace {
enum WaveFormats
{
    kWaveFormatPcm   = 0x0001,
    kWaveFormatALaw  = 0x0006,
    kWaveFormatMuLaw = 0x0007
};

// First 16 bytes the WAVE header. ckID should be "RIFF", wave_ckID should be
// "WAVE" and ckSize is the chunk size (4 + n)
struct WAVE_RIFF_header
{
    int8_t  ckID[4];
    int32_t ckSize;
    int8_t  wave_ckID[4];
};

// First 8 byte of the format chunk. fmt_ckID should be "fmt ". fmt_ckSize is
// the chunk size (16, 18 or 40 byte)
struct WAVE_CHUNK_header
{
   int8_t  fmt_ckID[4];
   int32_t fmt_ckSize;
};
}  // unnamed namespace

namespace webrtc {
ModuleFileUtility::ModuleFileUtility(const int32_t id)
    : _wavFormatObj(),
      _dataSize(0),
      _readSizeBytes(0),
      _id(id),
      _stopPointInMs(0),
      _startPointInMs(0),
      _playoutPositionMs(0),
      _bytesWritten(0),
      codec_info_(),
      _codecId(kCodecNoCodec),
      _bytesPerSample(0),
      _readPos(0),
      _reading(false),
      _writing(false),
      _tempData()
#ifdef WEBRTC_MODULE_UTILITY_VIDEO
      ,
      _aviAudioInFile(0),
      _aviVideoInFile(0),
      _aviOutFile(0)
#endif
{
    WEBRTC_TRACE(kTraceMemory, kTraceFile, _id,
                 "ModuleFileUtility::ModuleFileUtility()");
    memset(&codec_info_,0,sizeof(CodecInst));
    codec_info_.pltype = -1;
#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    memset(&_videoCodec,0,sizeof(_videoCodec));
#endif
}

ModuleFileUtility::~ModuleFileUtility()
{
    WEBRTC_TRACE(kTraceMemory, kTraceFile, _id,
                 "ModuleFileUtility::~ModuleFileUtility()");
#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    delete _aviAudioInFile;
    delete _aviVideoInFile;
#endif
}

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
int32_t ModuleFileUtility::InitAviWriting(
    const char* filename,
    const CodecInst& audioCodecInst,
    const VideoCodec& videoCodecInst,
    const bool videoOnly /*= false*/)
{
    _writing = false;

    delete _aviOutFile;
    _aviOutFile = new AviFile( );

    AVISTREAMHEADER videoStreamHeader;
    videoStreamHeader.fccType = AviFile::MakeFourCc('v', 'i', 'd', 's');

#ifdef VIDEOCODEC_I420
    if (strncmp(videoCodecInst.plName, "I420", 7) == 0)
    {
        videoStreamHeader.fccHandler = AviFile::MakeFourCc('I','4','2','0');
    }
#endif
#ifdef VIDEOCODEC_VP8
    if (strncmp(videoCodecInst.plName, "VP8", 7) == 0)
    {
        videoStreamHeader.fccHandler = AviFile::MakeFourCc('V','P','8','0');
    }
#endif
    if (videoStreamHeader.fccHandler == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "InitAviWriting() Codec not supported");

        return -1;
    }
    videoStreamHeader.dwScale                = 1;
    videoStreamHeader.dwRate                 = videoCodecInst.maxFramerate;
    videoStreamHeader.dwSuggestedBufferSize  = videoCodecInst.height *
        (videoCodecInst.width >> 1) * 3;
    videoStreamHeader.dwQuality              = (uint32_t)-1;
    videoStreamHeader.dwSampleSize           = 0;
    videoStreamHeader.rcFrame.top            = 0;
    videoStreamHeader.rcFrame.bottom         = videoCodecInst.height;
    videoStreamHeader.rcFrame.left           = 0;
    videoStreamHeader.rcFrame.right          = videoCodecInst.width;

    BITMAPINFOHEADER bitMapInfoHeader;
    bitMapInfoHeader.biSize         = sizeof(BITMAPINFOHEADER);
    bitMapInfoHeader.biHeight       = videoCodecInst.height;
    bitMapInfoHeader.biWidth        = videoCodecInst.width;
    bitMapInfoHeader.biPlanes       = 1;
    bitMapInfoHeader.biBitCount     = 12;
    bitMapInfoHeader.biClrImportant = 0;
    bitMapInfoHeader.biClrUsed      = 0;
    bitMapInfoHeader.biCompression  = videoStreamHeader.fccHandler;
    bitMapInfoHeader.biSizeImage    = bitMapInfoHeader.biWidth *
        bitMapInfoHeader.biHeight * bitMapInfoHeader.biBitCount / 8;

    if (_aviOutFile->CreateVideoStream(
        videoStreamHeader,
        bitMapInfoHeader,
        NULL,
        0) != 0)
    {
        return -1;
    }

    if(!videoOnly)
    {
        AVISTREAMHEADER audioStreamHeader;
        audioStreamHeader.fccType = AviFile::MakeFourCc('a', 'u', 'd', 's');
        // fccHandler is the FOURCC of the codec for decoding the stream.
        // It's an optional parameter that is not used by audio streams.
        audioStreamHeader.fccHandler   = 0;
        audioStreamHeader.dwScale      = 1;

        WAVEFORMATEX waveFormatHeader;
        waveFormatHeader.cbSize          = 0;
        waveFormatHeader.nChannels       = 1;

        if (strncmp(audioCodecInst.plname, "PCMU", 4) == 0)
        {
            audioStreamHeader.dwSampleSize = 1;
            audioStreamHeader.dwRate       = 8000;
            audioStreamHeader.dwQuality    = (uint32_t)-1;
            audioStreamHeader.dwSuggestedBufferSize = 80;

            waveFormatHeader.nAvgBytesPerSec = 8000;
            waveFormatHeader.nSamplesPerSec  = 8000;
            waveFormatHeader.wBitsPerSample  = 8;
            waveFormatHeader.nBlockAlign     = 1;
            waveFormatHeader.wFormatTag      = kWaveFormatMuLaw;

        } else if (strncmp(audioCodecInst.plname, "PCMA", 4) == 0)
        {
            audioStreamHeader.dwSampleSize = 1;
            audioStreamHeader.dwRate       = 8000;
            audioStreamHeader.dwQuality    = (uint32_t)-1;
            audioStreamHeader.dwSuggestedBufferSize = 80;

            waveFormatHeader.nAvgBytesPerSec = 8000;
            waveFormatHeader.nSamplesPerSec  = 8000;
            waveFormatHeader.wBitsPerSample  = 8;
            waveFormatHeader.nBlockAlign     = 1;
            waveFormatHeader.wFormatTag      = kWaveFormatALaw;

        } else if (strncmp(audioCodecInst.plname, "L16", 3) == 0)
        {
            audioStreamHeader.dwSampleSize = 2;
            audioStreamHeader.dwRate       = audioCodecInst.plfreq;
            audioStreamHeader.dwQuality    = (uint32_t)-1;
            audioStreamHeader.dwSuggestedBufferSize =
                (audioCodecInst.plfreq/100) * 2;

            waveFormatHeader.nAvgBytesPerSec = audioCodecInst.plfreq * 2;
            waveFormatHeader.nSamplesPerSec  = audioCodecInst.plfreq;
            waveFormatHeader.wBitsPerSample  = 16;
            waveFormatHeader.nBlockAlign     = 2;
            waveFormatHeader.wFormatTag      = kWaveFormatPcm;
        } else
        {
            return -1;
        }

        if(_aviOutFile->CreateAudioStream(
            audioStreamHeader,
            waveFormatHeader) != 0)
        {
            return -1;
        }


        if( InitWavCodec(waveFormatHeader.nSamplesPerSec,
            waveFormatHeader.nChannels,
            waveFormatHeader.wBitsPerSample,
            waveFormatHeader.wFormatTag) != 0)
        {
            return -1;
        }
    }
    _aviOutFile->Create(filename);
    _writing = true;
    return 0;
}

int32_t ModuleFileUtility::WriteAviAudioData(
    const int8_t* buffer,
    uint32_t bufferLengthInBytes)
{
    if( _aviOutFile != 0)
    {
        return _aviOutFile->WriteAudio(
            reinterpret_cast<const uint8_t*>(buffer),
            bufferLengthInBytes);
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "AVI file not initialized");
        return -1;
    }
}

int32_t ModuleFileUtility::WriteAviVideoData(
        const int8_t* buffer,
        uint32_t bufferLengthInBytes)
{
    if( _aviOutFile != 0)
    {
        return _aviOutFile->WriteVideo(
            reinterpret_cast<const uint8_t*>(buffer),
            bufferLengthInBytes);
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "AVI file not initialized");
        return -1;
    }
}


int32_t ModuleFileUtility::CloseAviFile( )
{
    if( _reading && _aviAudioInFile)
    {
        delete _aviAudioInFile;
        _aviAudioInFile = 0;
    }

    if( _reading && _aviVideoInFile)
    {
        delete _aviVideoInFile;
        _aviVideoInFile = 0;
    }

    if( _writing && _aviOutFile)
    {
        delete _aviOutFile;
        _aviOutFile = 0;
    }
    return 0;
}


int32_t ModuleFileUtility::InitAviReading(const char* filename, bool videoOnly,
                                          bool loop)
{
    _reading = false;
    delete _aviVideoInFile;
    _aviVideoInFile = new AviFile( );

    if ((_aviVideoInFile != 0) && _aviVideoInFile->Open(AviFile::AVI_VIDEO,
                                                        filename, loop) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Unable to open AVI file (video)");
        return -1;
    }


    AVISTREAMHEADER videoInStreamHeader;
    BITMAPINFOHEADER bitmapInfo;
    char codecConfigParameters[AviFile::CODEC_CONFIG_LENGTH] = {};
    int32_t configLength = 0;
    if( _aviVideoInFile->GetVideoStreamInfo(videoInStreamHeader, bitmapInfo,
                                            codecConfigParameters,
                                            configLength) != 0)
    {
        return -1;
    }
    _videoCodec.width = static_cast<uint16_t>(
        videoInStreamHeader.rcFrame.right);
    _videoCodec.height = static_cast<uint16_t>(
        videoInStreamHeader.rcFrame.bottom);
    _videoCodec.maxFramerate = static_cast<uint8_t>(
        videoInStreamHeader.dwRate);

    const size_t plnameLen = sizeof(_videoCodec.plName) / sizeof(char);
    if (bitmapInfo.biCompression == AviFile::MakeFourCc('I','4','2','0'))
    {
        strncpy(_videoCodec.plName, "I420", plnameLen);
       _videoCodec.codecType = kVideoCodecI420;
    }
    else if (bitmapInfo.biCompression ==
             AviFile::MakeFourCc('V', 'P', '8', '0'))
    {
        strncpy(_videoCodec.plName, "VP8", plnameLen);
        _videoCodec.codecType = kVideoCodecVP8;
    }
    else
    {
        return -1;
    }

    if(!videoOnly)
    {
        delete _aviAudioInFile;
        _aviAudioInFile = new AviFile();

        if ( (_aviAudioInFile != 0) &&
            _aviAudioInFile->Open(AviFile::AVI_AUDIO, filename, loop) == -1)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "Unable to open AVI file (audio)");
            return -1;
        }

        WAVEFORMATEX waveHeader;
        if(_aviAudioInFile->GetAudioStreamInfo(waveHeader) != 0)
        {
            return -1;
        }
        if(InitWavCodec(waveHeader.nSamplesPerSec, waveHeader.nChannels,
                        waveHeader.wBitsPerSample, waveHeader.wFormatTag) != 0)
        {
            return -1;
        }
    }
    _reading = true;
    return 0;
}

int32_t ModuleFileUtility::ReadAviAudioData(
    int8_t*  outBuffer,
    const uint32_t bufferLengthInBytes)
{
    if(_aviAudioInFile == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "AVI file not opened.");
        return -1;
    }

    int32_t length = bufferLengthInBytes;
    if(_aviAudioInFile->ReadAudio(
        reinterpret_cast<uint8_t*>(outBuffer),
        length) != 0)
    {
        return -1;
    }
    else
    {
        return length;
    }
}

int32_t ModuleFileUtility::ReadAviVideoData(
    int8_t* outBuffer,
    const uint32_t bufferLengthInBytes)
{
    if(_aviVideoInFile == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "AVI file not opened.");
        return -1;
    }

    int32_t length = bufferLengthInBytes;
    if( _aviVideoInFile->ReadVideo(
        reinterpret_cast<uint8_t*>(outBuffer),
        length) != 0)
    {
        return -1;
    } else {
        return length;
    }
}

int32_t ModuleFileUtility::VideoCodecInst(VideoCodec& codecInst)
{
    WEBRTC_TRACE(kTraceStream, kTraceFile, _id,
               "ModuleFileUtility::CodecInst(codecInst= 0x%x)", &codecInst);

   if(!_reading)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "CodecInst: not currently reading audio file!");
        return -1;
    }
    memcpy(&codecInst,&_videoCodec,sizeof(VideoCodec));
    return 0;
}
#endif

int32_t ModuleFileUtility::ReadWavHeader(InStream& wav)
{
    WAVE_RIFF_header RIFFheaderObj;
    WAVE_CHUNK_header CHUNKheaderObj;
    // TODO (hellner): tmpStr and tmpStr2 seems unnecessary here.
    char tmpStr[6] = "FOUR";
    unsigned char tmpStr2[4];
    int32_t i, len;
    bool dataFound = false;
    bool fmtFound = false;
    int8_t dummyRead;


    _dataSize = 0;
    len = wav.Read(&RIFFheaderObj, sizeof(WAVE_RIFF_header));
    if(len != sizeof(WAVE_RIFF_header))
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "Not a wave file (too short)");
        return -1;
    }

    for (i = 0; i < 4; i++)
    {
        tmpStr[i] = RIFFheaderObj.ckID[i];
    }
    if(strcmp(tmpStr, "RIFF") != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "Not a wave file (does not have RIFF)");
        return -1;
    }
    for (i = 0; i < 4; i++)
    {
        tmpStr[i] = RIFFheaderObj.wave_ckID[i];
    }
    if(strcmp(tmpStr, "WAVE") != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "Not a wave file (does not have WAVE)");
        return -1;
    }

    len = wav.Read(&CHUNKheaderObj, sizeof(WAVE_CHUNK_header));

    // WAVE files are stored in little endian byte order. Make sure that the
    // data can be read on big endian as well.
    // TODO (hellner): little endian to system byte order should be done in
    //                 in a subroutine.
    memcpy(tmpStr2, &CHUNKheaderObj.fmt_ckSize, 4);
    CHUNKheaderObj.fmt_ckSize =
        (int32_t) ((uint32_t) tmpStr2[0] +
                         (((uint32_t)tmpStr2[1])<<8) +
                         (((uint32_t)tmpStr2[2])<<16) +
                         (((uint32_t)tmpStr2[3])<<24));

    memcpy(tmpStr, CHUNKheaderObj.fmt_ckID, 4);

    while ((len == sizeof(WAVE_CHUNK_header)) && (!fmtFound || !dataFound))
    {
        if(strcmp(tmpStr, "fmt ") == 0)
        {
            len = wav.Read(&_wavFormatObj, sizeof(WAVE_FMTINFO_header));

            memcpy(tmpStr2, &_wavFormatObj.formatTag, 2);
            _wavFormatObj.formatTag =
                (WaveFormats) ((uint32_t)tmpStr2[0] +
                               (((uint32_t)tmpStr2[1])<<8));
            memcpy(tmpStr2, &_wavFormatObj.nChannels, 2);
            _wavFormatObj.nChannels =
                (int16_t) ((uint32_t)tmpStr2[0] +
                                 (((uint32_t)tmpStr2[1])<<8));
            memcpy(tmpStr2, &_wavFormatObj.nSamplesPerSec, 4);
            _wavFormatObj.nSamplesPerSec =
                (int32_t) ((uint32_t)tmpStr2[0] +
                                 (((uint32_t)tmpStr2[1])<<8) +
                                 (((uint32_t)tmpStr2[2])<<16) +
                                 (((uint32_t)tmpStr2[3])<<24));
            memcpy(tmpStr2, &_wavFormatObj.nAvgBytesPerSec, 4);
            _wavFormatObj.nAvgBytesPerSec =
                (int32_t) ((uint32_t)tmpStr2[0] +
                                 (((uint32_t)tmpStr2[1])<<8) +
                                 (((uint32_t)tmpStr2[2])<<16) +
                                 (((uint32_t)tmpStr2[3])<<24));
            memcpy(tmpStr2, &_wavFormatObj.nBlockAlign, 2);
            _wavFormatObj.nBlockAlign =
                (int16_t) ((uint32_t)tmpStr2[0] +
                                 (((uint32_t)tmpStr2[1])<<8));
            memcpy(tmpStr2, &_wavFormatObj.nBitsPerSample, 2);
            _wavFormatObj.nBitsPerSample =
                (int16_t) ((uint32_t)tmpStr2[0] +
                                 (((uint32_t)tmpStr2[1])<<8));

            for (i = 0;
                 i < (CHUNKheaderObj.fmt_ckSize -
                      (int32_t)sizeof(WAVE_FMTINFO_header));
                 i++)
            {
                len = wav.Read(&dummyRead, 1);
                if(len != 1)
                {
                    WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                                 "File corrupted, reached EOF (reading fmt)");
                    return -1;
                }
            }
            fmtFound = true;
        }
        else if(strcmp(tmpStr, "data") == 0)
        {
            _dataSize = CHUNKheaderObj.fmt_ckSize;
            dataFound = true;
            break;
        }
        else
        {
            for (i = 0; i < (CHUNKheaderObj.fmt_ckSize); i++)
            {
                len = wav.Read(&dummyRead, 1);
                if(len != 1)
                {
                    WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                                 "File corrupted, reached EOF (reading other)");
                    return -1;
                }
            }
        }

        len = wav.Read(&CHUNKheaderObj, sizeof(WAVE_CHUNK_header));

        memcpy(tmpStr2, &CHUNKheaderObj.fmt_ckSize, 4);
        CHUNKheaderObj.fmt_ckSize =
            (int32_t) ((uint32_t)tmpStr2[0] +
                             (((uint32_t)tmpStr2[1])<<8) +
                             (((uint32_t)tmpStr2[2])<<16) +
                             (((uint32_t)tmpStr2[3])<<24));

        memcpy(tmpStr, CHUNKheaderObj.fmt_ckID, 4);
    }

    // Either a proper format chunk has been read or a data chunk was come
    // across.
    if( (_wavFormatObj.formatTag != kWaveFormatPcm) &&
        (_wavFormatObj.formatTag != kWaveFormatALaw) &&
        (_wavFormatObj.formatTag != kWaveFormatMuLaw))
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "Coding formatTag value=%d not supported!",
                     _wavFormatObj.formatTag);
        return -1;
    }
    if((_wavFormatObj.nChannels < 1) ||
        (_wavFormatObj.nChannels > 2))
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "nChannels value=%d not supported!",
                     _wavFormatObj.nChannels);
        return -1;
    }

    if((_wavFormatObj.nBitsPerSample != 8) &&
        (_wavFormatObj.nBitsPerSample != 16))
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "nBitsPerSample value=%d not supported!",
                     _wavFormatObj.nBitsPerSample);
        return -1;
    }

    // Calculate the number of bytes that 10 ms of audio data correspond to.
    if(_wavFormatObj.formatTag == kWaveFormatPcm)
    {
        // TODO (hellner): integer division for 22050 and 11025 would yield
        //                 the same result as the else statement. Remove those
        //                 special cases?
        if(_wavFormatObj.nSamplesPerSec == 44100)
        {
            _readSizeBytes = 441 * _wavFormatObj.nChannels *
                (_wavFormatObj.nBitsPerSample / 8);
        } else if(_wavFormatObj.nSamplesPerSec == 22050) {
            _readSizeBytes = 220 * _wavFormatObj.nChannels * // XXX inexact!
                (_wavFormatObj.nBitsPerSample / 8);
        } else if(_wavFormatObj.nSamplesPerSec == 11025) {
            _readSizeBytes = 110 * _wavFormatObj.nChannels * // XXX inexact!
                (_wavFormatObj.nBitsPerSample / 8);
        } else {
            _readSizeBytes = (_wavFormatObj.nSamplesPerSec/100) *
              _wavFormatObj.nChannels * (_wavFormatObj.nBitsPerSample / 8);
        }

    } else {
        _readSizeBytes = (_wavFormatObj.nSamplesPerSec/100) *
            _wavFormatObj.nChannels * (_wavFormatObj.nBitsPerSample / 8);
    }
    return 0;
}

int32_t ModuleFileUtility::InitWavCodec(uint32_t samplesPerSec,
                                        uint32_t channels,
                                        uint32_t bitsPerSample,
                                        uint32_t formatTag)
{
    codec_info_.pltype   = -1;
    codec_info_.plfreq   = samplesPerSec;
    codec_info_.channels = channels;
    codec_info_.rate     = bitsPerSample * samplesPerSec;

    // Calculate the packet size for 10ms frames
    switch(formatTag)
    {
    case kWaveFormatALaw:
        strcpy(codec_info_.plname, "PCMA");
        _codecId = kCodecPcma;
        codec_info_.pltype = 8;
        codec_info_.pacsize  = codec_info_.plfreq / 100;
        break;
    case kWaveFormatMuLaw:
        strcpy(codec_info_.plname, "PCMU");
        _codecId = kCodecPcmu;
        codec_info_.pltype = 0;
        codec_info_.pacsize  = codec_info_.plfreq / 100;
         break;
    case kWaveFormatPcm:
        codec_info_.pacsize  = (bitsPerSample * (codec_info_.plfreq / 100)) / 8;
        if(samplesPerSec == 8000)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_8Khz;
        }
        else if(samplesPerSec == 16000)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_16kHz;
        }
        else if(samplesPerSec == 32000)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_32Khz;
        }
        // Set the packet size for "odd" sampling frequencies so that it
        // properly corresponds to _readSizeBytes.
        else if(samplesPerSec == 11025)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_16kHz;
            codec_info_.pacsize = 110; // XXX inexact!
            codec_info_.plfreq = 11000; // XXX inexact!
        }
        else if(samplesPerSec == 22050)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_16kHz;
            codec_info_.pacsize = 220; // XXX inexact!
            codec_info_.plfreq = 22000; // XXX inexact!
        }
        else if(samplesPerSec == 44100)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_16kHz;
            codec_info_.pacsize = 441;
            codec_info_.plfreq = 44100;
        }
        else if(samplesPerSec == 48000)
        {
            strcpy(codec_info_.plname, "L16");
            _codecId = kCodecL16_16kHz;
            codec_info_.pacsize = 480;
            codec_info_.plfreq = 48000;
        }
        else
        {
            WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                         "Unsupported PCM frequency!");
            return -1;
        }
        break;
        default:
            WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                         "unknown WAV format TAG!");
            return -1;
            break;
    }
    return 0;
}

int32_t ModuleFileUtility::InitWavReading(InStream& wav,
                                          const uint32_t start,
                                          const uint32_t stop)
{

    _reading = false;

    if(ReadWavHeader(wav) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "failed to read WAV header!");
        return -1;
    }

    _playoutPositionMs = 0;
    _readPos = 0;

    if(start > 0)
    {
        uint8_t dummy[WAV_MAX_BUFFER_SIZE];
        int32_t readLength;
        if(_readSizeBytes <= WAV_MAX_BUFFER_SIZE)
        {
            while (_playoutPositionMs < start)
            {
                readLength = wav.Read(dummy, _readSizeBytes);
                if(readLength == _readSizeBytes)
                {
                    _readPos += readLength;
                    _playoutPositionMs += 10;
                }
                else // Must have reached EOF before start position!
                {
                    WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                       "InitWavReading(), EOF before start position");
                    return -1;
                }
            }
        }
        else
        {
            return -1;
        }
    }
    if( InitWavCodec(_wavFormatObj.nSamplesPerSec, _wavFormatObj.nChannels,
                     _wavFormatObj.nBitsPerSample,
                     _wavFormatObj.formatTag) != 0)
    {
        return -1;
    }
    _bytesPerSample = _wavFormatObj.nBitsPerSample / 8;


    _startPointInMs = start;
    _stopPointInMs = stop;
    _reading = true;
    return 0;
}

int32_t ModuleFileUtility::ReadWavDataAsMono(
    InStream& wav,
    int8_t* outData,
    const uint32_t bufferSize)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::ReadWavDataAsMono(wav= 0x%x, outData= 0x%d,\
 bufSize= %ld)",
        &wav,
        outData,
        bufferSize);

    // The number of bytes that should be read from file.
    const uint32_t totalBytesNeeded = _readSizeBytes;
    // The number of bytes that will be written to outData.
    const uint32_t bytesRequested = (codec_info_.channels == 2) ?
        totalBytesNeeded >> 1 : totalBytesNeeded;
    if(bufferSize < bytesRequested)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsMono: output buffer is too short!");
        return -1;
    }
    if(outData == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsMono: output buffer NULL!");
        return -1;
    }

    if(!_reading)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsMono: no longer reading file.");
        return -1;
    }

    int32_t bytesRead = ReadWavData(
        wav,
        (codec_info_.channels == 2) ? _tempData : (uint8_t*)outData,
        totalBytesNeeded);
    if(bytesRead == 0)
    {
        return 0;
    }
    if(bytesRead < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsMono: failed to read data from WAV file.");
        return -1;
    }
    // Output data is should be mono.
    if(codec_info_.channels == 2)
    {
        for (uint32_t i = 0; i < bytesRequested / _bytesPerSample; i++)
        {
            // Sample value is the average of left and right buffer rounded to
            // closest integer value. Note samples can be either 1 or 2 byte.
            if(_bytesPerSample == 1)
            {
                _tempData[i] = ((_tempData[2 * i] + _tempData[(2 * i) + 1] +
                                 1) >> 1);
            }
            else
            {
                int16_t* sampleData = (int16_t*) _tempData;
                sampleData[i] = ((sampleData[2 * i] + sampleData[(2 * i) + 1] +
                                  1) >> 1);
            }
        }
        memcpy(outData, _tempData, bytesRequested);
    }
    return bytesRequested;
}

int32_t ModuleFileUtility::ReadWavDataAsStereo(
    InStream& wav,
    int8_t* outDataLeft,
    int8_t* outDataRight,
    const uint32_t bufferSize)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::ReadWavDataAsStereo(wav= 0x%x, outLeft= 0x%x,\
 outRight= 0x%x, bufSize= %ld)",
        &wav,
        outDataLeft,
        outDataRight,
        bufferSize);

    if((outDataLeft == NULL) ||
       (outDataRight == NULL))
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsMono: an input buffer is NULL!");
        return -1;
    }
    if(codec_info_.channels != 2)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceFile,
            _id,
            "ReadWavDataAsStereo: WAV file does not contain stereo data!");
        return -1;
    }
    if(! _reading)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsStereo: no longer reading file.");
        return -1;
    }

    // The number of bytes that should be read from file.
    const uint32_t totalBytesNeeded = _readSizeBytes;
    // The number of bytes that will be written to the left and the right
    // buffers.
    const uint32_t bytesRequested = totalBytesNeeded >> 1;
    if(bufferSize < bytesRequested)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavData: Output buffers are too short!");
        assert(false);
        return -1;
    }

    int32_t bytesRead = ReadWavData(wav, _tempData, totalBytesNeeded);
    if(bytesRead <= 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsStereo: failed to read data from WAV file.");
        return -1;
    }

    // Turn interleaved audio to left and right buffer. Note samples can be
    // either 1 or 2 bytes
    if(_bytesPerSample == 1)
    {
        for (uint32_t i = 0; i < bytesRequested; i++)
        {
            outDataLeft[i]  = _tempData[2 * i];
            outDataRight[i] = _tempData[(2 * i) + 1];
        }
    }
    else if(_bytesPerSample == 2)
    {
        int16_t* sampleData = reinterpret_cast<int16_t*>(_tempData);
        int16_t* outLeft = reinterpret_cast<int16_t*>(outDataLeft);
        int16_t* outRight = reinterpret_cast<int16_t*>(
            outDataRight);

        // Bytes requested to samples requested.
        uint32_t sampleCount = bytesRequested >> 1;
        for (uint32_t i = 0; i < sampleCount; i++)
        {
            outLeft[i] = sampleData[2 * i];
            outRight[i] = sampleData[(2 * i) + 1];
        }
    } else {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                   "ReadWavStereoData: unsupported sample size %d!",
                   _bytesPerSample);
        assert(false);
        return -1;
    }
    return bytesRequested;
}

int32_t ModuleFileUtility::ReadWavData(
    InStream& wav,
    uint8_t* buffer,
    const uint32_t dataLengthInBytes)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::ReadWavData(wav= 0x%x, buffer= 0x%x, dataLen= %ld)",
        &wav,
        buffer,
        dataLengthInBytes);


    if(buffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadWavDataAsMono: output buffer NULL!");
        return -1;
    }

    // Make sure that a read won't return too few samples.
    // TODO (hellner): why not read the remaining bytes needed from the start
    //                 of the file?
    if((_dataSize - _readPos) < (int32_t)dataLengthInBytes)
    {
        // Rewind() being -1 may be due to the file not supposed to be looped.
        if(wav.Rewind() == -1)
        {
            _reading = false;
            return 0;
        }
        if(InitWavReading(wav, _startPointInMs, _stopPointInMs) == -1)
        {
            _reading = false;
            return -1;
        }
    }

    int32_t bytesRead = wav.Read(buffer, dataLengthInBytes);
    if(bytesRead < 0)
    {
        _reading = false;
        return -1;
    }

    // This should never happen due to earlier sanity checks.
    // TODO (hellner): change to an assert and fail here since this should
    //                 never happen...
    if(bytesRead < (int32_t)dataLengthInBytes)
    {
        if((wav.Rewind() == -1) ||
            (InitWavReading(wav, _startPointInMs, _stopPointInMs) == -1))
        {
            _reading = false;
            return -1;
        }
        else
        {
            bytesRead = wav.Read(buffer, dataLengthInBytes);
            if(bytesRead < (int32_t)dataLengthInBytes)
            {
                _reading = false;
                return -1;
            }
        }
    }

    _readPos += bytesRead;

    // TODO (hellner): Why is dataLengthInBytes let dictate the number of bytes
    //                 to read when exactly 10ms should be read?!
    _playoutPositionMs += 10;
    if((_stopPointInMs > 0) &&
        (_playoutPositionMs >= _stopPointInMs))
    {
        if((wav.Rewind() == -1) ||
            (InitWavReading(wav, _startPointInMs, _stopPointInMs) == -1))
        {
            _reading = false;
        }
    }
    return bytesRead;
}

int32_t ModuleFileUtility::InitWavWriting(OutStream& wav,
                                          const CodecInst& codecInst)
{

    if(set_codec_info(codecInst) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "codecInst identifies unsupported codec!");
        return -1;
    }
    _writing = false;
    uint32_t channels = (codecInst.channels == 0) ?
        1 : codecInst.channels;

    if(STR_CASE_CMP(codecInst.plname, "PCMU") == 0)
    {
        _bytesPerSample = 1;
        if(WriteWavHeader(wav, 8000, _bytesPerSample, channels,
                          kWaveFormatMuLaw, 0) == -1)
        {
            return -1;
        }
    }else if(STR_CASE_CMP(codecInst.plname, "PCMA") == 0)
    {
        _bytesPerSample = 1;
        if(WriteWavHeader(wav, 8000, _bytesPerSample, channels, kWaveFormatALaw,
                          0) == -1)
        {
            return -1;
        }
    }
    else if(STR_CASE_CMP(codecInst.plname, "L16") == 0)
    {
        _bytesPerSample = 2;
        if(WriteWavHeader(wav, codecInst.plfreq, _bytesPerSample, channels,
                          kWaveFormatPcm, 0) == -1)
        {
            return -1;
        }
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                   "codecInst identifies unsupported codec for WAV file!");
        return -1;
    }
    _writing = true;
    _bytesWritten = 0;
    return 0;
}

int32_t ModuleFileUtility::WriteWavData(OutStream& out,
                                        const int8_t*  buffer,
                                        const uint32_t dataLength)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::WriteWavData(out= 0x%x, buf= 0x%x, dataLen= %d)",
        &out,
        buffer,
        dataLength);

    if(buffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "WriteWavData: input buffer NULL!");
        return -1;
    }

    if(!out.Write(buffer, dataLength))
    {
        return -1;
    }
    _bytesWritten += dataLength;
    return dataLength;
}


int32_t ModuleFileUtility::WriteWavHeader(
    OutStream& wav,
    const uint32_t freq,
    const uint32_t bytesPerSample,
    const uint32_t channels,
    const uint32_t format,
    const uint32_t lengthInBytes)
{

    // Frame size in bytes for 10 ms of audio.
    int32_t frameSize = (freq / 100) * bytesPerSample * channels;

    // Calculate the number of full frames that the wave file contain.
    const int32_t dataLengthInBytes = frameSize *
        (lengthInBytes / frameSize);

    int8_t tmpStr[4];
    int8_t tmpChar;
    uint32_t tmpLong;

    memcpy(tmpStr, "RIFF", 4);
    wav.Write(tmpStr, 4);

    tmpLong = dataLengthInBytes + 36;
    tmpChar = (int8_t)(tmpLong);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 8);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 16);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 24);
    wav.Write(&tmpChar, 1);

    memcpy(tmpStr, "WAVE", 4);
    wav.Write(tmpStr, 4);

    memcpy(tmpStr, "fmt ", 4);
    wav.Write(tmpStr, 4);

    tmpChar = 16;
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);

    tmpChar = (int8_t)(format);
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);

    tmpChar = (int8_t)(channels);
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);

    tmpLong = freq;
    tmpChar = (int8_t)(tmpLong);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 8);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 16);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 24);
    wav.Write(&tmpChar, 1);

    // nAverageBytesPerSec = Sample rate * Bytes per sample * Channels
    tmpLong = bytesPerSample * freq * channels;
    tmpChar = (int8_t)(tmpLong);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 8);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 16);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 24);
    wav.Write(&tmpChar, 1);

    // nBlockAlign = Bytes per sample * Channels
    tmpChar = (int8_t)(bytesPerSample * channels);
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);

    tmpChar = (int8_t)(bytesPerSample*8);
    wav.Write(&tmpChar, 1);
    tmpChar = 0;
    wav.Write(&tmpChar, 1);

    memcpy(tmpStr, "data", 4);
    wav.Write(tmpStr, 4);

    tmpLong = dataLengthInBytes;
    tmpChar = (int8_t)(tmpLong);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 8);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 16);
    wav.Write(&tmpChar, 1);
    tmpChar = (int8_t)(tmpLong >> 24);
    wav.Write(&tmpChar, 1);

    return 0;
}

int32_t ModuleFileUtility::UpdateWavHeader(OutStream& wav)
{
    int32_t res = -1;
    if(wav.Rewind() == -1)
    {
        return -1;
    }
    uint32_t channels = (codec_info_.channels == 0) ?
        1 : codec_info_.channels;

    if(STR_CASE_CMP(codec_info_.plname, "L16") == 0)
    {
        res = WriteWavHeader(wav, codec_info_.plfreq, 2, channels,
                             kWaveFormatPcm, _bytesWritten);
    } else if(STR_CASE_CMP(codec_info_.plname, "PCMU") == 0) {
            res = WriteWavHeader(wav, 8000, 1, channels, kWaveFormatMuLaw,
                                 _bytesWritten);
    } else if(STR_CASE_CMP(codec_info_.plname, "PCMA") == 0) {
            res = WriteWavHeader(wav, 8000, 1, channels, kWaveFormatALaw,
                                 _bytesWritten);
    } else {
        // Allow calling this API even if not writing to a WAVE file.
        // TODO (hellner): why?!
        return 0;
    }
    return res;
}


int32_t ModuleFileUtility::InitPreEncodedReading(InStream& in,
                                                 const CodecInst& cinst)
{

    uint8_t preEncodedID;
    in.Read(&preEncodedID, 1);

    MediaFileUtility_CodecType codecType =
        (MediaFileUtility_CodecType)preEncodedID;

    if(set_codec_info(cinst) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "Pre-encoded file send codec mismatch!");
        return -1;
    }
    if(codecType != _codecId)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "Pre-encoded file format codec mismatch!");
        return -1;
    }
    memcpy(&codec_info_,&cinst,sizeof(CodecInst));
    _reading = true;
    return 0;
}

int32_t ModuleFileUtility::ReadPreEncodedData(
    InStream& in,
    int8_t* outData,
    const uint32_t bufferSize)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::ReadPreEncodedData(in= 0x%x, outData= 0x%x,\
 bufferSize= %d)",
        &in,
        outData,
        bufferSize);

    if(outData == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "output buffer NULL");
    }

    uint32_t frameLen;
    uint8_t buf[64];
    // Each frame has a two byte header containing the frame length.
    int32_t res = in.Read(buf, 2);
    if(res != 2)
    {
        if(!in.Rewind())
        {
            // The first byte is the codec identifier.
            in.Read(buf, 1);
            res = in.Read(buf, 2);
        }
        else
        {
            return -1;
        }
    }
    frameLen = buf[0] + buf[1] * 256;
    if(bufferSize < frameLen)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceFile,
            _id,
            "buffer not large enough to read %d bytes of pre-encoded data!",
            frameLen);
        return -1;
    }
    return in.Read(outData, frameLen);
}

int32_t ModuleFileUtility::InitPreEncodedWriting(
    OutStream& out,
    const CodecInst& codecInst)
{

    if(set_codec_info(codecInst) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "CodecInst not recognized!");
        return -1;
    }
    _writing = true;
    _bytesWritten = 1;
     out.Write(&_codecId, 1);
     return 0;
}

int32_t ModuleFileUtility::WritePreEncodedData(
    OutStream& out,
    const int8_t*  buffer,
    const uint32_t dataLength)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::WritePreEncodedData(out= 0x%x, inData= 0x%x,\
 dataLen= %d)",
        &out,
        buffer,
        dataLength);

    if(buffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,"buffer NULL");
    }

    int32_t bytesWritten = 0;
    // The first two bytes is the size of the frame.
    int16_t lengthBuf;
    lengthBuf = (int16_t)dataLength;
    if(!out.Write(&lengthBuf, 2))
    {
       return -1;
    }
    bytesWritten = 2;

    if(!out.Write(buffer, dataLength))
    {
        return -1;
    }
    bytesWritten += dataLength;
    return bytesWritten;
}

int32_t ModuleFileUtility::InitCompressedReading(
    InStream& in,
    const uint32_t start,
    const uint32_t stop)
{
    WEBRTC_TRACE(
        kTraceDebug,
        kTraceFile,
        _id,
        "ModuleFileUtility::InitCompressedReading(in= 0x%x, start= %d,\
 stop= %d)",
        &in,
        start,
        stop);

#if defined(WEBRTC_CODEC_AMR) || defined(WEBRTC_CODEC_AMRWB) || \
    defined(WEBRTC_CODEC_ILBC)
    int16_t read_len = 0;
#endif
    _codecId = kCodecNoCodec;
    _playoutPositionMs = 0;
    _reading = false;

    _startPointInMs = start;
    _stopPointInMs = stop;

#ifdef WEBRTC_CODEC_AMR
    int32_t AMRmode2bytes[9]={12,13,15,17,19,20,26,31,5};
#endif
#ifdef WEBRTC_CODEC_AMRWB
    int32_t AMRWBmode2bytes[10]={17,23,32,36,40,46,50,58,60,6};
#endif

    // Read the codec name
    int32_t cnt = 0;
    char buf[64];
    do
    {
        in.Read(&buf[cnt++], 1);
    } while ((buf[cnt-1] != '\n') && (64 > cnt));

    if(cnt==64)
    {
        return -1;
    } else {
        buf[cnt]=0;
    }

#ifdef WEBRTC_CODEC_AMR
    if(!strcmp("#!AMR\n", buf))
    {
        strcpy(codec_info_.plname, "amr");
        codec_info_.pacsize = 160;
        _codecId = kCodecAmr;
        codec_info_.pltype = 112;
        codec_info_.rate = 12200;
        codec_info_.plfreq = 8000;
        codec_info_.channels = 1;

        int16_t mode = 0;
        if(_startPointInMs > 0)
        {
            while (_playoutPositionMs <= _startPointInMs)
            {
                // First read byte contain the AMR mode.
                read_len = in.Read(buf, 1);
                if(read_len != 1)
                {
                    return -1;
                }

                mode = (buf[0]>>3)&0xF;
                if((mode < 0) || (mode > 8))
                {
                    if(mode != 15)
                    {
                        return -1;
                    }
                }
                if(mode != 15)
                {
                    read_len = in.Read(&buf[1], AMRmode2bytes[mode]);
                    if(read_len != AMRmode2bytes[mode])
                    {
                        return -1;
                    }
                }
                _playoutPositionMs += 20;
            }
        }
    }
#endif
#ifdef WEBRTC_CODEC_AMRWB
    if(!strcmp("#!AMRWB\n", buf))
    {
        strcpy(codec_info_.plname, "amr-wb");
        codec_info_.pacsize = 320;
        _codecId = kCodecAmrWb;
        codec_info_.pltype = 120;
        codec_info_.rate = 20000;
        codec_info_.plfreq = 16000;
        codec_info_.channels = 1;

        int16_t mode = 0;
        if(_startPointInMs > 0)
        {
            while (_playoutPositionMs <= _startPointInMs)
            {
                // First read byte contain the AMR mode.
                read_len = in.Read(buf, 1);
                if(read_len != 1)
                {
                    return -1;
                }

                mode = (buf[0]>>3)&0xF;
                if((mode < 0) || (mode > 9))
                {
                    if(mode != 15)
                    {
                        return -1;
                    }
                }
                if(mode != 15)
                {
                    read_len = in.Read(&buf[1], AMRWBmode2bytes[mode]);
                    if(read_len != AMRWBmode2bytes[mode])
                    {
                        return -1;
                    }
                }
                _playoutPositionMs += 20;
            }
        }
    }
#endif
#ifdef WEBRTC_CODEC_ILBC
    if(!strcmp("#!iLBC20\n", buf))
    {
        codec_info_.pltype = 102;
        strcpy(codec_info_.plname, "ilbc");
        codec_info_.plfreq   = 8000;
        codec_info_.pacsize  = 160;
        codec_info_.channels = 1;
        codec_info_.rate     = 13300;
        _codecId = kCodecIlbc20Ms;

        if(_startPointInMs > 0)
        {
            while (_playoutPositionMs <= _startPointInMs)
            {
                read_len = in.Read(buf, 38);
                if(read_len == 38)
                {
                    _playoutPositionMs += 20;
                }
                else
                {
                    return -1;
                }
            }
        }
    }

    if(!strcmp("#!iLBC30\n", buf))
    {
        codec_info_.pltype = 102;
        strcpy(codec_info_.plname, "ilbc");
        codec_info_.plfreq   = 8000;
        codec_info_.pacsize  = 240;
        codec_info_.channels = 1;
        codec_info_.rate     = 13300;
        _codecId = kCodecIlbc30Ms;

        if(_startPointInMs > 0)
        {
            while (_playoutPositionMs <= _startPointInMs)
            {
                read_len = in.Read(buf, 50);
                if(read_len == 50)
                {
                    _playoutPositionMs += 20;
                }
                else
                {
                    return -1;
                }
            }
        }
    }
#endif
    if(_codecId == kCodecNoCodec)
    {
        return -1;
    }
    _reading = true;
    return 0;
}

int32_t ModuleFileUtility::ReadCompressedData(InStream& in,
                                              int8_t* outData,
                                              uint32_t bufferSize)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::ReadCompressedData(in=0x%x, outData=0x%x,\
 bytes=%ld)",
        &in,
        outData,
        bufferSize);

#ifdef WEBRTC_CODEC_AMR
    uint32_t AMRmode2bytes[9]={12,13,15,17,19,20,26,31,5};
#endif
#ifdef WEBRTC_CODEC_AMRWB
    uint32_t AMRWBmode2bytes[10]={17,23,32,36,40,46,50,58,60,6};
#endif
    uint32_t bytesRead = 0;

    if(! _reading)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "not currently reading!");
        return -1;
    }

#ifdef WEBRTC_CODEC_AMR
    if(_codecId == kCodecAmr)
    {
        int32_t res = in.Read(outData, 1);
        if(res != 1)
        {
            if(!in.Rewind())
            {
                InitCompressedReading(in, _startPointInMs, _stopPointInMs);
                res = in.Read(outData, 1);
                if(res != 1)
                {
                    _reading = false;
                    return -1;
                }
            }
            else
            {
                _reading = false;
                return -1;
            }
        }
         const int16_t mode = (outData[0]>>3)&0xF;
        if((mode < 0) ||
           (mode > 8))
        {
            if(mode != 15)
            {
                return -1;
            }
        }
        if(mode != 15)
        {
            if(bufferSize < AMRmode2bytes[mode] + 1)
            {
                WEBRTC_TRACE(
                    kTraceError,
                    kTraceFile,
                    _id,
                    "output buffer is too short to read AMR compressed data.");
                assert(false);
                return -1;
            }
            bytesRead = in.Read(&outData[1], AMRmode2bytes[mode]);
            if(bytesRead != AMRmode2bytes[mode])
            {
                _reading = false;
                return -1;
            }
            // Count the mode byte to bytes read.
            bytesRead++;
        }
        else
        {
            bytesRead = 1;
        }
    }
#endif
#ifdef WEBRTC_CODEC_AMRWB
    if(_codecId == kCodecAmrWb)
    {
        int32_t res = in.Read(outData, 1);
        if(res != 1)
        {
            if(!in.Rewind())
            {
                InitCompressedReading(in, _startPointInMs, _stopPointInMs);
                res = in.Read(outData, 1);
                if(res != 1)
                {
                    _reading = false;
                    return -1;
                }
            }
            else
            {
                _reading = false;
                return -1;
            }
        }
         int16_t mode = (outData[0]>>3)&0xF;
        if((mode < 0) ||
           (mode > 8))
        {
            if(mode != 15)
            {
                return -1;
            }
        }
        if(mode != 15)
        {
            if(bufferSize < AMRWBmode2bytes[mode] + 1)
            {
                WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                           "output buffer is too short to read AMRWB\
 compressed.");
                assert(false);
                return -1;
            }
             bytesRead = in.Read(&outData[1], AMRWBmode2bytes[mode]);
            if(bytesRead != AMRWBmode2bytes[mode])
            {
                _reading = false;
                return -1;
            }
            bytesRead++;
        }
        else
        {
            bytesRead = 1;
        }
    }
#endif
#ifdef WEBRTC_CODEC_ILBC
    if((_codecId == kCodecIlbc20Ms) ||
        (_codecId == kCodecIlbc30Ms))
    {
        uint32_t byteSize = 0;
         if(_codecId == kCodecIlbc30Ms)
        {
            byteSize = 50;
        }
        if(_codecId == kCodecIlbc20Ms)
        {
            byteSize = 38;
        }
        if(bufferSize < byteSize)
        {
            WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                           "output buffer is too short to read ILBC compressed\
 data.");
            assert(false);
            return -1;
        }

        bytesRead = in.Read(outData, byteSize);
        if(bytesRead != byteSize)
        {
            if(!in.Rewind())
            {
                InitCompressedReading(in, _startPointInMs, _stopPointInMs);
                bytesRead = in.Read(outData, byteSize);
                if(bytesRead != byteSize)
                {
                    _reading = false;
                    return -1;
                }
            }
            else
            {
                _reading = false;
                return -1;
            }
        }
    }
#endif
    if(bytesRead == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "ReadCompressedData() no bytes read, codec not supported");
        return -1;
    }

    _playoutPositionMs += 20;
    if((_stopPointInMs > 0) &&
        (_playoutPositionMs >= _stopPointInMs))
    {
        if(!in.Rewind())
        {
            InitCompressedReading(in, _startPointInMs, _stopPointInMs);
        }
        else
        {
            _reading = false;
        }
    }
    return bytesRead;
}

int32_t ModuleFileUtility::InitCompressedWriting(
    OutStream& out,
    const CodecInst& codecInst)
{
    WEBRTC_TRACE(kTraceDebug, kTraceFile, _id,
               "ModuleFileUtility::InitCompressedWriting(out= 0x%x,\
 codecName= %s)",
               &out, codecInst.plname);

    _writing = false;

#ifdef WEBRTC_CODEC_AMR
    if(STR_CASE_CMP(codecInst.plname, "amr") == 0)
    {
        if(codecInst.pacsize == 160)
        {
            memcpy(&codec_info_,&codecInst,sizeof(CodecInst));
            _codecId = kCodecAmr;
            out.Write("#!AMR\n",6);
            _writing = true;
            return 0;
        }
    }
#endif
#ifdef WEBRTC_CODEC_AMRWB
    if(STR_CASE_CMP(codecInst.plname, "amr-wb") == 0)
    {
        if(codecInst.pacsize == 320)
        {
            memcpy(&codec_info_,&codecInst,sizeof(CodecInst));
            _codecId = kCodecAmrWb;
            out.Write("#!AMRWB\n",8);
            _writing = true;
            return 0;
        }
    }
#endif
#ifdef WEBRTC_CODEC_ILBC
    if(STR_CASE_CMP(codecInst.plname, "ilbc") == 0)
    {
        if(codecInst.pacsize == 160)
        {
            _codecId = kCodecIlbc20Ms;
            out.Write("#!iLBC20\n",9);
        }
        else if(codecInst.pacsize == 240)
        {
            _codecId = kCodecIlbc30Ms;
            out.Write("#!iLBC30\n",9);
        }
        else
        {
          WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                       "codecInst defines unsupported compression codec!");
            return -1;
        }
        memcpy(&codec_info_,&codecInst,sizeof(CodecInst));
        _writing = true;
        return 0;
    }
#endif

    WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                 "codecInst defines unsupported compression codec!");
    return -1;
}

int32_t ModuleFileUtility::WriteCompressedData(
    OutStream& out,
    const int8_t* buffer,
    const uint32_t dataLength)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::WriteCompressedData(out= 0x%x, buf= 0x%x,\
 dataLen= %d)",
        &out,
        buffer,
        dataLength);

    if(buffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,"buffer NULL");
    }

    if(!out.Write(buffer, dataLength))
    {
        return -1;
    }
    return dataLength;
}

int32_t ModuleFileUtility::InitPCMReading(InStream& pcm,
                                          const uint32_t start,
                                          const uint32_t stop,
                                          uint32_t freq)
{
    WEBRTC_TRACE(
        kTraceInfo,
        kTraceFile,
        _id,
        "ModuleFileUtility::InitPCMReading(pcm= 0x%x, start=%d, stop=%d,\
 freq=%d)",
        &pcm,
        start,
        stop,
        freq);

    int8_t dummy[320];
    int32_t read_len;

    _playoutPositionMs = 0;
    _startPointInMs = start;
    _stopPointInMs = stop;
    _reading = false;

    if(freq == 8000)
    {
        strcpy(codec_info_.plname, "L16");
        codec_info_.pltype   = -1;
        codec_info_.plfreq   = 8000;
        codec_info_.pacsize  = 160;
        codec_info_.channels = 1;
        codec_info_.rate     = 128000;
        _codecId = kCodecL16_8Khz;
    }
    else if(freq == 16000)
    {
        strcpy(codec_info_.plname, "L16");
        codec_info_.pltype   = -1;
        codec_info_.plfreq   = 16000;
        codec_info_.pacsize  = 320;
        codec_info_.channels = 1;
        codec_info_.rate     = 256000;
        _codecId = kCodecL16_16kHz;
    }
    else if(freq == 32000)
    {
        strcpy(codec_info_.plname, "L16");
        codec_info_.pltype   = -1;
        codec_info_.plfreq   = 32000;
        codec_info_.pacsize  = 320;
        codec_info_.channels = 1;
        codec_info_.rate     = 512000;
        _codecId = kCodecL16_32Khz;
    }

    // Readsize for 10ms of audio data (2 bytes per sample).
    _readSizeBytes = 2 * codec_info_. plfreq / 100;
    if(_startPointInMs > 0)
    {
        while (_playoutPositionMs < _startPointInMs)
        {
            read_len = pcm.Read(dummy, _readSizeBytes);
            if(read_len == _readSizeBytes)
            {
                _playoutPositionMs += 10;
            }
            else // Must have reached EOF before start position!
            {
                return -1;
            }
        }
    }
    _reading = true;
    return 0;
}

int32_t ModuleFileUtility::ReadPCMData(InStream& pcm,
                                       int8_t* outData,
                                       uint32_t bufferSize)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::ReadPCMData(pcm= 0x%x, outData= 0x%x, bufSize= %d)",
        &pcm,
        outData,
        bufferSize);

    if(outData == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,"buffer NULL");
    }

    // Readsize for 10ms of audio data (2 bytes per sample).
    uint32_t bytesRequested = 2 * codec_info_.plfreq / 100;
    if(bufferSize <  bytesRequested)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                   "ReadPCMData: buffer not long enough for a 10ms frame.");
        assert(false);
        return -1;
    }

    uint32_t bytesRead = pcm.Read(outData, bytesRequested);
    if(bytesRead < bytesRequested)
    {
        if(pcm.Rewind() == -1)
        {
            _reading = false;
        }
        else
        {
            if(InitPCMReading(pcm, _startPointInMs, _stopPointInMs,
                              codec_info_.plfreq) == -1)
            {
                _reading = false;
            }
            else
            {
                int32_t rest = bytesRequested - bytesRead;
                int32_t len = pcm.Read(&(outData[bytesRead]), rest);
                if(len == rest)
                {
                    bytesRead += len;
                }
                else
                {
                    _reading = false;
                }
            }
            if(bytesRead <= 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                        "ReadPCMData: Failed to rewind audio file.");
                return -1;
            }
        }
    }

    if(bytesRead <= 0)
    {
        WEBRTC_TRACE(kTraceStream, kTraceFile, _id,
                   "ReadPCMData: end of file");
        return -1;
    }
    _playoutPositionMs += 10;
    if(_stopPointInMs && _playoutPositionMs >= _stopPointInMs)
    {
        if(!pcm.Rewind())
        {
            if(InitPCMReading(pcm, _startPointInMs, _stopPointInMs,
                              codec_info_.plfreq) == -1)
            {
                _reading = false;
            }
        }
    }
    return bytesRead;
}

int32_t ModuleFileUtility::InitPCMWriting(OutStream& out, uint32_t freq)
{

    if(freq == 8000)
    {
        strcpy(codec_info_.plname, "L16");
        codec_info_.pltype   = -1;
        codec_info_.plfreq   = 8000;
        codec_info_.pacsize  = 160;
        codec_info_.channels = 1;
        codec_info_.rate     = 128000;

        _codecId = kCodecL16_8Khz;
    }
    else if(freq == 16000)
    {
        strcpy(codec_info_.plname, "L16");
        codec_info_.pltype   = -1;
        codec_info_.plfreq   = 16000;
        codec_info_.pacsize  = 320;
        codec_info_.channels = 1;
        codec_info_.rate     = 256000;

        _codecId = kCodecL16_16kHz;
    }
    else if(freq == 32000)
    {
        strcpy(codec_info_.plname, "L16");
        codec_info_.pltype   = -1;
        codec_info_.plfreq   = 32000;
        codec_info_.pacsize  = 320;
        codec_info_.channels = 1;
        codec_info_.rate     = 512000;

        _codecId = kCodecL16_32Khz;
    }
    if((_codecId != kCodecL16_8Khz) &&
       (_codecId != kCodecL16_16kHz) &&
       (_codecId != kCodecL16_32Khz))
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "CodecInst is not 8KHz PCM or 16KHz PCM!");
        return -1;
    }
    _writing = true;
    _bytesWritten = 0;
    return 0;
}

int32_t ModuleFileUtility::WritePCMData(OutStream& out,
                                        const int8_t*  buffer,
                                        const uint32_t dataLength)
{
    WEBRTC_TRACE(
        kTraceStream,
        kTraceFile,
        _id,
        "ModuleFileUtility::WritePCMData(out= 0x%x, buf= 0x%x, dataLen= %d)",
        &out,
        buffer,
        dataLength);

    if(buffer == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "buffer NULL");
    }

    if(!out.Write(buffer, dataLength))
    {
        return -1;
    }

    _bytesWritten += dataLength;
    return dataLength;
}

int32_t ModuleFileUtility::codec_info(CodecInst& codecInst)
{
    WEBRTC_TRACE(kTraceStream, kTraceFile, _id,
                 "ModuleFileUtility::codec_info(codecInst= 0x%x)", &codecInst);

    if(!_reading && !_writing)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "CodecInst: not currently reading audio file!");
        return -1;
    }
    memcpy(&codecInst,&codec_info_,sizeof(CodecInst));
    return 0;
}

int32_t ModuleFileUtility::set_codec_info(const CodecInst& codecInst)
{

    _codecId = kCodecNoCodec;
    if(STR_CASE_CMP(codecInst.plname, "PCMU") == 0)
    {
        _codecId = kCodecPcmu;
    }
    else if(STR_CASE_CMP(codecInst.plname, "PCMA") == 0)
    {
        _codecId = kCodecPcma;
    }
    else if(STR_CASE_CMP(codecInst.plname, "L16") == 0)
    {
        if(codecInst.plfreq == 8000)
        {
            _codecId = kCodecL16_8Khz;
        }
        else if(codecInst.plfreq == 16000)
        {
            _codecId = kCodecL16_16kHz;
        }
        else if(codecInst.plfreq == 32000)
        {
            _codecId = kCodecL16_32Khz;
        }
    }
#ifdef WEBRTC_CODEC_AMR
    else if(STR_CASE_CMP(codecInst.plname, "amr") == 0)
    {
        _codecId = kCodecAmr;
    }
#endif
#ifdef WEBRTC_CODEC_AMRWB
    else if(STR_CASE_CMP(codecInst.plname, "amr-wb") == 0)
    {
        _codecId = kCodecAmrWb;
    }
#endif
#ifdef WEBRTC_CODEC_ILBC
    else if(STR_CASE_CMP(codecInst.plname, "ilbc") == 0)
    {
        if(codecInst.pacsize == 160)
        {
            _codecId = kCodecIlbc20Ms;
        }
        else if(codecInst.pacsize == 240)
        {
            _codecId = kCodecIlbc30Ms;
        }
    }
#endif
#if(defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    else if(STR_CASE_CMP(codecInst.plname, "isac") == 0)
    {
        if(codecInst.plfreq == 16000)
        {
            _codecId = kCodecIsac;
        }
        else if(codecInst.plfreq == 32000)
        {
            _codecId = kCodecIsacSwb;
        }
    }
#endif
#ifdef WEBRTC_CODEC_ISACLC
    else if(STR_CASE_CMP(codecInst.plname, "isaclc") == 0)
    {
        _codecId = kCodecIsacLc;
    }
#endif
#ifdef WEBRTC_CODEC_G722
    else if(STR_CASE_CMP(codecInst.plname, "G722") == 0)
    {
        _codecId = kCodecG722;
    }
#endif
    else if(STR_CASE_CMP(codecInst.plname, "G7221") == 0)
    {
#ifdef WEBRTC_CODEC_G722_1
        if(codecInst.plfreq == 16000)
        {
            if(codecInst.rate == 16000)
            {
                _codecId = kCodecG722_1_16Kbps;
            }
            else if(codecInst.rate == 24000)
            {
                _codecId = kCodecG722_1_24Kbps;
            }
            else if(codecInst.rate == 32000)
            {
                _codecId = kCodecG722_1_32Kbps;
            }
        }
#endif
#ifdef WEBRTC_CODEC_G722_1C
        if(codecInst.plfreq == 32000)
        {
            if(codecInst.rate == 48000)
            {
                _codecId = kCodecG722_1c_48;
            }
            else if(codecInst.rate == 32000)
            {
                _codecId = kCodecG722_1c_32;
            }
            else if(codecInst.rate == 24000)
            {
                _codecId = kCodecG722_1c_24;
            }
        }
#endif
    }
#ifdef WEBRTC_CODEC_G726
    else if(STR_CASE_CMP(codecInst.plname, "G726-40") == 0)
    {
        _codecId = kCodecG726_40;
    }
    else if(STR_CASE_CMP(codecInst.plname, "G726-32") == 0)
    {
        _codecId = kCodecG726_24;
    }
    else if(STR_CASE_CMP(codecInst.plname, "G726-24") == 0)
    {
        _codecId = kCodecG726_32;
    }
    else if(STR_CASE_CMP(codecInst.plname, "G726-16") == 0)
    {
        _codecId = kCodecG726_16;
    }
#endif
#ifdef WEBRTC_CODEC_G729
    else if(STR_CASE_CMP(codecInst.plname, "G729") == 0)
    {
        _codecId = kCodecG729;
    }
#endif
#ifdef WEBRTC_CODEC_G729_1
    else if(STR_CASE_CMP(codecInst.plname, "G7291") == 0)
    {
        _codecId = kCodecG729_1;
    }
#endif
#ifdef WEBRTC_CODEC_SPEEX
    else if(STR_CASE_CMP(codecInst.plname, "speex") == 0)
    {
        if(codecInst.plfreq == 8000)
        {
            _codecId = kCodecSpeex8Khz;
        }
        else if(codecInst.plfreq == 16000)
        {
            _codecId = kCodecSpeex16Khz;
        }
    }
#endif
    if(_codecId == kCodecNoCodec)
    {
        return -1;
    }
    memcpy(&codec_info_, &codecInst, sizeof(CodecInst));
    return 0;
}

int32_t ModuleFileUtility::FileDurationMs(const char* fileName,
                                          const FileFormats fileFormat,
                                          const uint32_t freqInHz)
{

    if(fileName == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id, "filename NULL");
        return -1;
    }

    int32_t time_in_ms = -1;
    struct stat file_size;
    if(stat(fileName,&file_size) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "failed to retrieve file size with stat!");
        return -1;
    }
    FileWrapper* inStreamObj = FileWrapper::Create();
    if(inStreamObj == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceFile, _id,
                     "failed to create InStream object!");
        return -1;
    }
    if(inStreamObj->OpenFile(fileName, true) == -1)
    {
        delete inStreamObj;
        WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                     "failed to open file %s!", fileName);
        return -1;
    }

    switch (fileFormat)
    {
        case kFileFormatWavFile:
        {
            if(ReadWavHeader(*inStreamObj) == -1)
            {
                WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                             "failed to read WAV file header!");
                return -1;
            }
            time_in_ms = ((file_size.st_size - 44) /
                          (_wavFormatObj.nAvgBytesPerSec/1000));
            break;
        }
        case kFileFormatPcm16kHzFile:
        {
            // 16 samples per ms. 2 bytes per sample.
            int32_t denominator = 16*2;
            time_in_ms = (file_size.st_size)/denominator;
            break;
        }
        case kFileFormatPcm8kHzFile:
        {
            // 8 samples per ms. 2 bytes per sample.
            int32_t denominator = 8*2;
            time_in_ms = (file_size.st_size)/denominator;
            break;
        }
        case kFileFormatCompressedFile:
        {
            int32_t cnt = 0;
            int32_t read_len = 0;
            char buf[64];
            do
            {
                read_len = inStreamObj->Read(&buf[cnt++], 1);
                if(read_len != 1)
                {
                    return -1;
                }
            } while ((buf[cnt-1] != '\n') && (64 > cnt));

            if(cnt == 64)
            {
                return -1;
            }
            else
            {
                buf[cnt] = 0;
            }
#ifdef WEBRTC_CODEC_AMR
            if(!strcmp("#!AMR\n", buf))
            {
                uint8_t dummy;
                read_len = inStreamObj->Read(&dummy, 1);
                if(read_len != 1)
                {
                    return -1;
                }

                int16_t AMRMode = (dummy>>3)&0xF;

                // TODO (hellner): use tables instead of hardcoding like this!
                //                 Additionally, this calculation does not
                //                 take octet alignment into consideration.
                switch (AMRMode)
                {
                        // Mode 0: 4.75 kbit/sec -> 95 bits per 20 ms frame.
                        // 20 ms = 95 bits ->
                        // file size in bytes * 8 / 95 is the number of
                        // 20 ms frames in the file ->
                        // time_in_ms = file size * 8 / 95 * 20
                    case 0:
                        time_in_ms = ((file_size.st_size)*160)/95;
                        break;
                        // Mode 1: 5.15 kbit/sec -> 103 bits per 20 ms frame.
                    case 1:
                        time_in_ms = ((file_size.st_size)*160)/103;
                        break;
                        // Mode 2: 5.90 kbit/sec -> 118 bits per 20 ms frame.
                    case 2:
                        time_in_ms = ((file_size.st_size)*160)/118;
                        break;
                        // Mode 3: 6.70 kbit/sec -> 134 bits per 20 ms frame.
                    case 3:
                        time_in_ms = ((file_size.st_size)*160)/134;
                        break;
                        // Mode 4: 7.40 kbit/sec -> 148 bits per 20 ms frame.
                    case 4:
                        time_in_ms = ((file_size.st_size)*160)/148;
                        break;
                        // Mode 5: 7.95 bit/sec -> 159 bits per 20 ms frame.
                    case 5:
                        time_in_ms = ((file_size.st_size)*160)/159;
                        break;
                        // Mode 6: 10.2 bit/sec -> 204 bits per 20 ms frame.
                    case 6:
                        time_in_ms = ((file_size.st_size)*160)/204;
                        break;
                        // Mode 7: 12.2 bit/sec -> 244 bits per 20 ms frame.
                    case 7:
                        time_in_ms = ((file_size.st_size)*160)/244;
                        break;
                        // Mode 8: SID Mode -> 39 bits per 20 ms frame.
                    case 8:
                        time_in_ms = ((file_size.st_size)*160)/39;
                        break;
                    default:
                        break;
                }
            }
#endif
#ifdef WEBRTC_CODEC_AMRWB
            if(!strcmp("#!AMRWB\n", buf))
            {
                uint8_t dummy;
                read_len = inStreamObj->Read(&dummy, 1);
                if(read_len != 1)
                {
                    return -1;
                }

                // TODO (hellner): use tables instead of hardcoding like this!
                int16_t AMRWBMode = (dummy>>3)&0xF;
                switch(AMRWBMode)
                {
                        // Mode 0: 6.6 kbit/sec -> 132 bits per 20 ms frame.
                    case 0:
                        time_in_ms = ((file_size.st_size)*160)/132;
                        break;
                        // Mode 1: 8.85 kbit/sec -> 177 bits per 20 ms frame.
                    case 1:
                        time_in_ms = ((file_size.st_size)*160)/177;
                        break;
                        // Mode 2: 12.65 kbit/sec -> 253 bits per 20 ms frame.
                    case 2:
                        time_in_ms = ((file_size.st_size)*160)/253;
                        break;
                        // Mode 3: 14.25 kbit/sec -> 285 bits per 20 ms frame.
                    case 3:
                        time_in_ms = ((file_size.st_size)*160)/285;
                        break;
                        // Mode 4: 15.85 kbit/sec -> 317 bits per 20 ms frame.
                    case 4:
                        time_in_ms = ((file_size.st_size)*160)/317;
                        break;
                        // Mode 5: 18.25 kbit/sec -> 365 bits per 20 ms frame.
                    case 5:
                        time_in_ms = ((file_size.st_size)*160)/365;
                        break;
                        // Mode 6: 19.85 kbit/sec -> 397 bits per 20 ms frame.
                    case 6:
                        time_in_ms = ((file_size.st_size)*160)/397;
                        break;
                        // Mode 7: 23.05 kbit/sec -> 461 bits per 20 ms frame.
                    case 7:
                        time_in_ms = ((file_size.st_size)*160)/461;
                        break;
                        // Mode 8: 23.85 kbit/sec -> 477 bits per 20 ms frame.
                    case 8:
                        time_in_ms = ((file_size.st_size)*160)/477;
                        break;
                    default:
                        delete inStreamObj;
                        return -1;
                }
            }
#endif
#ifdef WEBRTC_CODEC_ILBC
            if(!strcmp("#!iLBC20\n", buf))
            {
                // 20 ms is 304 bits
                time_in_ms = ((file_size.st_size)*160)/304;
                break;
            }
            if(!strcmp("#!iLBC30\n", buf))
            {
                // 30 ms takes 400 bits.
                // file size in bytes * 8 / 400 is the number of
                // 30 ms frames in the file ->
                // time_in_ms = file size * 8 / 400 * 30
                time_in_ms = ((file_size.st_size)*240)/400;
                break;
            }
#endif
        }
        case kFileFormatPreencodedFile:
        {
            WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                         "cannot determine duration of Pre-Encoded file!");
            break;
        }
        default:
            WEBRTC_TRACE(kTraceError, kTraceFile, _id,
                         "unsupported file format %d!", fileFormat);
            break;
    }
    inStreamObj->CloseFile();
    delete inStreamObj;
    return time_in_ms;
}

uint32_t ModuleFileUtility::PlayoutPositionMs()
{
    WEBRTC_TRACE(kTraceStream, kTraceFile, _id,
               "ModuleFileUtility::PlayoutPosition()");

    if(_reading)
    {
        return _playoutPositionMs;
    }
    else
    {
        return 0;
    }
}
}  // namespace webrtc
