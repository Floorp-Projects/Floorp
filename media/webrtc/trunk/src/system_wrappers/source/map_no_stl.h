/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_MAP_NO_STL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_MAP_NO_STL_H_

#include "constructor_magic.h"

namespace webrtc {
class CriticalSectionWrapper;

class MapNoStlItem
{
friend class Map;

public:
    MapNoStlItem(int id, void* ptr);
    virtual ~MapNoStlItem();
    void* GetItem();
    int GetId();
    unsigned int GetUnsignedId();
    void SetItem(void* ptr);

protected:
    MapNoStlItem* next_;
    MapNoStlItem* prev_;

private:
    int item_id_;
    void* item_ptr_;
    DISALLOW_COPY_AND_ASSIGN(MapNoStlItem);
};

class MapNoStl
{
public:
    MapNoStl();
    virtual ~MapNoStl();

    // MapWrapper functions.
    int Insert(int id, void* ptr);
    int Erase(MapNoStlItem* item);
    int Erase(int id);
    int Size() const;
    MapNoStlItem* First() const;
    MapNoStlItem* Last() const;
    MapNoStlItem* Next(MapNoStlItem* item) const;
    MapNoStlItem* Previous(MapNoStlItem* item) const;
    MapNoStlItem* Find(int id) const;

private:
    MapNoStlItem* Locate(int id) const;
    int Remove(MapNoStlItem* item);

    CriticalSection* critical_section_;
    MapNoStlItem* first_;
    MapNoStlItem* last_;
    int size_;
    DISALLOW_COPY_AND_ASSIGN(MapNoStl);
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_MAP_NO_STL_H_
