/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerThreadRegistrationData.h"

#if defined(XP_WIN)
#  include <windows.h>
#elif defined(XP_DARWIN)
#  include <pthread.h>
#endif

namespace mozilla::profiler {

ThreadRegistrationData::ThreadRegistrationData(const char* aName,
                                               const void* aStackTop)
    : mInfo(aName),
      mPlatformData(mInfo.ThreadId()),
      mStackTop(
#if defined(XP_WIN)
          // We don't have to guess on Windows.
          reinterpret_cast<const void*>(
              reinterpret_cast<PNT_TIB>(NtCurrentTeb())->StackBase)
#elif defined(XP_DARWIN)
          // We don't have to guess on Mac/Darwin.
          reinterpret_cast<const void*>(
              pthread_get_stackaddr_np(pthread_self()))
#else
          // Otherwise use the given guess.
          aStackTop
#endif
      ) {
}

}  // namespace mozilla::profiler
