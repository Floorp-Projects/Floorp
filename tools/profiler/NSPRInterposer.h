/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPRINTERPOSER_H_
#define NSPRINTERPOSER_H_

#ifdef MOZ_ENABLE_PROFILER_SPS

#include "IOInterposer.h"
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "prio.h"

namespace mozilla {

/**
 * This class provides main thread I/O interposing for NSPR file reads, writes,
 * and fsyncs. Most methods must be called from the main thread, except for:
 *   Enable
 *   Read
 *   Write
 *   FSync
 */
class NSPRInterposer MOZ_FINAL : public IOInterposerModule
{
public:
  static IOInterposerModule* GetInstance(IOInterposeObserver* aObserver, 
      IOInterposeObserver::Operation aOpsToInterpose);

  /**
   * This function must be called from the main thread, and furthermore
   * it must be called when no other threads are executing in Read, Write,
   * or FSync. Effectivly this restricts us to calling it only after all other
   * threads have been stopped during shutdown.
   */
  static void ClearInstance();

  ~NSPRInterposer();
  void Enable(bool aEnable);

private:
  NSPRInterposer();
  bool Init(IOInterposeObserver* aObserver,
            IOInterposeObserver::Operation aOpsToInterpose);

  static PRInt32 PR_CALLBACK Read(PRFileDesc* aFd, void* aBuf, PRInt32 aAmt);
  static PRInt32 PR_CALLBACK Write(PRFileDesc* aFd, const void* aBuf,
                                   PRInt32 aAmt);
  static PRStatus PR_CALLBACK FSync(PRFileDesc* aFd);
  static StaticAutoPtr<NSPRInterposer> sSingleton;

  friend class NSPRAutoTimer;

  IOInterposeObserver*  mObserver;
  PRIOMethods*          mFileIOMethods;
  Atomic<uint32_t,ReleaseAcquire> mEnabled;
  PRReadFN              mOrigReadFn;
  PRWriteFN             mOrigWriteFn;
  PRFsyncFN             mOrigFSyncFn;
};

/**
 * RAII class for timing the duration of an NSPR I/O call and reporting the
 * result to the IOInterposeObserver.
 */
class NSPRAutoTimer
{
public:
  NSPRAutoTimer(IOInterposeObserver::Operation aOp)
    :mOp(aOp),
     mShouldObserve(NSPRInterposer::sSingleton->mEnabled && NS_IsMainThread())
  {
    if (mShouldObserve) {
      mStart = mozilla::TimeStamp::Now();
    }
  }

  ~NSPRAutoTimer()
  {
    if (mShouldObserve) {
      double duration = (mozilla::TimeStamp::Now() - mStart).ToMilliseconds();
      NSPRInterposer::sSingleton->mObserver->Observe(mOp, duration,
                                                     sModuleInfo);
    }
  }

private:
  IOInterposeObserver::Operation  mOp;
  bool                            mShouldObserve;
  mozilla::TimeStamp              mStart;
  static const char*              sModuleInfo;
};

} // namespace mozilla

#endif // MOZ_ENABLE_PROFILER_SPS

#endif // NSPRINTERPOSER_H_

