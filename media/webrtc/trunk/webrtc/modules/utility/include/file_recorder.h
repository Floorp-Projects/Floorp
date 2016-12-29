/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INCLUDE_FILE_RECORDER_H_
#define WEBRTC_MODULES_UTILITY_INCLUDE_FILE_RECORDER_H_

#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/media_file/media_file_defines.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {

class FileRecorder
{
public:

    // Note: will return NULL for unsupported formats.
    static FileRecorder* CreateFileRecorder(const uint32_t instanceID,
                                            const FileFormats fileFormat);

    static void DestroyFileRecorder(FileRecorder* recorder);

    virtual int32_t RegisterModuleFileCallback(
        FileCallback* callback) = 0;

    virtual FileFormats RecordingFileFormat() const = 0;

    virtual int32_t StartRecordingAudioFile(
        const char* fileName,
        const CodecInst& codecInst,
        uint32_t notification) = 0;

    virtual int32_t StartRecordingAudioFile(
        OutStream& destStream,
        const CodecInst& codecInst,
        uint32_t notification) = 0;

    // Stop recording.
    // Note: this API is for both audio and video.
    virtual int32_t StopRecording() = 0;

    // Return true if recording.
    // Note: this API is for both audio and video.
    virtual bool IsRecording() const = 0;

    virtual int32_t codec_info(CodecInst& codecInst) const = 0;

    // Write frame to file. Frame should contain 10ms of un-ecoded audio data.
    virtual int32_t RecordAudioToFile(
        const AudioFrame& frame,
        const TickTime* playoutTS = NULL) = 0;

    // Open/create the file specified by fileName for writing audio/video data
    // (relative path is allowed). audioCodecInst specifies the encoding of the
    // audio data. videoCodecInst specifies the encoding of the video data.
    // Only video data will be recorded if videoOnly is true. amrFormat
    // specifies the amr/amrwb storage format.
    // Note: the file format is AVI.
    virtual int32_t StartRecordingVideoFile(
        const char* fileName,
        const CodecInst& audioCodecInst,
        const VideoCodec& videoCodecInst,
        bool videoOnly = false) = 0;

    // Record the video frame in videoFrame to AVI file.
    virtual int32_t RecordVideoToFile(const VideoFrame& videoFrame) = 0;

protected:
    virtual ~FileRecorder() {}

};
}  // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_INCLUDE_FILE_RECORDER_H_
