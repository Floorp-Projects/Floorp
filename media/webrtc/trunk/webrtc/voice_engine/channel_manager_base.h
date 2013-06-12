/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CHANNEL_MANAGER_BASE_H
#define WEBRTC_VOICE_ENGINE_CHANNEL_MANAGER_BASE_H

#include "typedefs.h"
#include "map_wrapper.h"
#include "voice_engine_defines.h"

namespace webrtc
{
class CriticalSectionWrapper;
class RWLockWrapper;

namespace voe
{

class ScopedChannel;
class Channel;

class ChannelManagerBase
{
protected:
    bool CreateItem(int32_t& itemId);

    void InsertItem(int32_t itemId, void* item);

    void* RemoveItem(int32_t itemId);

    void* GetItem(int32_t itemId) const;

    void* GetFirstItem(void*& iterator) const ;

    void* GetNextItem(void*& iterator) const;

    void ReleaseItem();

    void AddFreeItemId(int32_t itemId);

    bool GetFreeItemId(int32_t& itemId);

    void RemoveFreeItemIds();

    void DestroyAllItems();

    int32_t NumOfItems() const;

    int32_t MaxNumOfItems() const;

    void GetItemIds(int32_t* channelsArray,
                    int32_t& numOfChannels) const;

    void GetChannels(MapWrapper& channels) const;

    virtual void* NewItem(int32_t itemId) = 0;

    virtual void DeleteItem(void* item) = 0;

    ChannelManagerBase();

    virtual ~ChannelManagerBase();

private:
    // Protects _items and _freeItemIds
    CriticalSectionWrapper* _itemsCritSectPtr;

    MapWrapper _items;

    bool _freeItemIds[kVoiceEngineMaxNumChannels];

    // Protects channels from being destroyed while being used
    RWLockWrapper* _itemsRWLockPtr;
};

} // namespace voe

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CHANNEL_MANAGER_BASE_H
