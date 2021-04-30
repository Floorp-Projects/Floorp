/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc_abort.h"
#include "mozilla/mozalloc_oom.h"
#include "mozilla/Assertions.h"

static size_t gMozallocOOMAllocationSize = 0;

#define OOM_MSG_LEADER "out of memory: 0x"
#define OOM_MSG_DIGITS "0000000000000000"  // large enough for 2^64
#define OOM_MSG_TRAILER " bytes requested"
#define OOM_MSG_FIRST_DIGIT_OFFSET sizeof(OOM_MSG_LEADER) - 1
#define OOM_MSG_LAST_DIGIT_OFFSET \
  sizeof(OOM_MSG_LEADER) + sizeof(OOM_MSG_DIGITS) - 3

static const char* hex = "0123456789ABCDEF";

void mozalloc_handle_oom(size_t size) {
  char oomMsg[] = OOM_MSG_LEADER OOM_MSG_DIGITS OOM_MSG_TRAILER;
  size_t i;

  // NB: this is handle_oom() stage 1, which simply aborts on OOM.
  // we might proceed to a stage 2 in which an attempt is made to
  // reclaim memory
  // Warning: when stage 2 is done by, for example, notifying
  // "memory-pressure" synchronously, please audit all
  // nsExpirationTrackers and ensure that the actions they take
  // on memory-pressure notifications (via NotifyExpired) are safe.
  // Note that Document::SelectorCache::NotifyExpired is _known_
  // to not be safe: it will delete the selector it's caching,
  // which might be in use at the time under querySelector or
  // querySelectorAll.

  gMozallocOOMAllocationSize = size;

  static_assert(OOM_MSG_FIRST_DIGIT_OFFSET > 0,
                "Loop below will never terminate (i can't go below 0)");

  // Insert size into the diagnostic message using only primitive operations
  for (i = OOM_MSG_LAST_DIGIT_OFFSET; size && i >= OOM_MSG_FIRST_DIGIT_OFFSET;
       i--) {
    oomMsg[i] = hex[size % 16];
    size /= 16;
  }

  mozalloc_abort(oomMsg);
}

size_t mozalloc_get_oom_abort_size(void) { return gMozallocOOMAllocationSize; }
