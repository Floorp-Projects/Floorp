/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/media_file/interface/media_file.h"
#include "webrtc/modules/utility/source/file_recorder_impl.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
    #include "critical_section_wrapper.h"
    #include "frame_scaler.h"
    #include "video_coder.h"
    #include "video_frames_queue.h"
#endif

namespace webrtc {
FileRecorder* FileRecorder::CreateFileRecorder(uint32_t instanceID,
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

FileRecorderImpl::FileRecorderImpl(uint32_t instanceID,
                                   FileFormats fileFormat)
    : _instanceID(instanceID),
      _fileFormat(fileFormat),
      _moduleFile(MediaFile::CreateMediaFile(_instanceID)),
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

int32_t FileRecorderImpl::RegisterModuleFileCallback(
    FileCallback* callback)
{
    if(_moduleFile == NULL)
    {
        return -1;
    }
    return _moduleFile->SetModuleFileCallback(callback);
}

int32_t FileRecorderImpl::StartRecordingAudioFile(
    const char* fileName,
    const CodecInst& codecInst,
    uint32_t notificationTimeMs,
    ACMAMRPackingFormat amrFormat)
{
    if(_moduleFile == NULL)
    {
        return -1;
    }
    codec_info_ = codecInst;
    _amrFormat = amrFormat;

    int32_t retVal = 0;
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

int32_t FileRecorderImpl::StartRecordingAudioFile(
    OutStream& destStream,
    const CodecInst& codecInst,
    uint32_t notificationTimeMs,
    ACMAMRPackingFormat amrFormat)
{
    codec_info_ = codecInst;
    _amrFormat = amrFormat;

    int32_t retVal = _moduleFile->StartRecordingAudioStream(
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

int32_t FileRecorderImpl::StopRecording()
{
    memset(&codec_info_, 0, sizeof(CodecInst));
    return _moduleFile->StopRecording();
}

bool FileRecorderImpl::IsRecording() const
{
    return _moduleFile->IsRecording();
}

int32_t FileRecorderImpl::RecordAudioToFile(
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
    tempAudioFrame.samples_per_channel_ = 0;
    if( incomingAudioFrame.num_channels_ == 2 &&
        !_moduleFile->IsStereo())
    {
        // Recording mono but incoming audio is (interleaved) stereo.
        tempAudioFrame.num_channels_ = 1;
        tempAudioFrame.sample_rate_hz_ = incomingAudioFrame.sample_rate_hz_;
        tempAudioFrame.samples_per_channel_ =
          incomingAudioFrame.samples_per_channel_;
        for (uint16_t i = 0;
             i < (incomingAudioFrame.samples_per_channel_); i++)
        {
            // Sample value is the average of left and right buffer rounded to
            // closest integer value. Note samples can be either 1 or 2 byte.
             tempAudioFrame.data_[i] =
                 ((incomingAudioFrame.data_[2 * i] +
                   incomingAudioFrame.data_[(2 * i) + 1] + 1) >> 1);
        }
    }
    else if( incomingAudioFrame.num_channels_ == 1 &&
        _moduleFile->IsStereo())
    {
        // Recording stereo but incoming audio is mono.
        tempAudioFrame.num_channels_ = 2;
        tempAudioFrame.sample_rate_hz_ = incomingAudioFrame.sample_rate_hz_;
        tempAudioFrame.samples_per_channel_ =
          incomingAudioFrame.samples_per_channel_;
        for (uint16_t i = 0;
             i < (incomingAudioFrame.samples_per_channel_); i++)
        {
            // Duplicate sample to both channels
             tempAudioFrame.data_[2*i] =
               incomingAudioFrame.data_[i];
             tempAudioFrame.data_[2*i+1] =
               incomingAudioFrame.data_[i];
        }
    }

    const AudioFrame* ptrAudioFrame = &incomingAudioFrame;
    if(tempAudioFrame.samples_per_channel_ != 0)
    {
        // If ptrAudioFrame is not empty it contains the audio to be recorded.
        ptrAudioFrame = &tempAudioFrame;
    }

    // Encode the audio data before writing to file. Don't encode if the codec
    // is PCM.
    // NOTE: stereo recording is only supported for WAV files.
    // TODO (hellner): WAV expect PCM in little endian byte order. Not
    // "encoding" with PCM coder should be a problem for big endian systems.
    uint32_t encodedLenInBytes = 0;
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
        if(ptrAudioFrame->num_channels_ == 2)
        {
            // ptrAudioFrame contains interleaved stereo audio.
            _audioResampler.ResetIfNeeded(ptrAudioFrame->sample_rate_hz_,
                                          codec_info_.plfreq,
                                          kResamplerSynchronousStereo);
            _audioResampler.Push(ptrAudioFrame->data_,
                                 ptrAudioFrame->samples_per_channel_ *
                                 ptrAudioFrame->num_channels_,
                                 (int16_t*)_audioBuffer,
                                 MAX_AUDIO_BUFFER_IN_BYTES, outLen);
        } else {
            _audioResampler.ResetIfNeeded(ptrAudioFrame->sample_rate_hz_,
                                          codec_info_.plfreq,
                                          kResamplerSynchronous);
            _audioResampler.Push(ptrAudioFrame->data_,
                                 ptrAudioFrame->samples_per_channel_,
                                 (int16_t*)_audioBuffer,
                                 MAX_AUDIO_BUFFER_IN_BYTES, outLen);
        }
        encodedLenInBytes = outLen * sizeof(int16_t);
    }

    // Codec may not be operating at a frame rate of 10 ms. Whenever enough
    // 10 ms chunks of data has been pushed to the encoder an encoded frame
    // will be available. Wait until then.
    if (encodedLenInBytes)
    {
        uint16_t msOfData =
            ptrAudioFrame->samples_per_channel_ /
            uint16_t(ptrAudioFrame->sample_rate_hz_ / 1000);
        if (WriteEncodedAudioData(_audioBuffer,
                                  (uint16_t)encodedLenInBytes,
                                  msOfData, playoutTS) == -1)
        {
            return -1;
        }
    }
    return 0;
}

int32_t FileRecorderImpl::SetUpAudioEncoder()
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

int32_t FileRecorderImpl::codec_info(CodecInst& codecInst) const
{
    if(codec_info_.plfreq == 0)
    {
        return -1;
    }
    codecInst = codec_info_;
    return 0;
}

int32_t FileRecorderImpl::WriteEncodedAudioData(
    const int8_t* audioBuffer,
    uint16_t bufferLength,
    uint16_t /*millisecondsOfData*/,
    const TickTime* /*playoutTS*/)
{
    return _moduleFile->IncomingAudioData(audioBuffer, bufferLength);
}


#ifdef WEBRTC_MODULE_UTILITY_VIDEO
class AudioFrameFileInfo
{
    public:
       AudioFrameFileInfo(const int8_t* audioData,
                     const uint16_t audioSize,
                     const uint16_t audioMS,
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
    int8_t   _audioData[MAX_AUDIO_BUFFER_IN_BYTES];
    uint16_t _audioSize;
    uint16_t _audioMS;
    TickTime _playoutTS;
};

AviRecorder::AviRecorder(uint32_t instanceID, FileFormats fileFormat)
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

int32_t AviRecorder::StartRecordingVideoFile(
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

int32_t AviRecorder::StopRecording()
{
    _timeEvent.StopTimer();

    StopThread();
    return FileRecorderImpl::StopRecording();
}

int32_t AviRecorder::CalcI420FrameSize( ) const
{
    return 3 * _videoCodecInst.width * _videoCodecInst.height / 2;
}

int32_t AviRecorder::SetUpVideoEncoder()
{
    // Size of unencoded data (I420) should be the largest possible frame size
    // in a file.
    _videoMaxPayloadSize = CalcI420FrameSize();
    _videoEncodedData.VerifyAndAllocate(_videoMaxPayloadSize);

    _videoCodecInst.plType = _videoEncoder->DefaultPayloadType(
        _videoCodecInst.plName);

    int32_t useNumberOfCores = 1;
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

int32_t AviRecorder::RecordVideoToFile(const I420VideoFrame& videoFrame)
{
    CriticalSectionScoped lock(_critSec);
    if(!IsRecording() || videoFrame.IsZeroSize())
    {
        return -1;
    }
    // The frame is written to file in AviRecorder::Process().
    int32_t retVal = _videoFramesQueue->AddFrame(videoFrame);
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

int32_t AviRecorder::ProcessAudio()
{
    if (_writtenVideoFramesCounter == 0)
    {
        // Get the most recent frame that is due for writing to file. Since
        // frames are unencoded it's safe to throw away frames if necessary
        // for synchronizing audio and video.
        I420VideoFrame* frameToProcess = _videoFramesQueue->FrameToRecord();
        if(frameToProcess)
        {
            // Syncronize audio to the current frame to process by throwing away
            // audio samples with older timestamp than the video frame.
            uint32_t numberOfAudioElements =
                _audioFramesToWrite.GetSize();
            for (uint32_t i = 0; i < numberOfAudioElements; ++i)
            {
                AudioFrameFileInfo* frameInfo =
                    (AudioFrameFileInfo*)_audioFramesToWrite.First()->GetItem();
                if(frameInfo)
                {
                    if(TickTime::TicksToMilliseconds(
                           frameInfo->_playoutTS.Ticks()) <
                       frameToProcess->render_time_ms())
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
    int32_t error = 0;
    uint32_t numberOfAudioElements = _audioFramesToWrite.GetSize();
    for (uint32_t i = 0; i < numberOfAudioElements; ++i)
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
    I420VideoFrame* frameToProcess = _videoFramesQueue->FrameToRecord();
    if( frameToProcess == NULL)
    {
        return true;
    }
    int32_t error = 0;
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
                uint32_t frameLengthMS = 1000 /
                    _videoCodecInst.maxFramerate;
                _writtenVideoFramesCounter++;
                _writtenVideoMS += frameLengthMS;
                // A full seconds worth of frames have been written.
                if(_writtenVideoFramesCounter%_videoCodecInst.maxFramerate == 0)
                {
                    // Frame rate is in frames per seconds. Frame length is
                    // calculated as an integer division which means it may
                    // be rounded down. Compensate for this every second.
                    uint32_t rest = 1000 % frameLengthMS;
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
        uint32_t frameLengthMS = 1000/_videoCodecInst.maxFramerate;
        uint32_t restMS = 1000 % frameLengthMS;
        uint32_t frameSkip = (_videoCodecInst.maxFramerate *
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

int32_t AviRecorder::EncodeAndWriteVideoToFile(I420VideoFrame& videoFrame)
{
    if (!IsRecording() || videoFrame.IsZeroSize())
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
       int length  = CalcBufferSize(kI420, videoFrame.width(),
                                    videoFrame.height());
        _videoEncodedData.VerifyAndAllocate(length);

        // I420 is raw data. No encoding needed (each sample is represented by
        // 1 byte so there is no difference depending on endianness).
        int ret_length = ExtractBuffer(videoFrame, length,
                                       _videoEncodedData.payloadData);
        if (ret_length < 0)
          return -1;

        _videoEncodedData.payloadSize = ret_length;
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
               (int8_t*)(_videoEncodedData.payloadData),
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
int32_t AviRecorder::WriteEncodedAudioData(
    const int8_t* audioBuffer,
    uint16_t bufferLength,
    uint16_t millisecondsOfData,
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
}  // namespace webrtc
