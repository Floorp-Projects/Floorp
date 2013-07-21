/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "file_player_impl.h"
#include "trace.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    #include "frame_scaler.h"
    #include "tick_util.h"
    #include "video_coder.h"
#endif

// OS independent case insensitive string comparison.
#ifdef WIN32
    #define STR_CASE_CMP(x,y) ::_stricmp(x,y)
#else
    #define STR_CASE_CMP(x,y) ::strcasecmp(x,y)
#endif

namespace webrtc {
FilePlayer* FilePlayer::CreateFilePlayer(uint32_t instanceID,
                                         FileFormats fileFormat)
{
    switch(fileFormat)
    {
    case kFileFormatWavFile:
    case kFileFormatCompressedFile:
    case kFileFormatPreencodedFile:
    case kFileFormatPcm16kHzFile:
    case kFileFormatPcm8kHzFile:
    case kFileFormatPcm32kHzFile:
        // audio formats
        return new FilePlayerImpl(instanceID, fileFormat);
    case kFileFormatAviFile:
#ifdef WEBRTC_MODULE_UTILITY_VIDEO
        return new VideoFilePlayerImpl(instanceID, fileFormat);
#else
        WEBRTC_TRACE(kTraceError, kTraceFile, -1,
                     "Invalid file format: %d", kFileFormatAviFile);
        assert(false);
        return NULL;
#endif
    }
    assert(false);
    return NULL;
}

void FilePlayer::DestroyFilePlayer(FilePlayer* player)
{
    delete player;
}

FilePlayerImpl::FilePlayerImpl(const uint32_t instanceID,
                               const FileFormats fileFormat)
    : _instanceID(instanceID),
      _fileFormat(fileFormat),
      _fileModule(*MediaFile::CreateMediaFile(instanceID)),
      _decodedLengthInMS(0),
      _audioDecoder(instanceID),
      _codec(),
      _numberOf10MsPerFrame(0),
      _numberOf10MsInDecoder(0),
      _resampler(),
      _scaling(1.0)
{
    _codec.plfreq = 0;
}

FilePlayerImpl::~FilePlayerImpl()
{
    MediaFile::DestroyMediaFile(&_fileModule);
}

int32_t FilePlayerImpl::Frequency() const
{
    if(_codec.plfreq == 0)
    {
        return -1;
    }
    // Make sure that sample rate is 8,16 or 32 kHz. E.g. WAVE files may have
    // other sampling rates.
    if(_codec.plfreq == 11000)
    {
        return 16000;
    }
    else if(_codec.plfreq == 22000)
    {
        return 32000;
    }
    else if(_codec.plfreq == 44100 || _codec.plfreq == 44000 ) // XXX just 44100?
    {
        return 32000;
    }
    else if(_codec.plfreq == 48000)
    {
        return 32000;
    }
    else
    {
        return _codec.plfreq;
    }
}

int32_t FilePlayerImpl::AudioCodec(CodecInst& audioCodec) const
{
    audioCodec = _codec;
    return 0;
}

int32_t FilePlayerImpl::Get10msAudioFromFile(
    int16_t* outBuffer,
    int& lengthInSamples,
    int frequencyInHz)
{
    if(_codec.plfreq == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, _instanceID,
           "FilePlayerImpl::Get10msAudioFromFile() playing not started!\
 codecFreq = %d, wantedFreq = %d",
           _codec.plfreq, frequencyInHz);
        return -1;
    }

    AudioFrame unresampledAudioFrame;
    if(STR_CASE_CMP(_codec.plname, "L16") == 0)
    {
        unresampledAudioFrame.sample_rate_hz_ = _codec.plfreq;

        // L16 is un-encoded data. Just pull 10 ms.
        uint32_t lengthInBytes =
            sizeof(unresampledAudioFrame.data_);
        if (_fileModule.PlayoutAudioData(
                (int8_t*)unresampledAudioFrame.data_,
                lengthInBytes) == -1)
        {
            // End of file reached.
            return -1;
        }
        if(lengthInBytes == 0)
        {
            lengthInSamples = 0;
            return 0;
        }
        // One sample is two bytes.
        unresampledAudioFrame.samples_per_channel_ =
            (uint16_t)lengthInBytes >> 1;

    }else {
        // Decode will generate 10 ms of audio data. PlayoutAudioData(..)
        // expects a full frame. If the frame size is larger than 10 ms,
        // PlayoutAudioData(..) data should be called proportionally less often.
        int16_t encodedBuffer[MAX_AUDIO_BUFFER_IN_SAMPLES];
        uint32_t encodedLengthInBytes = 0;
        if(++_numberOf10MsInDecoder >= _numberOf10MsPerFrame)
        {
            _numberOf10MsInDecoder = 0;
            uint32_t bytesFromFile = sizeof(encodedBuffer);
            if (_fileModule.PlayoutAudioData((int8_t*)encodedBuffer,
                                             bytesFromFile) == -1)
            {
                // End of file reached.
                return -1;
            }
            encodedLengthInBytes = bytesFromFile;
        }
        if(_audioDecoder.Decode(unresampledAudioFrame,frequencyInHz,
                                (int8_t*)encodedBuffer,
                                encodedLengthInBytes) == -1)
        {
            return -1;
        }
    }

