/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExceptionHandler_h__
#define nsExceptionHandler_h__

#include "nscore.h"
#include "nsDataHashtable.h"
#include "nsXPCOM.h"
#include "nsStringGlue.h"

#include "nsIFile.h"

#if defined(XP_WIN32)
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(XP_MACOSX)
#include <mach/mach.h>
#endif

namespace CrashReporter {
nsresult SetExceptionHandler(nsIFile* aXREDirectory, bool force=false);
nsresult UnsetExceptionHandler();
bool     GetEnabled();
bool     GetServerURL(nsACString& aServerURL);
nsresult SetServerURL(const nsACString& aServerURL);
bool     GetMinidumpPath(nsAString& aPath);
nsresult SetMinidumpPath(const nsAString& aPath);


// AnnotateCrashReport and AppendAppNotesToCrashReport may be called from any
// thread in a chrome process, but may only be called from the main thread in
// a content process.
nsresult AnnotateCrashReport(const nsACString& key, const nsACString& data);
nsresult AppendAppNotesToCrashReport(const nsACString& data);

nsresult SetRestartArgs(int argc, char** argv);
nsresult SetupExtraData(nsIFile* aAppDataDirectory,
                        const nsACString& aBuildID);

// Registers an additional memory region to be included in the minidump
nsresult RegisterAppMemory(void* ptr, size_t length);
nsresult UnregisterAppMemory(void* ptr);

// Functions for working with minidumps and .extras
typedef nsDataHashtable<nsCStringHashKey, nsCString> AnnotationTable;

bool GetMinidumpForID(const nsAString& id, nsIFile** minidump);
bool GetIDFromMinidump(nsIFile* minidump, nsAString& id);
bool GetExtraFileForID(const nsAString& id, nsIFile** extraFile);
bool GetExtraFileForMinidump(nsIFile* minidump, nsIFile** extraFile);
bool AppendExtraData(const nsAString& id, const AnnotationTable& data);
bool AppendExtraData(nsIFile* extraFile, const AnnotationTable& data);

#ifdef XP_WIN32
  nsresult WriteMinidumpForException(EXCEPTION_POINTERS* aExceptionInfo);
#endif
#ifdef XP_MACOSX
  nsresult AppendObjCExceptionInfoToAppNotes(void *inException);
#endif
nsresult GetSubmitReports(bool* aSubmitReport);
nsresult SetSubmitReports(bool aSubmitReport);

// Out-of-process crash reporter API.

// Initializes out-of-process crash reporting. This method must be called
// before the platform-specifi notificationpipe APIs are called.
void OOPInit();

// Return true if a dump was found for |childPid|, and return the
// path in |dump|.  The caller owns the last reference to |dump| if it
// is non-NULL. The sequence parameter will be filled with an ordinal
// indicating which remote process crashed first.
bool TakeMinidumpForChild(PRUint32 childPid,
                          nsIFile** dump NS_OUTPARAM,
                          PRUint32* aSequence = NULL);

#if defined(XP_WIN)
typedef HANDLE ProcessHandle;
typedef DWORD ThreadId;
#elif defined(XP_MACOSX)
typedef task_t ProcessHandle;
typedef mach_port_t ThreadId;
#else
typedef int ProcessHandle;
typedef int ThreadId;
#endif

// Return the current thread's ID.
//
// XXX: this is a somewhat out-of-place interface to expose through
// crashreporter, but it takes significant work to call sys_gettid()
// correctly on Linux and breakpad has already jumped through those
// hoops for us.
ThreadId CurrentThreadId();

// Create new minidumps that are snapshots of the state of this parent
// process and |childPid|.  Return true on success along with the
// minidumps and a new UUID that can be used to correlate the dumps.
//
// If this function fails, it's the caller's responsibility to clean
// up |childDump| and |parentDump|.  Either or both can be created and
// returned non-null on failure.
bool CreatePairedMinidumps(ProcessHandle childPid,
                           ThreadId childBlamedThread,
                           nsAString* pairGUID NS_OUTPARAM,
                           nsIFile** childDump NS_OUTPARAM,
                           nsIFile** parentDump NS_OUTPARAM);

#  if defined(XP_WIN32) || defined(XP_MACOSX)
// Parent-side API for children
const char* GetChildNotificationPipe();

#ifdef MOZ_CRASHREPORTER_INJECTOR
// Inject a crash report client into an arbitrary process, and inform the
// callback object when it crashes. Parent process only.

class InjectorCrashCallback
{
public:
  InjectorCrashCallback() { }

  /**
   * Inform the callback of a crash. The client code should call
   * TakeMinidumpForChild to remove it from the PID mapping table.
   *
   * The callback will not be fired if the client has already called
   * TakeMinidumpForChild for this process ID.
   */
  virtual void OnCrash(DWORD processID) = 0;
};

// This method implies OOPInit
void InjectCrashReporterIntoProcess(DWORD processID, InjectorCrashCallback* cb);
void UnregisterInjectorCallback(DWORD processID);
#endif

// Child-side API
bool SetRemoteExceptionHandler(const nsACString& crashPipe);

#  elif defined(XP_LINUX)
// Parent-side API for children

// Set the outparams for crash reporter server's fd (|childCrashFd|)
// and the magic fd number it should be remapped to
// (|childCrashRemapFd|) before exec() in the child process.
// |SetRemoteExceptionHandler()| in the child process expects to find
// the server at |childCrashRemapFd|.  Return true iff successful.
//
// If crash reporting is disabled, both outparams will be set to -1
// and |true| will be returned.
bool CreateNotificationPipeForChild(int* childCrashFd, int* childCrashRemapFd);

// Child-side API
bool SetRemoteExceptionHandler();

#endif  // XP_WIN32

bool UnsetRemoteExceptionHandler();

#if defined(__ANDROID__)
// Android builds use a custom library loader, so /proc/<pid>/maps
// will just show anonymous mappings for all the non-system
// shared libraries. This API is to work around that by providing
// info about the shared libraries that are mapped into these anonymous
// mappings.
void AddLibraryMapping(const char* library_name,
                       const char* file_id,
                       uintptr_t   start_address,
                       size_t      mapping_length,
                       size_t      file_offset);

void AddLibraryMappingForChild(PRUint32    childPid,
                               const char* library_name,
                               const char* file_id,
                               uintptr_t   start_address,
                               size_t      mapping_length,
                               size_t      file_offset);
void RemoveLibraryMappingsForChild(PRUint32 childPid);
#endif
}

#endif /* nsExceptionHandler_h__ */
