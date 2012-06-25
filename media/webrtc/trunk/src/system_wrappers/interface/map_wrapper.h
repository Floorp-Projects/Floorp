/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_MAP_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_MAP_WRAPPER_H_

#include <map>

#include "constructor_magic.h"

namespace webrtc {
class MapItem
{
friend class MapWrapper;

public:
    MapItem(int id, void* ptr);
    virtual ~MapItem();
    void* GetItem();
    int GetId();
    unsigned int GetUnsignedId();
    void SetItem(void* ptr);

private:
    int   item_id_;
    void* item_pointer_;
};

class MapWrapper
{
public:
    MapWrapper();
    ~MapWrapper();

    // Puts a pointer to anything in the map and associates it with id. Note, id
    // needs to be unique for all items in the map.
    int Insert(int id, void* ptr);

    // Removes item from map.
    int Erase(MapItem* item);

    // Finds item with associated with id and removes it from the map.
    int Erase(int id);

    // Returns the number of elements stored in the map.
    int Size() const;

    // Returns a pointer to the first MapItem in the map.
    MapItem* First() const;

    // Returns a pointer to the last MapItem in the map.
    MapItem* Last() const;

    // Returns a pointer to the MapItem stored after item in the map.
    MapItem* Next(MapItem* item) const;

    // Returns a pointer to the MapItem stored before item in the map.
    MapItem* Previous(MapItem* item) const;

    // Returns a pointer to the MapItem associated with id from the map.
    MapItem* Find(int id) const;

private:
    std::map<int, MapItem*>    map_;
};
} // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_MAP_WRAPPER_H_
