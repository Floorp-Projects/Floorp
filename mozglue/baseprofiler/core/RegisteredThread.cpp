/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RegisteredThread.h"

#include "BaseProfiler.h"

namespace mozilla {
namespace baseprofiler {

RegisteredThread::RegisteredThread(ThreadInfo* aInfo, void* aStackTop)
    : mRacyRegisteredThread(aInfo->ThreadId()),
      mPlatformData(AllocPlatformData(aInfo->ThreadId())),
      mStackTop(aStackTop),
      mThreadInfo(aInfo) {
  // We don't have to guess on mac
#if defined(GP_OS_darwin)
  pthread_t self = pthread_self();
  mStackTop = pthread_get_stackaddr_np(self);
#endif
}

RegisteredThread::~RegisteredThread() {}

size_t RegisteredThread::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mPlatformData
  // - mRacyRegisteredThread.mPendingMarkers
  //
  // The following members are not measured:
  // - mThreadInfo: because it is non-owning

  return n;
}

}  // namespace baseprofiler
}  // namespace mozilla
