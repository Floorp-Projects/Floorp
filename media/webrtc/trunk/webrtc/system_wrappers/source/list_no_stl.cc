/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/list_wrapper.h"

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

ListItem::ListItem(const void* item)
    : next_(0),
      prev_(0),
      item_ptr_(item),
      item_(0) {
}

ListItem::ListItem(const unsigned int item)
    : next_(0),
      prev_(0),
      item_ptr_(0),
      item_(item) {
}

ListItem::~ListItem() {
}

void* ListItem::GetItem() const {
  return const_cast<void*>(item_ptr_);
}

unsigned int ListItem::GetUnsignedItem() const {
  return item_;
}

ListWrapper::ListWrapper()
    : critical_section_(CriticalSectionWrapper::CreateCriticalSection()),
      first_(0),
      last_(0),
      size_(0) {
}

ListWrapper::~ListWrapper() {
  if (!Empty()) {
    // TODO(hellner) I'm not sure this loggin is useful.
    WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1,
                 "Potential memory leak in ListWrapper");
    // Remove all remaining list items.
    while (Erase(First()) == 0)
    {}
  }
  delete critical_section_;
}

bool ListWrapper::Empty() const {
  return !first_ && !last_;
}

unsigned int ListWrapper::GetSize() const {
  return size_;
}

int ListWrapper::PushBack(const void* ptr) {
  ListItem* item = new ListItem(ptr);
  CriticalSectionScoped lock(critical_section_);
  PushBackImpl(item);
  return 0;
}

int ListWrapper::PushBack(const unsigned int item_id) {
  ListItem* item = new ListItem(item_id);
  CriticalSectionScoped lock(critical_section_);
  PushBackImpl(item);
  return 0;
}

int ListWrapper::PushFront(const unsigned int item_id) {
  ListItem* item = new ListItem(item_id);
  CriticalSectionScoped lock(critical_section_);
  PushFrontImpl(item);
  return 0;
}

int ListWrapper::PushFront(const void* ptr) {
  ListItem* item = new ListItem(ptr);
  CriticalSectionScoped lock(critical_section_);
  PushFrontImpl(item);
  return 0;
}

int ListWrapper::PopFront() {
  return Erase(first_);
}

int ListWrapper::PopBack() {
  return Erase(last_);
}

ListItem* ListWrapper::First() const {
  return first_;
}

ListItem* ListWrapper::Last() const {
  return last_;
}

ListItem* ListWrapper::Next(ListItem* item) const {
  if (!item) {
    return 0;
  }
  return item->next_;
}

ListItem* ListWrapper::Previous(ListItem* item) const {
  if (!item) {
    return 0;
  }
  return item->prev_;
}

int ListWrapper::Insert(ListItem* existing_previous_item, ListItem* new_item) {
  if (!new_item) {
    return -1;
  }
  // Allow existing_previous_item to be NULL if the list is empty.
  // TODO(hellner) why allow this? Keep it as is for now to avoid
  // breaking API contract.
  if (!existing_previous_item && !Empty()) {
    return -1;
  }
  CriticalSectionScoped lock(critical_section_);
  if (!existing_previous_item) {
    PushBackImpl(new_item);
    return 0;
  }
  ListItem* next_item = existing_previous_item->next_;
  new_item->next_ = existing_previous_item->next_;
  new_item->prev_ = existing_previous_item;
  existing_previous_item->next_ = new_item;
  if (next_item) {
    next_item->prev_ = new_item;
  } else {
    last_ = new_item;
  }
  size_++;
  return 0;
}

int ListWrapper::InsertBefore(ListItem* existing_next_item,
                              ListItem* new_item) {
  if (!new_item) {
    return -1;
  }
  // Allow existing_next_item to be NULL if the list is empty.
  // Todo: why allow this? Keep it as is for now to avoid breaking API
  // contract.
  if (!existing_next_item && !Empty()) {
    return -1;
  }
  CriticalSectionScoped lock(critical_section_);
  if (!existing_next_item) {
    PushBackImpl(new_item);
    return 0;
  }

  ListItem* previous_item = existing_next_item->prev_;
  new_item->next_ = existing_next_item;
  new_item->prev_ = previous_item;
  existing_next_item->prev_ = new_item;
  if (previous_item) {
    previous_item->next_ = new_item;
  } else {
    first_ = new_item;
  }
  size_++;
  return 0;
}

int ListWrapper::Erase(ListItem* item) {
  if (!item) {
    return -1;
  }
  size_--;
  ListItem* previous_item = item->prev_;
  ListItem* next_item = item->next_;
  if (!previous_item) {
    if (next_item) {
      next_item->prev_ = 0;
    }
    first_ = next_item;
  } else {
    previous_item->next_ = next_item;
  }
  if (!next_item) {
    if (previous_item) {
      previous_item->next_ = 0;
    }
    last_ = previous_item;
  } else {
    next_item->prev_ = previous_item;
  }
  delete item;
  return 0;
}

void ListWrapper::PushBackImpl(ListItem* item) {
  if (Empty()) {
    first_ = item;
    last_ = item;
    size_++;
    return;
  }

  item->prev_ = last_;
  last_->next_ = item;
  last_ = item;
  size_++;
}

void ListWrapper::PushFrontImpl(ListItem* item) {
  if (Empty()) {
    first_ = item;
    last_ = item;
    size_++;
    return;
  }

  item->next_ = first_;
  first_->prev_ = item;
  first_ = item;
  size_++;
}

} //namespace webrtc
