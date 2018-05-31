/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_codecoveragehandler_h
#define mozilla_codecoveragehandler_h

#include "mozilla/StaticPtr.h"
#include "mozilla/ipc/CrossProcessMutex.h"

namespace mozilla {

class CodeCoverageHandler {
public:
  static void Init();
  static void Init(const CrossProcessMutexHandle& aHandle);
  static CodeCoverageHandler* Get();
  CrossProcessMutex* GetMutex();
  CrossProcessMutexHandle GetMutexHandle(int aProcId);
  static void DumpCounters(int);
  static void ResetCounters(int);

private:
  CodeCoverageHandler();
  explicit CodeCoverageHandler(const CrossProcessMutexHandle& aHandle);

  static StaticAutoPtr<CodeCoverageHandler> instance;
  CrossProcessMutex mGcovLock;

  DISALLOW_COPY_AND_ASSIGN(CodeCoverageHandler);

  void SetSignalHandlers();
};

}

#endif // mozilla_codecoveragehandler_h
