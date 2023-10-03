/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/FileUtilsWin.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Mutex.h"
#include "mozilla/NativeNt.h"
#include "mozilla/SmallArrayLRUCache.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"
#include "nsWindowsDllInterceptor.h"

#ifdef MOZ_REPLACE_MALLOC
#  include "replace_malloc_bridge.h"
#endif

namespace {

// Keep track of poisoned state. Notice that there is no reason to lock access
// to this variable as it's only changed in InitPoisonIOInterposer and
// ClearPoisonIOInterposer which may only be called on the main-thread when no
// other threads are running.
static bool sIOPoisoned = false;

/************************ Internal NT API Declarations ************************/

/*
 * Function pointer declaration for internal NT routine to create/open files.
 * For documentation on the NtCreateFile routine, see MSDN.
 */
typedef NTSTATUS(NTAPI* NtCreateFileFn)(
    PHANDLE aFileHandle, ACCESS_MASK aDesiredAccess,
    POBJECT_ATTRIBUTES aObjectAttributes, PIO_STATUS_BLOCK aIoStatusBlock,
    PLARGE_INTEGER aAllocationSize, ULONG aFileAttributes, ULONG aShareAccess,
    ULONG aCreateDisposition, ULONG aCreateOptions, PVOID aEaBuffer,
    ULONG aEaLength);

/**
 * Function pointer declaration for internal NT routine to read data from file.
 * For documentation on the NtReadFile routine, see ZwReadFile on MSDN.
 */
typedef NTSTATUS(NTAPI* NtReadFileFn)(HANDLE aFileHandle, HANDLE aEvent,
                                      PIO_APC_ROUTINE aApc, PVOID aApcCtx,
                                      PIO_STATUS_BLOCK aIoStatus, PVOID aBuffer,
                                      ULONG aLength, PLARGE_INTEGER aOffset,
                                      PULONG aKey);

/**
 * Function pointer declaration for internal NT routine to read data from file.
 * No documentation exists, see wine sources for details.
 */
typedef NTSTATUS(NTAPI* NtReadFileScatterFn)(
    HANDLE aFileHandle, HANDLE aEvent, PIO_APC_ROUTINE aApc, PVOID aApcCtx,
    PIO_STATUS_BLOCK aIoStatus, FILE_SEGMENT_ELEMENT* aSegments, ULONG aLength,
    PLARGE_INTEGER aOffset, PULONG aKey);

/**
 * Function pointer declaration for internal NT routine to write data to file.
 * For documentation on the NtWriteFile routine, see ZwWriteFile on MSDN.
 */
typedef NTSTATUS(NTAPI* NtWriteFileFn)(HANDLE aFileHandle, HANDLE aEvent,
                                       PIO_APC_ROUTINE aApc, PVOID aApcCtx,
                                       PIO_STATUS_BLOCK aIoStatus,
                                       PVOID aBuffer, ULONG aLength,
                                       PLARGE_INTEGER aOffset, PULONG aKey);

/**
 * Function pointer declaration for internal NT routine to write data to file.
 * No documentation exists, see wine sources for details.
 */
typedef NTSTATUS(NTAPI* NtWriteFileGatherFn)(
    HANDLE aFileHandle, HANDLE aEvent, PIO_APC_ROUTINE aApc, PVOID aApcCtx,
    PIO_STATUS_BLOCK aIoStatus, FILE_SEGMENT_ELEMENT* aSegments, ULONG aLength,
    PLARGE_INTEGER aOffset, PULONG aKey);

/**
 * Function pointer declaration for internal NT routine to flush to disk.
 * For documentation on the NtFlushBuffersFile routine, see ZwFlushBuffersFile
 * on MSDN.
 */
typedef NTSTATUS(NTAPI* NtFlushBuffersFileFn)(HANDLE aFileHandle,
                                              PIO_STATUS_BLOCK aIoStatusBlock);

typedef struct _FILE_NETWORK_OPEN_INFORMATION* PFILE_NETWORK_OPEN_INFORMATION;
/**
 * Function pointer delaration for internal NT routine to query file attributes.
 * (equivalent to stat)
 */
typedef NTSTATUS(NTAPI* NtQueryFullAttributesFileFn)(
    POBJECT_ATTRIBUTES aObjectAttributes,
    PFILE_NETWORK_OPEN_INFORMATION aFileInformation);

/*************************** Auxiliary Declarations ***************************/

// Cache of filenames associated with handles.
// `static` to be shared between all calls to `Filename()`.
// This assumes handles are not reused, at least within a windows of 32
// handles.
// Profiling showed that during startup, around half of `Filename()` calls are
// resolved with the first entry (best case), and 32 entries cover >95% of
// cases, reducing the average `Filename()` cost by 5-10x.
using HandleToFilenameCache = mozilla::SmallArrayLRUCache<HANDLE, nsString, 32>;
static mozilla::UniquePtr<HandleToFilenameCache> sHandleToFilenameCache;

/**
 * RAII class for timing the duration of an I/O call and reporting the result
 * to the mozilla::IOInterposeObserver API.
 */
class WinIOAutoObservation : public mozilla::IOInterposeObserver::Observation {
 public:
  WinIOAutoObservation(mozilla::IOInterposeObserver::Operation aOp,
                       HANDLE aFileHandle, const LARGE_INTEGER* aOffset)
      : mozilla::IOInterposeObserver::Observation(
            aOp, sReference,
            !mozilla::IsDebugFile(reinterpret_cast<intptr_t>(aFileHandle))),
        mFileHandle(aFileHandle),
        mFileHandleType(GetFileType(aFileHandle)),
        mHasQueriedFilename(false) {
    if (mShouldReport) {
      mOffset.QuadPart = aOffset ? aOffset->QuadPart : 0;
    }
  }

