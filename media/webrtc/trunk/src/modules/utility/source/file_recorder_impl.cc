/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"
#include "file_recorder_impl.h"
#include "media_file.h"
#include "trace.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    #include "cpu_wrapper.h"
    #include "critical_section_wrapper.h"
    #include "frame_scaler.h"
    #include "video_coder.h"
    #include "video_frames_queue.h"
#endif

// OS independent case insensitive string comparison.
#ifdef WIN32
    #define STR_CASE_CMP(x,y) ::_stricmp(x,y)
#else
    #define STR_CASE_CMP(x,y) ::strcasecmp(x,y)
#endif

namespace webrtc {
FileRecorder* FileRecorder::CreateFileRecorder(WebRtc_UWord32 instanceID,
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
        return new FileRecorderImpl(instanceID, fileFormat);
    case kFileFormatAviFile:
#ifdef WEBRTC_MODULE_UTILITY_VIDEO
        return new AviRecorder(instanceID, fileFormat);
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

void FileRecorder::DestroyFileRecorder(FileRecorder* recorder)
{
    delete recorder;
}

FileRecorderImpl::FileRecorderImpl(WebRtc_UWord32 instanceID,
                                   FileFormats fileFormat)
    : _instanceID(instanceID),
      _fileFormat(fileFormat),
      _moduleFile(MediaFile::CreateMediaFile(_instanceID)),
      _stream(NULL),
      codec_info_(),
      _amrFormat(AMRFileStorage),
      _audioBuffer(),
      _audioEncoder(instanceID),
      _audioResampler()
{
}

FileRecorderImpl::~FileRecorderImpl()
{
    MediaFile::DestroyMediaFile(_moduleFile);
}

FileFormats FileRecorderImpl::RecordingFileFormat() const
{
    return _fileFormat;
}

WebRtc_Word32 FileRecorderImpl::RegisterModuleFileCallback(
    FileCallback* callback)
{
    if(_moduleFile == NULL)
    {
        return -1;
    }
    return _moduleFile->SetModuleFileCallback(callback);
}

WebRtc_Word32 FileRecorderImpl::StartRecordingAudioFile(
    const char* fileName,
    const CodecInst& codecInst,
    WebRtc_UWord32 notificationTimeMs,
    ACMAMRPackingFormat amrFormat)
{
    if(_moduleFile == NULL)
    {
        return -1;
    }
    codec_info_ = codecInst;
    _amrFormat = amrFormat;

    WebRtc_Word32 retVal = 0;
    if(_fileFormat != kFileFormatAviFile)
    {
        // AVI files should be started using StartRecordingVideoFile(..) all
        // other formats should use this API.
        retVal =_moduleFile->StartRecordingAudioFile(fileName, _fileFormat,
                                                     codecInst,
                                                     notificationTimeMs);
    }

    if( retVal == 0)
    {
        retVal = SetUpAudioEncoder();
    }
    if( retVal != 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVoice,
            _instanceID,
            "FileRecorder::StartRecording() failed to initialize file %s for\
 recording.",
            fileName);

        if(IsRecording())
        {
            StopRecording();
        }
    }
    return retVal;
}

WebRtc_Word32 FileRecorderImpl::StartRecordingAudioFile(
    OutStream& destStream,
    const CodecInst& codecInst,
    WebRtc_UWord32 notificationTimeMs,
    ACMAMRPackingFormat amrFormat)
{
    codec_info_ = codecInst;
    _amrFormat = amrFormat;

    WebRtc_Word32 retVal = _moduleFile->StartRecordingAudioStream(
        destStream,
        _fileFormat,
        codecInst,
        notificationTimeMs);

    if( retVal == 0)
    {
        retVal = SetUpAudioEncoder();
    }
    if( retVal != 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVoice,
            _instanceID,
            "FileRecorder::StartRecording() failed to initialize outStream for\
 recording.");

        if(IsRecording())
        {
            StopRecording();
        }
    }
    return retVal;
}

