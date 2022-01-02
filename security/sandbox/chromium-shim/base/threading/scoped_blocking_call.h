/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file
// base/threading/scoped_blocking_call.h
// To provide to a dummy ScopedBlockingCall class. This prevents dependency
// creep and we don't use the rest of the blocking call checking.

#ifndef BASE_THREADING_SCOPED_BLOCKING_CALL_H
#define BASE_THREADING_SCOPED_BLOCKING_CALL_H

#include "base/base_export.h"
#include "base/location.h"

namespace base {

enum class BlockingType {
  // The call might block (e.g. file I/O that might hit in memory cache).
  MAY_BLOCK,
  // The call will definitely block (e.g. cache already checked and now pinging
  // server synchronously).
  WILL_BLOCK
};

class BASE_EXPORT ScopedBlockingCall {
 public:
  ScopedBlockingCall(const Location& from_here, BlockingType blocking_type) {};
  ~ScopedBlockingCall() {};
};

namespace internal {

class BASE_EXPORT ScopedBlockingCallWithBaseSyncPrimitives {
 public:
  ScopedBlockingCallWithBaseSyncPrimitives(const Location& from_here,
                                           BlockingType blocking_type) {}
  ~ScopedBlockingCallWithBaseSyncPrimitives() {};
};

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_SCOPED_BLOCKING_CALL_H