    int outLen = 0;
    if(_resampler.ResetIfNeeded(unresampledAudioFrame.sample_rate_hz_,
                                frequencyInHz, kResamplerSynchronous))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, _instanceID,
           "FilePlayerImpl::Get10msAudioFromFile() unexpected codec");

        // New sampling frequency. Update state.
        outLen = frequencyInHz / 100;
        memset(outBuffer, 0, outLen * sizeof(int16_t));
        return 0;
    }
    _resampler.Push(unresampledAudioFrame.data_,
                    unresampledAudioFrame.samples_per_channel_,
                    outBuffer,
                    MAX_AUDIO_BUFFER_IN_SAMPLES,
                    outLen);

    lengthInSamples = outLen;

    if(_scaling != 1.0)
    {
        for (int i = 0;i < outLen; i++)
        {
            outBuffer[i] = (int16_t)(outBuffer[i] * _scaling);
        }
    }
    _decodedLengthInMS += 10;
    return 0;
}

int32_t FilePlayerImpl::RegisterModuleFileCallback(FileCallback* callback)
{
    return _fileModule.SetModuleFileCallback(callback);
}

int32_t FilePlayerImpl::SetAudioScaling(float scaleFactor)
{
    if((scaleFactor >= 0)&&(scaleFactor <= 2.0))
    {
        _scaling = scaleFactor;
        return 0;
    }
    WEBRTC_TRACE(kTraceWarning, kTraceVoice, _instanceID,
              "FilePlayerImpl::SetAudioScaling() not allowed scale factor");
    return -1;
}

int32_t FilePlayerImpl::StartPlayingFile(const char* fileName,
                                         bool loop,
                                         uint32_t startPosition,
                                         float volumeScaling,
                                         uint32_t notification,
                                         uint32_t stopPosition,
                                         const CodecInst* codecInst)
{
    if (_fileFormat == kFileFormatPcm16kHzFile ||
        _fileFormat == kFileFormatPcm8kHzFile||
        _fileFormat == kFileFormatPcm32kHzFile )
    {
        CodecInst codecInstL16;
        strncpy(codecInstL16.plname,"L16",32);
        codecInstL16.pltype   = 93;
        codecInstL16.channels = 1;

        if (_fileFormat == kFileFormatPcm8kHzFile)
        {
            codecInstL16.rate     = 128000;
            codecInstL16.plfreq   = 8000;
            codecInstL16.pacsize  = 80;

        } else if(_fileFormat == kFileFormatPcm16kHzFile)
        {
            codecInstL16.rate     = 256000;
            codecInstL16.plfreq   = 16000;
            codecInstL16.pacsize  = 160;

        }else if(_fileFormat == kFileFormatPcm32kHzFile)
        {
            codecInstL16.rate     = 512000;
            codecInstL16.plfreq   = 32000;
            codecInstL16.pacsize  = 160;
        } else
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, _instanceID,
                       "FilePlayerImpl::StartPlayingFile() sample frequency\
 specifed not supported for PCM format.");
            return -1;
        }

        if (_fileModule.StartPlayingAudioFile(fileName, notification, loop,
                                              _fileFormat, &codecInstL16,
                                              startPosition,
                                              stopPosition) == -1)
        {
            WEBRTC_TRACE(
                kTraceWarning,
                kTraceVoice,
                _instanceID,
                "FilePlayerImpl::StartPlayingFile() failed to initialize file\
 %s playout.", fileName);
            return -1;
        }
        SetAudioScaling(volumeScaling);
    }else if(_fileFormat == kFileFormatPreencodedFile)
    {
        if (_fileModule.StartPlayingAudioFile(fileName, notification, loop,
                                              _fileFormat, codecInst) == -1)
        {
            WEBRTC_TRACE(
                kTraceWarning,
                kTraceVoice,
                _instanceID,
                "FilePlayerImpl::StartPlayingPreEncodedFile() failed to\
 initialize pre-encoded file %s playout.",
                fileName);
            return -1;
        }
    } else
    {
        CodecInst* no_inst = NULL;
        if (_fileModule.StartPlayingAudioFile(fileName, notification, loop,
                                              _fileFormat, no_inst,
                                              startPosition,
                                              stopPosition) == -1)
        {
            WEBRTC_TRACE(
                kTraceWarning,
                kTraceVoice,
                _instanceID,
                "FilePlayerImpl::StartPlayingFile() failed to initialize file\
 %s playout.", fileName);
            return -1;
        }
        SetAudioScaling(volumeScaling);
    }
    if (SetUpAudioDecoder() == -1)
    {
        StopPlayingFile();
        return -1;
    }
    return 0;
}

