/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_MozglueUtils_h
#define mozilla_glue_MozglueUtils_h

#include <windows.h>

#include "mozilla/Attributes.h"

namespace mozilla {
namespace glue {

class MOZ_RAII AutoSharedLock final {
 public:
  explicit AutoSharedLock(SRWLOCK& aLock) : mLock(aLock) {
    ::AcquireSRWLockShared(&aLock);
  }

  ~AutoSharedLock() { ::ReleaseSRWLockShared(&mLock); }

  AutoSharedLock(const AutoSharedLock&) = delete;
  AutoSharedLock(AutoSharedLock&&) = delete;
  AutoSharedLock& operator=(const AutoSharedLock&) = delete;
  AutoSharedLock& operator=(AutoSharedLock&&) = delete;

 private:
  SRWLOCK& mLock;
};

class MOZ_RAII AutoExclusiveLock final {
 public:
  explicit AutoExclusiveLock(SRWLOCK& aLock) : mLock(aLock) {
    ::AcquireSRWLockExclusive(&aLock);
  }

  ~AutoExclusiveLock() { ::ReleaseSRWLockExclusive(&mLock); }

  AutoExclusiveLock(const AutoExclusiveLock&) = delete;
  AutoExclusiveLock(AutoExclusiveLock&&) = delete;
  AutoExclusiveLock& operator=(const AutoExclusiveLock&) = delete;
  AutoExclusiveLock& operator=(AutoExclusiveLock&&) = delete;

 private:
  SRWLOCK& mLock;
};

}  // namespace glue
}  // namespace mozilla

#endif  //  mozilla_glue_MozglueUtils_h
