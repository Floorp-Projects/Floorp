/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_LIST_STL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_LIST_STL_H_

#include <list>

#include "webrtc/system_wrappers/interface/constructor_magic.h"

namespace webrtc {

class ListItem {
 public:
  ListItem(const void* ptr);
  ListItem(const unsigned int item);
  virtual ~ListItem();
  void* GetItem() const;
  unsigned int GetUnsignedItem() const;

 private:
  friend class ListWrapper;
  mutable std::list<ListItem*>::iterator this_iter_;
  const void*         item_ptr_;
  const unsigned int  item_;
  DISALLOW_COPY_AND_ASSIGN(ListItem);
};

class ListWrapper {
 public:
  ListWrapper();
  ~ListWrapper();

  // ListWrapper functions
  unsigned int GetSize() const;
  int PushBack(const void* ptr);
  int PushBack(const unsigned int item_id);
  int PushFront(const void* ptr);
  int PushFront(const unsigned int item_id);
  int PopFront();
  int PopBack();
  bool Empty() const;
  ListItem* First() const;
  ListItem* Last() const;
  ListItem* Next(ListItem* item) const;
  ListItem* Previous(ListItem* item) const;
  int Erase(ListItem* item);
  int Insert(ListItem* existing_previous_item, ListItem* new_item);
  int InsertBefore(ListItem* existing_next_item, ListItem* new_item);

 private:
  mutable std::list<ListItem*> list_;
  DISALLOW_COPY_AND_ASSIGN(ListWrapper);
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_LIST_STL_H_
