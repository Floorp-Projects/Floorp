/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#define NS_EXPORT __attribute__((visibility("default")))

extern "C" NS_EXPORT int raise(int sig) {
  // Bug 741272: Bionic incorrectly uses kill(), which signals the
  // process, and thus could signal another thread (and let this one
  // return "successfully" from raising a fatal signal).
  //
  // Bug 943170: POSIX specifies pthread_kill(pthread_self(), sig) as
  // equivalent to raise(sig), but Bionic also has a bug with these
  // functions, where a forked child will kill its parent instead.

  extern pid_t gettid(void);
  return syscall(__NR_tgkill, getpid(), gettid(), sig);
}

/* Flash plugin uses symbols that are not present in Android >= 4.4 */
namespace android {
namespace VectorImpl {
NS_EXPORT void reservedVectorImpl1(void) {}
NS_EXPORT void reservedVectorImpl2(void) {}
NS_EXPORT void reservedVectorImpl3(void) {}
NS_EXPORT void reservedVectorImpl4(void) {}
NS_EXPORT void reservedVectorImpl5(void) {}
NS_EXPORT void reservedVectorImpl6(void) {}
NS_EXPORT void reservedVectorImpl7(void) {}
NS_EXPORT void reservedVectorImpl8(void) {}
}  // namespace VectorImpl
}  // namespace android
