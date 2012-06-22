/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ChannelPool.h"
#include "map_wrapper.h"
#include <string.h>
#include <assert.h>
#include "critical_section_wrapper.h"

ChannelPool::ChannelPool():
_critSect(*webrtc::CriticalSectionWrapper::CreateCriticalSection())
{
}

ChannelPool::~ChannelPool(void)
{
    assert(_channelMap.Size()==0);    
    delete &_critSect;
}

WebRtc_Word32 ChannelPool::AddChannel(int channel)
{
    return _channelMap.Insert(channel,(void*) channel);
}
WebRtc_Word32 ChannelPool::RemoveChannel(int channel)
{
    return _channelMap.Erase(channel);
}

webrtc::MapWrapper& ChannelPool::ChannelMap()
{
    return _channelMap;
}
