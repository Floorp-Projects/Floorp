/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/channel_manager.h"

namespace webrtc
{

namespace voe
{

ChannelManager::ChannelManager(uint32_t instanceId) :
    ChannelManagerBase(),
    _instanceId(instanceId)
{
}

ChannelManager::~ChannelManager()
{
    ChannelManagerBase::DestroyAllItems();
}

bool ChannelManager::CreateChannel(int32_t& channelId)
{
    return ChannelManagerBase::CreateItem(channelId);
}

int32_t ChannelManager::DestroyChannel(int32_t channelId)
{
    Channel* deleteChannel =
        static_cast<Channel*> (ChannelManagerBase::RemoveItem(channelId));
    if (!deleteChannel)
    {
        return -1;
    }
    delete deleteChannel;
    return 0;
}

int32_t ChannelManager::NumOfChannels() const
{
    return ChannelManagerBase::NumOfItems();
}

int32_t ChannelManager::MaxNumOfChannels() const
{
    return ChannelManagerBase::MaxNumOfItems();
}

void* ChannelManager::NewItem(int32_t itemID)
{
    Channel* channel;
    if (Channel::CreateChannel(channel, itemID, _instanceId) == -1)
    {
        return NULL;
    }
    return static_cast<void*> (channel);
}

void ChannelManager::DeleteItem(void* item)
{
    Channel* deleteItem = static_cast<Channel*> (item);
    delete deleteItem;
}

Channel* ChannelManager::GetChannel(int32_t channelId) const
{
    return static_cast<Channel*> (ChannelManagerBase::GetItem(channelId));
}

void ChannelManager::ReleaseChannel()
{
    ChannelManagerBase::ReleaseItem();
}

void ChannelManager::GetChannelIds(int32_t* channelsArray,
                                   int32_t& numOfChannels) const
{
    ChannelManagerBase::GetItemIds(channelsArray, numOfChannels);
}

void ChannelManager::GetChannels(MapWrapper& channels) const
{
    ChannelManagerBase::GetChannels(channels);
}

ScopedChannel::ScopedChannel(ChannelManager& chManager) :
    _chManager(chManager),
    _channelPtr(NULL)
{
    // Copy all existing channels to the local map.
    // It is not possible to utilize the ChannelPtr() API after
    // this constructor. The intention is that this constructor
    // is used in combination with the scoped iterator.
    _chManager.GetChannels(_channels);
}

ScopedChannel::ScopedChannel(ChannelManager& chManager,
                             int32_t channelId) :
    _chManager(chManager),
    _channelPtr(NULL)
{
    _channelPtr = _chManager.GetChannel(channelId);
}

ScopedChannel::~ScopedChannel()
{
    if (_channelPtr != NULL || _channels.Size() != 0)
    {
        _chManager.ReleaseChannel();
    }

    // Delete the map
    while (_channels.Erase(_channels.First()) == 0)
        ;
}

Channel* ScopedChannel::ChannelPtr()
{
    return _channelPtr;
}

Channel* ScopedChannel::GetFirstChannel(void*& iterator) const
{
    MapItem* it = _channels.First();
    iterator = (void*) it;
    if (!it)
    {
        return NULL;
    }
    return static_cast<Channel*> (it->GetItem());
}

Channel* ScopedChannel::GetNextChannel(void*& iterator) const
{
    MapItem* it = (MapItem*) iterator;
    if (!it)
    {
        iterator = NULL;
        return NULL;
    }
    it = _channels.Next(it);
    iterator = (void*) it;
    if (!it)
    {
        return NULL;
    }
    return static_cast<Channel*> (it->GetItem());
}

} // namespace voe

} // namespace webrtc
