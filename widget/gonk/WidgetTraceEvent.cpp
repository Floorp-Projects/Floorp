/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WidgetTraceEvent.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"
#include <mozilla/CondVar.h>
#include <mozilla/Mutex.h>

using mozilla::CondVar;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

namespace mozilla {
  class TracerRunnable : public nsRunnable {
    public:
      TracerRunnable() {
        mTracerLock = new Mutex("TracerRunnable");
        mTracerCondVar = new CondVar(*mTracerLock, "TracerRunnable");
        mMainThread = do_GetMainThread();
      }

      ~TracerRunnable() {
        delete mTracerCondVar;
        delete mTracerLock;
        mTracerLock = nullptr;
        mTracerCondVar = nullptr;
      }

      virtual nsresult Run() {
        MutexAutoLock lock(*mTracerLock);
        mHasRun = true;
        mTracerCondVar->Notify();
        return NS_OK;
      }

      bool Fire() {
        if (!mTracerLock || !mTracerCondVar) {
          return false;
        }

        MutexAutoLock lock(*mTracerLock);
        mHasRun = false;
        mMainThread->Dispatch(this, NS_DISPATCH_NORMAL);
        while (!mHasRun) {
          mTracerCondVar->Wait();
        }
        return true;
      }

      void Signal() {
        MutexAutoLock lock(*mTracerLock);
        mHasRun = true;
        mTracerCondVar->Notify();
      }

    private:
      Mutex* mTracerLock;
      CondVar* mTracerCondVar;
      bool mHasRun;
      nsCOMPtr<nsIThread> mMainThread;
  };

  StaticRefPtr<TracerRunnable> sTracerRunnable;

  bool InitWidgetTracing()
  {
    if (!sTracerRunnable) {
      sTracerRunnable = new TracerRunnable();
    }
    return true;
  }

  void CleanUpWidgetTracing()
  {
    sTracerRunnable = nullptr;
  }

  bool FireAndWaitForTracerEvent()
  {
    if (sTracerRunnable) {
      return sTracerRunnable->Fire();
    }

    return false;
  }

  void SignalTracerThread()
  {
    if (sTracerRunnable) {
      return sTracerRunnable->Signal();
    }
  }
}  // namespace mozilla

