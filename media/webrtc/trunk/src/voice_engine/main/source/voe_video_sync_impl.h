/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_IMPL_H

#include "voe_video_sync.h"

#include "ref_count.h"
#include "shared_data.h"

namespace webrtc {

class VoEVideoSyncImpl : public VoEVideoSync,
                         public voe::RefCount
{
public:
    virtual int Release();

    virtual int GetPlayoutBufferSize(int& bufferMs);

    virtual int SetMinimumPlayoutDelay(int channel, int delayMs);

    virtual int GetDelayEstimate(int channel, int& delayMs);

    virtual int SetInitTimestamp(int channel, unsigned int timestamp);

    virtual int SetInitSequenceNumber(int channel, short sequenceNumber);

    virtual int GetPlayoutTimestamp(int channel, unsigned int& timestamp);

    virtual int GetRtpRtcp(int channel, RtpRtcp* &rtpRtcpModule);

protected:
    VoEVideoSyncImpl(voe::SharedData* shared);
    virtual ~VoEVideoSyncImpl();

private:
    voe::SharedData* _shared;
};

}   // namespace webrtc

#endif    // WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_IMPL_H
