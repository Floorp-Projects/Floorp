/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadIOLogger.h"

#include "GeckoProfiler.h"
#include "IOInterposerPrivate.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "nsNativeCharsetUtils.h"

/**
 * This code uses NSPR stuff and STL containers because it must be detached
 * from leak checking code; this observer runs until the process terminates.
 */

#include <prenv.h>
#include <prprf.h>
#include <prthread.h>
#include <vector>

namespace {

struct ObservationWithStack
{
  ObservationWithStack(mozilla::IOInterposeObserver::Observation& aObs,
                       ProfilerBacktrace* aStack)
    : mObservation(aObs)
    , mStack(aStack)
  {
    const char16_t* filename = aObs.Filename();
    if (filename) {
      mFilename = filename;
    }
  }

  mozilla::IOInterposeObserver::Observation mObservation;
  ProfilerBacktrace*                        mStack;
  nsString                                  mFilename;
};

class MainThreadIOLoggerImpl final : public mozilla::IOInterposeObserver
{
public:
  MainThreadIOLoggerImpl();
  ~MainThreadIOLoggerImpl();

  bool Init();

  void Observe(Observation& aObservation);

private:
  static void sIOThreadFunc(void* aArg);
  void IOThreadFunc();

  TimeStamp             mLogStartTime;
  const char*           mFileName;
  PRThread*             mIOThread;
  IOInterposer::Monitor mMonitor;
  bool                  mShutdownRequired;
  std::vector<ObservationWithStack> mObservations;
};

static StaticAutoPtr<MainThreadIOLoggerImpl> sImpl;

MainThreadIOLoggerImpl::MainThreadIOLoggerImpl()
  : mFileName(nullptr)
  , mIOThread(nullptr)
  , mShutdownRequired(false)
{
}

MainThreadIOLoggerImpl::~MainThreadIOLoggerImpl()
{
  if (!mIOThread) {
    return;
  }
  {
    // Scope for lock
    IOInterposer::MonitorAutoLock lock(mMonitor);
    mShutdownRequired = true;
    lock.Notify();
  }
  PR_JoinThread(mIOThread);
  mIOThread = nullptr;
}

bool
MainThreadIOLoggerImpl::Init()
{
  if (mFileName) {
    // Already initialized
    return true;
  }
  mFileName = PR_GetEnv("MOZ_MAIN_THREAD_IO_LOG");
  if (!mFileName) {
    // Can't start
    return false;
  }
  mIOThread = PR_CreateThread(PR_USER_THREAD, &sIOThreadFunc, this,
                              PR_PRIORITY_LOW, PR_GLOBAL_THREAD,
                              PR_JOINABLE_THREAD, 0);
  if (!mIOThread) {
    return false;
  }
  return true;
}

/* static */ void
MainThreadIOLoggerImpl::sIOThreadFunc(void* aArg)
{
  AutoProfilerRegister registerThread("MainThreadIOLogger");
  PR_SetCurrentThreadName("MainThreadIOLogger");
  MainThreadIOLoggerImpl* obj = static_cast<MainThreadIOLoggerImpl*>(aArg);
  obj->IOThreadFunc();
}

void
MainThreadIOLoggerImpl::IOThreadFunc()
{
  PRFileDesc* fd = PR_Open(mFileName, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                           PR_IRUSR | PR_IWUSR | PR_IRGRP);
  if (!fd) {
    IOInterposer::MonitorAutoLock lock(mMonitor);
    mShutdownRequired = true;
    std::vector<ObservationWithStack>().swap(mObservations);
    return;
  }
  mLogStartTime = TimeStamp::Now();
  {
    // Scope for lock
    IOInterposer::MonitorAutoLock lock(mMonitor);
    while (true) {
      while (!mShutdownRequired && mObservations.empty()) {
        lock.Wait();
      }
      if (mShutdownRequired) {
        break;
      }
      // Pull events off the shared array onto a local one
      std::vector<ObservationWithStack> observationsToWrite;
      observationsToWrite.swap(mObservations);

      // Release the lock so that we're not holding anybody up during I/O
      IOInterposer::MonitorAutoUnlock unlock(mMonitor);

      // Now write the events.
      for (auto i = observationsToWrite.begin(), e = observationsToWrite.end();
           i != e; ++i) {
        if (i->mObservation.ObservedOperation() == OpNextStage) {
          PR_fprintf(fd, "%f,NEXT-STAGE\n",
                     (TimeStamp::Now() - mLogStartTime).ToMilliseconds());
          continue;
        }
        double durationMs = i->mObservation.Duration().ToMilliseconds();
        nsAutoCString nativeFilename;
        nativeFilename.AssignLiteral("(not available)");
        if (!i->mFilename.IsEmpty()) {
          if (NS_FAILED(NS_CopyUnicodeToNative(i->mFilename, nativeFilename))) {
            nativeFilename.AssignLiteral("(conversion failed)");
          }
        }
        /**
         * Format:
         * Start Timestamp (Milliseconds), Operation, Duration (Milliseconds), Event Source, Filename
         */
        if (PR_fprintf(fd, "%f,%s,%f,%s,%s\n",
                       (i->mObservation.Start() - mLogStartTime).ToMilliseconds(),
                       i->mObservation.ObservedOperationString(), durationMs,
                       i->mObservation.Reference(), nativeFilename.get()) > 0) {
          // TODO: Write out the callstack
          i->mStack = nullptr;
        }
      }
    }
  }
  PR_Close(fd);
}

void
MainThreadIOLoggerImpl::Observe(Observation& aObservation)
{
  if (!mFileName || !IsMainThread()) {
    return;
  }
  IOInterposer::MonitorAutoLock lock(mMonitor);
  if (mShutdownRequired) {
    // The writer thread isn't running. Don't enqueue any more data.
    return;
  }
  // Passing nullptr as aStack parameter for now
  mObservations.push_back(ObservationWithStack(aObservation, nullptr));
  lock.Notify();
}

} // namespace

namespace mozilla {

namespace MainThreadIOLogger {

bool
Init()
{
  nsAutoPtr<MainThreadIOLoggerImpl> impl(new MainThreadIOLoggerImpl());
  if (!impl->Init()) {
    return false;
  }
  sImpl = impl.forget();
  IOInterposer::Register(IOInterposeObserver::OpAllWithStaging, sImpl);
  return true;
}

} // namespace MainThreadIOLogger

} // namespace mozilla

