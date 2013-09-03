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

#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/channel_manager_base.h"

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
    bool CreateChannel(int32_t& channelId);

    int32_t DestroyChannel(int32_t channelId);

    int32_t MaxNumOfChannels() const;

    int32_t NumOfChannels() const;

    void GetChannelIds(int32_t* channelsArray,
                       int32_t& numOfChannels) const;

    ChannelManager(uint32_t instanceId);

    ~ChannelManager();

private:
    ChannelManager(const ChannelManager&);

    ChannelManager& operator=(const ChannelManager&);

    Channel* GetChannel(int32_t channelId) const;

    void GetChannels(MapWrapper& channels) const;

    void ReleaseChannel();

    virtual void* NewItem(int32_t itemID);

    virtual void DeleteItem(void* item);

    uint32_t _instanceId;
};

class ScopedChannel
{
public:
    // Can only be created by the channel manager
    ScopedChannel(ChannelManager& chManager);

    ScopedChannel(ChannelManager& chManager, int32_t channelId);

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
