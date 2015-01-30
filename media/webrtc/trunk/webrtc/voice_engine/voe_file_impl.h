/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_FILE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_FILE_IMPL_H

#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEFileImpl : public VoEFile
{
public:
    // Playout file locally

    virtual int StartPlayingFileLocally(
        int channel,
        const char fileNameUTF8[1024],
        bool loop = false,
        FileFormats format = kFileFormatPcm16kHzFile,
        float volumeScaling = 1.0,
        int startPointMs = 0,
        int stopPointMs = 0);

    virtual int StartPlayingFileLocally(
        int channel,
        InStream* stream,
        FileFormats format = kFileFormatPcm16kHzFile,
        float volumeScaling = 1.0,
        int startPointMs = 0, int stopPointMs = 0);

    virtual int StopPlayingFileLocally(int channel);

    virtual int IsPlayingFileLocally(int channel);

    // Use file as microphone input

    virtual int StartPlayingFileAsMicrophone(
        int channel,
        const char fileNameUTF8[1024],
        bool loop = false ,
        bool mixWithMicrophone = false,
        FileFormats format = kFileFormatPcm16kHzFile,
        float volumeScaling = 1.0);

    virtual int StartPlayingFileAsMicrophone(
        int channel,
        InStream* stream,
        bool mixWithMicrophone = false,
        FileFormats format = kFileFormatPcm16kHzFile,
        float volumeScaling = 1.0);

    virtual int StopPlayingFileAsMicrophone(int channel);

    virtual int IsPlayingFileAsMicrophone(int channel);

    // Record speaker signal to file

    virtual int StartRecordingPlayout(int channel,
                                      const char* fileNameUTF8,
                                      CodecInst* compression = NULL,
                                      int maxSizeBytes = -1);

    virtual int StartRecordingPlayout(int channel,
                                      OutStream* stream,
                                      CodecInst* compression = NULL);

    virtual int StopRecordingPlayout(int channel);

    // Record microphone signal to file

    virtual int StartRecordingMicrophone(const char* fileNameUTF8,
                                         CodecInst* compression = NULL,
                                         int maxSizeBytes = -1);

    virtual int StartRecordingMicrophone(OutStream* stream,
                                         CodecInst* compression = NULL);

    virtual int StopRecordingMicrophone();

protected:
    VoEFileImpl(voe::SharedData* shared);
    virtual ~VoEFileImpl();

private:
    voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_FILE_IMPL_H
