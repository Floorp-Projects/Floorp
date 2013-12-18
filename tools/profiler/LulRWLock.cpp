/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/NullPtr.h"

#include "LulRWLock.h"


namespace lul {

// An implementation for targets where libpthread does provide
// pthread_rwlock_t.  These are straight wrappers around the
// equivalent pthread functions.

#if defined(LUL_OS_linux)

LulRWLock::LulRWLock() {
  mozilla::DebugOnly<int> r = pthread_rwlock_init(&mLock, nullptr);
  MOZ_ASSERT(!r);
}

LulRWLock::~LulRWLock() {
  mozilla::DebugOnly<int>r = pthread_rwlock_destroy(&mLock);
  MOZ_ASSERT(!r);
}

void
LulRWLock::WrLock() {
  mozilla::DebugOnly<int>r = pthread_rwlock_wrlock(&mLock);
  MOZ_ASSERT(!r);
}

void
LulRWLock::RdLock() {
  mozilla::DebugOnly<int>r = pthread_rwlock_rdlock(&mLock);
  MOZ_ASSERT(!r);
}

void
LulRWLock::Unlock() {
  mozilla::DebugOnly<int>r = pthread_rwlock_unlock(&mLock);
  MOZ_ASSERT(!r);
}


// An implementation for cases where libpthread does not provide
// pthread_rwlock_t.  Currently this is a kludge in that it uses
// normal mutexes, resulting in the following limitations: (1) at most
// one reader is allowed at once, and (2) any thread that tries to
// read-acquire the lock more than once will deadlock.  (2) could be
// avoided if it were possible to use recursive pthread_mutex_t's.

#elif defined(LUL_OS_android)

LulRWLock::LulRWLock() {
  mozilla::DebugOnly<int> r = pthread_mutex_init(&mLock, nullptr);
  MOZ_ASSERT(!r);
}

LulRWLock::~LulRWLock() {
  mozilla::DebugOnly<int>r = pthread_mutex_destroy(&mLock);
  MOZ_ASSERT(!r);
}

void
LulRWLock::WrLock() {
  mozilla::DebugOnly<int>r = pthread_mutex_lock(&mLock);
  MOZ_ASSERT(!r);
}

void
LulRWLock::RdLock() {
  mozilla::DebugOnly<int>r = pthread_mutex_lock(&mLock);
  MOZ_ASSERT(!r);
}

void
LulRWLock::Unlock() {
  mozilla::DebugOnly<int>r = pthread_mutex_unlock(&mLock);
  MOZ_ASSERT(!r);
}


#else
# error "Unsupported OS"
#endif

} // namespace lul
