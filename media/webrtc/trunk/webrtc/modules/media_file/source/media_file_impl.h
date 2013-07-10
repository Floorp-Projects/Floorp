/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_IMPL_H_
#define WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_IMPL_H_

#include "common_types.h"
#include "media_file.h"
#include "media_file_defines.h"
#include "media_file_utility.h"
#include "module_common_types.h"

namespace webrtc {
class MediaFileImpl : public MediaFile
{

public:
    MediaFileImpl(const int32_t id);
    ~MediaFileImpl();

    int32_t ChangeUniqueId(const int32_t id);
    int32_t Process();
    int32_t TimeUntilNextProcess();

    // MediaFile functions
    int32_t PlayoutAudioData(int8_t* audioBuffer, uint32_t& dataLengthInBytes);
    int32_t PlayoutAVIVideoData(int8_t* videoBuffer,
                                uint32_t& dataLengthInBytes);
    int32_t PlayoutStereoData(int8_t* audioBufferLeft, int8_t* audioBufferRight,
                              uint32_t& dataLengthInBytes);
    virtual int32_t StartPlayingAudioFile(
        const char*  fileName,
        const uint32_t notificationTimeMs = 0,
        const bool           loop = false,
        const FileFormats    format = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst = NULL,
        const uint32_t startPointMs = 0,
        const uint32_t stopPointMs = 0);
    int32_t StartPlayingVideoFile(const char* fileName, const bool loop,
                                  bool videoOnly, const FileFormats format);
    int32_t StartPlayingAudioStream(InStream& stream,
        const uint32_t notificationTimeMs = 0,
        const FileFormats format = kFileFormatPcm16kHzFile,
        const CodecInst* codecInst = NULL,
        const uint32_t startPointMs = 0,
        const uint32_t stopPointMs = 0);
    int32_t StopPlaying();
    bool IsPlaying();
    int32_t PlayoutPositionMs(uint32_t& positionMs) const;
    int32_t IncomingAudioData(const int8_t*  audioBuffer,
                              const uint32_t bufferLength);
    int32_t IncomingAVIVideoData(const int8_t*  audioBuffer,
                                 const uint32_t bufferLength);
    int32_t StartRecordingAudioFile(
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const uint32_t notificationTimeMs = 0,
        const uint32_t maxSizeBytes = 0);
    int32_t StartRecordingVideoFile(
        const char* fileName,
        const FileFormats   format,
        const CodecInst&    codecInst,
        const VideoCodec&   videoCodecInst,
        bool                videoOnly = false);
    int32_t StartRecordingAudioStream(
        OutStream&           stream,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const uint32_t notificationTimeMs = 0);
    int32_t StopRecording();
    bool IsRecording();
    int32_t RecordDurationMs(uint32_t& durationMs);
    bool IsStereo();
    int32_t SetModuleFileCallback(FileCallback* callback);
    int32_t FileDurationMs(
        const char*  fileName,
        uint32_t&      durationMs,
        const FileFormats    format,
        const uint32_t freqInHz = 16000);
    int32_t codec_info(CodecInst& codecInst) const;
    int32_t VideoCodecInst(VideoCodec& codecInst) const;

private:
    // Returns true if the combination of format and codecInst is valid.
    static bool ValidFileFormat(const FileFormats format,
                                const CodecInst*  codecInst);


    // Returns true if the filename is valid
    static bool ValidFileName(const char* fileName);

  // Returns true if the combination of startPointMs and stopPointMs is valid.
    static bool ValidFilePositions(const uint32_t startPointMs,
                                   const uint32_t stopPointMs);

    // Open the file specified by fileName for reading (relative path is
    // allowed). FileCallback::PlayNotification(..) will be called after
    // notificationTimeMs of the file has been played if notificationTimeMs is
    // greater than zero. If loop is true the file will be played until
    // StopPlaying() is called. When end of file is reached the file is read
    // from the start. format specifies the type of file fileName refers to.
    // codecInst specifies the encoding of the audio data. Note that
    // file formats that contain this information (like WAV files) don't need to
    // provide a non-NULL codecInst. Only video will be read if videoOnly is
    // true. startPointMs and stopPointMs, unless zero,
    // specify what part of the file should be read. From startPointMs ms to
    // stopPointMs ms.
    int32_t StartPlayingFile(
        const char*  fileName,
        const uint32_t notificationTimeMs = 0,
        const bool           loop               = false,
        bool                 videoOnly          = false,
        const FileFormats    format             = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst          = NULL,
        const uint32_t startPointMs       = 0,
        const uint32_t stopPointMs        = 0);