int32_t FilePlayerImpl::StartPlayingFile(InStream& sourceStream,
                                         uint32_t startPosition,
                                         float volumeScaling,
                                         uint32_t notification,
                                         uint32_t stopPosition,
                                         const CodecInst* codecInst)
{
    if (_fileFormat == kFileFormatPcm16kHzFile ||
        _fileFormat == kFileFormatPcm32kHzFile ||
        _fileFormat == kFileFormatPcm8kHzFile)
    {
        CodecInst codecInstL16;
        strncpy(codecInstL16.plname,"L16",32);
        codecInstL16.pltype   = 93;
        codecInstL16.channels = 1;

        if (_fileFormat == kFileFormatPcm8kHzFile)
        {
            codecInstL16.rate     = 128000;
            codecInstL16.plfreq   = 8000;
            codecInstL16.pacsize  = 80;

        }else if (_fileFormat == kFileFormatPcm16kHzFile)
        {
            codecInstL16.rate     = 256000;
            codecInstL16.plfreq   = 16000;
            codecInstL16.pacsize  = 160;

        }else if (_fileFormat == kFileFormatPcm32kHzFile)
        {
            codecInstL16.rate     = 512000;
            codecInstL16.plfreq   = 32000;
            codecInstL16.pacsize  = 160;
        }else
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceVoice,
                _instanceID,
                "FilePlayerImpl::StartPlayingFile() sample frequency specifed\
 not supported for PCM format.");
            return -1;
        }
        if (_fileModule.StartPlayingAudioStream(sourceStream, notification,
                                                _fileFormat, &codecInstL16,
                                                startPosition,
                                                stopPosition) == -1)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceVoice,
                _instanceID,
                "FilePlayerImpl::StartPlayingFile() failed to initialize stream\
 playout.");
            return -1;
        }

    }else if(_fileFormat == kFileFormatPreencodedFile)
    {
        if (_fileModule.StartPlayingAudioStream(sourceStream, notification,
                                                _fileFormat, codecInst) == -1)
        {
            WEBRTC_TRACE(
                kTraceWarning,
                kTraceVoice,
                _instanceID,
                "FilePlayerImpl::StartPlayingFile() failed to initialize stream\
 playout.");
            return -1;
        }
    } else {
        CodecInst* no_inst = NULL;
        if (_fileModule.StartPlayingAudioStream(sourceStream, notification,
                                                _fileFormat, no_inst,
                                                startPosition,
                                                stopPosition) == -1)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice, _instanceID,
                       "FilePlayerImpl::StartPlayingFile() failed to initialize\
 stream playout.");
            return -1;
        }
    }
    SetAudioScaling(volumeScaling);

    if (SetUpAudioDecoder() == -1)
    {
        StopPlayingFile();
        return -1;
    }
    return 0;
}

int32_t FilePlayerImpl::StopPlayingFile()
{
    memset(&_codec, 0, sizeof(CodecInst));
    _numberOf10MsPerFrame  = 0;
    _numberOf10MsInDecoder = 0;
    return _fileModule.StopPlaying();
}

bool FilePlayerImpl::IsPlayingFile() const
{
    return _fileModule.IsPlaying();
}

int32_t FilePlayerImpl::GetPlayoutPosition(uint32_t& durationMs)
{
    return _fileModule.PlayoutPositionMs(durationMs);
}

