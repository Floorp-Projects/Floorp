/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_MEDIA_FILE_INTERFACE_MEDIA_FILE_H_
#define WEBRTC_MODULES_MEDIA_FILE_INTERFACE_MEDIA_FILE_H_

#include "common_types.h"
#include "typedefs.h"
#include "module.h"
#include "module_common_types.h"
#include "media_file_defines.h"

namespace webrtc {
class MediaFile : public Module
{
public:
    // Factory method. Constructor disabled. id is the identifier for the
    // MediaFile instance.
    static MediaFile* CreateMediaFile(const WebRtc_Word32 id);
    static void DestroyMediaFile(MediaFile* module);

    // Set the MediaFile instance identifier.
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) = 0;

    // Put 10-60ms of audio data from file into the audioBuffer depending on
    // codec frame size. dataLengthInBytes is both an input and output
    // parameter. As input parameter it indicates the size of audioBuffer.
    // As output parameter it indicates the number of bytes written to
    // audioBuffer.
    // Note: This API only play mono audio but can be used on file containing
    // audio with more channels (in which case the audio will be converted to
    // mono).
    virtual WebRtc_Word32 PlayoutAudioData(
        WebRtc_Word8* audioBuffer,
        WebRtc_UWord32& dataLengthInBytes) = 0;

    // Put one video frame into videoBuffer. dataLengthInBytes is both an input
    // and output parameter. As input parameter it indicates the size of
    // videoBuffer. As output parameter it indicates the number of bytes written
    // to videoBuffer.
    virtual WebRtc_Word32 PlayoutAVIVideoData(
        WebRtc_Word8* videoBuffer,
        WebRtc_UWord32& dataLengthInBytes) = 0;

    // Put 10-60ms, depending on codec frame size, of audio data from file into
    // audioBufferLeft and audioBufferRight. The buffers contain the left and
    // right channel of played out stereo audio.
    // dataLengthInBytes is both an input and output parameter. As input
    // parameter it indicates the size of both audioBufferLeft and
    // audioBufferRight. As output parameter it indicates the number of bytes
    // written to both audio buffers.
    // Note: This API can only be successfully called for WAV files with stereo
    // audio.
    virtual WebRtc_Word32 PlayoutStereoData(
        WebRtc_Word8* audioBufferLeft,
        WebRtc_Word8* audioBufferRight,
        WebRtc_UWord32& dataLengthInBytes) = 0;

    // Open the file specified by fileName (relative path is allowed) for
    // reading. FileCallback::PlayNotification(..) will be called after
    // notificationTimeMs of the file has been played if notificationTimeMs is
    // greater than zero. If loop is true the file will be played until
    // StopPlaying() is called. When end of file is reached the file is read
    // from the start. format specifies the type of file fileName refers to.
    // codecInst specifies the encoding of the audio data. Note that
    // file formats that contain this information (like WAV files) don't need to
    // provide a non-NULL codecInst. startPointMs and stopPointMs, unless zero,
    // specify what part of the file should be read. From startPointMs ms to
    // stopPointMs ms.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo audio is only supported for WAV files.
    virtual WebRtc_Word32 StartPlayingAudioFile(
        const char* fileName,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const bool loop                         = false,
        const FileFormats format                = kFileFormatPcm16kHzFile,
        const CodecInst* codecInst              = NULL,
        const WebRtc_UWord32 startPointMs       = 0,
        const WebRtc_UWord32 stopPointMs        = 0) = 0;

    // Open the file specified by fileName for reading (relative path is
    // allowed). If loop is true the file will be played until StopPlaying() is
    // called. When end of file is reached the file is read from the start.
    // format specifies the type of file fileName refers to. Only video will be
    // read if videoOnly is true.
    virtual WebRtc_Word32 StartPlayingVideoFile(const char* fileName,
                                                const bool loop,
                                                bool videoOnly,
                                                const FileFormats format) = 0;

    // Prepare for playing audio from stream.
    // FileCallback::PlayNotification(..) will be called after
    // notificationTimeMs of the file has been played if notificationTimeMs is
    // greater than zero. format specifies the type of file fileName refers to.
    // codecInst specifies the encoding of the audio data. Note that
    // file formats that contain this information (like WAV files) don't need to
    // provide a non-NULL codecInst. startPointMs and stopPointMs, unless zero,
    // specify what part of the file should be read. From startPointMs ms to
    // stopPointMs ms.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo audio is only supported for WAV files.
    virtual WebRtc_Word32 StartPlayingAudioStream(
        InStream& stream,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const FileFormats    format             = kFileFormatPcm16kHzFile,
        const CodecInst*     codecInst          = NULL,
        const WebRtc_UWord32 startPointMs       = 0,
        const WebRtc_UWord32 stopPointMs        = 0) = 0;

