/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

#include <dlfcn.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>

// Signal number used to enable seccomp on each thread.
extern int gSeccompTsyncBroadcastSignum;

// This file defines a hook for sigprocmask() and pthread_sigmask().
// Bug 1176099: some threads block SIGSYS signal which breaks our seccomp-bpf
// sandbox. To avoid this, we intercept the call and remove SIGSYS.
//
// ENOSYS indicates an error within the hook function itself.
static int HandleSigset(int (*aRealFunc)(int, const sigset_t*, sigset_t*),
                        int aHow, const sigset_t* aSet,
                        sigset_t* aOldSet, bool aUseErrno)
{
  if (!aRealFunc) {
    if (aUseErrno) {
      errno = ENOSYS;
      return -1;
    }

    return ENOSYS;
  }

  // Avoid unnecessary work
  if (aSet == nullptr || aHow == SIG_UNBLOCK) {
    return aRealFunc(aHow, aSet, aOldSet);
  }

  sigset_t newSet = *aSet;
  if (sigdelset(&newSet, SIGSYS) != 0 ||
     (gSeccompTsyncBroadcastSignum &&
      sigdelset(&newSet, gSeccompTsyncBroadcastSignum) != 0)) {
    if (aUseErrno) {
      errno = ENOSYS;
      return -1;
    }

    return ENOSYS;
  }

  return aRealFunc(aHow, &newSet, aOldSet);
}

extern "C" MOZ_EXPORT int
sigprocmask(int how, const sigset_t* set, sigset_t* oldset)
{
  static auto sRealFunc = (int (*)(int, const sigset_t*, sigset_t*))
    dlsym(RTLD_NEXT, "sigprocmask");

  return HandleSigset(sRealFunc, how, set, oldset, true);
}

extern "C" MOZ_EXPORT int
pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset)
{
  static auto sRealFunc = (int (*)(int, const sigset_t*, sigset_t*))
    dlsym(RTLD_NEXT, "pthread_sigmask");

  return HandleSigset(sRealFunc, how, set, oldset, false);
}

extern "C" MOZ_EXPORT int
inotify_init(void)
{
  return inotify_init1(0);
}

extern "C" MOZ_EXPORT int
inotify_init1(int flags)
{
  errno = ENOSYS;
  return -1;
}
