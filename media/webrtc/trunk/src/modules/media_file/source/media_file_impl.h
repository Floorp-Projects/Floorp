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
    MediaFileImpl(const WebRtc_Word32 id);
    ~MediaFileImpl();

    WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);
    WebRtc_Word32 Process();
    WebRtc_Word32 TimeUntilNextProcess();

    // MediaFile functions
    WebRtc_Word32 PlayoutAudioData(WebRtc_Word8*   audioBuffer,
                                   WebRtc_UWord32& dataLengthInBytes);
    WebRtc_Word32 PlayoutAVIVideoData(WebRtc_Word8* videoBuffer,
                                      WebRtc_UWord32& dataLengthInBytes);
    WebRtc_Word32 PlayoutStereoData(WebRtc_Word8* audioBufferLeft,
                                    WebRtc_Word8* audioBufferRight,
                                    WebRtc_UWord32& dataLengthInBytes);
    virtual WebRtc_Word32 StartPlayingAudioFile(
        const char*  fileName,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const bool           loop = false,
        const FileFormats    format = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst = NULL,
        const WebRtc_UWord32 startPointMs = 0,
        const WebRtc_UWord32 stopPointMs = 0);
    WebRtc_Word32 StartPlayingVideoFile(const char* fileName,
                                        const bool          loop,
                                        bool                videoOnly,
                                        const FileFormats   format);
    WebRtc_Word32 StartPlayingAudioStream(
        InStream&            stream,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const FileFormats    format = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst = NULL,
        const WebRtc_UWord32 startPointMs = 0,
        const WebRtc_UWord32 stopPointMs = 0);
    WebRtc_Word32 StopPlaying();
    bool IsPlaying();
    WebRtc_Word32 PlayoutPositionMs(WebRtc_UWord32& positionMs) const;
    WebRtc_Word32 IncomingAudioData(const WebRtc_Word8*  audioBuffer,
                                    const WebRtc_UWord32 bufferLength);
    WebRtc_Word32 IncomingAVIVideoData(const WebRtc_Word8*  audioBuffer,
                                       const WebRtc_UWord32 bufferLength);
    WebRtc_Word32 StartRecordingAudioFile(
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const WebRtc_UWord32 maxSizeBytes = 0);
    WebRtc_Word32 StartRecordingVideoFile(
        const char* fileName,
        const FileFormats   format,
        const CodecInst&    codecInst,
        const VideoCodec&   videoCodecInst,
        bool                videoOnly = false);
    WebRtc_Word32 StartRecordingAudioStream(
        OutStream&           stream,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const WebRtc_UWord32 notificationTimeMs = 0);
    WebRtc_Word32 StopRecording();
    bool IsRecording();
    WebRtc_Word32 RecordDurationMs(WebRtc_UWord32& durationMs);
    bool IsStereo();
    WebRtc_Word32 SetModuleFileCallback(FileCallback* callback);
    WebRtc_Word32 FileDurationMs(
        const char*  fileName,
        WebRtc_UWord32&      durationMs,
        const FileFormats    format,
        const WebRtc_UWord32 freqInHz = 16000);
    WebRtc_Word32 codec_info(CodecInst& codecInst) const;
    WebRtc_Word32 VideoCodecInst(VideoCodec& codecInst) const;

private:
    // Returns true if the combination of format and codecInst is valid.
    static bool ValidFileFormat(const FileFormats format,
                                const CodecInst*  codecInst);


    // Returns true if the filename is valid
    static bool ValidFileName(const char* fileName);

  // Returns true if the combination of startPointMs and stopPointMs is valid.
    static bool ValidFilePositions(const WebRtc_UWord32 startPointMs,
                                   const WebRtc_UWord32 stopPointMs);

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
    WebRtc_Word32 StartPlayingFile(
        const char*  fileName,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const bool           loop               = false,
        bool                 videoOnly          = false,
        const FileFormats    format             = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst          = NULL,
        const WebRtc_UWord32 startPointMs       = 0,
        const WebRtc_UWord32 stopPointMs        = 0);

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
    WebRtc_Word32 StartPlayingStream(
        InStream&            stream,
        const char*          fileName,
        bool                 loop,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const FileFormats    format             = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst          = NULL,
        const WebRtc_UWord32 startPointMs       = 0,
        const WebRtc_UWord32 stopPointMs        = 0,
        bool                 videoOnly          = true);

    // Writes one frame into dataBuffer. dataLengthInBytes is both an input and
    // output parameter. As input parameter it indicates the size of
    // audioBuffer. As output parameter it indicates the number of bytes
    // written to audioBuffer. If video is true the data written is a video
    // frame otherwise it is an audio frame.
    WebRtc_Word32 PlayoutData(WebRtc_Word8* dataBuffer,
                              WebRtc_UWord32& dataLengthInBytes, bool video);

    // Write one frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The frame is an audio frame if video is true otherwise it is an
    // audio frame.
    WebRtc_Word32 IncomingAudioVideoData(const WebRtc_Word8*  buffer,
                                         const WebRtc_UWord32 bufferLength,
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
    WebRtc_Word32 StartRecordingFile(
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const VideoCodec&    videoCodecInst,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const WebRtc_UWord32 maxSizeBytes = 0,
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
    WebRtc_Word32 StartRecordingStream(
        OutStream&           stream,
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const VideoCodec&    videoCodecInst,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const bool           videoOnly = false);

    // Returns true if frequencyInHz is a supported frequency.
    static bool ValidFrequency(const WebRtc_UWord32 frequencyInHz);

    void HandlePlayCallbacks(WebRtc_Word32 bytesRead);

    WebRtc_Word32 _id;
    CriticalSectionWrapper* _crit;
    CriticalSectionWrapper* _callbackCrit;

    ModuleFileUtility* _ptrFileUtilityObj;
    CodecInst codec_info_;

    InStream*  _ptrInStream;
    OutStream* _ptrOutStream;

    FileFormats _fileFormat;
    WebRtc_UWord32 _recordDurationMs;
    WebRtc_UWord32 _playoutPositionMs;
    WebRtc_UWord32 _notificationMs;

    bool _playingActive;
    bool _recordingActive;
    bool _isStereo;
    bool _openFile;

    char _fileName[512];

    FileCallback* _ptrCallback;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_MEDIA_FILE_SOURCE_MEDIA_FILE_IMPL_H_
