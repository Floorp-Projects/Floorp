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
PRCloseFN sCloseFn = nullptr;
PRReadFN  sReadFn  = nullptr;
PRWriteFN sWriteFn = nullptr;
PRFsyncFN sFSyncFn = nullptr;
PRFileInfoFN sFileInfoFn = nullptr;
PRFileInfo64FN sFileInfo64Fn = nullptr;

/**
 * RAII class for timing the duration of an NSPR I/O call and reporting the
 * result to the IOInterposeObserver API.
 */
class NSPRIOAutoObservation : public IOInterposeObserver::Observation
{
public:
  explicit NSPRIOAutoObservation(IOInterposeObserver::Operation aOp)
    : IOInterposeObserver::Observation(aOp, "NSPRIOInterposer")
  {
  }

  ~NSPRIOAutoObservation()
  {
    Report();
  }
};

PRStatus PR_CALLBACK interposedClose(PRFileDesc* aFd)
{
  // If we don't have a valid original function pointer something is very wrong.
  NS_ASSERTION(sCloseFn, "NSPR IO Interposing: sCloseFn is NULL");

  NSPRIOAutoObservation timer(IOInterposeObserver::OpClose);
  return sCloseFn(aFd);
}

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

PRStatus PR_CALLBACK interposedFileInfo(PRFileDesc *aFd, PRFileInfo *aInfo)
{
  // If we don't have a valid original function pointer something is very wrong.
  NS_ASSERTION(sFileInfoFn, "NSPR IO Interposing: sFileInfoFn is NULL");

  NSPRIOAutoObservation timer(IOInterposeObserver::OpStat);
  return sFileInfoFn(aFd, aInfo);
}

PRStatus PR_CALLBACK interposedFileInfo64(PRFileDesc *aFd, PRFileInfo64 *aInfo)
{
  // If we don't have a valid original function pointer something is very wrong.
  NS_ASSERTION(sFileInfo64Fn, "NSPR IO Interposing: sFileInfo64Fn is NULL");

  NSPRIOAutoObservation timer(IOInterposeObserver::OpStat);
  return sFileInfo64Fn(aFd, aInfo);
}

} // anonymous namespace

namespace mozilla {

void InitNSPRIOInterposing()
{
  // Check that we have not interposed any of the IO methods before
  MOZ_ASSERT(!sCloseFn && !sReadFn && !sWriteFn && !sFSyncFn && !sFileInfoFn &&
             !sFileInfo64Fn);

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

  // Store original functions
  sCloseFn      = methods->close;
  sReadFn       = methods->read;
  sWriteFn      = methods->write;
  sFSyncFn      = methods->fsync;
  sFileInfoFn   = methods->fileInfo;
  sFileInfo64Fn = methods->fileInfo64;

  // Overwrite with our interposed functions
  methods->close      = &interposedClose;
  methods->read       = &interposedRead;
  methods->write      = &interposedWrite;
  methods->fsync      = &interposedFSync;
  methods->fileInfo   = &interposedFileInfo;
  methods->fileInfo64 = &interposedFileInfo64;
}

void ClearNSPRIOInterposing()
{
  // If we have already cleared IO interposing, or not initialized it this is
  // actually bad.
  MOZ_ASSERT(sCloseFn && sReadFn && sWriteFn && sFSyncFn && sFileInfoFn &&
             sFileInfo64Fn);

  // Get IO methods from NSPR and const cast the structure so we can modify it.
  PRIOMethods* methods = const_cast<PRIOMethods*>(PR_GetFileMethods());

  // Something is badly wrong if we don't get IO methods... However, we don't
  // want to crash over that in non-debug builds. This is unlikely to happen
  // so an assert is enough, no need to report it to the caller.
  MOZ_ASSERT(methods);
  if (!methods) {
    return;
  }

  // Restore original functions
  methods->close      = sCloseFn;
  methods->read       = sReadFn;
  methods->write      = sWriteFn;
  methods->fsync      = sFSyncFn;
  methods->fileInfo   = sFileInfoFn;
  methods->fileInfo64 = sFileInfo64Fn;

  // Forget about original functions
  sCloseFn      = nullptr;
  sReadFn       = nullptr;
  sWriteFn      = nullptr;
  sFSyncFn      = nullptr;
  sFileInfoFn   = nullptr;
  sFileInfo64Fn = nullptr;
}

} // namespace mozilla

