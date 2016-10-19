/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExceptionHandler_h__
#define nsExceptionHandler_h__

#include "mozilla/Assertions.h"

#include <stddef.h>
#include <stdint.h>
#include "nsError.h"
#include "nsStringGlue.h"

#if defined(XP_WIN32)
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(XP_MACOSX)
#include <mach/mach.h>
#endif

#if defined(XP_LINUX)
#include <signal.h>
#endif

class nsIFile;
template<class KeyClass, class DataType> class nsDataHashtable;
class nsCStringHashKey;

namespace CrashReporter {
nsresult SetExceptionHandler(nsIFile* aXREDirectory, bool force=false);
nsresult UnsetExceptionHandler();

/**
 * Tell the crash reporter to recalculate where crash events files should go.
 * SetCrashEventsDir is used before XPCOM is initialized from the startup
 * code.
 *
 * UpdateCrashEventsDir uses the directory service to re-set the
 * crash event directory based on the current profile.
 *
 * 1. If environment variable is present, use it. We don't expect
 *    the environment variable except for tests and other atypical setups.
 * 2. <profile>/crashes/events
 * 3. <UAppData>/Crash Reports/events
 */
void SetUserAppDataDirectory(nsIFile* aDir);
void SetProfileDirectory(nsIFile* aDir);
void UpdateCrashEventsDir();
void SetMemoryReportFile(nsIFile* aFile);
nsresult GetDefaultMemoryReportFile(nsIFile** aFile);
void SetTelemetrySessionId(const nsACString& id);

/**
 * Get the path where crash event files should be written.
 */
bool     GetCrashEventsDir(nsAString& aPath);

bool     GetEnabled();
bool     GetServerURL(nsACString& aServerURL);
nsresult SetServerURL(const nsACString& aServerURL);
bool     GetMinidumpPath(nsAString& aPath);
nsresult SetMinidumpPath(const nsAString& aPath);


// AnnotateCrashReport, RemoveCrashReportAnnotation and
// AppendAppNotesToCrashReport may be called from any thread in a chrome
// process, but may only be called from the main thread in a content process.
nsresult AnnotateCrashReport(const nsACString& key, const nsACString& data);
nsresult RemoveCrashReportAnnotation(const nsACString& key);
nsresult AppendAppNotesToCrashReport(const nsACString& data);

void AnnotateOOMAllocationSize(size_t size);
void AnnotateTexturesSize(size_t size);
void AnnotatePendingIPC(size_t aNumOfPendingIPC,
                        uint32_t aTopPendingIPCCount,
                        const char* aTopPendingIPCName,
                        uint32_t aTopPendingIPCType);
nsresult SetGarbageCollecting(bool collecting);
void SetEventloopNestingLevel(uint32_t level);

nsresult SetRestartArgs(int argc, char** argv);
nsresult SetupExtraData(nsIFile* aAppDataDirectory,
                        const nsACString& aBuildID);
bool GetLastRunCrashID(nsAString& id);

// Registers an additional memory region to be included in the minidump
nsresult RegisterAppMemory(void* ptr, size_t length);
nsresult UnregisterAppMemory(void* ptr);

// Functions for working with minidumps and .extras
typedef nsDataHashtable<nsCStringHashKey, nsCString> AnnotationTable;

void DeleteMinidumpFilesForID(const nsAString& id);
bool GetMinidumpForID(const nsAString& id, nsIFile** minidump);
bool GetIDFromMinidump(nsIFile* minidump, nsAString& id);
bool GetExtraFileForID(const nsAString& id, nsIFile** extraFile);
bool GetExtraFileForMinidump(nsIFile* minidump, nsIFile** extraFile);
bool AppendExtraData(const nsAString& id, const AnnotationTable& data);
bool AppendExtraData(nsIFile* extraFile, const AnnotationTable& data);
void RunMinidumpAnalyzer(const nsAString& id);

/*
 * Renames the stand alone dump file aDumpFile to:
 *  |aOwnerDumpFile-aDumpFileProcessType.dmp|
 * and moves it into the same directory as aOwnerDumpFile. Does not
 * modify aOwnerDumpFile in any way.
 *
 * @param aDumpFile - the dump file to associate with aOwnerDumpFile.
 * @param aOwnerDumpFile - the new owner of aDumpFile.
 * @param aDumpFileProcessType - process name associated with aDumpFile.
 */
void RenameAdditionalHangMinidump(nsIFile* aDumpFile, const nsIFile* aOwnerDumpFile,
                                  const nsACString& aDumpFileProcessType);

#ifdef XP_WIN32
  nsresult WriteMinidumpForException(EXCEPTION_POINTERS* aExceptionInfo);
#endif
#ifdef XP_LINUX
  bool WriteMinidumpForSigInfo(int signo, siginfo_t* info, void* uc);
#endif
#ifdef XP_MACOSX
  nsresult AppendObjCExceptionInfoToAppNotes(void *inException);
#endif
nsresult GetSubmitReports(bool* aSubmitReport);
nsresult SetSubmitReports(bool aSubmitReport);

// Out-of-process crash reporter API.

// Initializes out-of-process crash reporting. This method must be called
// before the platform-specific notification pipe APIs are called. If called
// from off the main thread, this method will synchronously proxy to the main
// thread.
void OOPInit();

/*
 * Takes a minidump for the current process and returns the dump file.
 * Callers are responsible for managing the resulting file.
 *
 * @param aResult - file pointer that holds the resulting minidump.
 * @param aMoveToPending - if true move the report to the report
 *   pending directory.
 * @returns boolean indicating success or failure.
 */
bool TakeMinidump(nsIFile** aResult, bool aMoveToPending = false);

// Return true if a dump was found for |childPid|, and return the
// path in |dump|.  The caller owns the last reference to |dump| if it
// is non-nullptr. The sequence parameter will be filled with an ordinal
// indicating which remote process crashed first.
bool TakeMinidumpForChild(uint32_t childPid,
                          nsIFile** dump,
                          uint32_t* aSequence = nullptr);

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

/*
 * Take a minidump of the target process and pair it with an incoming minidump
 * provided by the caller or a new minidump of the calling process and thread.
 * The caller will own both dumps after this call. If this function fails
 * it will attempt to delete any files that were created.
 *
 * The .extra information created will not include an 'additional_minidumps'
 * annotation.
 *
 * @param aTargetPid The target process for the minidump.
 * @param aTargetBlamedThread The target thread for the minidump.
 * @param aIncomingPairName The name to apply to the paired dump the caller
 *   passes in.
 * @param aIncomingDumpToPair Existing dump to pair with the new dump. if this
 *   is null, TakeMinidumpAndPair will take a new minidump of the calling
 *   process and thread and use it in aIncomingDumpToPairs place.
 * @param aTargetDumpOut The target minidump file paired up with
 *   aIncomingDumpToPair.
 * @return bool indicating success or failure
 */
bool CreateMinidumpsAndPair(ProcessHandle aTargetPid,
                            ThreadId aTargetBlamedThread,
                            const nsACString& aIncomingPairName,
                            nsIFile* aIncomingDumpToPair,
                            nsIFile** aTargetDumpOut);

// Create an additional minidump for a child of a process which already has
// a minidump (|parentMinidump|).
// The resulting dump will get the id of the parent and use the |name| as
// an extension.
bool CreateAdditionalChildMinidump(ProcessHandle childPid,
                                   ThreadId childBlamedThread,
                                   nsIFile* parentMinidump,
                                   const nsACString& name);

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
void InitChildProcessTmpDir();

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

#if defined(MOZ_WIDGET_ANDROID)
// Android creates child process as services so we must explicitly set
// the handle for the pipe since it can't get remapped to a default value.
void SetNotificationPipeForChild(int childCrashFd);

// Android builds use a custom library loader, so /proc/<pid>/maps
// will just show anonymous mappings for all the non-system
// shared libraries. This API is to work around that by providing
// info about the shared libraries that are mapped into these anonymous
// mappings.
void AddLibraryMapping(const char* library_name,
                       uintptr_t   start_address,
                       size_t      mapping_length,
                       size_t      file_offset);

#endif
} // namespace CrashReporter

#endif /* nsExceptionHandler_h__ */