    // Stop playing from file or stream.
    virtual WebRtc_Word32 StopPlaying() = 0;

    // Return true if playing.
    virtual bool IsPlaying() = 0;


    // Set durationMs to the number of ms that has been played from file.
    virtual WebRtc_Word32 PlayoutPositionMs(
        WebRtc_UWord32& durationMs) const = 0;

    // Write one audio frame, i.e. the bufferLength first bytes of audioBuffer,
    // to file. The audio frame size is determined by the codecInst.pacsize
    // parameter of the last sucessfull StartRecordingAudioFile(..) call.
    // Note: bufferLength must be exactly one frame.
    virtual WebRtc_Word32 IncomingAudioData(
        const WebRtc_Word8*  audioBuffer,
        const WebRtc_UWord32 bufferLength) = 0;

    // Write one video frame, i.e. the bufferLength first bytes of videoBuffer,
    // to file.
    // Note: videoBuffer can contain encoded data. The codec used must be the
    // same as what was specified by videoCodecInst for the last successfull
    // StartRecordingVideoFile(..) call. The videoBuffer must contain exactly
    // one video frame.
    virtual WebRtc_Word32 IncomingAVIVideoData(
        const WebRtc_Word8*  videoBuffer,
        const WebRtc_UWord32 bufferLength) = 0;

    // Open/creates file specified by fileName for writing (relative path is
    // allowed). FileCallback::RecordNotification(..) will be called after
    // notificationTimeMs of audio data has been recorded if
    // notificationTimeMs is greater than zero.
    // format specifies the type of file that should be created/opened.
    // codecInst specifies the encoding of the audio data. maxSizeBytes
    // specifies the number of bytes allowed to be written to file if it is
    // greater than zero.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    virtual WebRtc_Word32 StartRecordingAudioFile(
        const char*  fileName,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const WebRtc_UWord32 notificationTimeMs = 0,
        const WebRtc_UWord32 maxSizeBytes       = 0) = 0;

    // Open/create the file specified by fileName for writing audio/video data
    // (relative path is allowed). format specifies the type of file fileName
    // should be. codecInst specifies the encoding of the audio data.
    // videoCodecInst specifies the encoding of the video data. Only video data
    // will be recorded if videoOnly is true.
    virtual WebRtc_Word32 StartRecordingVideoFile(
        const char* fileName,
        const FileFormats   format,
        const CodecInst&    codecInst,
        const VideoCodec&   videoCodecInst,
        bool videoOnly = false) = 0;

    // Prepare for recording audio to stream.
    // FileCallback::RecordNotification(..) will be called after
    // notificationTimeMs of audio data has been recorded if
    // notificationTimeMs is greater than zero.
    // format specifies the type of file that stream should correspond to.
    // codecInst specifies the encoding of the audio data.
    // Note: codecInst.channels should be set to 2 for stereo (and 1 for
    // mono). Stereo is only supported for WAV files.
    virtual WebRtc_Word32 StartRecordingAudioStream(
        OutStream&           stream,
        const FileFormats    format,
        const CodecInst&     codecInst,
        const WebRtc_UWord32 notificationTimeMs = 0) = 0;

    // Stop recording to file or stream.
    virtual WebRtc_Word32 StopRecording() = 0;

    // Return true if recording.
    virtual bool IsRecording() = 0;

    // Set durationMs to the number of ms that has been recorded to file.
    virtual WebRtc_Word32 RecordDurationMs(WebRtc_UWord32& durationMs) = 0;

    // Return true if recording or playing is stereo.
    virtual bool IsStereo() = 0;

    // Register callback to receive media file related notifications. Disables
    // callbacks if callback is NULL.
    virtual WebRtc_Word32 SetModuleFileCallback(FileCallback* callback) = 0;

    // Set durationMs to the size of the file (in ms) specified by fileName.
    // format specifies the type of file fileName refers to. freqInHz specifies
    // the sampling frequency of the file.
    virtual WebRtc_Word32 FileDurationMs(
        const char*  fileName,
        WebRtc_UWord32&      durationMs,
        const FileFormats    format,
        const WebRtc_UWord32 freqInHz = 16000) = 0;

    // Update codecInst according to the current audio codec being used for
    // reading or writing.
    virtual WebRtc_Word32 codec_info(CodecInst& codecInst) const = 0;

    // Update videoCodecInst according to the current video codec being used for
    // reading or writing.
    virtual WebRtc_Word32 VideoCodecInst(VideoCodec& videoCodecInst) const = 0;

protected:
    MediaFile() {}
    virtual ~MediaFile() {}
};
} // namespace webrtc
#endif // WEBRTC_MODULES_MEDIA_FILE_INTERFACE_MEDIA_FILE_H_
