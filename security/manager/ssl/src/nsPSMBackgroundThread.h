/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSPSMBACKGROUNDTHREAD_H_
#define _NSPSMBACKGROUNDTHREAD_H_

#include "nspr.h"
#include "nscore.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsNSSComponent.h"

class nsPSMBackgroundThread
{
protected:
  static void PR_CALLBACK nsThreadRunner(void *arg);
  virtual void Run(void) = 0;

  // used to join the thread
  PRThread *mThreadHandle;

  // Shared mutex used for condition variables,
  // and to protect access to mExitState.
  // Derived classes may use it to protect additional
  // resources.
  mozilla::Mutex mMutex;

  // Used to signal the thread's Run loop when a job is added 
  // and/or exit is requested.
  mozilla::CondVar mCond;

  bool exitRequested(::mozilla::MutexAutoLock const & proofOfLock) const;
  bool exitRequestedNoLock() const { return mExitState != ePSMThreadRunning; }
  nsresult postStoppedEventToMainThread(::mozilla::MutexAutoLock const & proofOfLock);

private:
  enum {
    ePSMThreadRunning = 0,
    ePSMThreadStopRequested = 1,
    ePSMThreadStopped = 2
  } mExitState;

  // The thread's name.
  nsCString mName;

public:
  nsPSMBackgroundThread();
  virtual ~nsPSMBackgroundThread();

  nsresult startThread(const nsCSubstring & name);
  void requestExit();
};


#endif
