/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CHANNEL_MANAGER_H
#define WEBRTC_VOICE_ENGINE_CHANNEL_MANAGER_H

#include "channel_manager_base.h"
#include "typedefs.h"

namespace webrtc
{

namespace voe
{

class ScopedChannel;
class Channel;

class ChannelManager: private ChannelManagerBase
{
    friend class ScopedChannel;

public:
    bool CreateChannel(WebRtc_Word32& channelId);

    WebRtc_Word32 DestroyChannel(const WebRtc_Word32 channelId);

    WebRtc_Word32 MaxNumOfChannels() const;

    WebRtc_Word32 NumOfChannels() const;

    void GetChannelIds(WebRtc_Word32* channelsArray,
                       WebRtc_Word32& numOfChannels) const;

    ChannelManager(const WebRtc_UWord32 instanceId);

    ~ChannelManager();

private:
    ChannelManager(const ChannelManager&);

    ChannelManager& operator=(const ChannelManager&);

    Channel* GetChannel(const WebRtc_Word32 channelId) const;

    void GetChannels(MapWrapper& channels) const;

    void ReleaseChannel();

    virtual void* NewItem(WebRtc_Word32 itemID);

    virtual void DeleteItem(void* item);

    WebRtc_UWord32 _instanceId;
};

class ScopedChannel
{
public:
    // Can only be created by the channel manager
    ScopedChannel(ChannelManager& chManager);

    ScopedChannel(ChannelManager& chManager, WebRtc_Word32 channelId);

    Channel* ChannelPtr();

    Channel* GetFirstChannel(void*& iterator) const;

    Channel* GetNextChannel(void*& iterator) const;

    ~ScopedChannel();
private:
    ChannelManager& _chManager;
    Channel* _channelPtr;
    MapWrapper _channels;
};

} // namespace voe

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CHANNEL_MANAGER_H
