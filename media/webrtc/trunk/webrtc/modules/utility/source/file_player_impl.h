/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_FILE_PLAYER_IMPL_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_FILE_PLAYER_IMPL_H_

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/media_file/media_file.h"
#include "webrtc/modules/media_file/media_file_defines.h"
#include "webrtc/modules/utility/include/file_player.h"
#include "webrtc/modules/utility/source/coder.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class FilePlayerImpl : public FilePlayer
{
public:
    FilePlayerImpl(uint32_t instanceID, FileFormats fileFormat);
    ~FilePlayerImpl();

    virtual int Get10msAudioFromFile(
        int16_t* outBuffer,
        size_t& lengthInSamples,
        int frequencyInHz);
    virtual int32_t RegisterModuleFileCallback(FileCallback* callback);
    virtual int32_t StartPlayingFile(
        const char* fileName,
        bool loop,
        uint32_t startPosition,
        float volumeScaling,
        uint32_t notification,
        uint32_t stopPosition = 0,
        const CodecInst* codecInst = NULL);
    virtual int32_t StartPlayingFile(
        InStream& sourceStream,
        uint32_t startPosition,
        float volumeScaling,
        uint32_t notification,
        uint32_t stopPosition = 0,
        const CodecInst* codecInst = NULL);
    virtual int32_t StopPlayingFile();
    virtual bool IsPlayingFile() const;
    virtual int32_t GetPlayoutPosition(uint32_t& durationMs);
    virtual int32_t AudioCodec(CodecInst& audioCodec) const;
    virtual int32_t Frequency() const;
    virtual int32_t SetAudioScaling(float scaleFactor);

protected:
    int32_t SetUpAudioDecoder();

    uint32_t _instanceID;
    const FileFormats _fileFormat;
    MediaFile& _fileModule;

    uint32_t _decodedLengthInMS;

private:
    AudioCoder _audioDecoder;

    CodecInst _codec;
    int32_t _numberOf10MsPerFrame;
    int32_t _numberOf10MsInDecoder;

    Resampler _resampler;
    float _scaling;
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_SOURCE_FILE_PLAYER_IMPL_H_