WebRtc_Word32 FileRecorderImpl::StopRecording()
{
    memset(&codec_info_, 0, sizeof(CodecInst));
    return _moduleFile->StopRecording();
}

bool FileRecorderImpl::IsRecording() const
{
    return _moduleFile->IsRecording();
}

WebRtc_Word32 FileRecorderImpl::RecordAudioToFile(
    const AudioFrame& incomingAudioFrame,
    const TickTime* playoutTS)
{
    if (codec_info_.plfreq == 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceVoice,
            _instanceID,
            "FileRecorder::RecordAudioToFile() recording audio is not turned\
 on");
        return -1;
    }
    AudioFrame tempAudioFrame;
    tempAudioFrame._payloadDataLengthInSamples = 0;
    if( incomingAudioFrame._audioChannel == 2 &&
        !_moduleFile->IsStereo())
    {
        // Recording mono but incoming audio is (interleaved) stereo.
        tempAudioFrame._audioChannel = 1;
        tempAudioFrame._frequencyInHz = incomingAudioFrame._frequencyInHz;
        tempAudioFrame._payloadDataLengthInSamples =
          incomingAudioFrame._payloadDataLengthInSamples;
        for (WebRtc_UWord16 i = 0;
             i < (incomingAudioFrame._payloadDataLengthInSamples); i++)
        {
            // Sample value is the average of left and right buffer rounded to
            // closest integer value. Note samples can be either 1 or 2 byte.
             tempAudioFrame._payloadData[i] =
                 ((incomingAudioFrame._payloadData[2 * i] +
                   incomingAudioFrame._payloadData[(2 * i) + 1] + 1) >> 1);
        }
    }
    else if( incomingAudioFrame._audioChannel == 1 &&
        _moduleFile->IsStereo())
    {
        // Recording stereo but incoming audio is mono.
        tempAudioFrame._audioChannel = 2;
        tempAudioFrame._frequencyInHz = incomingAudioFrame._frequencyInHz;
        tempAudioFrame._payloadDataLengthInSamples =
          incomingAudioFrame._payloadDataLengthInSamples;
        for (WebRtc_UWord16 i = 0;
             i < (incomingAudioFrame._payloadDataLengthInSamples); i++)
        {
            // Duplicate sample to both channels
             tempAudioFrame._payloadData[2*i] =
               incomingAudioFrame._payloadData[i];
             tempAudioFrame._payloadData[2*i+1] =
               incomingAudioFrame._payloadData[i];
        }
    }

    const AudioFrame* ptrAudioFrame = &incomingAudioFrame;
    if(tempAudioFrame._payloadDataLengthInSamples != 0)
    {
        // If ptrAudioFrame is not empty it contains the audio to be recorded.
        ptrAudioFrame = &tempAudioFrame;
    }

    // Encode the audio data before writing to file. Don't encode if the codec
    // is PCM.
    // NOTE: stereo recording is only supported for WAV files.
    // TODO (hellner): WAV expect PCM in little endian byte order. Not
    // "encoding" with PCM coder should be a problem for big endian systems.
    WebRtc_UWord32 encodedLenInBytes = 0;
    if (_fileFormat == kFileFormatPreencodedFile ||
        STR_CASE_CMP(codec_info_.plname, "L16") != 0)
    {
        if (_audioEncoder.Encode(*ptrAudioFrame, _audioBuffer,
                                 encodedLenInBytes) == -1)
        {
            WEBRTC_TRACE(
                kTraceWarning,
                kTraceVoice,
                _instanceID,
                "FileRecorder::RecordAudioToFile() codec %s not supported or\
 failed to encode stream",
                codec_info_.plname);
            return -1;
        }
    } else {
        int outLen = 0;
        if(ptrAudioFrame->_audioChannel == 2)
        {
            // ptrAudioFrame contains interleaved stereo audio.
            _audioResampler.ResetIfNeeded(ptrAudioFrame->_frequencyInHz,
                                          codec_info_.plfreq,
                                          kResamplerSynchronousStereo);
            _audioResampler.Push(ptrAudioFrame->_payloadData,
                                 ptrAudioFrame->_payloadDataLengthInSamples *
                                 ptrAudioFrame->_audioChannel,
                                 (WebRtc_Word16*)_audioBuffer,
                                 MAX_AUDIO_BUFFER_IN_BYTES, outLen);
        } else {
            _audioResampler.ResetIfNeeded(ptrAudioFrame->_frequencyInHz,
                                          codec_info_.plfreq,
                                          kResamplerSynchronous);
            _audioResampler.Push(ptrAudioFrame->_payloadData,
                                 ptrAudioFrame->_payloadDataLengthInSamples,
                                 (WebRtc_Word16*)_audioBuffer,
                                 MAX_AUDIO_BUFFER_IN_BYTES, outLen);
        }
        encodedLenInBytes = outLen * sizeof(WebRtc_Word16);
    }

    // Codec may not be operating at a frame rate of 10 ms. Whenever enough
    // 10 ms chunks of data has been pushed to the encoder an encoded frame
    // will be available. Wait until then.
    if (encodedLenInBytes)
    {
        WebRtc_UWord16 msOfData =
            ptrAudioFrame->_payloadDataLengthInSamples /
            WebRtc_UWord16(ptrAudioFrame->_frequencyInHz / 1000);
        if (WriteEncodedAudioData(_audioBuffer,
                                  (WebRtc_UWord16)encodedLenInBytes,
                                  msOfData, playoutTS) == -1)
        {
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32 FileRecorderImpl::SetUpAudioEncoder()
{
    if (_fileFormat == kFileFormatPreencodedFile ||
        STR_CASE_CMP(codec_info_.plname, "L16") != 0)
    {
        if(_audioEncoder.SetEncodeCodec(codec_info_,_amrFormat) == -1)
        {
            WEBRTC_TRACE(
                kTraceError,
                kTraceVoice,
                _instanceID,
                "FileRecorder::StartRecording() codec %s not supported",
                codec_info_.plname);
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32 FileRecorderImpl::codec_info(CodecInst& codecInst) const
{
    if(codec_info_.plfreq == 0)
    {
        return -1;
    }
    codecInst = codec_info_;
    return 0;
}

WebRtc_Word32 FileRecorderImpl::WriteEncodedAudioData(
    const WebRtc_Word8* audioBuffer,
    WebRtc_UWord16 bufferLength,
    WebRtc_UWord16 /*millisecondsOfData*/,
    const TickTime* /*playoutTS*/)
{
    return _moduleFile->IncomingAudioData(audioBuffer, bufferLength);
}


#ifdef WEBRTC_MODULE_UTILITY_VIDEO
class AudioFrameFileInfo
{
    public:
       AudioFrameFileInfo(const WebRtc_Word8* audioData,
                     const WebRtc_UWord16 audioSize,
                     const WebRtc_UWord16 audioMS,
                     const TickTime& playoutTS)
           : _audioData(), _audioSize(audioSize), _audioMS(audioMS),
             _playoutTS(playoutTS)
       {
           if(audioSize > MAX_AUDIO_BUFFER_IN_BYTES)
           {
               assert(false);
               _audioSize = 0;
               return;
           }
           memcpy(_audioData, audioData, audioSize);
       };
    // TODO (hellner): either turn into a struct or provide get/set functions.
    WebRtc_Word8   _audioData[MAX_AUDIO_BUFFER_IN_BYTES];
    WebRtc_UWord16 _audioSize;
    WebRtc_UWord16 _audioMS;
    TickTime _playoutTS;
};

AviRecorder::AviRecorder(WebRtc_UWord32 instanceID, FileFormats fileFormat)
    : FileRecorderImpl(instanceID, fileFormat),
      _videoOnly(false),
      _thread( 0),
      _timeEvent(*EventWrapper::Create()),
      _critSec(CriticalSectionWrapper::CreateCriticalSection()),
      _writtenVideoFramesCounter(0),
      _writtenAudioMS(0),
      _writtenVideoMS(0)
{
    _videoEncoder = new VideoCoder(instanceID);
    _frameScaler = new FrameScaler();
    _videoFramesQueue = new VideoFramesQueue();
    _thread = ThreadWrapper::CreateThread(Run, this, kNormalPriority,
                                          "AviRecorder()");
}

AviRecorder::~AviRecorder( )
{
    StopRecording( );

    delete _videoEncoder;
    delete _frameScaler;
    delete _videoFramesQueue;
    delete _thread;
    delete &_timeEvent;
    delete _critSec;
}

WebRtc_Word32 AviRecorder::StartRecordingVideoFile(
    const char* fileName,
    const CodecInst& audioCodecInst,
    const VideoCodec& videoCodecInst,
    ACMAMRPackingFormat amrFormat,
    bool videoOnly)
{
    _firstAudioFrameReceived = false;
    _videoCodecInst = videoCodecInst;
    _videoOnly = videoOnly;

    if(_moduleFile->StartRecordingVideoFile(fileName, _fileFormat,
                                            audioCodecInst, videoCodecInst,
                                            videoOnly) != 0)
    {
        return -1;
    }

    if(!videoOnly)
    {
        if(FileRecorderImpl::StartRecordingAudioFile(fileName,audioCodecInst, 0,
                                                     amrFormat) !=0)
        {
            StopRecording();
            return -1;
        }
    }
    if( SetUpVideoEncoder() != 0)
    {
        StopRecording();
        return -1;
    }
    if(_videoOnly)
    {
        // Writing to AVI file is non-blocking.
        // Start non-blocking timer if video only. If recording both video and
        // audio let the pushing of audio frames be the timer.
        _timeEvent.StartTimer(true, 1000 / _videoCodecInst.maxFramerate);
    }
    StartThread();
    return 0;
}

WebRtc_Word32 AviRecorder::StopRecording()
{
    _timeEvent.StopTimer();

    StopThread();
    return FileRecorderImpl::StopRecording();
}

WebRtc_Word32 AviRecorder::CalcI420FrameSize( ) const
{
    return 3 * _videoCodecInst.width * _videoCodecInst.height / 2;
}

WebRtc_Word32 AviRecorder::SetUpVideoEncoder()
{
    // Size of unencoded data (I420) should be the largest possible frame size
    // in a file.
    _videoMaxPayloadSize = CalcI420FrameSize();
    _videoEncodedData.VerifyAndAllocate(_videoMaxPayloadSize);

    _videoCodecInst.plType = _videoEncoder->DefaultPayloadType(
        _videoCodecInst.plName);

    WebRtc_Word32 useNumberOfCores = 1;
    // Set the max payload size to 16000. This means that the codec will try to
    // create slices that will fit in 16000 kByte packets. However, the
    // Encode() call will still generate one full frame.
    if(_videoEncoder->SetEncodeCodec(_videoCodecInst, useNumberOfCores,
                                     16000))
    {
        return -1;
    }
    return 0;
}

WebRtc_Word32 AviRecorder::RecordVideoToFile(const VideoFrame& videoFrame)
{
    CriticalSectionScoped lock(_critSec);

    if(!IsRecording() || ( videoFrame.Length() == 0))
    {
        return -1;
    }
    // The frame is written to file in AviRecorder::Process().
    WebRtc_Word32 retVal = _videoFramesQueue->AddFrame(videoFrame);
    if(retVal != 0)
    {
        StopRecording();
    }
    return retVal;
}

bool AviRecorder::StartThread()
{
    unsigned int id;
    if( _thread == 0)
    {
        return false;
    }

    return _thread->Start(id);
}

bool AviRecorder::StopThread()
{
    _critSec->Enter();

    if(_thread)
    {
        _thread->SetNotAlive();

        ThreadWrapper* thread = _thread;
        _thread = NULL;

        _timeEvent.Set();

        _critSec->Leave();

        if(thread->Stop())
        {
            delete thread;
        } else {
            return false;
        }
    } else {
        _critSec->Leave();
    }
    return true;
}

bool AviRecorder::Run( ThreadObj threadObj)
{
    return static_cast<AviRecorder*>( threadObj)->Process();
}

WebRtc_Word32 AviRecorder::ProcessAudio()
{
    if (_writtenVideoFramesCounter == 0)
    {
        // Get the most recent frame that is due for writing to file. Since
        // frames are unencoded it's safe to throw away frames if necessary
        // for synchronizing audio and video.
        VideoFrame* frameToProcess = _videoFramesQueue->FrameToRecord();
        if(frameToProcess)
        {
            // Syncronize audio to the current frame to process by throwing away
            // audio samples with older timestamp than the video frame.
            WebRtc_UWord32 numberOfAudioElements =
                _audioFramesToWrite.GetSize();
            for (WebRtc_UWord32 i = 0; i < numberOfAudioElements; ++i)
            {
                AudioFrameFileInfo* frameInfo =
                    (AudioFrameFileInfo*)_audioFramesToWrite.First()->GetItem();
                if(frameInfo)
                {
                    if(TickTime::TicksToMilliseconds(
                           frameInfo->_playoutTS.Ticks()) <
                       frameToProcess->RenderTimeMs())
                    {
                        delete frameInfo;
                        _audioFramesToWrite.PopFront();
                    } else
                    {
                        break;
                    }
                }
            }
        }
    }
    // Write all audio up to current timestamp.
    WebRtc_Word32 error = 0;
    WebRtc_UWord32 numberOfAudioElements = _audioFramesToWrite.GetSize();
    for (WebRtc_UWord32 i = 0; i < numberOfAudioElements; ++i)
    {
        AudioFrameFileInfo* frameInfo =
            (AudioFrameFileInfo*)_audioFramesToWrite.First()->GetItem();
        if(frameInfo)
        {
            if((TickTime::Now() - frameInfo->_playoutTS).Milliseconds() > 0)
            {
                _moduleFile->IncomingAudioData(frameInfo->_audioData,
                                               frameInfo->_audioSize);
                _writtenAudioMS += frameInfo->_audioMS;
                delete frameInfo;
                _audioFramesToWrite.PopFront();
            } else {
                break;
            }
        } else {
            _audioFramesToWrite.PopFront();
        }
    }
    return error;
}

bool AviRecorder::Process()
{
    switch(_timeEvent.Wait(500))
    {
    case kEventSignaled:
        if(_thread == NULL)
        {
            return false;
        }
        break;
    case kEventError:
        return false;
    case kEventTimeout:
        // No events triggered. No work to do.
        return true;
    }
    CriticalSectionScoped lock( _critSec);

    // Get the most recent frame to write to file (if any). Synchronize it with
    // the audio stream (if any). Synchronization the video based on its render
    // timestamp (i.e. VideoFrame::RenderTimeMS())
    VideoFrame* frameToProcess = _videoFramesQueue->FrameToRecord();
    if( frameToProcess == NULL)
    {
        return true;
    }
    WebRtc_Word32 error = 0;
    if(!_videoOnly)
    {
        if(!_firstAudioFrameReceived)
        {
            // Video and audio can only be synchronized if both have been
            // received.
            return true;
        }
        error = ProcessAudio();

        while (_writtenAudioMS > _writtenVideoMS)
        {
            error = EncodeAndWriteVideoToFile( *frameToProcess);
            if( error != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceVideo, _instanceID,
                        "AviRecorder::Process() error writing to file.");
                break;
            } else {
                WebRtc_UWord32 frameLengthMS = 1000 /
                    _videoCodecInst.maxFramerate;
                _writtenVideoFramesCounter++;
                _writtenVideoMS += frameLengthMS;
                // A full seconds worth of frames have been written.
                if(_writtenVideoFramesCounter%_videoCodecInst.maxFramerate == 0)
                {
                    // Frame rate is in frames per seconds. Frame length is
                    // calculated as an integer division which means it may
                    // be rounded down. Compensate for this every second.
                    WebRtc_UWord32 rest = 1000 % frameLengthMS;
                    _writtenVideoMS += rest;
                }
            }
        }
    } else {
        // Frame rate is in frames per seconds. Frame length is calculated as an
        // integer division which means it may be rounded down. This introduces
        // drift. Once a full frame worth of drift has happened, skip writing
        // one frame. Note that frame rate is in frames per second so the
        // drift is completely compensated for.
        WebRtc_UWord32 frameLengthMS = 1000/_videoCodecInst.maxFramerate;
        WebRtc_UWord32 restMS = 1000 % frameLengthMS;
        WebRtc_UWord32 frameSkip = (_videoCodecInst.maxFramerate *
                                    frameLengthMS) / restMS;

        _writtenVideoFramesCounter++;
        if(_writtenVideoFramesCounter % frameSkip == 0)
        {
            _writtenVideoMS += frameLengthMS;
            return true;
        }

        error = EncodeAndWriteVideoToFile( *frameToProcess);
        if(error != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, _instanceID,
                    "AviRecorder::Process() error writing to file.");
        } else {
            _writtenVideoMS += frameLengthMS;
        }
    }
    return error == 0;
}

WebRtc_Word32 AviRecorder::EncodeAndWriteVideoToFile(VideoFrame& videoFrame)
{
    if(!IsRecording() || (videoFrame.Length() == 0))
    {
        return -1;
    }

    if(_frameScaler->ResizeFrameIfNeeded(&videoFrame, _videoCodecInst.width,
                                         _videoCodecInst.height) != 0)
    {
        return -1;
    }

    _videoEncodedData.payloadSize = 0;

    if( STR_CASE_CMP(_videoCodecInst.plName, "I420") == 0)
    {
        _videoEncodedData.VerifyAndAllocate(videoFrame.Length());

        // I420 is raw data. No encoding needed (each sample is represented by
        // 1 byte so there is no difference depending on endianness).
        memcpy(_videoEncodedData.payloadData, videoFrame.Buffer(),
               videoFrame.Length());

        _videoEncodedData.payloadSize = videoFrame.Length();
        _videoEncodedData.frameType = kVideoFrameKey;
    }else {
        if( _videoEncoder->Encode(videoFrame, _videoEncodedData) != 0)
        {
            return -1;
        }
    }

    if(_videoEncodedData.payloadSize > 0)
    {
        if(_moduleFile->IncomingAVIVideoData(
               (WebRtc_Word8*)(_videoEncodedData.payloadData),
               _videoEncodedData.payloadSize))
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, _instanceID,
                         "Error writing AVI file");
            return -1;
        }
    } else {
        WEBRTC_TRACE(
            kTraceError,
            kTraceVideo,
            _instanceID,
            "FileRecorder::RecordVideoToFile() frame dropped by encoder bitrate\
 likely to low.");
    }
    return 0;
}

// Store audio frame in the _audioFramesToWrite buffer. The writing to file
// happens in AviRecorder::Process().
WebRtc_Word32 AviRecorder::WriteEncodedAudioData(
    const WebRtc_Word8* audioBuffer,
    WebRtc_UWord16 bufferLength,
    WebRtc_UWord16 millisecondsOfData,
    const TickTime* playoutTS)
{
    if (!IsRecording())
    {
        return -1;
    }
    if (bufferLength > MAX_AUDIO_BUFFER_IN_BYTES)
    {
        return -1;
    }
    if (_videoOnly)
    {
        return -1;
    }
    if (_audioFramesToWrite.GetSize() > kMaxAudioBufferQueueLength)
    {
        StopRecording();
        return -1;
    }
    _firstAudioFrameReceived = true;

    if(playoutTS)
    {
        _audioFramesToWrite.PushBack(new AudioFrameFileInfo(audioBuffer,
                                                            bufferLength,
                                                            millisecondsOfData,
                                                            *playoutTS));
    } else {
        _audioFramesToWrite.PushBack(new AudioFrameFileInfo(audioBuffer,
                                                            bufferLength,
                                                            millisecondsOfData,
                                                            TickTime::Now()));
    }
    _timeEvent.Set();
    return 0;
}

#endif // WEBRTC_MODULE_UTILITY_VIDEO
} // namespace webrtc
