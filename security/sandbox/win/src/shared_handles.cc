// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/shared_handles.h"

namespace sandbox {

// Note once again the the assumption here is that the shared memory is
// initialized with zeros in the process that calls SetHandle and that
// the process that calls GetHandle 'sees' this memory.

SharedHandles::SharedHandles() {
  shared_.items = NULL;
  shared_.max_items = 0;
}

bool SharedHandles::Init(void* raw_mem, size_t size_bytes) {
  if (size_bytes < sizeof(shared_.items[0])) {
    // The shared memory is too small!
    return false;
  }
  shared_.items = static_cast<SharedItem*>(raw_mem);
  shared_.max_items = size_bytes / sizeof(shared_.items[0]);
  return true;
}

// Note that an empty slot is marked with a tag == 0 that is why is
// not a valid imput tag
bool SharedHandles::SetHandle(uint32 tag, HANDLE handle) {
  if (0 == tag) {
    // Invalid tag
    return false;
  }
  // Find empty slot and put the tag and the handle there
  SharedItem* empty_slot = FindByTag(0);
  if (NULL == empty_slot) {
    return false;
  }
  empty_slot->tag = tag;
  empty_slot->item = handle;
  return true;
}

bool SharedHandles::GetHandle(uint32 tag, HANDLE* handle) {
  if (0 == tag) {
    // Invalid tag
    return false;
  }
  SharedItem* found = FindByTag(tag);
  if (NULL == found) {
    return false;
  }
  *handle = found->item;
  return true;
}

SharedHandles::SharedItem* SharedHandles::FindByTag(uint32 tag) {
  for (size_t ix = 0; ix != shared_.max_items; ++ix) {
    if (tag == shared_.items[ix].tag) {
      return &shared_.items[ix];
    }
  }
  return NULL;
}

}  // namespace sandbox
