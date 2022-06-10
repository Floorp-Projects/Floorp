/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringStats.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/MemoryReporting.h"
#include "nsString.h"

#include <stdint.h>
#include <stdio.h>

#ifdef XP_WIN
#  include <windows.h>
#  include <process.h>
#else
#  include <unistd.h>
#  include <pthread.h>
#endif

nsStringStats gStringStats;

nsStringStats::~nsStringStats() {
  // this is a hack to suppress duplicate string stats printing
  // in seamonkey as a result of the string code being linked
  // into seamonkey and libxpcom! :-(
  if (!mAllocCount && !mAdoptCount) {
    return;
  }

  // Only print the stats if we detect a leak.
  if (mAllocCount <= mFreeCount && mAdoptCount <= mAdoptFreeCount) {
    return;
  }

  printf("nsStringStats\n");
  printf(" => mAllocCount:     % 10d\n", int(mAllocCount));
  printf(" => mReallocCount:   % 10d\n", int(mReallocCount));
  printf(" => mFreeCount:      % 10d", int(mFreeCount));
  if (mAllocCount > mFreeCount) {
    printf("  --  LEAKED %d !!!\n", mAllocCount - mFreeCount);
  } else {
    printf("\n");
  }
  printf(" => mShareCount:     % 10d\n", int(mShareCount));
  printf(" => mAdoptCount:     % 10d\n", int(mAdoptCount));
  printf(" => mAdoptFreeCount: % 10d", int(mAdoptFreeCount));
  if (mAdoptCount > mAdoptFreeCount) {
    printf("  --  LEAKED %d !!!\n", mAdoptCount - mAdoptFreeCount);
  } else {
    printf("\n");
  }

#ifdef XP_WIN
  auto pid = uintptr_t(_getpid());
  auto tid = uintptr_t(GetCurrentThreadId());
#else
  auto pid = uintptr_t(getpid());
  auto tid = uintptr_t(pthread_self());
#endif

  printf(" => Process ID: %" PRIuPTR ", Thread ID: %" PRIuPTR "\n", pid, tid);
}
