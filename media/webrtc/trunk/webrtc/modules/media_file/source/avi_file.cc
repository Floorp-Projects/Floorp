/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(henrike): reassess the error handling in this class. Currently failure
// is detected by asserts in many places. Also a refactoring of this class would
// be beneficial.

#include "avi_file.h"

#include <assert.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "critical_section_wrapper.h"
#include "file_wrapper.h"
#include "list_wrapper.h"
#include "trace.h"

// http://msdn2.microsoft.com/en-us/library/ms779636.aspx
// A chunk has the following form:
// ckID ckSize ckData
// where ckID is a FOURCC that identifies the data contained in the
// chunk, ckData is a 4-byte value giving the size of the data in
// ckData, and ckData is zero or more bytes of data. The data is always
// padded to nearest WORD boundary. ckSize gives the size of the valid
// data in the chunk; it does not include the padding, the size of
// ckID, or the size of ckSize.
//http://msdn2.microsoft.com/en-us/library/ms779632.aspx
//NOTE: Workaround to make MPEG4 files play on WMP. MPEG files
//      place the config parameters efter the BITMAPINFOHEADER and
//      *NOT* in the 'strd'!
// http://msdn.microsoft.com/en-us/library/dd183375.aspx
// http://msdn.microsoft.com/en-us/library/dd183376.aspx

namespace webrtc {
namespace {
static const WebRtc_UWord32 kAvifHasindex       = 0x00000010;
static const WebRtc_UWord32 kAvifMustuseindex   = 0x00000020;
static const WebRtc_UWord32 kAvifIsinterleaved  = 0x00000100;
static const WebRtc_UWord32 kAvifTrustcktype    = 0x00000800;
static const WebRtc_UWord32 kAvifWascapturefile = 0x00010000;

template <class T>
T MinValue(T a, T b)
{
    return a < b ? a : b;
}
}  // namespace

AviFile::AVIMAINHEADER::AVIMAINHEADER()
    : fcc(                  0),
      cb(                   0),
      dwMicroSecPerFrame(   0),
      dwMaxBytesPerSec(     0),
      dwPaddingGranularity( 0),
      dwFlags(              0),
      dwTotalFrames(        0),
      dwInitialFrames(      0),
      dwStreams(            0),
      dwSuggestedBufferSize(0),
      dwWidth(              0),
      dwHeight(             0)
{
    dwReserved[0] = 0;
    dwReserved[1] = 0;
    dwReserved[2] = 0;
    dwReserved[3] = 0;
}

AVISTREAMHEADER::AVISTREAMHEADER()
    : fcc(                  0),
      cb(                   0),
      fccType(              0),
      fccHandler(           0),
      dwFlags(              0),
      wPriority(            0),
      wLanguage(            0),
      dwInitialFrames(      0),
      dwScale(              0),
      dwRate(               0),
      dwStart(              0),
      dwLength(             0),
      dwSuggestedBufferSize(0),
      dwQuality(            0),
      dwSampleSize(         0)
{
    rcFrame.left   = 0;
    rcFrame.top    = 0;
    rcFrame.right  = 0;
    rcFrame.bottom = 0;
}

BITMAPINFOHEADER::BITMAPINFOHEADER()
    : biSize(         0),
      biWidth(        0),
      biHeight(       0),
      biPlanes(       0),
      biBitCount(     0),
      biCompression(  0),
      biSizeImage(    0),
      biXPelsPerMeter(0),
      biYPelsPerMeter(0),
      biClrUsed(      0),
      biClrImportant( 0)
{
}

WAVEFORMATEX::WAVEFORMATEX()
    : wFormatTag(     0),
      nChannels(      0),
      nSamplesPerSec( 0),
      nAvgBytesPerSec(0),
      nBlockAlign(    0),
      wBitsPerSample( 0),
      cbSize(         0)
{
}

AviFile::AVIINDEXENTRY::AVIINDEXENTRY(WebRtc_UWord32 inckid,
                                      WebRtc_UWord32 indwFlags,
                                      WebRtc_UWord32 indwChunkOffset,
                                      WebRtc_UWord32 indwChunkLength)
    : ckid(inckid),
      dwFlags(indwFlags),
      dwChunkOffset(indwChunkOffset),
      dwChunkLength(indwChunkLength)
{
}

AviFile::AviFile()
    : _crit(CriticalSectionWrapper::CreateCriticalSection()),
      _aviFile(NULL),
      _aviHeader(),
      _videoStreamHeader(),
      _audioStreamHeader(),
      _videoFormatHeader(),
      _audioFormatHeader(),
      _videoConfigParameters(),
      _videoConfigLength(0),
      _videoStreamName(),
      _audioConfigParameters(),
      _audioStreamName(),
      _videoStream(),
      _audioStream(),
      _nrStreams(0),
      _aviLength(0),
      _dataLength(0),
      _bytesRead(0),
      _dataStartByte(0),
      _framesRead(0),
      _videoFrames(0),
      _audioFrames(0),
      _reading(false),
      _openedAs(AVI_AUDIO),
      _loop(false),
      _writing(false),
      _bytesWritten(0),
      _riffSizeMark(0),
      _moviSizeMark(0),
      _totNumFramesMark(0),
      _videoStreamLengthMark(0),
      _audioStreamLengthMark(0),
      _moviListOffset(0),
      _writeAudioStream(false),
      _writeVideoStream(false),
      _aviMode(NotSet),
      _videoCodecConfigParams(NULL),
      _videoCodecConfigParamsLength(0),
      _videoStreamDataChunkPrefix(0),
      _audioStreamDataChunkPrefix(0),
      _created(false),
      _indexList(new ListWrapper())
{
  ResetComplexMembers();
}

AviFile::~AviFile()
{
    Close();

    delete _indexList;
    delete[] _videoCodecConfigParams;
    delete _crit;
}

WebRtc_Word32 AviFile::Open(AVIStreamType streamType, const char* fileName,
                            bool loop)
{
    WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, -1,  "OpenAVIFile(%s)",
                 fileName);
    _crit->Enter();

    if (_aviMode != NotSet)
    {
        _crit->Leave();
        return -1;
    }

    _aviMode = Read;

    if (!fileName)
    {
        _crit->Leave();
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,  "\tfileName not valid!");
        return -1;
    }

#ifdef _WIN32
    // fopen does not support wide characters on Windows, ergo _wfopen.
    wchar_t wideFileName[FileWrapper::kMaxFileNameSize];
    wideFileName[0] = 0;
    MultiByteToWideChar(CP_UTF8,0,fileName, -1, // convert the whole string
                        wideFileName, FileWrapper::kMaxFileNameSize);

    _aviFile = _wfopen(wideFileName, L"rb");
#else
    _aviFile = fopen(fileName, "rb");
