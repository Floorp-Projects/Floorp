/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <Foundation/Foundation.h>

#include "gtest/gtest.h"
#include "mozilla/SyncRunnable.h"
#include "nsIThread.h"
#include "nsIThreadManager.h"

using namespace mozilla;

// Tests whether an nsThread ran a block in its NSRunLoop.
// ThreadCreationOptions.isUiThread gets set to aIsUiThread.
// Returns true if a block ran on the NSRunLoop.
bool UiThreadRunsRunLoop(bool aIsUiThread) {
  nsCOMPtr<nsIThread> thread;
  nsIThreadManager::ThreadCreationOptions options;
  options.isUiThread = aIsUiThread;
  MOZ_ALWAYS_SUCCEEDS(NS_NewNamedThread(
      "Testing Thread", getter_AddRefs(thread), nullptr, options));

  __block bool blockRanInRunLoop = false;
  {
    // We scope this so `loop` doesn't live past `thread-Shutdown()` since this
    // file is compiled without ARC.
    NSRunLoop* loop = nil;
    auto syncRunnable =
        MakeRefPtr<SyncRunnable>(NS_NewRunnableFunction(__func__, [&] {
          // If the thread doesn't already have an NSRunLoop, one will be
          // created.
          loop = NSRunLoop.currentRunLoop;
        }));
    MOZ_ALWAYS_SUCCEEDS(syncRunnable->DispatchToThread(thread));

    [loop performBlock:^void() {
      blockRanInRunLoop = true;
    }];
  }

  thread->Shutdown();
  return blockRanInRunLoop;
}

TEST(ThreadsMac, OptionsIsUiThread)
{
  const bool isUiThread = true;
  const bool isNoUiThread = false;

  EXPECT_TRUE(UiThreadRunsRunLoop(isUiThread));
  EXPECT_FALSE(UiThreadRunsRunLoop(isNoUiThread));
}
