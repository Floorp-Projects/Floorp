/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header has two implementations, the real one in nsExceptionHandler.cpp
// and a dummy in nsDummyExceptionHandler.cpp. The latter is used in builds
// configured with --disable-crashreporter. If you add or remove a function
// from this header you must update both implementations otherwise you'll break
// builds that disable the crash reporter.

#ifndef nsExceptionHandler_h__
#define nsExceptionHandler_h__

#include "mozilla/Assertions.h"
#include "mozilla/EnumeratedArray.h"

#include "CrashAnnotations.h"

#include <stddef.h>
#include <stdint.h>
#include "nsError.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "prio.h"

#if defined(XP_WIN)
#  ifdef WIN32_LEAN_AND_MEAN
#    undef WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#if defined(XP_MACOSX)
#  include <mach/mach.h>
#endif

#if defined(XP_LINUX)
#  include <signal.h>
#endif

class nsIFile;

namespace CrashReporter {

/**
 * Returns true if the crash reporter is using the dummy implementation.
 */
static inline bool IsDummy() {
#ifdef MOZ_CRASHREPORTER
  return false;
#else
  return true;
#endif
}

nsresult SetExceptionHandler(nsIFile* aXREDirectory, bool force = false);
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

/**
 * Get the path where crash event files should be written.
 */
bool GetCrashEventsDir(nsAString& aPath);

bool GetEnabled();
bool GetServerURL(nsACString& aServerURL);
nsresult SetServerURL(const nsACString& aServerURL);
bool GetMinidumpPath(nsAString& aPath);
nsresult SetMinidumpPath(const nsAString& aPath);

// These functions are thread safe and can be called in both the parent and
// child processes. Annotations added in the main process will be included in
// child process crashes too unless the child process sets its own annotations.
// If it does the child-provided annotation overrides the one set in the parent.
nsresult AnnotateCrashReport(Annotation key, bool data);
nsresult AnnotateCrashReport(Annotation key, int data);
nsresult AnnotateCrashReport(Annotation key, unsigned int data);
nsresult AnnotateCrashReport(Annotation key, const nsACString& data);
nsresult RemoveCrashReportAnnotation(Annotation key);
nsresult AppendAppNotesToCrashReport(const nsACString& data);

// RAII class for setting a crash annotation during a limited scope of time.
// Will reset the named annotation to its previous value when destroyed.
//
// This type's behavior is identical to that of AnnotateCrashReport().
class MOZ_RAII AutoAnnotateCrashReport final {
 public:
  AutoAnnotateCrashReport(Annotation key, bool data);
  AutoAnnotateCrashReport(Annotation key, int data);
  AutoAnnotateCrashReport(Annotation key, unsigned int data);
  AutoAnnotateCrashReport(Annotation key, const nsACString& data);
  ~AutoAnnotateCrashReport();

#ifdef MOZ_CRASHREPORTER
 private:
  Annotation mKey;
  nsCString mPrevious;
#endif
};

void AnnotateOOMAllocationSize(size_t size);
void AnnotateTexturesSize(size_t size);
nsresult SetGarbageCollecting(bool collecting);
void SetEventloopNestingLevel(uint32_t level);
void SetMinidumpAnalysisAllThreads();
void ClearInactiveStateStart();
void SetInactiveStateStart();

nsresult SetRestartArgs(int argc, char** argv);
nsresult SetupExtraData(nsIFile* aAppDataDirectory, const nsACString& aBuildID);
// Registers an additional memory region to be included in the minidump
nsresult RegisterAppMemory(void* ptr, size_t length);
nsresult UnregisterAppMemory(void* ptr);

// Include heap regions of the crash context.
void SetIncludeContextHeap(bool aValue);

void GetAnnotation(uint32_t childPid, Annotation annotation,
                   nsACString& outStr);

// Functions for working with minidumps and .extras
typedef mozilla::EnumeratedArray<Annotation, Annotation::Count, nsCString>
    AnnotationTable;
void DeleteMinidumpFilesForID(const nsAString& id);
bool GetMinidumpForID(const nsAString& id, nsIFile** minidump);
bool GetIDFromMinidump(nsIFile* minidump, nsAString& id);
bool GetExtraFileForID(const nsAString& id, nsIFile** extraFile);
bool GetExtraFileForMinidump(nsIFile* minidump, nsIFile** extraFile);
bool WriteExtraFile(const nsAString& id, const AnnotationTable& annotations);

/**
 * Copies the non-empty annotations in the source table to the destination
 * overwriting the corresponding entries.
 */
void MergeCrashAnnotations(AnnotationTable& aDst, const AnnotationTable& aSrc);

#ifdef XP_WIN
nsresult WriteMinidumpForException(EXCEPTION_POINTERS* aExceptionInfo);
#endif
#ifdef XP_LINUX
bool WriteMinidumpForSigInfo(int signo, siginfo_t* info, void* uc);
#endif
#ifdef XP_MACOSX
nsresult AppendObjCExceptionInfoToAppNotes(void* inException);
#endif
nsresult GetSubmitReports(bool* aSubmitReport);
nsresult SetSubmitReports(bool aSubmitReport);

// Out-of-process crash reporter API.

#ifdef XP_WIN
// This data is stored in the parent process, there is one copy for each child
// process. The mChildPid and mMinidumpFile fields are filled by the WER runtime
// exception module when the associated child process crashes.
struct WindowsErrorReportingData {
  // Points to the WerNotifyProc function.
  LPTHREAD_START_ROUTINE mWerNotifyProc;
  // PID of the child process that crashed.
  DWORD mChildPid;
  // Filename of the generated minidump; this is not a 0-terminated string
  char mMinidumpFile[40];
  // OOM allocation size for the crash (ignore if zero)
  size_t mOOMAllocationSize;
};
#endif  // XP_WIN

// Initializes out-of-process crash reporting. This method must be called
// before the platform-specific notification pipe APIs are called. If called
// from off the main thread, this method will synchronously proxy to the main
// thread.
void OOPInit();

// Return true if a dump was found for |childPid|, and return the
// path in |dump|.  The caller owns the last reference to |dump| if it
// is non-nullptr. The annotations for the crash will be stored in
// |aAnnotations|. The sequence parameter will be filled with an ordinal
// indicating which remote process crashed first.
bool TakeMinidumpForChild(uint32_t childPid, nsIFile** dump,
                          AnnotationTable& aAnnotations,
                          uint32_t* aSequence = nullptr);

/**
 * If a dump was found for |childPid| then write a minimal .extra file to
 * complete it and remove it from the list of pending crash dumps. It's
 * required to call this method after a non-main process crash if the crash
 * report could not be finalized via the CrashReporterHost (for example because
 * it wasn't instanced yet).
 *
 * @param aChildPid The pid of the crashed child process
 * @param aType The type of the crashed process
 * @param aDumpId A string that will be filled with the dump ID
 */
[[nodiscard]] bool FinalizeOrphanedMinidump(uint32_t aChildPid,
                                            GeckoProcessType aType,
                                            nsString* aDumpId = nullptr);

#if defined(XP_WIN)
typedef HANDLE ProcessHandle;
typedef DWORD ProcessId;
typedef DWORD ThreadId;
typedef HANDLE FileHandle;
const FileHandle kInvalidFileHandle = INVALID_HANDLE_VALUE;
#elif defined(XP_MACOSX)
typedef task_t ProcessHandle;
typedef pid_t ProcessId;
typedef mach_port_t ThreadId;
typedef int FileHandle;
const FileHandle kInvalidFileHandle = -1;
#else
typedef int ProcessHandle;
typedef pid_t ProcessId;
typedef int ThreadId;
typedef int FileHandle;
const FileHandle kInvalidFileHandle = -1;
#endif

#if !defined(XP_WIN)
FileHandle GetAnnotationTimeCrashFd();
#endif
void RegisterChildCrashAnnotationFileDescriptor(ProcessId aProcess,
                                                PRFileDesc* aFd);
void DeregisterChildCrashAnnotationFileDescriptor(ProcessId aProcess);

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
 * @param aTargetAnnotations The crash annotations of the target process.
 * @return bool indicating success or failure
 */
bool CreateMinidumpsAndPair(ProcessHandle aTargetPid,
                            ThreadId aTargetBlamedThread,
                            const nsACString& aIncomingPairName,
                            nsIFile* aIncomingDumpToPair,
                            AnnotationTable& aTargetAnnotations,
                            nsIFile** aTargetDumpOut);

// Create an additional minidump for a child of a process which already has
// a minidump (|parentMinidump|).
// The resulting dump will get the id of the parent and use the |name| as
// an extension.
bool CreateAdditionalChildMinidump(ProcessHandle childPid,
                                   ThreadId childBlamedThread,
                                   nsIFile* parentMinidump,
                                   const nsACString& name);

#if defined(XP_WIN) || defined(XP_MACOSX)
// Parent-side API for children
const char* GetChildNotificationPipe();

#  ifdef MOZ_CRASHREPORTER_INJECTOR
// Inject a crash report client into an arbitrary process, and inform the
// callback object when it crashes. Parent process only.

class InjectorCrashCallback {
 public:
  InjectorCrashCallback() {}

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
#  endif
#else
// Parent-side API for children

// Set the outparams for crash reporter server's fd (|childCrashFd|)
// and the magic fd number it should be remapped to
// (|childCrashRemapFd|) before exec() in the child process.
// |SetRemoteExceptionHandler()| in the child process expects to find
// the server at |childCrashRemapFd|.  Return true if successful.
//
// If crash reporting is disabled, both outparams will be set to -1
// and |true| will be returned.
bool CreateNotificationPipeForChild(int* childCrashFd, int* childCrashRemapFd);

#endif  // XP_WIN

// Windows Error Reporting helper
#if defined(XP_WIN)
DWORD WINAPI WerNotifyProc(LPVOID aParameter);
#endif

// Child-side API
bool SetRemoteExceptionHandler(
    const char* aCrashPipe = nullptr,
    FileHandle aCrashTimeAnnotationFile = kInvalidFileHandle);
bool UnsetRemoteExceptionHandler();

#if defined(MOZ_WIDGET_ANDROID)
// Android creates child process as services so we must explicitly set
// the handle for the pipe since it can't get remapped to a default value.
void SetNotificationPipeForChild(FileHandle childCrashFd);
void SetCrashAnnotationPipeForChild(FileHandle childCrashAnnotationFd);
#endif

// Annotates the crash report with the name of the calling thread.
void SetCurrentThreadName(const char* aName);

}  // namespace CrashReporter

#endif /* nsExceptionHandler_h__ */
