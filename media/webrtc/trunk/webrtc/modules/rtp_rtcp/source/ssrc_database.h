/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_

#include <map>

#include "webrtc/system_wrappers/interface/static_instance.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;

class SSRCDatabase
{
public:
    static SSRCDatabase* GetSSRCDatabase();
    static void ReturnSSRCDatabase();

    uint32_t CreateSSRC();
    int32_t RegisterSSRC(const uint32_t ssrc);
    int32_t ReturnSSRC(const uint32_t ssrc);

protected:
    SSRCDatabase();
    virtual ~SSRCDatabase();

    static SSRCDatabase* CreateInstance() { return new SSRCDatabase(); }

private:
    // Friend function to allow the SSRC destructor to be accessed from the
    // template class.
    friend SSRCDatabase* GetStaticInstance<SSRCDatabase>(
        CountOperation count_operation);
    static SSRCDatabase* StaticInstance(CountOperation count_operation);

    uint32_t GenerateRandom();

    std::map<uint32_t, uint32_t>    _ssrcMap;

    CriticalSectionWrapper* _critSect;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_SSRC_DATABASE_H_