#endif

    if (!_aviFile)
    {
        _crit->Leave();
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,  "Could not open file!");
        return -1;
    }

    // ReadRIFF verifies that the file is AVI and figures out the file length.
    WebRtc_Word32 err = ReadRIFF();
    if (err)
    {
        if (_aviFile)
        {
            fclose(_aviFile);
            _aviFile = NULL;
        }
        _crit->Leave();
        return -1;
    }

   err = ReadHeaders();
    if (err)
    {
        if (_aviFile)
        {
            fclose(_aviFile);
            _aviFile = NULL;
        }
        _crit->Leave();
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Unsupported or corrupt AVI format");
        return -1;
    }

    _dataStartByte = _bytesRead;
    _reading = true;
    _openedAs = streamType;
    _loop = loop;
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::Close()
{
    _crit->Enter();
    switch (_aviMode)
    {
    case Read:
        CloseRead();
        break;
    case Write:
        CloseWrite();
        break;
    default:
        break;
    }

    if (_videoCodecConfigParams)
    {
        delete [] _videoCodecConfigParams;
        _videoCodecConfigParams = 0;
    }
    ResetMembers();
    _crit->Leave();
    return 0;
}

WebRtc_UWord32 AviFile::MakeFourCc(WebRtc_UWord8 ch0, WebRtc_UWord8 ch1,
                                   WebRtc_UWord8 ch2, WebRtc_UWord8 ch3)
{
    return ((WebRtc_UWord32)(WebRtc_UWord8)(ch0)         |
            ((WebRtc_UWord32)(WebRtc_UWord8)(ch1) << 8)  |
            ((WebRtc_UWord32)(WebRtc_UWord8)(ch2) << 16) |
            ((WebRtc_UWord32)(WebRtc_UWord8)(ch3) << 24 ));
}

WebRtc_Word32 AviFile::GetVideoStreamInfo(AVISTREAMHEADER& videoStreamHeader,
                                          BITMAPINFOHEADER& bitmapInfo,
                                          char* codecConfigParameters,
                                          WebRtc_Word32& configLength)
{
    _crit->Enter();
    if (!_reading && !_created)
    {
        _crit->Leave();
        return -1;
    }

    memcpy(&videoStreamHeader, &_videoStreamHeader, sizeof(_videoStreamHeader));
    memcpy(&bitmapInfo, &_videoFormatHeader, sizeof(_videoFormatHeader));

    if (configLength <= _videoConfigLength)
    {
        memcpy(codecConfigParameters, _videoConfigParameters,
               _videoConfigLength);
        configLength = _videoConfigLength;
    }
    else
    {
        configLength = 0;
    }
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::GetDuration(WebRtc_Word32& durationMs)
{
    _crit->Enter();
    if (_videoStreamHeader.dwRate==0 || _videoStreamHeader.dwScale==0)
    {
        _crit->Leave();
        return -1;
    }

    durationMs = _videoStreamHeader.dwLength * 1000 /
        (_videoStreamHeader.dwRate/_videoStreamHeader.dwScale);
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::GetAudioStreamInfo(WAVEFORMATEX& waveHeader)
{
    _crit->Enter();
    if (_aviMode != Read)
    {
        _crit->Leave();
        return -1;
    }
    if (!_reading && !_created)
    {
        _crit->Leave();
        return -1;
    }
    memcpy(&waveHeader, &_audioFormatHeader, sizeof(_audioFormatHeader));
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::WriteAudio(const WebRtc_UWord8* data,
                                  WebRtc_Word32 length)
{
    _crit->Enter();
    size_t newBytesWritten = _bytesWritten;

    if (_aviMode != Write)
    {
        _crit->Leave();
        return -1;
    }
    if (!_created)
    {
        _crit->Leave();
        return -1;
    }
    if (!_writeAudioStream)
    {
        _crit->Leave();
        return -1;
    }

    // Start of chunk.
    const WebRtc_UWord32 chunkOffset = ftell(_aviFile) - _moviListOffset;
    _bytesWritten += PutLE32(_audioStreamDataChunkPrefix);
    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t chunkSizeMark = _bytesWritten;

    _bytesWritten += PutBuffer(data, length);

    const long chunkSize = PutLE32LengthFromCurrent(
        static_cast<long>(chunkSizeMark));

    // Make sure that the chunk is aligned on 2 bytes (= 1 sample).
    if (chunkSize % 2)
    {
        _bytesWritten += PutByte(0);
    }
    // End of chunk

    // Save chunk information for use when closing file.
    AddChunkToIndexList(_audioStreamDataChunkPrefix, 0, // No flags.
                        chunkOffset, chunkSize);

    ++_audioFrames;
    newBytesWritten = _bytesWritten - newBytesWritten;
    _crit->Leave();
    return static_cast<WebRtc_Word32>(newBytesWritten);
}

WebRtc_Word32 AviFile::WriteVideo(const WebRtc_UWord8* data,
                                  WebRtc_Word32 length)
{
    _crit->Enter();
    size_t newBytesWritten = _bytesWritten;
    if (_aviMode != Write)
    {
        _crit->Leave();
        return -1;
    }
    if (!_created)
    {
        _crit->Leave();
        return -1;
    }
    if (!_writeVideoStream)
    {
        _crit->Leave();
        return -1;
    }

    // Start of chunk.
    const WebRtc_UWord32 chunkOffset = ftell(_aviFile) - _moviListOffset;
    _bytesWritten += PutLE32(_videoStreamDataChunkPrefix);
    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t chunkSizeMark = _bytesWritten;

    _bytesWritten += PutBuffer(data, length);

    const long chunkSize = PutLE32LengthFromCurrent(
        static_cast<long>(chunkSizeMark));

    // Make sure that the chunk is aligned on 2 bytes (= 1 sample).
    if (chunkSize % 2)
    {
        //Pad one byte, to WORD align.
        _bytesWritten += PutByte(0);
    }
     //End chunk!
    AddChunkToIndexList(_videoStreamDataChunkPrefix, 0, // No flags.
                        chunkOffset, static_cast<WebRtc_UWord32>(chunkSize));

    ++_videoFrames;
    newBytesWritten = _bytesWritten - newBytesWritten;
    _crit->Leave();
    return static_cast<WebRtc_Word32>(newBytesWritten);
}

WebRtc_Word32 AviFile::PrepareDataChunkHeaders()
{
    // 00 video stream, 01 audio stream.
    // db uncompresses video,  dc compressed video, wb WAV audio
    if (_writeVideoStream)
    {
        if (strncmp((const char*) &_videoStreamHeader.fccHandler, "I420", 4) ==
            0)
        {
            _videoStreamDataChunkPrefix = MakeFourCc('0', '0', 'd', 'b');
        }
        else
        {
            _videoStreamDataChunkPrefix = MakeFourCc('0', '0', 'd', 'c');
        }
        _audioStreamDataChunkPrefix = MakeFourCc('0', '1', 'w', 'b');
    }
    else
    {
        _audioStreamDataChunkPrefix = MakeFourCc('0', '0', 'w', 'b');
    }
    return 0;
}

WebRtc_Word32 AviFile::ReadMoviSubChunk(WebRtc_UWord8* data,
                                        WebRtc_Word32& length,
                                        WebRtc_UWord32 tag1,
                                        WebRtc_UWord32 tag2)
{
    if (!_reading)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1,
                     "AviFile::ReadMoviSubChunk(): File not open!");
        length = 0;
        return -1;
    }

    WebRtc_UWord32 size;
    bool isEOFReached = false;
    // Try to read one data chunk header
    while (true)
    {
        // TODO (hellner): what happens if an empty AVI file is opened with
        // _loop set to true? Seems like this while-loop would never exit!

        // tag = db uncompresses video,  dc compressed video or wb WAV audio.
        WebRtc_UWord32 tag;
        _bytesRead += GetLE32(tag);
        _bytesRead += GetLE32(size);

        const WebRtc_Word32 eof = feof(_aviFile);
        if (!eof)
        {
            if (tag == tag1)
            {
                // Supported tag found.
                break;
            }
            else if ((tag == tag2) && (tag2 != 0))
            {
                // Supported tag found.
                break;
            }

            // Jump to next chunk. The size is in bytes but chunks are aligned
            // on 2 byte boundaries.
            const WebRtc_UWord32 seekSize = (size % 2) ? size + 1 : size;
            const WebRtc_Word32 err = fseek(_aviFile, seekSize, SEEK_CUR);

            if (err)
            {
                isEOFReached = true;
            }
        }
        else
        {
            isEOFReached = true;
        }

        if (isEOFReached)
        {
            clearerr(_aviFile);

            if (_loop)
            {
                WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1,
                             "AviFile::ReadMoviSubChunk(): Reached end of AVI\
                              data file, starting from the beginning.");

                fseek(_aviFile, static_cast<long>(_dataStartByte), SEEK_SET);

                _bytesRead = _dataStartByte;
                _framesRead = 0;
                isEOFReached = false;
            }
            else
            {
                WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1,
                             "AviFile::ReadMoviSubChunk(): Reached end of AVI\
                             file!");
                length = 0;
                return -1;
            }
        }
        _bytesRead += size;
    }

    if (static_cast<WebRtc_Word32>(size) > length)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1,
                     "AviFile::ReadMoviSubChunk(): AVI read buffer too small!");

        // Jump to next chunk. The size is in bytes but chunks are aligned
        // on 2 byte boundaries.
        const WebRtc_UWord32 seekSize = (size % 2) ? size + 1 : size;
        fseek(_aviFile, seekSize, SEEK_CUR);
        _bytesRead += seekSize;
        length = 0;
        return -1;
    }
    _bytesRead += GetBuffer(data, size);

    // The size is in bytes but chunks are aligned on 2 byte boundaries.
    if (size % 2)
    {
        WebRtc_UWord8 dummy_byte;
        _bytesRead += GetByte(dummy_byte);
    }
    length = size;
    ++_framesRead;
    return 0;
}