int32_t FilePlayerImpl::SetUpAudioDecoder()
{
    if ((_fileModule.codec_info(_codec) == -1))
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVoice,
            _instanceID,
            "FilePlayerImpl::StartPlayingFile() failed to retrieve Codec info\
 of file data.");
        return -1;
    }
    if( STR_CASE_CMP(_codec.plname, "L16") != 0 &&
        _audioDecoder.SetDecodeCodec(_codec,AMRFileStorage) == -1)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVoice,
            _instanceID,
            "FilePlayerImpl::StartPlayingFile() codec %s not supported",
            _codec.plname);
        return -1;
    }
    _numberOf10MsPerFrame = _codec.pacsize / (_codec.plfreq / 100);
    _numberOf10MsInDecoder = 0;
    return 0;
}

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
VideoFilePlayerImpl::VideoFilePlayerImpl(uint32_t instanceID,
                                         FileFormats fileFormat)
    : FilePlayerImpl(instanceID,fileFormat),
      _videoDecoder(*new VideoCoder(instanceID)),
      video_codec_info_(),
      _decodedVideoFrames(0),
      _encodedData(*new EncodedVideoData()),
      _frameScaler(*new FrameScaler()),
      _critSec(CriticalSectionWrapper::CreateCriticalSection()),
      _startTime(),
      _accumulatedRenderTimeMs(0),
      _frameLengthMS(0),
      _numberOfFramesRead(0),
      _videoOnly(false)
{
    memset(&video_codec_info_, 0, sizeof(video_codec_info_));
}

VideoFilePlayerImpl::~VideoFilePlayerImpl()
{
    delete _critSec;
    delete &_frameScaler;
    delete &_videoDecoder;
    delete &_encodedData;
}

int32_t VideoFilePlayerImpl::StartPlayingVideoFile(
    const char* fileName,
    bool loop,
    bool videoOnly)
{
    CriticalSectionScoped lock( _critSec);

    if(_fileModule.StartPlayingVideoFile(fileName, loop, videoOnly,
                                         _fileFormat) != 0)
    {
        return -1;
    }

    _decodedVideoFrames = 0;
    _accumulatedRenderTimeMs = 0;
    _frameLengthMS = 0;
    _numberOfFramesRead = 0;
    _videoOnly = videoOnly;

    // Set up video_codec_info_ according to file,
    if(SetUpVideoDecoder() != 0)
    {
        StopPlayingFile();
        return -1;
    }
    if(!videoOnly)
    {
        // Set up _codec according to file,
        if(SetUpAudioDecoder() != 0)
        {
            StopPlayingFile();
            return -1;
        }
    }
    return 0;
}

int32_t VideoFilePlayerImpl::StopPlayingFile()
{
    CriticalSectionScoped lock( _critSec);

    _decodedVideoFrames = 0;
    _videoDecoder.ResetDecoder();

    return FilePlayerImpl::StopPlayingFile();
}

int32_t VideoFilePlayerImpl::GetVideoFromFile(I420VideoFrame& videoFrame,
                                              uint32_t outWidth,
                                              uint32_t outHeight)
{
    CriticalSectionScoped lock( _critSec);

    int32_t retVal = GetVideoFromFile(videoFrame);
    if(retVal != 0)
    {
        return retVal;
    }
    if (!videoFrame.IsZeroSize())
    {
        retVal = _frameScaler.ResizeFrameIfNeeded(&videoFrame, outWidth,
                                                  outHeight);
    }
    return retVal;
}

int32_t VideoFilePlayerImpl::GetVideoFromFile(I420VideoFrame& videoFrame)
{
    CriticalSectionScoped lock( _critSec);
    // No new video data read from file.
    if(_encodedData.payloadSize == 0)
    {
        videoFrame.ResetSize();
        return -1;
    }
    int32_t retVal = 0;
    if(strncmp(video_codec_info_.plName, "I420", 5) == 0)
    {
      int size_y = video_codec_info_.width * video_codec_info_.height;
      int half_width = (video_codec_info_.width + 1) / 2;
      int half_height = (video_codec_info_.height + 1) / 2;
      int size_uv = half_width * half_height;

      // TODO(mikhal): Do we need to align the stride here?
      const uint8_t* buffer_y = _encodedData.payloadData;
      const uint8_t* buffer_u = buffer_y + size_y;
      const uint8_t* buffer_v = buffer_u + size_uv;
      videoFrame.CreateFrame(size_y, buffer_y,
                             size_uv, buffer_u,
                             size_uv, buffer_v,
                             video_codec_info_.width, video_codec_info_.height,
                             video_codec_info_.height, half_width, half_width);
    }else
    {
        // Set the timestamp manually since there is no timestamp in the file.
        // Update timestam according to 90 kHz stream.
        _encodedData.timeStamp += (90000 / video_codec_info_.maxFramerate);
        retVal = _videoDecoder.Decode(videoFrame, _encodedData);
    }

    int64_t renderTimeMs = TickTime::MillisecondTimestamp();
    videoFrame.set_render_time_ms(renderTimeMs);

     // Indicate that the current frame in the encoded buffer is old/has
     // already been read.
    _encodedData.payloadSize = 0;
    if( retVal == 0)
    {
        _decodedVideoFrames++;
    }
    return retVal;
}

