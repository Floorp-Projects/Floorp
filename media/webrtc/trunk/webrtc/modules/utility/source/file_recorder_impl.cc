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
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
FileRecorder* FileRecorder::CreateFileRecorder(uint32_t instanceID,
                                               FileFormats fileFormat)
{
    return new FileRecorderImpl(instanceID, fileFormat);
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
    retVal =_moduleFile->StartRecordingAudioFile(fileName, _fileFormat,
                                                 codecInst,
                                                 notificationTimeMs);

    if( retVal == 0)
    {
        retVal = SetUpAudioEncoder();
    }
    if( retVal != 0)
    {
        LOG(LS_WARNING) << "Failed to initialize file " << fileName
                        << " for recording.";

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
        LOG(LS_WARNING) << "Failed to initialize outStream for recording.";

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
        LOG(LS_WARNING) << "RecordAudioToFile() recording audio is not "
                        << "turned on.";
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
    size_t encodedLenInBytes = 0;
    if (_fileFormat == kFileFormatPreencodedFile ||
        STR_CASE_CMP(codec_info_.plname, "L16") != 0)
    {
        if (_audioEncoder.Encode(*ptrAudioFrame, _audioBuffer,
                                 encodedLenInBytes) == -1)
        {
            LOG(LS_WARNING) << "RecordAudioToFile() codec "
                            << codec_info_.plname
                            << " not supported or failed to encode stream.";
            return -1;
        }
    } else {
        int outLen = 0;
        _audioResampler.ResetIfNeeded(ptrAudioFrame->sample_rate_hz_,
                                      codec_info_.plfreq,
                                      ptrAudioFrame->num_channels_);
        _audioResampler.Push(ptrAudioFrame->data_,
                             ptrAudioFrame->samples_per_channel_ *
                             ptrAudioFrame->num_channels_,
                             (int16_t*)_audioBuffer,
                             MAX_AUDIO_BUFFER_IN_BYTES, outLen);
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
        if (WriteEncodedAudioData(_audioBuffer, encodedLenInBytes, msOfData,
                                  playoutTS) == -1)
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
            LOG(LS_ERROR) << "SetUpAudioEncoder() codec "
                          << codec_info_.plname << " not supported.";
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
    size_t bufferLength,
    uint16_t /*millisecondsOfData*/,
    const TickTime* /*playoutTS*/)
{
    return _moduleFile->IncomingAudioData(audioBuffer, bufferLength);
}
}  // namespace webrtc