WebRtc_Word32 AviFile::ReadAudio(WebRtc_UWord8* data, WebRtc_Word32& length)
{
    _crit->Enter();
    WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1,  "AviFile::ReadAudio()");

    if (_aviMode != Read)
    {
        _crit->Leave();
        return -1;
    }
    if (_openedAs != AVI_AUDIO)
    {
        length = 0;
        _crit->Leave();
        WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1,  "File not open as audio!");
        return -1;
    }

    const WebRtc_Word32 ret = ReadMoviSubChunk(
        data,
        length,
        StreamAndTwoCharCodeToTag(_audioStream.streamNumber, "wb"));

    _crit->Leave();
    return ret;
}

WebRtc_Word32 AviFile::ReadVideo(WebRtc_UWord8* data, WebRtc_Word32& length)
{
    WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1, "AviFile::ReadVideo()");

    _crit->Enter();
    if (_aviMode != Read)
    {
        //Has to be Read!
        _crit->Leave();
        return -1;
    }
    if (_openedAs != AVI_VIDEO)
    {
        length = 0;
        _crit->Leave();
        WEBRTC_TRACE(kTraceDebug, kTraceVideo, -1, "File not open as video!");
        return -1;
    }

    const WebRtc_Word32 ret = ReadMoviSubChunk(
        data,
        length,
        StreamAndTwoCharCodeToTag(_videoStream.streamNumber, "dc"),
        StreamAndTwoCharCodeToTag(_videoStream.streamNumber, "db"));
    _crit->Leave();
    return ret;
}