  WinIOAutoObservation(mozilla::IOInterposeObserver::Operation aOp,
                       nsAString& aFilename)
      : mozilla::IOInterposeObserver::Observation(aOp, sReference),
        mFileHandle(nullptr),
        mFileHandleType(FILE_TYPE_UNKNOWN),
        mHasQueriedFilename(false) {
    if (mShouldReport) {
      nsAutoString dosPath;
      if (mozilla::NtPathToDosPath(aFilename, dosPath)) {
        mFilename = dosPath;
      } else {
        // If we can't get a dosPath, what we have is better than nothing.
        mFilename = aFilename;
      }
      mHasQueriedFilename = true;
      mOffset.QuadPart = 0;
    }
  }

  void SetHandle(HANDLE aFileHandle) {
    mFileHandle = aFileHandle;
    if (aFileHandle) {
      // Note: `GetFileType()` is fast enough that we don't need to cache it.
      mFileHandleType = GetFileType(aFileHandle);

      if (mHasQueriedFilename) {
        // `mHasQueriedFilename` indicates we already have a filename, add it to
        // the cache with the now-known handle.
        sHandleToFilenameCache->Add(aFileHandle, mFilename);
      }
    }
  }

  const char* FileType() const override;

  void Filename(nsAString& aFilename) override;

  ~WinIOAutoObservation() { Report(); }

