/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_NETEQ_STATS_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_NETEQ_STATS_IMPL_H

#include "voe_neteq_stats.h"

#include "ref_count.h"
#include "shared_data.h"

namespace webrtc {

class VoENetEqStatsImpl : public VoENetEqStats,
                          public voe::RefCount
{
public:
    virtual int Release();

    virtual int GetNetworkStatistics(int channel,
                                     NetworkStatistics& stats);

protected:
    VoENetEqStatsImpl(voe::SharedData* shared);
    virtual ~VoENetEqStatsImpl();

private:
    voe::SharedData* _shared;
};

}  // namespace webrtc

#endif    // WEBRTC_VOICE_ENGINE_VOE_NETEQ_STATS_IMPL_H
