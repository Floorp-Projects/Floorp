/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LulRWLock_h
#define LulRWLock_h

#include "LulPlatformMacros.h"
#include <pthread.h>

// This class provides a simple wrapper around reader-writer locks.
// This is necessary because Android 2.2's libpthread does not
// provide such functionality, so there has to be a way to provide
// an alternative with an equivalent interface.

namespace lul {

class LulRWLock {

public:
  LulRWLock();
  ~LulRWLock();
  void WrLock();
  void RdLock();
  void Unlock();

private:
#if defined(LUL_OS_android)
  pthread_mutex_t mLock;
#elif defined(LUL_OS_linux)
  pthread_rwlock_t mLock;
#else
# error "Unsupported OS"
#endif

};

} // namespace lul

#endif // LulRWLock_h