WebRtc_Word32 AviFile::Create(const char* fileName)
{
    _crit->Enter();
    if (_aviMode != Write)
    {
        _crit->Leave();
        return -1;
    }

    if (!_writeVideoStream && !_writeAudioStream)
    {
        _crit->Leave();
        return -1;
    }
    if (_created)
    {
        _crit->Leave();
        return -1;
    }

#ifdef _WIN32
    // fopen does not support wide characters on Windows, ergo _wfopen.
    wchar_t wideFileName[FileWrapper::kMaxFileNameSize];
    wideFileName[0] = 0;

    MultiByteToWideChar(CP_UTF8,0,fileName, -1, // convert the whole string
                        wideFileName, FileWrapper::kMaxFileNameSize);

    _aviFile = _wfopen(wideFileName, L"w+b");
    if (!_aviFile)
    {
        _crit->Leave();
        return -1;
    }
#else
    _aviFile = fopen(fileName, "w+b");
    if (!_aviFile)
    {
        _crit->Leave();
        return -1;
    }
#endif

    WriteRIFF();
    WriteHeaders();

    _created = true;

    PrepareDataChunkHeaders();
    ClearIndexList();
    WriteMoviStart();
    _aviMode = Write;
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::CreateVideoStream(
    const AVISTREAMHEADER& videoStreamHeader,
    const BITMAPINFOHEADER& bitMapInfoHeader,
    const WebRtc_UWord8* codecConfigParams,
    WebRtc_Word32 codecConfigParamsLength)
{
    _crit->Enter();
    if (_aviMode == Read)
    {
        _crit->Leave();
        return -1;
    }

    if (_created)
    {
        _crit->Leave();
        return -1;
    }

    _aviMode = Write;
    _writeVideoStream = true;

    _videoStreamHeader = videoStreamHeader;
    _videoFormatHeader = bitMapInfoHeader;

    if (codecConfigParams && codecConfigParamsLength > 0)
    {
        if (_videoCodecConfigParams)
        {
            delete [] _videoCodecConfigParams;
            _videoCodecConfigParams = 0;
        }

        _videoCodecConfigParams = new WebRtc_UWord8[codecConfigParamsLength];
        _videoCodecConfigParamsLength = codecConfigParamsLength;

        memcpy(_videoCodecConfigParams, codecConfigParams,
               _videoCodecConfigParamsLength);
    }
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::CreateAudioStream(
    const AVISTREAMHEADER& audioStreamHeader,
    const WAVEFORMATEX& waveFormatHeader)
{
    _crit->Enter();

    if (_aviMode == Read)
    {
        _crit->Leave();
        return -1;
    }

    if (_created)
    {
        _crit->Leave();
        return -1;
    }

    _aviMode = Write;
    _writeAudioStream = true;
    _audioStreamHeader = audioStreamHeader;
    _audioFormatHeader = waveFormatHeader;
    _crit->Leave();
    return 0;
}

WebRtc_Word32 AviFile::WriteRIFF()
{
    const WebRtc_UWord32 riffTag = MakeFourCc('R', 'I', 'F', 'F');
    _bytesWritten += PutLE32(riffTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    _riffSizeMark = _bytesWritten;

    const WebRtc_UWord32 aviTag = MakeFourCc('A', 'V', 'I', ' ');
    _bytesWritten += PutLE32(aviTag);

    return 0;
}


WebRtc_Word32 AviFile::WriteHeaders()
{
    // Main AVI header list.
    const WebRtc_UWord32 listTag = MakeFourCc('L', 'I', 'S', 'T');
    _bytesWritten += PutLE32(listTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t listhdrlSizeMark = _bytesWritten;

    const WebRtc_UWord32 hdrlTag = MakeFourCc('h', 'd', 'r', 'l');
    _bytesWritten += PutLE32(hdrlTag);

    WriteAVIMainHeader();
    WriteAVIStreamHeaders();

    const long hdrlLen = PutLE32LengthFromCurrent(
        static_cast<long>(listhdrlSizeMark));

    // Junk chunk to align on 2048 boundry (CD-ROM sector boundary).
    const WebRtc_UWord32 junkTag = MakeFourCc('J', 'U', 'N', 'K');
    _bytesWritten += PutLE32(junkTag);
    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t junkSizeMark = _bytesWritten;

    const WebRtc_UWord32 junkBufferSize =
        0x800     // 2048 byte alignment
        - 12      // RIFF SIZE 'AVI '
        - 8       // LIST SIZE
        - hdrlLen //
        - 8       // JUNK SIZE
        - 12;     // LIST SIZE 'MOVI'

    // TODO (hellner): why not just fseek here?
    WebRtc_UWord8* junkBuffer = new WebRtc_UWord8[junkBufferSize];
    memset(junkBuffer, 0, junkBufferSize);
    _bytesWritten += PutBuffer(junkBuffer, junkBufferSize);
    delete [] junkBuffer;

    PutLE32LengthFromCurrent(static_cast<long>(junkSizeMark));
    // End of JUNK chunk.
    // End of main AVI header list.
    return 0;
}

WebRtc_Word32 AviFile::WriteAVIMainHeader()
{
    const WebRtc_UWord32 avihTag = MakeFourCc('a', 'v', 'i', 'h');
    _bytesWritten += PutLE32(avihTag);
    _bytesWritten += PutLE32(14 * sizeof(WebRtc_UWord32));

    const WebRtc_UWord32 scale = _videoStreamHeader.dwScale ?
        _videoStreamHeader.dwScale : 1;
    const WebRtc_UWord32 microSecPerFrame = 1000000 /
        (_videoStreamHeader.dwRate / scale);
    _bytesWritten += PutLE32(microSecPerFrame);
    _bytesWritten += PutLE32(0);
    _bytesWritten += PutLE32(0);

    WebRtc_UWord32 numStreams = 0;
    if (_writeVideoStream)
    {
        ++numStreams;
    }
    if (_writeAudioStream)
    {
        ++numStreams;
    }

    if (numStreams == 1)
    {
        _bytesWritten += PutLE32(
            kAvifTrustcktype
            | kAvifHasindex
            | kAvifWascapturefile);
    }
    else
    {
        _bytesWritten += PutLE32(
            kAvifTrustcktype
            | kAvifHasindex
            | kAvifWascapturefile
            | kAvifIsinterleaved);
    }

    _totNumFramesMark = _bytesWritten;
    _bytesWritten += PutLE32(0);
    _bytesWritten += PutLE32(0);
    _bytesWritten += PutLE32(numStreams);

    if (_writeVideoStream)
    {
        _bytesWritten += PutLE32(
            _videoStreamHeader.dwSuggestedBufferSize);
        _bytesWritten += PutLE32(
            _videoStreamHeader.rcFrame.right-_videoStreamHeader.rcFrame.left);
        _bytesWritten += PutLE32(
            _videoStreamHeader.rcFrame.bottom-_videoStreamHeader.rcFrame.top);
    } else {
        _bytesWritten += PutLE32(0);
        _bytesWritten += PutLE32(0);
        _bytesWritten += PutLE32(0);
    }
    _bytesWritten += PutLE32(0);
    _bytesWritten += PutLE32(0);
    _bytesWritten += PutLE32(0);
    _bytesWritten += PutLE32(0);
    return 0;
}

WebRtc_Word32 AviFile::WriteAVIStreamHeaders()
{
    if (_writeVideoStream)
    {
        WriteAVIVideoStreamHeaders();
    }
    if (_writeAudioStream)
    {
        WriteAVIAudioStreamHeaders();
    }
    return 0;
}

WebRtc_Word32 AviFile::WriteAVIVideoStreamHeaders()
{
    const WebRtc_UWord32 listTag = MakeFourCc('L', 'I', 'S', 'T');
    _bytesWritten += PutLE32(listTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t liststrlSizeMark = _bytesWritten;

    const WebRtc_UWord32 hdrlTag = MakeFourCc('s', 't', 'r', 'l');
    _bytesWritten += PutLE32(hdrlTag);

    WriteAVIVideoStreamHeaderChunks();

    PutLE32LengthFromCurrent(static_cast<long>(liststrlSizeMark));

    return 0;
}

WebRtc_Word32 AviFile::WriteAVIVideoStreamHeaderChunks()
{
    // Start of strh
    const WebRtc_UWord32 strhTag = MakeFourCc('s', 't', 'r', 'h');
    _bytesWritten += PutLE32(strhTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t strhSizeMark = _bytesWritten;

    _bytesWritten += PutLE32(_videoStreamHeader.fccType);
    _bytesWritten += PutLE32(_videoStreamHeader.fccHandler);
    _bytesWritten += PutLE32(_videoStreamHeader.dwFlags);
    _bytesWritten += PutLE16(_videoStreamHeader.wPriority);
    _bytesWritten += PutLE16(_videoStreamHeader.wLanguage);
    _bytesWritten += PutLE32(_videoStreamHeader.dwInitialFrames);
    _bytesWritten += PutLE32(_videoStreamHeader.dwScale);
    _bytesWritten += PutLE32(_videoStreamHeader.dwRate);
    _bytesWritten += PutLE32(_videoStreamHeader.dwStart);

    _videoStreamLengthMark = _bytesWritten;
    _bytesWritten += PutLE32(_videoStreamHeader.dwLength);

    _bytesWritten += PutLE32(_videoStreamHeader.dwSuggestedBufferSize);
    _bytesWritten += PutLE32(_videoStreamHeader.dwQuality);
    _bytesWritten += PutLE32(_videoStreamHeader.dwSampleSize);
    _bytesWritten += PutLE16(_videoStreamHeader.rcFrame.left);
    _bytesWritten += PutLE16(_videoStreamHeader.rcFrame.top);
    _bytesWritten += PutLE16(_videoStreamHeader.rcFrame.right);
    _bytesWritten += PutLE16(_videoStreamHeader.rcFrame.bottom);

    PutLE32LengthFromCurrent(static_cast<long>(strhSizeMark));
    // End of strh

    // Start of strf
    const WebRtc_UWord32 strfTag = MakeFourCc('s', 't', 'r', 'f');
    _bytesWritten += PutLE32(strfTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t strfSizeMark = _bytesWritten;

    _bytesWritten += PutLE32(_videoFormatHeader.biSize);
    _bytesWritten += PutLE32(_videoFormatHeader.biWidth);
    _bytesWritten += PutLE32(_videoFormatHeader.biHeight);
    _bytesWritten += PutLE16(_videoFormatHeader.biPlanes);
    _bytesWritten += PutLE16(_videoFormatHeader.biBitCount);
    _bytesWritten += PutLE32(_videoFormatHeader.biCompression);
    _bytesWritten += PutLE32(_videoFormatHeader.biSizeImage);
    _bytesWritten += PutLE32(_videoFormatHeader.biXPelsPerMeter);
    _bytesWritten += PutLE32(_videoFormatHeader.biYPelsPerMeter);
    _bytesWritten += PutLE32(_videoFormatHeader.biClrUsed);
    _bytesWritten += PutLE32(_videoFormatHeader.biClrImportant);

    const bool isMpegFile = _videoStreamHeader.fccHandler ==
        AviFile::MakeFourCc('M','4','S','2');
    if (isMpegFile)
    {
        if (_videoCodecConfigParams && _videoCodecConfigParamsLength > 0)
        {
            _bytesWritten += PutBuffer(_videoCodecConfigParams,
                                       _videoCodecConfigParamsLength);
        }
    }

    PutLE32LengthFromCurrent(static_cast<long>(strfSizeMark));
    // End of strf

    if ( _videoCodecConfigParams
         && (_videoCodecConfigParamsLength > 0)
         && !isMpegFile)
    {
        // Write strd, unless it's an MPEG file
        const WebRtc_UWord32 strdTag = MakeFourCc('s', 't', 'r', 'd');
        _bytesWritten += PutLE32(strdTag);

        // Size is unknown at this point. Update later.
        _bytesWritten += PutLE32(0);
        const size_t strdSizeMark = _bytesWritten;

        _bytesWritten += PutBuffer(_videoCodecConfigParams,
                                   _videoCodecConfigParamsLength);

        PutLE32LengthFromCurrent(static_cast<long>(strdSizeMark));
        // End of strd
    }

    // Start of strn
    const WebRtc_UWord32 strnTag = MakeFourCc('s', 't', 'r', 'n');
    _bytesWritten += PutLE32(strnTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t strnSizeMark = _bytesWritten;

    _bytesWritten += PutBufferZ("WebRtc.avi ");

    PutLE32LengthFromCurrent(static_cast<long>(strnSizeMark));
    // End of strd

    return 0;
}

WebRtc_Word32 AviFile::WriteAVIAudioStreamHeaders()
{
    // Start of LIST
    WebRtc_UWord32 listTag = MakeFourCc('L', 'I', 'S', 'T');
    _bytesWritten += PutLE32(listTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t liststrlSizeMark = _bytesWritten;

    WebRtc_UWord32 hdrlTag = MakeFourCc('s', 't', 'r', 'l');
    _bytesWritten += PutLE32(hdrlTag);

    WriteAVIAudioStreamHeaderChunks();

    PutLE32LengthFromCurrent(static_cast<long>(liststrlSizeMark));
    //End of LIST
    return 0;
}

WebRtc_Word32 AviFile::WriteAVIAudioStreamHeaderChunks()
{
    // Start of strh
    const WebRtc_UWord32 strhTag = MakeFourCc('s', 't', 'r', 'h');
    _bytesWritten += PutLE32(strhTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t strhSizeMark = _bytesWritten;

    _bytesWritten += PutLE32(_audioStreamHeader.fccType);
    _bytesWritten += PutLE32(_audioStreamHeader.fccHandler);
    _bytesWritten += PutLE32(_audioStreamHeader.dwFlags);
    _bytesWritten += PutLE16(_audioStreamHeader.wPriority);
    _bytesWritten += PutLE16(_audioStreamHeader.wLanguage);
    _bytesWritten += PutLE32(_audioStreamHeader.dwInitialFrames);
    _bytesWritten += PutLE32(_audioStreamHeader.dwScale);
    _bytesWritten += PutLE32(_audioStreamHeader.dwRate);
    _bytesWritten += PutLE32(_audioStreamHeader.dwStart);

    _audioStreamLengthMark = _bytesWritten;
    _bytesWritten += PutLE32(_audioStreamHeader.dwLength);

    _bytesWritten += PutLE32(_audioStreamHeader.dwSuggestedBufferSize);
    _bytesWritten += PutLE32(_audioStreamHeader.dwQuality);
    _bytesWritten += PutLE32(_audioStreamHeader.dwSampleSize);
    _bytesWritten += PutLE16(_audioStreamHeader.rcFrame.left);
    _bytesWritten += PutLE16(_audioStreamHeader.rcFrame.top);
    _bytesWritten += PutLE16(_audioStreamHeader.rcFrame.right);
    _bytesWritten += PutLE16(_audioStreamHeader.rcFrame.bottom);

    PutLE32LengthFromCurrent(static_cast<long>(strhSizeMark));
    // End of strh

    // Start of strf
    const WebRtc_UWord32 strfTag = MakeFourCc('s', 't', 'r', 'f');
    _bytesWritten += PutLE32(strfTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t strfSizeMark = _bytesWritten;

    _bytesWritten += PutLE16(_audioFormatHeader.wFormatTag);
    _bytesWritten += PutLE16(_audioFormatHeader.nChannels);
    _bytesWritten += PutLE32(_audioFormatHeader.nSamplesPerSec);
    _bytesWritten += PutLE32(_audioFormatHeader.nAvgBytesPerSec);
    _bytesWritten += PutLE16(_audioFormatHeader.nBlockAlign);
    _bytesWritten += PutLE16(_audioFormatHeader.wBitsPerSample);
    _bytesWritten += PutLE16(_audioFormatHeader.cbSize);

    PutLE32LengthFromCurrent(static_cast<long>(strfSizeMark));
    // End end of strf.

    // Audio doesn't have strd.

    // Start of strn
    const WebRtc_UWord32 strnTag = MakeFourCc('s', 't', 'r', 'n');
    _bytesWritten += PutLE32(strnTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t strnSizeMark = _bytesWritten;

    _bytesWritten += PutBufferZ("WebRtc.avi ");

    PutLE32LengthFromCurrent(static_cast<long>(strnSizeMark));
    // End of strd.

    return 0;
}

WebRtc_Word32 AviFile::WriteMoviStart()
{
    // Create template movi list. Fill out size when known (i.e. when closing
    // file).
    const WebRtc_UWord32 listTag = MakeFourCc('L', 'I', 'S', 'T');
    _bytesWritten += PutLE32(listTag);

    _bytesWritten += PutLE32(0); //Size! Change later!
    _moviSizeMark = _bytesWritten;
    _moviListOffset = ftell(_aviFile);

    const WebRtc_UWord32 moviTag = MakeFourCc('m', 'o', 'v', 'i');
    _bytesWritten += PutLE32(moviTag);

    return 0;
}

size_t AviFile::PutByte(WebRtc_UWord8 byte)
{
    return fwrite(&byte, sizeof(WebRtc_UWord8), sizeof(WebRtc_UWord8),
                  _aviFile);
}

size_t AviFile::PutLE16(WebRtc_UWord16 word)
{
    return fwrite(&word, sizeof(WebRtc_UWord8), sizeof(WebRtc_UWord16),
                  _aviFile);
}

size_t AviFile::PutLE32(WebRtc_UWord32 word)
{
    return fwrite(&word, sizeof(WebRtc_UWord8), sizeof(WebRtc_UWord32),
                  _aviFile);
}

size_t AviFile::PutBuffer(const WebRtc_UWord8* str, size_t size)
{
    return fwrite(str, sizeof(WebRtc_UWord8), size,
                  _aviFile);
}

size_t AviFile::PutBufferZ(const char* str)
{
    // Include NULL charachter, hence the + 1
    return PutBuffer(reinterpret_cast<const WebRtc_UWord8*>(str),
                     strlen(str) + 1);
}

long AviFile::PutLE32LengthFromCurrent(long startPos)
{
    const long endPos = ftell(_aviFile);
    if (endPos < 0) {
        return 0;
    }
    bool success = (0 == fseek(_aviFile, startPos - 4, SEEK_SET));
    if (!success) {
        assert(false);
        return 0;
    }
    const long len = endPos - startPos;
    if (endPos > startPos) {
        PutLE32(len);
    }
    else {
        assert(false);
    }
    success = (0 == fseek(_aviFile, endPos, SEEK_SET));
    assert(success);
    return len;
}

void AviFile::PutLE32AtPos(long pos, WebRtc_UWord32 word)
{
    const long currPos = ftell(_aviFile);
    if (currPos < 0) {
        assert(false);
        return;
    }
    bool success = (0 == fseek(_aviFile, pos, SEEK_SET));
    if (!success) {
      assert(false);
      return;
    }
    PutLE32(word);
    success = (0 == fseek(_aviFile, currPos, SEEK_SET));
    assert(success);
}

void AviFile::CloseRead()
{
    if (_aviFile)
    {
        fclose(_aviFile);
        _aviFile = NULL;
    }
}

void AviFile::CloseWrite()
{
    if (_created)
    {
        // Update everything that isn't known until the file is closed. The
        // marks indicate where in the headers this update should be.
        PutLE32LengthFromCurrent(static_cast<long>(_moviSizeMark));

        PutLE32AtPos(static_cast<long>(_totNumFramesMark), _videoFrames);

        if (_writeVideoStream)
        {
            PutLE32AtPos(static_cast<long>(_videoStreamLengthMark),
                         _videoFrames);
        }

        if (_writeAudioStream)
        {
            PutLE32AtPos(static_cast<long>(_audioStreamLengthMark),
                         _audioFrames);
        }

        WriteIndex();
        PutLE32LengthFromCurrent(static_cast<long>(_riffSizeMark));
        ClearIndexList();

        if (_aviFile)
        {
            fclose(_aviFile);
            _aviFile = NULL;
        }
    }
}

void AviFile::ResetMembers()
{
    ResetComplexMembers();

    _aviFile = NULL;

    _nrStreams     = 0;
    _aviLength     = 0;
    _dataLength    = 0;
    _bytesRead     = 0;
    _dataStartByte = 0;
    _framesRead    = 0;
    _videoFrames   = 0;
    _audioFrames   = 0;

    _reading = false;
    _openedAs = AVI_AUDIO;
    _loop = false;
    _writing = false;

    _bytesWritten          = 0;

    _riffSizeMark          = 0;
    _moviSizeMark          = 0;
    _totNumFramesMark      = 0;
    _videoStreamLengthMark = 0;
    _audioStreamLengthMark = 0;

    _writeAudioStream = false;
    _writeVideoStream = false;

    _aviMode                      = NotSet;
    _videoCodecConfigParams       = 0;
    _videoCodecConfigParamsLength = 0;

    _videoStreamDataChunkPrefix = 0;
    _audioStreamDataChunkPrefix = 0;

    _created = false;

    _moviListOffset = 0;

    _videoConfigLength = 0;
}

void AviFile::ResetComplexMembers()
{
    memset(&_aviHeader, 0, sizeof(AVIMAINHEADER));
    memset(&_videoStreamHeader, 0, sizeof(AVISTREAMHEADER));
    memset(&_audioStreamHeader, 0, sizeof(AVISTREAMHEADER));
    memset(&_videoFormatHeader, 0, sizeof(BITMAPINFOHEADER));
    memset(&_audioFormatHeader, 0, sizeof(WAVEFORMATEX));
    memset(_videoConfigParameters, 0, CODEC_CONFIG_LENGTH);
    memset(_videoStreamName, 0, STREAM_NAME_LENGTH);
    memset(_audioStreamName, 0, STREAM_NAME_LENGTH);
    memset(&_videoStream, 0, sizeof(AVIStream));
    memset(&_audioStream, 0, sizeof(AVIStream));
}

size_t AviFile::GetByte(WebRtc_UWord8& word)
{
    return fread(&word, sizeof(WebRtc_UWord8), sizeof(WebRtc_UWord8), _aviFile);
}

size_t AviFile::GetLE16(WebRtc_UWord16& word)
{
    return fread(&word, sizeof(WebRtc_UWord8), sizeof(WebRtc_UWord16),
                 _aviFile);
}

size_t AviFile::GetLE32(WebRtc_UWord32& word)
{
    return fread(&word, sizeof(WebRtc_UWord8), sizeof(WebRtc_UWord32),
                 _aviFile);
}

size_t AviFile::GetBuffer(WebRtc_UWord8* str, size_t size)
{
    return fread(str, sizeof(WebRtc_UWord8), size, _aviFile);
}

WebRtc_Word32 AviFile::ReadRIFF()
{
    WebRtc_UWord32 tag;
    _bytesRead = GetLE32(tag);
    if (tag != MakeFourCc('R', 'I', 'F', 'F'))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,  "Not a RIFF file!");
        return -1;
    }

    WebRtc_UWord32 size;
    _bytesRead += GetLE32(size);
    _aviLength = size;

    _bytesRead += GetLE32(tag);
    if (tag != MakeFourCc('A', 'V', 'I', ' '))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,  "Not an AVI file!");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AviFile::ReadHeaders()
{
    WebRtc_UWord32 tag;
    _bytesRead += GetLE32(tag);
    WebRtc_UWord32 size;
    _bytesRead += GetLE32(size);

    if (tag != MakeFourCc('L', 'I', 'S', 'T'))
    {
        return -1;
    }

    WebRtc_UWord32 listTag;
    _bytesRead += GetLE32(listTag);
    if (listTag != MakeFourCc('h', 'd', 'r', 'l'))
    {
        return -1;
    }

    WebRtc_Word32 err = ReadAVIMainHeader();
    if (err)
    {
        return -1;
    }

    return 0;
}

WebRtc_Word32 AviFile::ReadAVIMainHeader()
{
    _bytesRead += GetLE32(_aviHeader.fcc);
    _bytesRead += GetLE32(_aviHeader.cb);
    _bytesRead += GetLE32(_aviHeader.dwMicroSecPerFrame);
    _bytesRead += GetLE32(_aviHeader.dwMaxBytesPerSec);
    _bytesRead += GetLE32(_aviHeader.dwPaddingGranularity);
    _bytesRead += GetLE32(_aviHeader.dwFlags);
    _bytesRead += GetLE32(_aviHeader.dwTotalFrames);
    _bytesRead += GetLE32(_aviHeader.dwInitialFrames);
    _bytesRead += GetLE32(_aviHeader.dwStreams);
    _bytesRead += GetLE32(_aviHeader.dwSuggestedBufferSize);
    _bytesRead += GetLE32(_aviHeader.dwWidth);
    _bytesRead += GetLE32(_aviHeader.dwHeight);
    _bytesRead += GetLE32(_aviHeader.dwReserved[0]);
    _bytesRead += GetLE32(_aviHeader.dwReserved[1]);
    _bytesRead += GetLE32(_aviHeader.dwReserved[2]);
    _bytesRead += GetLE32(_aviHeader.dwReserved[3]);

    if (_aviHeader.fcc != MakeFourCc('a', 'v', 'i', 'h'))
    {
        return -1;
    }

    if (_aviHeader.dwFlags & kAvifMustuseindex)
    {
        return -1;
    }

    bool readVideoStreamHeader = false;
    bool readAudioStreamHeader = false;
    unsigned int streamsRead = 0;
    while (_aviHeader.dwStreams > streamsRead)
    {
        WebRtc_UWord32 strltag;
        _bytesRead += GetLE32(strltag);
        WebRtc_UWord32 strlsize;
        _bytesRead += GetLE32(strlsize);
        const long endSeekPos = ftell(_aviFile) +
            static_cast<WebRtc_Word32>(strlsize);

        if (strltag != MakeFourCc('L', 'I', 'S', 'T'))
        {
            return -1;
        }

        WebRtc_UWord32 listTag;
        _bytesRead += GetLE32(listTag);
        if (listTag != MakeFourCc('s', 't', 'r', 'l'))
        {
            return -1;
        }

        WebRtc_UWord32 chunktag;
        _bytesRead += GetLE32(chunktag);
        WebRtc_UWord32 chunksize;
        _bytesRead += GetLE32(chunksize);

        if (chunktag != MakeFourCc('s', 't', 'r', 'h'))
        {
            return -1;
        }

        AVISTREAMHEADER tmpStreamHeader;
        tmpStreamHeader.fcc = chunktag;
        tmpStreamHeader.cb  = chunksize;

        _bytesRead += GetLE32(tmpStreamHeader.fccType);
        _bytesRead += GetLE32(tmpStreamHeader.fccHandler);
        _bytesRead += GetLE32(tmpStreamHeader.dwFlags);
        _bytesRead += GetLE16(tmpStreamHeader.wPriority);
        _bytesRead += GetLE16(tmpStreamHeader.wLanguage);
        _bytesRead += GetLE32(tmpStreamHeader.dwInitialFrames);
        _bytesRead += GetLE32(tmpStreamHeader.dwScale);
        _bytesRead += GetLE32(tmpStreamHeader.dwRate);
        _bytesRead += GetLE32(tmpStreamHeader.dwStart);
        _bytesRead += GetLE32(tmpStreamHeader.dwLength);
        _bytesRead += GetLE32(tmpStreamHeader.dwSuggestedBufferSize);
        _bytesRead += GetLE32(tmpStreamHeader.dwQuality);
        _bytesRead += GetLE32(tmpStreamHeader.dwSampleSize);

        WebRtc_UWord16 left;
        _bytesRead += GetLE16(left);
        tmpStreamHeader.rcFrame.left = left;
        WebRtc_UWord16 top;
        _bytesRead += GetLE16(top);
        tmpStreamHeader.rcFrame.top = top;
        WebRtc_UWord16 right;
        _bytesRead += GetLE16(right);
        tmpStreamHeader.rcFrame.right = right;
        WebRtc_UWord16 bottom;
        _bytesRead += GetLE16(bottom);
        tmpStreamHeader.rcFrame.bottom = bottom;

        if (!readVideoStreamHeader
            && (tmpStreamHeader.fccType == MakeFourCc('v', 'i', 'd', 's')))
        {
            _videoStreamHeader = tmpStreamHeader; //Bitwise copy is OK!
            const WebRtc_Word32 err = ReadAVIVideoStreamHeader(endSeekPos);
            if (err)
            {
                return -1;
            }
            // Make sure there actually is video data in the file...
            if (_videoStreamHeader.dwLength == 0)
            {
                return -1;
            }
            readVideoStreamHeader = true;
        } else if(!readAudioStreamHeader &&
                  (tmpStreamHeader.fccType == MakeFourCc('a', 'u', 'd', 's'))) {
            _audioStreamHeader = tmpStreamHeader;
            const WebRtc_Word32 err = ReadAVIAudioStreamHeader(endSeekPos);
            if (err)
            {
                return -1;
            }
            readAudioStreamHeader = true;
        }
        else
        {
            fseek(_aviFile, endSeekPos, SEEK_SET);
            _bytesRead += endSeekPos;
        }

        ++streamsRead;
    }

    if (!readVideoStreamHeader && !readAudioStreamHeader)
    {
        return -1;
    }

    WebRtc_UWord32 tag;
    _bytesRead += GetLE32(tag);
    WebRtc_UWord32 size;
    _bytesRead += GetLE32(size);

    if (tag == MakeFourCc('J', 'U', 'N', 'K'))
    {
        fseek(_aviFile, size, SEEK_CUR);
        _bytesRead += size;
        _bytesRead += GetLE32(tag);
        _bytesRead += GetLE32(size);
    }
    if (tag != MakeFourCc('L', 'I', 'S', 'T'))
    {
        return -1;
    }
    WebRtc_UWord32 listTag;
    _bytesRead += GetLE32(listTag);
    if (listTag != MakeFourCc('m', 'o', 'v', 'i'))
    {
        return -1;
    }
    _dataLength = size;
    return 0;
}

WebRtc_Word32 AviFile::ReadAVIVideoStreamHeader(WebRtc_Word32 endpos)
{
    WebRtc_UWord32 chunktag;
    _bytesRead += GetLE32(chunktag);
    WebRtc_UWord32 chunksize;
    _bytesRead += GetLE32(chunksize);

    if (chunktag != MakeFourCc('s', 't', 'r', 'f'))
    {
        return -1;
    }

    _bytesRead += GetLE32(_videoFormatHeader.biSize);
    _bytesRead += GetLE32(_videoFormatHeader.biWidth);
    _bytesRead += GetLE32(_videoFormatHeader.biHeight);
    _bytesRead += GetLE16(_videoFormatHeader.biPlanes);
    _bytesRead += GetLE16(_videoFormatHeader.biBitCount);
    _bytesRead += GetLE32(_videoFormatHeader.biCompression);
    _bytesRead += GetLE32(_videoFormatHeader.biSizeImage);
    _bytesRead += GetLE32(_videoFormatHeader.biXPelsPerMeter);
    _bytesRead += GetLE32(_videoFormatHeader.biYPelsPerMeter);
    _bytesRead += GetLE32(_videoFormatHeader.biClrUsed);
    _bytesRead += GetLE32(_videoFormatHeader.biClrImportant);

    if (chunksize >  _videoFormatHeader.biSize)
    {
        const WebRtc_UWord32 size = chunksize - _videoFormatHeader.biSize;
        const WebRtc_UWord32 readSize = MinValue(size, CODEC_CONFIG_LENGTH);
        _bytesRead += GetBuffer(
            reinterpret_cast<WebRtc_UWord8*>(_videoConfigParameters), readSize);
        _videoConfigLength = readSize;
        WebRtc_Word32 skipSize = chunksize - _videoFormatHeader.biSize -
            readSize;
        if (skipSize > 0)
        {
            fseek(_aviFile, skipSize, SEEK_CUR);
            _bytesRead += skipSize;
        }
    }

    while (static_cast<long>(_bytesRead) < endpos)
    {
        WebRtc_UWord32 chunktag;
        _bytesRead += GetLE32(chunktag);
        WebRtc_UWord32 chunksize;
        _bytesRead += GetLE32(chunksize);

        if (chunktag == MakeFourCc('s', 't', 'r', 'n'))
        {
            const WebRtc_UWord32 size = MinValue(chunksize, STREAM_NAME_LENGTH);
            _bytesRead += GetBuffer(
                reinterpret_cast<WebRtc_UWord8*>(_videoStreamName), size);
        }
        else if (chunktag == MakeFourCc('s', 't', 'r', 'd'))
        {
            const WebRtc_UWord32 size = MinValue(chunksize,
                                                 CODEC_CONFIG_LENGTH);
            _bytesRead += GetBuffer(
                reinterpret_cast<WebRtc_UWord8*>(_videoConfigParameters), size);
            _videoConfigLength = size;
        }
        else
        {
            fseek(_aviFile, chunksize, SEEK_CUR);
            _bytesRead += chunksize;
        }

        if (feof(_aviFile))
        {
            return -1;
        }
    }
    _videoStream.streamType = AviFile::AVI_VIDEO;
    _videoStream.streamNumber = _nrStreams++;

    return 0;
}

WebRtc_Word32 AviFile::ReadAVIAudioStreamHeader(WebRtc_Word32 endpos)
{
    WebRtc_UWord32 chunktag;
    _bytesRead += GetLE32(chunktag);
    WebRtc_UWord32 chunksize;
    _bytesRead += GetLE32(chunksize);

    if (chunktag != MakeFourCc('s', 't', 'r', 'f'))
    {
        return -1;
    }

    const size_t startRead = _bytesRead;
    _bytesRead += GetLE16(_audioFormatHeader.wFormatTag);
    _bytesRead += GetLE16(_audioFormatHeader.nChannels);
    _bytesRead += GetLE32(_audioFormatHeader.nSamplesPerSec);
    _bytesRead += GetLE32(_audioFormatHeader.nAvgBytesPerSec);
    _bytesRead += GetLE16(_audioFormatHeader.nBlockAlign);
    _bytesRead += GetLE16(_audioFormatHeader.wBitsPerSample);
    if (chunksize > 0x10) {
        _bytesRead += GetLE16(_audioFormatHeader.cbSize);
    }

    const WebRtc_UWord32 diffRead = chunksize - (_bytesRead - startRead);
    if (diffRead > 0)
    {
        const WebRtc_UWord32 size = MinValue(diffRead, CODEC_CONFIG_LENGTH);
        _bytesRead += GetBuffer(
            reinterpret_cast<WebRtc_UWord8*>(_audioConfigParameters), size);
    }

    while (static_cast<long>(_bytesRead) < endpos)
    {
        WebRtc_UWord32 chunktag;
        _bytesRead += GetLE32(chunktag);
        WebRtc_UWord32 chunksize;
        _bytesRead += GetLE32(chunksize);

        if (chunktag == MakeFourCc('s', 't', 'r', 'n'))
        {
            const WebRtc_UWord32 size = MinValue(chunksize, STREAM_NAME_LENGTH);
            _bytesRead += GetBuffer(
                reinterpret_cast<WebRtc_UWord8*>(_audioStreamName), size);
        }
        else if (chunktag == MakeFourCc('s', 't', 'r', 'd'))
        {
            const WebRtc_UWord32 size = MinValue(chunksize,
                                                 CODEC_CONFIG_LENGTH);
            _bytesRead += GetBuffer(
                reinterpret_cast<WebRtc_UWord8*>(_audioConfigParameters), size);
        }
        else
        {
            fseek(_aviFile, chunksize, SEEK_CUR);
            _bytesRead += chunksize;
        }

        if (feof(_aviFile))
        {
            return -1;
        }
    }
    _audioStream.streamType = AviFile::AVI_AUDIO;
    _audioStream.streamNumber = _nrStreams++;
    return 0;
}

WebRtc_UWord32 AviFile::StreamAndTwoCharCodeToTag(WebRtc_Word32 streamNum,
                                                  const char* twoCharCode)
{
    WebRtc_UWord8 a = '0';
    WebRtc_UWord8 b;
    switch (streamNum)
    {
    case 1:
        b = '1';
        break;
    case 2:
        b = '2';
        break;
    default:
        b = '0';
    }
    return MakeFourCc(a, b, twoCharCode[0], twoCharCode[1]);
}

void AviFile::ClearIndexList()
{
    while (!_indexList->Empty())
    {
        ListItem* listItem = _indexList->First();
        if (listItem == 0)
        {
            break;
        }

        AVIINDEXENTRY* item = static_cast<AVIINDEXENTRY*>(listItem->GetItem());
        if (item != NULL)
        {
            delete item;
        }
        _indexList->PopFront();
    }
}

void AviFile::AddChunkToIndexList(WebRtc_UWord32 inChunkId,
                                  WebRtc_UWord32 inFlags,
                                  WebRtc_UWord32 inOffset,
                                  WebRtc_UWord32 inSize)
{
    _indexList->PushBack(new AVIINDEXENTRY(inChunkId, inFlags, inOffset,
                                           inSize));
}

void AviFile::WriteIndex()
{
    const WebRtc_UWord32 idxTag = MakeFourCc('i', 'd', 'x', '1');
    _bytesWritten += PutLE32(idxTag);

    // Size is unknown at this point. Update later.
    _bytesWritten += PutLE32(0);
    const size_t idxChunkSize = _bytesWritten;

    for (ListItem* listItem = _indexList->First();
         listItem != NULL;
         listItem = _indexList->Next(listItem))
    {
        const AVIINDEXENTRY* item =
            static_cast<AVIINDEXENTRY*>(listItem->GetItem());
        if (item != NULL)
        {
            _bytesWritten += PutLE32(item->ckid);
            _bytesWritten += PutLE32(item->dwFlags);
            _bytesWritten += PutLE32(item->dwChunkOffset);
            _bytesWritten += PutLE32(item->dwChunkLength);
        }
    }
    PutLE32LengthFromCurrent(static_cast<long>(idxChunkSize));
}
} // namespace webrtc