 private:
  HANDLE mFileHandle;
  DWORD mFileHandleType;
  LARGE_INTEGER mOffset;
  bool mHasQueriedFilename;
  nsString mFilename;
  static const char* sReference;
};

const char* WinIOAutoObservation::sReference = "PoisonIOInterposer";

// Get filename for this observation
void WinIOAutoObservation::Filename(nsAString& aFilename) {
  // If mHasQueriedFilename is true, then filename is already stored in
  // mFilename
  if (mHasQueriedFilename) {
    aFilename = mFilename;
    return;
  }

  if (mFileHandle) {
    mFilename = sHandleToFilenameCache->FetchOrAdd(mFileHandle, [&]() {
      nsString filename;
      if (!mozilla::HandleToFilename(mFileHandle, mOffset, filename)) {
        // HandleToFilename could fail (return false) but still have added
        // something to `filename`, so it should be cleared in this case.
        filename.Truncate();
      }
      return filename;
    });
  }
  mHasQueriedFilename = true;

  aFilename = mFilename;
}

const char* WinIOAutoObservation::FileType() const {
  if (mFileHandle) {
    switch (mFileHandleType) {
      case FILE_TYPE_CHAR:
        return "Char";
      case FILE_TYPE_DISK:
        return "File";
      case FILE_TYPE_PIPE:
        return "Pipe";
      case FILE_TYPE_REMOTE:
        return "Remote";
      case FILE_TYPE_UNKNOWN:
      default:
        break;
    }
  }
  // Fallback to base class default implementation.
  return mozilla::IOInterposeObserver::Observation::FileType();
}

/*************************** IO Interposing Methods ***************************/

// Function pointers to original functions
static mozilla::WindowsDllInterceptor::FuncHookType<NtCreateFileFn>
    gOriginalNtCreateFile;
static mozilla::WindowsDllInterceptor::FuncHookType<NtReadFileFn>
    gOriginalNtReadFile;
static mozilla::WindowsDllInterceptor::FuncHookType<NtReadFileScatterFn>
    gOriginalNtReadFileScatter;
static mozilla::WindowsDllInterceptor::FuncHookType<NtWriteFileFn>
    gOriginalNtWriteFile;
static mozilla::WindowsDllInterceptor::FuncHookType<NtWriteFileGatherFn>
    gOriginalNtWriteFileGather;
static mozilla::WindowsDllInterceptor::FuncHookType<NtFlushBuffersFileFn>
    gOriginalNtFlushBuffersFile;
static mozilla::WindowsDllInterceptor::FuncHookType<NtQueryFullAttributesFileFn>
    gOriginalNtQueryFullAttributesFile;

static NTSTATUS NTAPI InterposedNtCreateFile(
    PHANDLE aFileHandle, ACCESS_MASK aDesiredAccess,
    POBJECT_ATTRIBUTES aObjectAttributes, PIO_STATUS_BLOCK aIoStatusBlock,
    PLARGE_INTEGER aAllocationSize, ULONG aFileAttributes, ULONG aShareAccess,
    ULONG aCreateDisposition, ULONG aCreateOptions, PVOID aEaBuffer,
    ULONG aEaLength) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtCreateFile);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtCreateFile(
        aFileHandle, aDesiredAccess, aObjectAttributes, aIoStatusBlock,
        aAllocationSize, aFileAttributes, aShareAccess, aCreateDisposition,
        aCreateOptions, aEaBuffer, aEaLength);
  }

  // Report IO
  const wchar_t* buf =
      aObjectAttributes ? aObjectAttributes->ObjectName->Buffer : L"";
  uint32_t len = aObjectAttributes
                     ? aObjectAttributes->ObjectName->Length / sizeof(WCHAR)
                     : 0;
  nsDependentSubstring filename(buf, len);
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpCreateOrOpen,
                             filename);

  // Execute original function
  NTSTATUS status = gOriginalNtCreateFile(
      aFileHandle, aDesiredAccess, aObjectAttributes, aIoStatusBlock,
      aAllocationSize, aFileAttributes, aShareAccess, aCreateDisposition,
      aCreateOptions, aEaBuffer, aEaLength);
  if (NT_SUCCESS(status) && aFileHandle) {
    timer.SetHandle(*aFileHandle);
  }
  return status;
}

static NTSTATUS NTAPI InterposedNtReadFile(HANDLE aFileHandle, HANDLE aEvent,
                                           PIO_APC_ROUTINE aApc, PVOID aApcCtx,
                                           PIO_STATUS_BLOCK aIoStatus,
                                           PVOID aBuffer, ULONG aLength,
                                           PLARGE_INTEGER aOffset,
                                           PULONG aKey) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtReadFile);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtReadFile(aFileHandle, aEvent, aApc, aApcCtx, aIoStatus,
                               aBuffer, aLength, aOffset, aKey);
  }

  // Report IO
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpRead, aFileHandle,
                             aOffset);

  // Execute original function
  return gOriginalNtReadFile(aFileHandle, aEvent, aApc, aApcCtx, aIoStatus,
                             aBuffer, aLength, aOffset, aKey);
}

