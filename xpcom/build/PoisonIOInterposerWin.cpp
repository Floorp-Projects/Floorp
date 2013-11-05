/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PoisonIOInterposer.h"

#include <algorithm>
#include <stdio.h>
#include <vector>

#include <io.h>
#include <windows.h>
#include <winternl.h>

#include "mozilla/Assertions.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"
#include "nsWindowsDllInterceptor.h"
#include "plstr.h"

using namespace mozilla;

namespace {

// Keep track of poisoned state. Notice that there is no reason to lock access
// to this variable as it's only changed in InitPoisonIOInterposer and
// ClearPoisonIOInterposer which may only be called on the main-thread when no
// other threads are running.
static bool sIOPoisoned = false;

/************************ Internal NT API Declarations ************************/

/**
 * Function pointer declaration for internal NT routine to write data to file.
 * For documentation on the NtWriteFile routine, see ZwWriteFile on MSDN.
 */
typedef NTSTATUS (WINAPI *NtWriteFileFn)(
  IN    HANDLE                  aFileHandle,
  IN    HANDLE                  aEvent,
  IN    PIO_APC_ROUTINE         aApc,
  IN    PVOID                   aApcCtx,
  OUT   PIO_STATUS_BLOCK        aIoStatus,
  IN    PVOID                   aBuffer,
  IN    ULONG                   aLength,
  IN    PLARGE_INTEGER          aOffset,
  IN    PULONG                  aKey
);

/**
 * Function pointer declaration for internal NT routine to write data to file.
 * No documentation exists, see wine sources for details.
 */
typedef NTSTATUS (WINAPI *NtWriteFileGatherFn)(
  IN    HANDLE                  aFileHandle,
  IN    HANDLE                  aEvent,
  IN    PIO_APC_ROUTINE         aApc,
  IN    PVOID                   aApcCtx,
  OUT   PIO_STATUS_BLOCK        aIoStatus,
  IN    FILE_SEGMENT_ELEMENT*   aSegments,
  IN    ULONG                   aLength,
  IN    PLARGE_INTEGER          aOffset,
  IN    PULONG                  aKey
);

/*************************** Auxiliary Declarations ***************************/

/**
 * RAII class for timing the duration of an I/O call and reporting the result
 * to the IOInterposeObserver API.
 */
class WinIOAutoObservation : public IOInterposeObserver::Observation
{
public:
  WinIOAutoObservation(IOInterposeObserver::Operation aOp,
                       const char* aReference, HANDLE aFileHandle)
    : mFileHandle(aFileHandle),
      mShouldObserve(IOInterposer::IsObservedOperation(aOp) &&
                     !IsDebugFile(reinterpret_cast<intptr_t>(aFileHandle)))
  {
    if (mShouldObserve) {
      mOperation = aOp;
      mReference = aReference;
      mStart = TimeStamp::Now();
    }
  }

  ~WinIOAutoObservation()
  {
    if (mShouldObserve) {
      mEnd = TimeStamp::Now();
      // Report this observation
      IOInterposer::Report(*this);
    }
  }

private:
  HANDLE              mFileHandle;
  bool                mShouldObserve;
};

/*************************** IO Interposing Methods ***************************/

// Function pointers to original functions
static NtWriteFileFn          gOriginalNtWriteFile;
static NtWriteFileGatherFn    gOriginalNtWriteFileGather;

// Interposed NtWriteFile function
static NTSTATUS WINAPI InterposedNtWriteFile(
  HANDLE                        aFileHandle,
  HANDLE                        aEvent,
  PIO_APC_ROUTINE               aApc,
  PVOID                         aApcCtx,
  PIO_STATUS_BLOCK              aIoStatus,
  PVOID                         aBuffer,
  ULONG                         aLength,
  PLARGE_INTEGER                aOffset,
  PULONG                        aKey)
{
  // Report IO
  const char* ref = "NtWriteFile";
  WinIOAutoObservation timer(IOInterposeObserver::OpWrite, ref, aFileHandle);

  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtWriteFile);

  // Execute original function
  return gOriginalNtWriteFile(
    aFileHandle,
    aEvent,
    aApc,
    aApcCtx,
    aIoStatus,
    aBuffer,
    aLength,
    aOffset,
    aKey
  );
}

// Interposed NtWriteFileGather function
static NTSTATUS WINAPI InterposedNtWriteFileGather(
  HANDLE                        aFileHandle,
  HANDLE                        aEvent,
  PIO_APC_ROUTINE               aApc,
  PVOID                         aApcCtx,
  PIO_STATUS_BLOCK              aIoStatus,
  FILE_SEGMENT_ELEMENT*         aSegments,
  ULONG                         aLength,
  PLARGE_INTEGER                aOffset,
  PULONG                        aKey)
{
  // Report IO
  const char* ref = "NtWriteFileGather";
  WinIOAutoObservation timer(IOInterposeObserver::OpWrite, ref, aFileHandle);

  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtWriteFileGather);

  // Execute original function
  return gOriginalNtWriteFileGather(
    aFileHandle,
    aEvent,
    aApc,
    aApcCtx,
    aIoStatus,
    aSegments,
    aLength,
    aOffset,
    aKey
  );
}

} // anonymous namespace

/******************************** IO Poisoning ********************************/

// Windows DLL interceptor
static WindowsDllInterceptor sNtDllInterceptor;

namespace mozilla {

void InitPoisonIOInterposer() {
  // Don't poison twice... as this function may only be invoked on the main
  // thread when no other threads are running, it safe to allow multiple calls
  // to InitPoisonIOInterposer() without complaining (ie. failing assertions).
  if (sIOPoisoned) {
    return;
  }
  sIOPoisoned = true;

  // Stdout and Stderr are OK.
  MozillaRegisterDebugFD(1);
  MozillaRegisterDebugFD(2);

  // Initialize dll interceptor and add hooks
  sNtDllInterceptor.Init("ntdll.dll");
  sNtDllInterceptor.AddHook(
    "NtWriteFile",
    reinterpret_cast<intptr_t>(InterposedNtWriteFile),
    reinterpret_cast<void**>(&gOriginalNtWriteFile)
  );
  sNtDllInterceptor.AddHook(
    "NtWriteFileGather",
    reinterpret_cast<intptr_t>(InterposedNtWriteFileGather),
    reinterpret_cast<void**>(&gOriginalNtWriteFileGather)
  );
}

void ClearPoisonIOInterposer() {
  MOZ_ASSERT(false);
  if (sIOPoisoned) {
    // Destroy the DLL interceptor
    sIOPoisoned = false;
    sNtDllInterceptor = WindowsDllInterceptor();
  }
}

} // namespace mozilla
