/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once
#include "StdAfx.h"
#include "common_types.h"

#include "vie_base.h"
#include "map_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
}

class ChannelPool
{
public:
    ChannelPool();
    ~ChannelPool(void);
    WebRtc_Word32 AddChannel(int channel);
    WebRtc_Word32 RemoveChannel(int channel);    

    webrtc::MapWrapper& ChannelMap();

    private:     
        webrtc::CriticalSectionWrapper& _critSect;        
        webrtc::MapWrapper _channelMap;

};
