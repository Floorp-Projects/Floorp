/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_FILE_PLAYER_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_FILE_PLAYER_H_

#include "common_types.h"
#include "engine_configurations.h"
#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc {
class FileCallback;

class FilePlayer
{
public:
    // The largest decoded frame size in samples (60ms with 32kHz sample rate).
    enum {MAX_AUDIO_BUFFER_IN_SAMPLES = 60*32};
    enum {MAX_AUDIO_BUFFER_IN_BYTES = MAX_AUDIO_BUFFER_IN_SAMPLES*2};

    // Note: will return NULL for video file formats (e.g. AVI) if the flag
    //       WEBRTC_MODULE_UTILITY_VIDEO is not defined.
    static FilePlayer* CreateFilePlayer(const WebRtc_UWord32 instanceID,
                                        const FileFormats fileFormat);

    static void DestroyFilePlayer(FilePlayer* player);

    virtual WebRtc_Word32 Get10msAudioFromFile(
        WebRtc_Word16* decodedDataBuffer,
        WebRtc_UWord32& decodedDataLengthInSamples,
        const WebRtc_UWord32 frequencyInHz) = 0;

    // Register callback for receiving file playing notifications.
    virtual WebRtc_Word32 RegisterModuleFileCallback(
        FileCallback* callback) = 0;

    // API for playing audio from fileName to channel.
    // Note: codecInst is used for pre-encoded files.
    virtual WebRtc_Word32 StartPlayingFile(
        const char* fileName,
        bool loop,
        WebRtc_UWord32 startPosition,
        float volumeScaling,
        WebRtc_UWord32 notification,
        WebRtc_UWord32 stopPosition = 0,
        const CodecInst* codecInst = NULL) = 0;

    // Note: codecInst is used for pre-encoded files.
    virtual WebRtc_Word32 StartPlayingFile(
        InStream& sourceStream,
        WebRtc_UWord32 startPosition,
        float volumeScaling,
        WebRtc_UWord32 notification,
        WebRtc_UWord32 stopPosition = 0,
        const CodecInst* codecInst = NULL) = 0;

    virtual WebRtc_Word32 StopPlayingFile() = 0;

    virtual bool IsPlayingFile() const = 0;

    virtual WebRtc_Word32 GetPlayoutPosition(WebRtc_UWord32& durationMs) = 0;

    // Set audioCodec to the currently used audio codec.
    virtual WebRtc_Word32 AudioCodec(CodecInst& audioCodec) const = 0;

    virtual WebRtc_Word32 Frequency() const = 0;

    // Note: scaleFactor is in the range [0.0 - 2.0]
    virtual WebRtc_Word32 SetAudioScaling(float scaleFactor) = 0;

    // Return the time in ms until next video frame should be pulled (by
    // calling GetVideoFromFile(..)).
    // Note: this API reads one video frame from file. This means that it should
    //       be called exactly once per GetVideoFromFile(..) API call.
    virtual WebRtc_Word32 TimeUntilNextVideoFrame() { return -1;}

    virtual WebRtc_Word32 StartPlayingVideoFile(
        const char* /*fileName*/,
        bool /*loop*/,
        bool /*videoOnly*/) { return -1;}

    virtual WebRtc_Word32 video_codec_info(VideoCodec& /*videoCodec*/) const
    {return -1;}

    virtual WebRtc_Word32 GetVideoFromFile(VideoFrame& /*videoFrame*/)
    { return -1;}

    // Same as GetVideoFromFile(). videoFrame will have the resolution specified
    // by the width outWidth and height outHeight in pixels.
    virtual WebRtc_Word32 GetVideoFromFile(VideoFrame& /*videoFrame*/,
                                           const WebRtc_UWord32 /*outWidth*/,
                                           const WebRtc_UWord32 /*outHeight*/)
    {return -1;}
protected:
    virtual ~FilePlayer() {}

};
} // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_INTERFACE_FILE_PLAYER_H_
