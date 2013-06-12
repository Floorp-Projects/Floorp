/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "channel_manager_base.h"

#include "critical_section_wrapper.h"
#include "rw_lock_wrapper.h"
#include <cassert>

namespace webrtc
{

namespace voe
{

ChannelManagerBase::ChannelManagerBase() :
    _itemsCritSectPtr(CriticalSectionWrapper::CreateCriticalSection()),
    _itemsRWLockPtr(RWLockWrapper::CreateRWLock())
{
    for (int i = 0; i < kVoiceEngineMaxNumChannels; i++)
    {
        _freeItemIds[i] = true;
    }
}

ChannelManagerBase::~ChannelManagerBase()
{
    if (_itemsRWLockPtr)
    {
        delete _itemsRWLockPtr;
        _itemsRWLockPtr = NULL;
    }
    if (_itemsCritSectPtr)
    {
        delete _itemsCritSectPtr;
        _itemsCritSectPtr = NULL;
    }
}

bool ChannelManagerBase::GetFreeItemId(int32_t& itemId)
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    int32_t i(0);
    while (i < kVoiceEngineMaxNumChannels)
    {
        if (_freeItemIds[i])
        {
            itemId = i;
            _freeItemIds[i] = false;
            return true;
        }
        i++;
    }
    return false;
}

void ChannelManagerBase::AddFreeItemId(int32_t itemId)
{
    assert(itemId < kVoiceEngineMaxNumChannels);
    _freeItemIds[itemId] = true;
}

void ChannelManagerBase::RemoveFreeItemIds()
{
    for (int i = 0; i < kVoiceEngineMaxNumChannels; i++)
    {
        _freeItemIds[i] = false;
    }
}

bool ChannelManagerBase::CreateItem(int32_t& itemId)
{
    _itemsCritSectPtr->Enter();
    void* itemPtr;
    itemId = -1;
    const bool success = GetFreeItemId(itemId);
    if (!success)
    {
        _itemsCritSectPtr->Leave();
        return false;
    }
    itemPtr = NewItem(itemId);
    if (!itemPtr)
    {
        _itemsCritSectPtr->Leave();
        return false;
    }
    _itemsCritSectPtr->Leave();
    InsertItem(itemId, itemPtr);

    return true;
}

void ChannelManagerBase::InsertItem(int32_t itemId, void* item)
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    assert(!_items.Find(itemId));
    _items.Insert(itemId, item);
}

void*
ChannelManagerBase::RemoveItem(int32_t itemId)
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    WriteLockScoped wlock(*_itemsRWLockPtr);
    MapItem* it = _items.Find(itemId);
    if (!it)
    {
        return 0;
    }
    void* returnItem = it->GetItem();
    _items.Erase(it);
    AddFreeItemId(itemId);

    return returnItem;
}

void ChannelManagerBase::DestroyAllItems()
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    MapItem* it = _items.First();
    while (it)
    {
        DeleteItem(it->GetItem());
        _items.Erase(it);
        it = _items.First();
    }
    RemoveFreeItemIds();
}

int32_t ChannelManagerBase::NumOfItems() const
{
    return _items.Size();
}

int32_t ChannelManagerBase::MaxNumOfItems() const
{
    return static_cast<int32_t> (kVoiceEngineMaxNumChannels);
}

void*
ChannelManagerBase::GetItem(int32_t itemId) const
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    MapItem* it = _items.Find(itemId);
    if (!it)
    {
        return 0;
    }
    _itemsRWLockPtr->AcquireLockShared();
    return it->GetItem();
}

void*
ChannelManagerBase::GetFirstItem(void*& iterator) const
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    MapItem* it = _items.First();
    iterator = (void*) it;
    if (!it)
    {
        return 0;
    }
    return it->GetItem();
}

void*
ChannelManagerBase::GetNextItem(void*& iterator) const
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    MapItem* it = (MapItem*) iterator;
    if (!it)
    {
        iterator = 0;
        return 0;
    }
    it = _items.Next(it);
    iterator = (void*) it;
    if (!it)
    {
        return 0;
    }
    return it->GetItem();
}

void ChannelManagerBase::ReleaseItem()
{
    _itemsRWLockPtr->ReleaseLockShared();
}

void ChannelManagerBase::GetItemIds(int32_t* channelsArray,
                                    int32_t& numOfChannels) const
{
    MapItem* it = _items.First();
    numOfChannels = (numOfChannels <= _items.Size()) ?
        numOfChannels : _items.Size();
    for (int i = 0; i < numOfChannels && it != NULL; i++)
    {
        channelsArray[i] = it->GetId();
        it = _items.Next(it);
    }
}

void ChannelManagerBase::GetChannels(MapWrapper& channels) const
{
    CriticalSectionScoped cs(_itemsCritSectPtr);
    if (_items.Size() == 0)
    {
        return;
    }
    _itemsRWLockPtr->AcquireLockShared();
    for (MapItem* it = _items.First(); it != NULL; it = _items.Next(it))
    {
        channels.Insert(it->GetId(), it->GetItem());
    }
}

} // namespace voe

} // namespace webrtc