int32_t VideoFilePlayerImpl::video_codec_info(
    VideoCodec& videoCodec) const
{
    if(video_codec_info_.plName[0] == 0)
    {
        return -1;
    }
    memcpy(&videoCodec, &video_codec_info_, sizeof(VideoCodec));
    return 0;
}

int32_t VideoFilePlayerImpl::TimeUntilNextVideoFrame()
{
    if(_fileFormat != kFileFormatAviFile)
    {
        return -1;
    }
    if(!_fileModule.IsPlaying())
    {
        return -1;
    }
    if(_encodedData.payloadSize <= 0)
    {
        // Read next frame from file.
        CriticalSectionScoped lock( _critSec);

        if(_fileFormat == kFileFormatAviFile)
        {
            // Get next video frame
            uint32_t encodedBufferLengthInBytes = _encodedData.bufferSize;
            if(_fileModule.PlayoutAVIVideoData(
                   reinterpret_cast< int8_t*>(_encodedData.payloadData),
                   encodedBufferLengthInBytes) != 0)
            {
                 WEBRTC_TRACE(
                     kTraceWarning,
                     kTraceVideo,
                     _instanceID,
                     "FilePlayerImpl::TimeUntilNextVideoFrame() error reading\
 video data");
                return -1;
            }
            _encodedData.payloadSize = encodedBufferLengthInBytes;
            _encodedData.codec = video_codec_info_.codecType;
            _numberOfFramesRead++;

            if(_accumulatedRenderTimeMs == 0)
            {
                _startTime = TickTime::Now();
                // This if-statement should only trigger once.
                _accumulatedRenderTimeMs = 1;
            } else {
                // A full seconds worth of frames have been read.
                if(_numberOfFramesRead % video_codec_info_.maxFramerate == 0)
                {
                    // Frame rate is in frames per seconds. Frame length is
                    // calculated as an integer division which means it may
                    // be rounded down. Compensate for this every second.
                    uint32_t rest = 1000%_frameLengthMS;
                    _accumulatedRenderTimeMs += rest;
                }
                _accumulatedRenderTimeMs += _frameLengthMS;
            }
        }
    }

    int64_t timeToNextFrame;
    if(_videoOnly)
    {
        timeToNextFrame = _accumulatedRenderTimeMs -
            (TickTime::Now() - _startTime).Milliseconds();

    } else {
        // Synchronize with the audio stream instead of system clock.
        timeToNextFrame = _accumulatedRenderTimeMs - _decodedLengthInMS;
    }
    if(timeToNextFrame < 0)
    {
        return 0;

    } else if(timeToNextFrame > 0x0fffffff)
    {
        // Wraparound or audio stream has gone to far ahead of the video stream.
        return -1;
    }
    return static_cast<int32_t>(timeToNextFrame);
}

int32_t VideoFilePlayerImpl::SetUpVideoDecoder()
{
    if (_fileModule.VideoCodecInst(video_codec_info_) != 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVideo,
            _instanceID,
            "FilePlayerImpl::SetVideoDecoder() failed to retrieve Codec info of\
 file data.");
        return -1;
    }

    int32_t useNumberOfCores = 1;
    if(_videoDecoder.SetDecodeCodec(video_codec_info_, useNumberOfCores) != 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVideo,
            _instanceID,
            "FilePlayerImpl::SetUpVideoDecoder() codec %s not supported",
            video_codec_info_.plName);
        return -1;
    }

    _frameLengthMS = 1000/video_codec_info_.maxFramerate;

    // Size of unencoded data (I420) should be the largest possible frame size
    // in a file.
    const uint32_t KReadBufferSize = 3 * video_codec_info_.width *
        video_codec_info_.height / 2;
    _encodedData.VerifyAndAllocate(KReadBufferSize);
    _encodedData.encodedHeight = video_codec_info_.height;
    _encodedData.encodedWidth = video_codec_info_.width;
    _encodedData.payloadType = video_codec_info_.plType;
    _encodedData.timeStamp = 0;
    return 0;
}
#endif // WEBRTC_MODULE_UTILITY_VIDEO
} // namespace webrtc