static NTSTATUS NTAPI InterposedNtReadFileScatter(
    HANDLE aFileHandle, HANDLE aEvent, PIO_APC_ROUTINE aApc, PVOID aApcCtx,
    PIO_STATUS_BLOCK aIoStatus, FILE_SEGMENT_ELEMENT* aSegments, ULONG aLength,
    PLARGE_INTEGER aOffset, PULONG aKey) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtReadFileScatter);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtReadFileScatter(aFileHandle, aEvent, aApc, aApcCtx,
                                      aIoStatus, aSegments, aLength, aOffset,
                                      aKey);
  }

  // Report IO
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpRead, aFileHandle,
                             aOffset);

  // Execute original function
  return gOriginalNtReadFileScatter(aFileHandle, aEvent, aApc, aApcCtx,
                                    aIoStatus, aSegments, aLength, aOffset,
                                    aKey);
}

// Interposed NtWriteFile function
static NTSTATUS NTAPI InterposedNtWriteFile(HANDLE aFileHandle, HANDLE aEvent,
                                            PIO_APC_ROUTINE aApc, PVOID aApcCtx,
                                            PIO_STATUS_BLOCK aIoStatus,
                                            PVOID aBuffer, ULONG aLength,
                                            PLARGE_INTEGER aOffset,
                                            PULONG aKey) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtWriteFile);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtWriteFile(aFileHandle, aEvent, aApc, aApcCtx, aIoStatus,
                                aBuffer, aLength, aOffset, aKey);
  }

  // Report IO
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpWrite, aFileHandle,
                             aOffset);

  // Execute original function
  return gOriginalNtWriteFile(aFileHandle, aEvent, aApc, aApcCtx, aIoStatus,
                              aBuffer, aLength, aOffset, aKey);
}

// Interposed NtWriteFileGather function
static NTSTATUS NTAPI InterposedNtWriteFileGather(
    HANDLE aFileHandle, HANDLE aEvent, PIO_APC_ROUTINE aApc, PVOID aApcCtx,
    PIO_STATUS_BLOCK aIoStatus, FILE_SEGMENT_ELEMENT* aSegments, ULONG aLength,
    PLARGE_INTEGER aOffset, PULONG aKey) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtWriteFileGather);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtWriteFileGather(aFileHandle, aEvent, aApc, aApcCtx,
                                      aIoStatus, aSegments, aLength, aOffset,
                                      aKey);
  }

  // Report IO
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpWrite, aFileHandle,
                             aOffset);

  // Execute original function
  return gOriginalNtWriteFileGather(aFileHandle, aEvent, aApc, aApcCtx,
                                    aIoStatus, aSegments, aLength, aOffset,
                                    aKey);
}

static NTSTATUS NTAPI InterposedNtFlushBuffersFile(
    HANDLE aFileHandle, PIO_STATUS_BLOCK aIoStatusBlock) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtFlushBuffersFile);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtFlushBuffersFile(aFileHandle, aIoStatusBlock);
  }

  // Report IO
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpFSync, aFileHandle,
                             nullptr);

  // Execute original function
  return gOriginalNtFlushBuffersFile(aFileHandle, aIoStatusBlock);
}

static NTSTATUS NTAPI InterposedNtQueryFullAttributesFile(
    POBJECT_ATTRIBUTES aObjectAttributes,
    PFILE_NETWORK_OPEN_INFORMATION aFileInformation) {
  // Something is badly wrong if this function is undefined
  MOZ_ASSERT(gOriginalNtQueryFullAttributesFile);

  if (!mozilla::nt::RtlGetThreadLocalStoragePointer()) {
    return gOriginalNtQueryFullAttributesFile(aObjectAttributes,
                                              aFileInformation);
  }

  // Report IO
  const wchar_t* buf =
      aObjectAttributes ? aObjectAttributes->ObjectName->Buffer : L"";
  uint32_t len = aObjectAttributes
                     ? aObjectAttributes->ObjectName->Length / sizeof(WCHAR)
                     : 0;
  nsDependentSubstring filename(buf, len);
  WinIOAutoObservation timer(mozilla::IOInterposeObserver::OpStat, filename);

  // Execute original function
  return gOriginalNtQueryFullAttributesFile(aObjectAttributes,
                                            aFileInformation);
}

}  // namespace

