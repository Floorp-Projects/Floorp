/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "map_no_stl.h"

#include "critical_section_wrapper.h"
#include "trace.h"

namespace webrtc {
MapNoStlItem::MapNoStlItem(int id, void* item)
    : next_(0),
      prev_(0),
      item_id_(id),
      item_ptr_(item)
{
}

MapNoStlItem::~MapNoStlItem()
{
}

void* MapNoStlItem::GetItem()
{
    return item_ptr_;
}

int MapNoStlItem::GetId()
{
    return item_id_;
}

unsigned int MapNoStlItem::GetUnsignedId()
{
    return static_cast<unsigned int>(item_id_);
}

void MapNoStlItem::SetItem(void* ptr)
{
    item_ptr_ = ptr;
}

MapNoStl::MapNoStl()
    : critical_section_(CriticalSectionWrapper::CreateCriticalSection()),
      first_(0),
      last_(0),
      size_(0)
{
}

MapNoStl::~MapNoStl()
{
    if (First())
    {
        WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1,
                   "Potential memory leak in MapNoStl");
        while (Erase(First()) == 0)
        {}
    }
    delete critical_section_;
}

int MapNoStl::Size() const
{
    return size_;
}

int MapNoStl::Insert(int id, void* ptr)
{
    MapNoStlItem* new_item = new MapNoStlItem(id, ptr);

    CriticalSectionScoped lock(critical_section_);
    MapNoStlItem* item = first_;
    size_++;
    if (!item)
    {
        first_ = new_item;
        last_ = new_item;
        return 0;
    }
    while(item->next_)
    {
        // Three scenarios
        // 1. Item should be inserted first.
        // 2. Item should be inserted between two items
        // 3. Item should be inserted last
        if (item->GetId() > id)
        {
            new_item->next_ = item;
            item->prev_ = new_item;
            if (item == first_)
            {
                first_ = new_item;
            }
            else
            {
                new_item->prev_ = item->prev_;
                new_item->prev_->next_ = new_item;
            }
            return 0;
        }
        item = item->next_;
    }
    // 3
    item->next_ = new_item;
    new_item->prev_ = item;
    last_ = new_item;
    return 0;
}

MapNoStlItem* MapNoStl::First() const
{
    return first_;
}

MapNoStlItem* MapNoStl::Last() const
{
    return last_;
}

MapNoStlItem* MapNoStl::Next(MapNoStlItem* item) const
{
    if (!item)
    {
        return 0;
    }
    return item->next_;
}

MapNoStlItem* MapNoStl::Previous(MapNoStlItem* item) const
{
    if (!item)
    {
        return 0;
    }
    return item->prev_;
}

MapNoStlItem* MapNoStl::Find(int id) const
{
    CriticalSectionScoped lock(critical_section_);
    MapNoStlItem* item = Locate(id);
    return item;
}

int MapNoStl::Erase(MapNoStlItem* item)
{
    if(!item)
    {
        return -1;
    }
    CriticalSectionScoped lock(critical_section_);
    return Remove(item);
}

int MapNoStl::Erase(const int id)
{
    CriticalSectionScoped lock(critical_section_);
    MapNoStlItem* item = Locate(id);
    if(!item)
    {
        return -1;
    }
    return Remove(item);
}

MapNoStlItem* MapNoStl::Locate(int id) const
{
    MapNoStlItem* item = first_;
    while(item)
    {
        if (item->GetId() == id)
        {
            return item;
        }
        item = item->next_;
    }
    return 0;
}

int MapNoStl::Remove(MapNoStlItem* item)
{
    if (!item)
    {
        return -1;
    }
    size_--;
    MapNoStlItem* previous_item = item->prev_;
    MapNoStlItem* next_item = item->next_;
    if (!previous_item)
    {
        next_item->prev_ = 0;
        first_ = next_item;
    }
    else
    {
        previous_item->next_ = next_item;
    }
    if (!next_item)
    {
        previous_item->next_ = 0;
        last_ = previous_item;
    }
    else
    {
        next_item->prev_ = previous_item;
    }
    delete item;
    return 0;
}
} // namespace webrtc
