/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_LIST_NO_STL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_LIST_NO_STL_H_

#include "constructor_magic.h"

namespace webrtc {
class CriticalSectionWrapper;

class ListNoStlItem
{
public:
    ListNoStlItem(const void* ptr);
    ListNoStlItem(const unsigned int item);
    virtual ~ListNoStlItem();
    void* GetItem() const;
    unsigned int GetUnsignedItem() const;

protected:
    ListNoStlItem* next_;
    ListNoStlItem* prev_;

private:
    friend class ListNoStl;

    const void*         item_ptr_;
    const unsigned int  item_;
    DISALLOW_COPY_AND_ASSIGN(ListNoStlItem);
};


class ListNoStl
{
public:
    ListNoStl();
    virtual ~ListNoStl();

    // ListWrapper functions
    unsigned int GetSize() const;
    int PushBack(const void* ptr);
    int PushBack(const unsigned int item_id);
    int PushFront(const void* ptr);
    int PushFront(const unsigned int item_id);
    int PopFront();
    int PopBack();
    bool Empty() const;
    ListNoStlItem* First() const;
    ListNoStlItem* Last() const;
    ListNoStlItem* Next(ListNoStlItem* item) const;
    ListNoStlItem* Previous(ListNoStlItem* item) const;
    int Erase(ListNoStlItem* item);
    int Insert(ListNoStlItem* existing_previous_item,
               ListNoStlItem* new_item);

    int InsertBefore(ListNoStlItem* existing_next_item,
                     ListNoStlItem* new_item);

private:
    void PushBack(ListNoStlItem* item);
    void PushFront(ListNoStlItem* item);

    CriticalSectionWrapper* critical_section_;
    ListNoStlItem* first_;
    ListNoStlItem* last_;
    unsigned int size_;
    DISALLOW_COPY_AND_ASSIGN(ListNoStl);
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_LIST_NO_STL_H_
