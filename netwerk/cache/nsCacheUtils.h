/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCacheUtils_h_
#define _nsCacheUtils_h_

#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"

class nsIThread;

/**
 * A class with utility methods for shutting down nsIThreads easily.
  */
class nsShutdownThread : public nsRunnable {
public:
  explicit nsShutdownThread(nsIThread *aThread);
  ~nsShutdownThread();

  NS_IMETHOD Run();

/**
 * Shutdown ensures that aThread->Shutdown() is called on a main thread
 */
  static nsresult Shutdown(nsIThread *aThread);

/**
 * BlockingShutdown ensures that by the time it returns, aThread->Shutdown() has
 * been called and no pending events have been processed on the current thread.
 */
  static nsresult BlockingShutdown(nsIThread *aThread);

private:
  mozilla::Mutex      mLock;
  mozilla::CondVar    mCondVar;
  nsCOMPtr<nsIThread> mThread;
};

#endif
