/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxChroot_h
#define mozilla_SandboxChroot_h

#include <pthread.h>

#include "mozilla/Attributes.h"

// This class uses the chroot(2) system call and Linux namespaces to
// revoke the process's access to the filesystem.  It requires that
// the process be able to create user namespaces; this is the
// kHasUserNamespaces in common/SandboxInfo.h.
//
// Usage: call Prepare() from a thread with CAP_SYS_CHROOT in its
// effective capability set, then later call Invoke() when ready to
// drop filesystem access.  Prepare() creates a thread to do the
// chrooting, so the caller can (and should!) drop its own
// capabilities afterwards.  When Invoke() returns, the thread will
// have exited.
//
// (Exception: on Android/B2G <= KitKat, because of how pthread_join
// is implemented, the thread may still exist, but it will not have
// capabilities.  Accordingly, on such systems, be careful about
// namespaces or other resources the thread might have inherited.)
//
// Prepare() can fail (return false); for example, if it doesn't have
// CAP_SYS_CHROOT or if it can't create a directory to chroot into.
//
// The root directory will be empty and deleted, so the process will
// not be able to create new entries in it regardless of permissions.

namespace mozilla {

class SandboxChroot final {
public:
  SandboxChroot();
  ~SandboxChroot();
  bool Prepare();
  void Invoke();
private:
  enum Command {
    NO_THREAD,
    NO_COMMAND,
    DO_CHROOT,
    JUST_EXIT,
  };

  pthread_t mThread;
  pthread_mutex_t mMutex;
  pthread_cond_t mWakeup;
  Command mCommand;
  int mFd;

  void ThreadMain();
  static void* StaticThreadMain(void* aVoidPtr);
  bool SendCommand(Command aComm);
};

} // namespace mozilla

#endif // mozilla_SandboxChroot_h
