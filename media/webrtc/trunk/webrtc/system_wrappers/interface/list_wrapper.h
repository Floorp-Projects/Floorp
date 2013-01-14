/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_LIST_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_LIST_WRAPPER_H_

#include "webrtc/system_wrappers/interface/constructor_magic.h"

namespace webrtc {

class CriticalSectionWrapper;

class ListItem {
  friend class ListWrapper;

 public:
  ListItem(const void* ptr);
  ListItem(const unsigned int item);
  virtual ~ListItem();
  void* GetItem() const;
  unsigned int GetUnsignedItem() const;

 protected:
  ListItem* next_;
  ListItem* prev_;

 private:
  const void*         item_ptr_;
  const unsigned int  item_;
};

class ListWrapper {
 public:
  ListWrapper();
  virtual ~ListWrapper();

  // Returns the number of elements stored in the list.
  unsigned int GetSize() const;

  // Puts a pointer to anything last in the list.
  int PushBack(const void* ptr);
  // Puts a pointer to anything first in the list.
  int PushFront(const void* ptr);

  // Puts a copy of the specified integer last in the list.
  int PushBack(const unsigned int item_id);
  // Puts a copy of the specified integer first in the list.
  int PushFront(const unsigned int item_id);

  // Pops the first ListItem from the list
  int PopFront();

  // Pops the last ListItem from the list
  int PopBack();

  // Returns true if the list is empty
  bool Empty() const;

  // Returns a pointer to the first ListItem in the list.
  ListItem* First() const;

  // Returns a pointer to the last ListItem in the list.
  ListItem* Last() const;

  // Returns a pointer to the ListItem stored after item in the list.
  ListItem* Next(ListItem* item) const;

  // Returns a pointer to the ListItem stored before item in the list.
  ListItem* Previous(ListItem* item) const;

  // Removes item from the list.
  int Erase(ListItem* item);

  // Insert list item after existing_previous_item. Please note that new_item
  // must be created using new ListItem(). The map will take ownership of
  // new_item following a successfull insert. If insert fails new_item will
  // not be released by the List
  int Insert(ListItem* existing_previous_item,
             ListItem* new_item);

  // Insert list item before existing_next_item. Please note that new_item
  // must be created using new ListItem(). The map will take ownership of
  // new_item following a successfull insert. If insert fails new_item will
  // not be released by the List
  int InsertBefore(ListItem* existing_next_item,
                   ListItem* new_item);

 private:
  void PushBackImpl(ListItem* item);
  void PushFrontImpl(ListItem* item);

  CriticalSectionWrapper* critical_section_;
  ListItem* first_;
  ListItem* last_;
  unsigned int size_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_LIST_WRAPPER_H_
