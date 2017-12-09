/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Atomics.h"
#include "mozilla/Types.h"

#include <dlfcn.h>
#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>

// Signal number used to enable seccomp on each thread.
extern mozilla::Atomic<int> gSeccompTsyncBroadcastSignum;

static bool
SigSetNeedsFixup(const sigset_t* aSet)
{
  int tsyncSignum = gSeccompTsyncBroadcastSignum;

  return aSet != nullptr &&
         (sigismember(aSet, SIGSYS) ||
          (tsyncSignum != 0 &&
           sigismember(aSet, tsyncSignum)));
}

static void
SigSetFixup(sigset_t* aSet)
{
  int tsyncSignum = gSeccompTsyncBroadcastSignum;
  int rv = sigdelset(aSet, SIGSYS);
  MOZ_RELEASE_ASSERT(rv == 0);
  if (tsyncSignum != 0) {
    rv = sigdelset(aSet, tsyncSignum);
    MOZ_RELEASE_ASSERT(rv == 0);
  }
}

// This file defines a hook for sigprocmask() and pthread_sigmask().
// Bug 1176099: some threads block SIGSYS signal which breaks our seccomp-bpf
// sandbox. To avoid this, we intercept the call and remove SIGSYS.
//
// ENOSYS indicates an error within the hook function itself.
static int
HandleSigset(int (*aRealFunc)(int, const sigset_t*, sigset_t*),
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
  if (aHow == SIG_UNBLOCK || !SigSetNeedsFixup(aSet)) {
    return aRealFunc(aHow, aSet, aOldSet);
  }

  sigset_t newSet = *aSet;
  SigSetFixup(&newSet);
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
sigaction(int signum, const struct sigaction* act, struct sigaction* oldact)
{
  static auto sRealFunc =
    (int (*)(int, const struct sigaction*, struct sigaction*))
    dlsym(RTLD_NEXT, "sigaction");

  if (!sRealFunc) {
    errno = ENOSYS;
    return -1;
  }

  if (act == nullptr || !SigSetNeedsFixup(&act->sa_mask)) {
    return sRealFunc(signum, act, oldact);
  }

  struct sigaction newact = *act;
  SigSetFixup(&newact.sa_mask);
  return sRealFunc(signum, &newact, oldact);
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
