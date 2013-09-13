/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IOInterposer.h"
#include "NSPRInterposer.h"

#include "prio.h"
#include "private/pprio.h"

namespace {

using namespace mozilla;

/* Original IO methods */
PRReadFN  sReadFn  = nullptr;
PRWriteFN sWriteFn = nullptr;
PRFsyncFN sFSyncFn = nullptr;

/**
 * RAII class for timing the duration of an NSPR I/O call and reporting the
 * result to the IOInterposeObserver API.
 */
class NSPRIOAutoObservation : public IOInterposeObserver::Observation
{
public:
  NSPRIOAutoObservation(IOInterposeObserver::Operation aOp)
    : mShouldObserve(IOInterposer::IsObservedOperation(aOp))
  {
    if (mShouldObserve) {
      mOperation = aOp;
      mStart = TimeStamp::Now(); 
    }
  }

  ~NSPRIOAutoObservation()
  {
    if (mShouldObserve) {
      mEnd  = TimeStamp::Now();
      const char* ref = "NSPRIOInterposing";
      mReference = ref;

      // Report this auto observation
      IOInterposer::Report(*this);
    }
  }

private:
  bool mShouldObserve;
};

int32_t PR_CALLBACK interposedRead(PRFileDesc* aFd, void* aBuf, int32_t aAmt)
{
  // If we don't have a valid original function pointer something is very wrong.
  NS_ASSERTION(sReadFn, "NSPR IO Interposing: sReadFn is NULL");

  NSPRIOAutoObservation timer(IOInterposeObserver::OpRead);
  return sReadFn(aFd, aBuf, aAmt);
}

int32_t PR_CALLBACK interposedWrite(PRFileDesc* aFd, const void* aBuf,
                                    int32_t aAmt)
{
  // If we don't have a valid original function pointer something is very wrong.
  NS_ASSERTION(sWriteFn, "NSPR IO Interposing: sWriteFn is NULL");

  NSPRIOAutoObservation timer(IOInterposeObserver::OpWrite);
  return sWriteFn(aFd, aBuf, aAmt);
}

PRStatus PR_CALLBACK interposedFSync(PRFileDesc* aFd)
{
  // If we don't have a valid original function pointer something is very wrong.
  NS_ASSERTION(sFSyncFn, "NSPR IO Interposing: sFSyncFn is NULL");

  NSPRIOAutoObservation timer(IOInterposeObserver::OpFSync);
  return sFSyncFn(aFd);
}

} // anonymous namespace

namespace mozilla {

void InitNSPRIOInterposing()
{
  // Check that we have not interposed any of the IO methods before
  MOZ_ASSERT(!sReadFn && !sWriteFn && !sFSyncFn);

  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());

  // Get IO methods from NSPR and const cast the structure so we can modify it.
  PRIOMethods* methods = const_cast<PRIOMethods*>(PR_GetFileMethods());

  // Something is badly wrong if we don't get IO methods... However, we don't
  // want to crash over that in non-debug builds. This is unlikely to happen
  // so an assert is enough, no need to report it to the caller.
  MOZ_ASSERT(methods);
  if (!methods) {
    return;
  }

  // Store original read, write, sync functions
  sReadFn   = methods->read;
  sWriteFn  = methods->write;
  sFSyncFn  = methods->fsync;

  // Overwrite with our interposed read, write, sync functions
  methods->read   = &interposedRead;
  methods->write  = &interposedWrite;
  methods->fsync  = &interposedFSync;
}

void ClearNSPRIOInterposing()
{
  // If we have already cleared IO interposing, or not initialized it this is
  // actually bad.
  MOZ_ASSERT(sReadFn && sWriteFn && sFSyncFn);

  // Get IO methods from NSPR and const cast the structure so we can modify it.
  PRIOMethods* methods = const_cast<PRIOMethods*>(PR_GetFileMethods());

  // Something is badly wrong if we don't get IO methods... However, we don't
  // want to crash over that in non-debug builds. This is unlikely to happen
  // so an assert is enough, no need to report it to the caller.
  MOZ_ASSERT(methods);
  if (!methods) {
    return;
  }

  // Restore original read, write, sync functions
  methods->read   = sReadFn;
  methods->write  = sWriteFn;
  methods->fsync  = sFSyncFn;

  // Forget about original functions
  sReadFn   = nullptr;
  sWriteFn  = nullptr;
  sFSyncFn  = nullptr;
}

} // namespace mozilla
