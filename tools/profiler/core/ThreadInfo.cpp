/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadInfo.h"
#include "ThreadProfile.h"

#include "mozilla/DebugOnly.h"

ThreadInfo::ThreadInfo(const char* aName, int aThreadId,
                       bool aIsMainThread, PseudoStack* aPseudoStack,
                       void* aStackTop)
  : mName(strdup(aName))
  , mThreadId(aThreadId)
  , mIsMainThread(aIsMainThread)
  , mPseudoStack(aPseudoStack)
  , mPlatformData(Sampler::AllocPlatformData(aThreadId))
  , mProfile(nullptr)
  , mStackTop(aStackTop)
  , mPendingDelete(false)
{
#ifndef SPS_STANDALONE
  mThread = NS_GetCurrentThread();
#endif

  // We don't have to guess on mac
#ifdef XP_MACOSX
  pthread_t self = pthread_self();
  mStackTop = pthread_get_stackaddr_np(self);
#endif
}

ThreadInfo::~ThreadInfo() {
  free(mName);

  if (mProfile)
    delete mProfile;

  Sampler::FreePlatformData(mPlatformData);
}

void
ThreadInfo::SetPendingDelete()
{
  mPendingDelete = true;
  // We don't own the pseudostack so disconnect it.
  mPseudoStack = nullptr;
  if (mProfile) {
    mProfile->SetPendingDelete();
  }
}

bool
ThreadInfo::CanInvokeJS() const
{
#ifdef SPS_STANDALONE
  return false;
#else
  nsIThread* thread = GetThread();
  if (!thread) {
    MOZ_ASSERT(IsMainThread());
    return true;
  }
  bool result;
  mozilla::DebugOnly<nsresult> rv = thread->GetCanInvokeJS(&result);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return result;
#endif
}
