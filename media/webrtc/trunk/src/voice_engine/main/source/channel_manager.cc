/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "channel.h"
#include "channel_manager.h"

namespace webrtc
{

namespace voe
{

ChannelManager::ChannelManager(const WebRtc_UWord32 instanceId) :
    ChannelManagerBase(),
    _instanceId(instanceId)
{
}

ChannelManager::~ChannelManager()
{
    ChannelManagerBase::DestroyAllItems();
}

bool ChannelManager::CreateChannel(WebRtc_Word32& channelId)
{
    return ChannelManagerBase::CreateItem(channelId);
}

WebRtc_Word32 ChannelManager::DestroyChannel(const WebRtc_Word32 channelId)
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

WebRtc_Word32 ChannelManager::NumOfChannels() const
{
    return ChannelManagerBase::NumOfItems();
}

WebRtc_Word32 ChannelManager::MaxNumOfChannels() const
{
    return ChannelManagerBase::MaxNumOfItems();
}

void* ChannelManager::NewItem(WebRtc_Word32 itemID)
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

Channel* ChannelManager::GetChannel(const WebRtc_Word32 channelId) const
{
    return static_cast<Channel*> (ChannelManagerBase::GetItem(channelId));
}

void ChannelManager::ReleaseChannel()
{
    ChannelManagerBase::ReleaseItem();
}

void ChannelManager::GetChannelIds(WebRtc_Word32* channelsArray,
                                   WebRtc_Word32& numOfChannels) const
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
                             WebRtc_Word32 channelId) :
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
