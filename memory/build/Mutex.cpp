/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Mutex.h"

#include <errno.h>

#include "mozilla/Assertions.h"

bool Mutex::TryLock() {
#if defined(XP_WIN)
  return !!TryEnterCriticalSection(&mMutex);
#elif defined(XP_DARWIN)
  return os_unfair_lock_trylock(&mMutex);
#else
  switch (pthread_mutex_trylock(&mMutex)) {
    case 0:
      return true;
    case EBUSY:
      return false;
    default:
      MOZ_CRASH("pthread_mutex_trylock error.");
  }
#endif
}

#if defined(XP_DARWIN)

// static
bool Mutex::SpinInKernelSpace() {
#  ifdef __aarch64__
  return true;
#  else
  if (__builtin_available(macOS 10.15, *)) {
    return true;
  }

  return false;
#  endif
}

// static
const bool Mutex::gSpinInKernelSpace = SpinInKernelSpace();

#endif  // defined(XP_DARWIN)