    // Opens the file specified by fileName for reading (relative path is
    // allowed) if format is kFileFormatAviFile otherwise use stream for
    // reading. FileCallback::PlayNotification(..) will be called after
    // notificationTimeMs of the file has been played if notificationTimeMs is
    // greater than zero. If loop is true the file will be played until
    // StopPlaying() is called. When end of file is reached the file is read
    // from the start. format specifies the type of file fileName refers to.
    // codecInst specifies the encoding of the audio data. Note that
    // file formats that contain this information (like WAV files) don't need to
    // provide a non-NULL codecInst. Only video will be read if videoOnly is
    // true. startPointMs and stopPointMs, unless zero,
    // specify what part of the file should be read. From startPointMs ms to
    // stopPointMs ms.
    // TODO (hellner): there is no reason why fileName should be needed here.
    int32_t StartPlayingStream(
        InStream&            stream,
        const char*          fileName,
        bool                 loop,
        const uint32_t notificationTimeMs = 0,
        const FileFormats    format             = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst          = NULL,
        const uint32_t startPointMs       = 0,
        const uint32_t stopPointMs        = 0,
        bool                 videoOnly          = true);

    // Writes one frame into dataBuffer. dataLengthInBytes is both an input and
    // output parameter. As input parameter it indicates the size of
    // audioBuffer. As output parameter it indicates the number of bytes
    // written to audioBuffer. If video is true the data written is a video
    // frame otherwise it is an audio frame.
    int32_t PlayoutData(int8_t* dataBuffer, uint32_t& dataLengthInBytes,
                        bool video);

    // Write one frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The frame is an audio frame if video is true otherwise it is an
    // audio frame.
    int32_t IncomingAudioVideoData(const int8_t*  buffer,
                                   const uint32_t bufferLength,
                                   const bool video);

    // Open/creates file specified by fileName for writing (relative path is
    // allowed) if format is kFileFormatAviFile otherwise use stream for
    // writing. FileCallback::RecordNotification(..) will be called after
    // notificationTimeMs of audio data has been recorded if
    // notificationTimeMs is greater than zero.
    // format specifies the type of file that should be created/opened.
    // codecInst specifies the encoding of the audio data. videoCodecInst
    // specifies the encoding of the video data. maxSizeBytes specifies the
    // number of bytes allowed to be written to file if it is greater than zero.
    // If format is kFileFormatAviFile and videoOnly is true the AVI file will
    // only contain video frames.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    int32_t StartRecordingFile(
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const VideoCodec&    videoCodecInst,
        const uint32_t notificationTimeMs = 0,
        const uint32_t maxSizeBytes = 0,
        bool                 videoOnly = false);

    // Open/creates file specified by fileName for writing (relative path is
    // allowed). FileCallback::RecordNotification(..) will be called after
    // notificationTimeMs of audio data has been recorded if
    // notificationTimeMs is greater than zero.
    // format specifies the type of file that should be created/opened.
    // codecInst specifies the encoding of the audio data. videoCodecInst
    // specifies the encoding of the video data. maxSizeBytes specifies the
    // number of bytes allowed to be written to file if it is greater than zero.
    // If format is kFileFormatAviFile and videoOnly is true the AVI file will
    // only contain video frames.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    // TODO (hellner): there is no reason why fileName should be needed here.
    int32_t StartRecordingStream(
        OutStream&           stream,
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const VideoCodec&    videoCodecInst,
        const uint32_t notificationTimeMs = 0,
        const bool           videoOnly = false);

    // Returns true if frequencyInHz is a supported frequency.
    static bool ValidFrequency(const uint32_t frequencyInHz);

    void HandlePlayCallbacks(int32_t bytesRead);

    int32_t _id;
    CriticalSectionWrapper* _crit;
    CriticalSectionWrapper* _callbackCrit;

    ModuleFileUtility* _ptrFileUtilityObj;
    CodecInst codec_info_;

    InStream*  _ptrInStream;
    OutStream* _ptrOutStream;

    FileFormats _fileFormat;
    uint32_t _recordDurationMs;
    uint32_t _playoutPositionMs;
    uint32_t _notificationMs;

    bool _playingActive;
    bool _recordingActive;
    bool _isStereo;
    bool _openFile;

    char _fileName[512];

    FileCallback* _ptrCallback;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_IMPL_H_
