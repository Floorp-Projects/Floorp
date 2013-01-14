/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/map_wrapper.h"

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

MapItem::MapItem(int id, void* item)
    : item_id_(id),
      item_pointer_(item) {
}

MapItem::~MapItem() {
}

void* MapItem::GetItem() {
  return item_pointer_;
}

int MapItem::GetId() {
  return item_id_;
}

unsigned int MapItem::GetUnsignedId() {
  return static_cast<unsigned int>(item_id_);
}

void MapItem::SetItem(void* ptr) {
  item_pointer_ = ptr;
}

MapWrapper::MapWrapper() : map_() {
}

MapWrapper::~MapWrapper() {
  if (!map_.empty()) {
    WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1,
                 "Potential memory leak in MapWrapper");
    // Remove all map items. Please note that std::map::clear() can't be
    // used because each item has some dynamically allocated memory
    // associated with it (i.e. using std::map::clear would introduce a
    // memory leak).
    while (Erase(First()) == 0)
    {}
  }
}

int MapWrapper::Size() const {
  return (int)map_.size();
}

int MapWrapper::Insert(int id, void* ptr) {
  map_[id] = new MapItem(id, ptr);
  return 0;
}

MapItem* MapWrapper::First() const {
  std::map<int, MapItem*>::const_iterator it = map_.begin();
  if (it != map_.end()) {
    return it->second;
  }
  return 0;
}

MapItem* MapWrapper::Last() const {
  std::map<int, MapItem*>::const_reverse_iterator it = map_.rbegin();
  if (it != map_.rend()) {
    return it->second;
  }
  return 0;
}

MapItem* MapWrapper::Next(MapItem* item) const {
  if (item == 0) {
    return 0;
  }
  std::map<int, MapItem*>::const_iterator it = map_.find(item->item_id_);
  if (it != map_.end()) {
    it++;
    if (it != map_.end()) {
      return it->second;
    }
  }
  return 0;
}

MapItem* MapWrapper::Previous(MapItem* item) const {
  if (item == 0) {
    return 0;
  }

  std::map<int, MapItem*>::const_iterator it = map_.find(item->item_id_);
  if ((it != map_.end()) &&
      (it != map_.begin())) {
    --it;
    return it->second;
  }
  return 0;
}

MapItem* MapWrapper::Find(int id) const {
  std::map<int, MapItem*>::const_iterator it = map_.find(id);
  if (it != map_.end()) {
    return it->second;
  }
  return 0;
}

int MapWrapper::Erase(MapItem* item) {
  if (item == 0) {
    return -1;
  }
  std::map<int, MapItem*>::iterator it = map_.find(item->item_id_);
  if (it != map_.end()) {
    delete it->second;
    map_.erase(it);
    return 0;
  }
  return -1;
}

int MapWrapper::Erase(const int id) {
  std::map<int, MapItem*>::iterator it = map_.find(id);
  if (it != map_.end()) {
    delete it->second;
    map_.erase(it);
    return 0;
  }
  return -1;
}

} // namespace webrtc