/******************************** IO Poisoning ********************************/

// Windows DLL interceptor
static mozilla::WindowsDllInterceptor sNtDllInterceptor;

namespace mozilla {

void InitPoisonIOInterposer() {
  // Currently we hook the functions not early enough to precede third-party
  // injections.  Until we implement a compatible way e.g. applying a hook
  // in the parent process (bug 1646804), we skip interposing functions under
  // the known condition(s).

  // Bug 1679741: Kingsoft Internet Security calls NtReadFile in their thread
  // simultaneously when we're applying a hook on NtReadFile.
  // Bug 1705042: Symantec applies its own hook on NtReadFile, and ends up
  // overwriting part of ours in an incompatible way.
  if (::GetModuleHandleW(L"kwsui64.dll") || ::GetModuleHandleW(L"ffm64.dll")) {
    return;
  }

  // Don't poison twice... as this function may only be invoked on the main
  // thread when no other threads are running, it safe to allow multiple calls
  // to InitPoisonIOInterposer() without complaining (ie. failing assertions).
  if (sIOPoisoned) {
    return;
  }
  sIOPoisoned = true;

  MOZ_RELEASE_ASSERT(!sHandleToFilenameCache);
  sHandleToFilenameCache = mozilla::MakeUnique<HandleToFilenameCache>();
  mozilla::RunOnShutdown([]() {
    // The interposer may still be active after the final shutdown phase
    // (especially since ClearPoisonIOInterposer() is never called, see bug
    // 1647107), so we cannot just reset the pointer. Instead we put the cache
    // in shutdown mode, to clear its memory and stop caching operations.
    sHandleToFilenameCache->Shutdown();
  });

  // Stdout and Stderr are OK.
  MozillaRegisterDebugFD(1);
  if (::GetStdHandle(STD_OUTPUT_HANDLE) != ::GetStdHandle(STD_ERROR_HANDLE)) {
    MozillaRegisterDebugFD(2);
  }

#ifdef MOZ_REPLACE_MALLOC
  // The contract with InitDebugFd is that the given registry can be used
  // at any moment, so the instance needs to persist longer than the scope
  // of this functions.
  static DebugFdRegistry registry;
  ReplaceMalloc::InitDebugFd(registry);
#endif

  // Initialize dll interceptor and add hooks
  sNtDllInterceptor.Init("ntdll.dll");
  gOriginalNtCreateFile.Set(sNtDllInterceptor, "NtCreateFile",
                            &InterposedNtCreateFile);
  gOriginalNtReadFile.Set(sNtDllInterceptor, "NtReadFile",
                          &InterposedNtReadFile);
  gOriginalNtReadFileScatter.Set(sNtDllInterceptor, "NtReadFileScatter",
                                 &InterposedNtReadFileScatter);
  gOriginalNtWriteFile.Set(sNtDllInterceptor, "NtWriteFile",
                           &InterposedNtWriteFile);
  gOriginalNtWriteFileGather.Set(sNtDllInterceptor, "NtWriteFileGather",
                                 &InterposedNtWriteFileGather);
  gOriginalNtFlushBuffersFile.Set(sNtDllInterceptor, "NtFlushBuffersFile",
                                  &InterposedNtFlushBuffersFile);
  gOriginalNtQueryFullAttributesFile.Set(sNtDllInterceptor,
                                         "NtQueryFullAttributesFile",
                                         &InterposedNtQueryFullAttributesFile);
}

void ClearPoisonIOInterposer() {
  MOZ_ASSERT(false, "Never called! See bug 1647107");
  if (sIOPoisoned) {
    // Destroy the DLL interceptor
    sIOPoisoned = false;
    sNtDllInterceptor.Clear();
    sHandleToFilenameCache->Clear();
  }
}

}  // namespace mozilla
