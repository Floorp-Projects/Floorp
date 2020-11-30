/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GlobalSemaphore.h"

#include "commonupdatedir.h"  // for GetInstallHash
#include "mozilla/UniquePtr.h"
#include "updatedefines.h"  // for NS_t* definitions

#ifndef XP_WIN
#  include <fcntl.h>     // for O_CREAT
#  include <sys/stat.h>  // for mode constants
#else
#  include <limits.h>  // for _XOPEN_PATH_MAX / _POSIX_PATH_MAX
#endif

#ifdef XP_WIN
#  define SEMAPHORE_NAME_PREFIX "Global\\"
#else
#  define SEMAPHORE_NAME_PREFIX "/"
#endif

namespace mozilla {

// Prevent this function failing the build for being unused on Mac.
#ifndef XP_MACOSX

static bool GetSemaphoreName(const char* nameToken, const char16_t* installPath,
                             mozilla::UniquePtr<NS_tchar[]>& semName) {
  mozilla::UniquePtr<NS_tchar[]> pathHash;
  if (!GetInstallHash(installPath, MOZ_APP_VENDOR, pathHash)) {
    return false;
  }

  size_t semNameLen = strlen(SEMAPHORE_NAME_PREFIX) + strlen(nameToken) +
                      NS_tstrlen(pathHash.get()) + 1;
  semName = mozilla::MakeUnique<NS_tchar[]>(semNameLen + 1);
  if (!semName) {
    return false;
  }

// On Windows, we need to convert the token string to UTF-16.
// printf can do that for us, but we need to tell it to by changing the type
// indicator in the format string.
// We also need different prefixes to the semaphore name, because it's a
// path-like string and because Windows has the "global" concept.
#  ifdef XP_WIN
  const NS_tchar* kSemaphoreNameFormat = NS_T(SEMAPHORE_NAME_PREFIX "%S-%s");
#  else
  const NS_tchar* kSemaphoreNameFormat = NS_T(SEMAPHORE_NAME_PREFIX "%s-%s");
#  endif

  NS_tsnprintf(semName.get(), semNameLen + 1, kSemaphoreNameFormat, nameToken,
               pathHash.get());

  return true;
}

#endif

#ifdef XP_WIN

GlobalSemHandle OpenGlobalSemaphore(const char* nameToken,
                                    const char16_t* installPath) {
  mozilla::UniquePtr<NS_tchar[]> semName;
  if (!GetSemaphoreName(nameToken, installPath, semName)) {
    return nullptr;
  }

  // Initialize the semaphore to LONG_MAX because we're not actually using it
  // to control access to any limited resource, we just want to know how many
  // times the semaphore is taken. And taking a semaphore decrements it, so we
  // have to start at the top of the range instead of at zero.
  GlobalSemHandle sem =
      ::CreateSemaphoreW(nullptr, LONG_MAX, LONG_MAX, semName.get());
  if (sem) {
    // Claim a reference to the semaphore. After this point, the semaphore count
    // should always be LONG_MAX - [number of running instances].
    if (::WaitForSingleObject(sem, 0) != WAIT_OBJECT_0) {
      // Either there are LONG_MAX instances running or something is wrong and
      // this semaphore is not usable.
      ::CloseHandle(sem);
      sem = nullptr;
    }
  }
  return sem;
}

void ReleaseGlobalSemaphore(GlobalSemHandle sem) {
  if (sem) {
    ::ReleaseSemaphore(sem, 1, nullptr);
    ::CloseHandle(sem);
  }
}

bool IsOtherInstanceRunning(GlobalSemHandle sem, bool* aResult) {
  // There's no documented way to get a semaphore's current count except to
  // wait on it (which decrements it) and then release it (which increments it
  // back and returns the pre-incremented count), so that's what we'll do. There
  // is also NtQuerySemaphore, but using the native API doesn't seem worth the
  // trouble here.
  if (sem && ::WaitForSingleObject(sem, 0) == WAIT_OBJECT_0) {
    LONG count = 0;
    if (::ReleaseSemaphore(sem, 1, &count)) {
      // At rest, the count is (LONG_MAX - [number of running instances]), but
      // the count we read had been
      // decremented once more by the Wait call here, so we need to compare
      // against one less than that to see if there's exactly one instance
      // running.
      *aResult = (count != (LONG_MAX - 2));
      return true;
    }
  }

  return false;
}

#elif defined(XP_MACOSX)

// We don't actually have Mac support here, because sem_getvalue isn't supported
// there (the header calls it deprecated, but actually it's just unimplemented),
// and there is no other way to get the value of a POSIX-semaphore. We'll have
// to do something platform-specific.

GlobalSemHandle OpenGlobalSemaphore(const char* /* unused */,
                                    const char16_t* /* unused */) {
  // Return something that isn't a valid pointer, but that the manager won't
  // think represents a failure.
  return (GlobalSemHandle)1;
}

void ReleaseGlobalSemaphore(GlobalSemHandle /* unused */) {}

bool IsOtherInstanceRunning(GlobalSemHandle /* unused */, bool* aResult) {
  *aResult = false;
  return true;
}

#else  // Neither Windows nor Mac

GlobalSemHandle OpenGlobalSemaphore(const char* nameToken,
                                    const char16_t* installPath) {
  mozilla::UniquePtr<NS_tchar[]> semName;
  if (!GetSemaphoreName(nameToken, installPath, semName)) {
    return nullptr;
  }

  // Initialize the semaphore to the maximum value because we don't actually
  // want to limit the number of instances, and assign all permissions because
  // we also don't want to lock out any other users. The update service has
  // protections to prevent problems resulting from this semaphore
  // being messed with by an attacker.
  GlobalSemHandle sem = sem_open(semName.get(), O_CREAT,
                                 S_IRWXU | S_IRWXG | S_IRWXO, SEM_VALUE_MAX);
  if (sem == SEM_FAILED) {
    return nullptr;
  }

  // Decrement the semaphore whether we created it or opened it. This way the
  // count is always SEM_VALUE_MAX - [number of running instances].
  if (sem_trywait(sem)) {
    // Either there are SEM_VALUE_MAX instances running or something is wrong
    // and this semaphore is not usable.
    sem_close(sem);
    return nullptr;
  }

  return sem;
}

void ReleaseGlobalSemaphore(GlobalSemHandle sem) {
  if (sem) {
    sem_post(sem);
    sem_close(sem);
  }
}

bool IsOtherInstanceRunning(GlobalSemHandle sem, bool* aResult) {
  int value = 0;
  if (sem && !sem_getvalue(sem, &value)) {
    // Zero and negative values are all error states.
    if (value <= 0) {
      return false;
    }
    *aResult = (value != (SEM_VALUE_MAX - 1));
    return true;
  }
  return false;
}

#endif

};  // namespace mozilla
