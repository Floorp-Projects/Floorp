/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************
 *
 * nsProcess is used to execute new processes and specify if you want to
 * wait (blocking) or continue (non-blocking).
 *
 *****************************************************************************
 */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"
#include "nsProcess.h"
#include "prio.h"
#include "prenv.h"
#include "nsCRT.h"
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"
#include "mozilla/Services.h"
#include "GeckoProfiler.h"

#include <stdlib.h>

#if defined(PROCESSMODEL_WINAPI)
#  include "nsString.h"
#  include "nsLiteralString.h"
#  include "nsReadableUtils.h"
#  include "mozilla/UniquePtrExtensions.h"
#else
#  ifdef XP_MACOSX
#    include <crt_externs.h>
#    include <spawn.h>
#  endif
#  ifdef XP_UNIX
#    ifndef XP_MACOSX
#      include "base/process_util.h"
#    endif
#    include <sys/wait.h>
#    include <sys/errno.h>
#  endif
#  include <sys/types.h>
#  include <signal.h>
#endif

using namespace mozilla;

//-------------------------------------------------------------------//
// nsIProcess implementation
//-------------------------------------------------------------------//
NS_IMPL_ISUPPORTS(nsProcess, nsIProcess, nsIObserver)

// Constructor
nsProcess::nsProcess()
    : mThread(nullptr),
      mLock("nsProcess.mLock"),
      mShutdown(false),
      mBlocking(false),
      mStartHidden(false),
      mNoShell(false),
      mPid(-1),
      mObserver(nullptr),
      mWeakObserver(nullptr),
      mExitValue(-1)
#if !defined(XP_UNIX)
      ,
      mProcess(nullptr)
#endif
{
}

// Destructor
nsProcess::~nsProcess() {}

NS_IMETHODIMP
nsProcess::Init(nsIFile* aExecutable) {
  if (mExecutable) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  if (NS_WARN_IF(!aExecutable)) {
    return NS_ERROR_INVALID_ARG;
  }
  bool isFile;

  // First make sure the file exists
  nsresult rv = aExecutable->IsFile(&isFile);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!isFile) {
    return NS_ERROR_FAILURE;
  }

  // Store the nsIFile in mExecutable
  mExecutable = aExecutable;
  // Get the path because it is needed by the NSPR process creation
#ifdef XP_WIN
  rv = mExecutable->GetTarget(mTargetPath);
  if (NS_FAILED(rv) || mTargetPath.IsEmpty())
#endif
    rv = mExecutable->GetPath(mTargetPath);

  return rv;
}

#if defined(XP_WIN)
// Out param `aWideCmdLine` must be free()d by the caller.
static int assembleCmdLine(char* const* aArgv, wchar_t** aWideCmdLine,
                           UINT aCodePage) {
  char* const* arg;
  char* p;
  char* q;
  char* cmdLine;
  int cmdLineSize;
  int numBackslashes;
  int i;
  int argNeedQuotes;

  /*
   * Find out how large the command line buffer should be.
   */
  cmdLineSize = 0;
  for (arg = aArgv; *arg; ++arg) {
    /*
     * \ and " need to be escaped by a \.  In the worst case,
     * every character is a \ or ", so the string of length
     * may double.  If we quote an argument, that needs two ".
     * Finally, we need a space between arguments, and
     * a null byte at the end of command line.
     */
    cmdLineSize += 2 * strlen(*arg) /* \ and " need to be escaped */
                   + 2              /* we quote every argument */
                   + 1;             /* space in between, or final null */
  }
  p = cmdLine = (char*)malloc(cmdLineSize * sizeof(char));
  if (!p) {
    return -1;
  }

  for (arg = aArgv; *arg; ++arg) {
    /* Add a space to separates the arguments */
    if (arg != aArgv) {
      *p++ = ' ';
    }
    q = *arg;
    numBackslashes = 0;
    argNeedQuotes = 0;

    /* If the argument contains white space, it needs to be quoted. */
    if (strpbrk(*arg, " \f\n\r\t\v")) {
      argNeedQuotes = 1;
    }

    if (argNeedQuotes) {
      *p++ = '"';
    }
    while (*q) {
      if (*q == '\\') {
        numBackslashes++;
        q++;
      } else if (*q == '"') {
        if (numBackslashes) {
          /*
           * Double the backslashes since they are followed
           * by a quote
           */
          for (i = 0; i < 2 * numBackslashes; i++) {
            *p++ = '\\';
          }
          numBackslashes = 0;
        }
        /* To escape the quote */
        *p++ = '\\';
        *p++ = *q++;
      } else {
        if (numBackslashes) {
          /*
           * Backslashes are not followed by a quote, so
           * don't need to double the backslashes.
           */
          for (i = 0; i < numBackslashes; i++) {
            *p++ = '\\';
          }
          numBackslashes = 0;
        }
        *p++ = *q++;
      }
    }

    /* Now we are at the end of this argument */
    if (numBackslashes) {
      /*
       * Double the backslashes if we have a quote string
       * delimiter at the end.
       */
      if (argNeedQuotes) {
        numBackslashes *= 2;
      }
      for (i = 0; i < numBackslashes; i++) {
        *p++ = '\\';
      }
    }
    if (argNeedQuotes) {
      *p++ = '"';
    }
  }

  *p = '\0';
  int32_t numChars = MultiByteToWideChar(aCodePage, 0, cmdLine, -1, nullptr, 0);
  *aWideCmdLine = (wchar_t*)malloc(numChars * sizeof(wchar_t));
  MultiByteToWideChar(aCodePage, 0, cmdLine, -1, *aWideCmdLine, numChars);
  free(cmdLine);
  return 0;
}
#endif

void nsProcess::Monitor(void* aArg) {
  RefPtr<nsProcess> process = dont_AddRef(static_cast<nsProcess*>(aArg));

#ifdef MOZ_GECKO_PROFILER
  Maybe<AutoProfilerRegisterThread> registerThread;
  if (!process->mBlocking) {
    registerThread.emplace("RunProcess");
  }
#endif
  if (!process->mBlocking) {
    NS_SetCurrentThreadName("RunProcess");
  }

#if defined(PROCESSMODEL_WINAPI)
  DWORD dwRetVal;
  unsigned long exitCode = -1;

  dwRetVal = WaitForSingleObject(process->mProcess, INFINITE);
  if (dwRetVal != WAIT_FAILED) {
    if (GetExitCodeProcess(process->mProcess, &exitCode) == FALSE) {
      exitCode = -1;
    }
  }

  // Lock in case Kill or GetExitCode are called during this
  {
    MutexAutoLock lock(process->mLock);
    CloseHandle(process->mProcess);
    process->mProcess = nullptr;
    process->mExitValue = exitCode;
    if (process->mShutdown) {
      return;
    }
  }
#else
#  ifdef XP_UNIX
  int exitCode = -1;
  int status = 0;
  pid_t result;
  do {
    result = waitpid(process->mPid, &status, 0);
  } while (result == -1 && errno == EINTR);
  if (result == process->mPid) {
    if (WIFEXITED(status)) {
      exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      exitCode = 256;  // match NSPR's signal exit status
    }
  }
#  else
  int32_t exitCode = -1;
  if (PR_WaitProcess(process->mProcess, &exitCode) != PR_SUCCESS) {
    exitCode = -1;
  }
#  endif

  // Lock in case Kill or GetExitCode are called during this
  {
    MutexAutoLock lock(process->mLock);
#  if !defined(XP_UNIX)
    process->mProcess = nullptr;
#  endif
    process->mExitValue = exitCode;
    if (process->mShutdown) {
      return;
    }
  }
#endif

  // If we ran a background thread for the monitor then notify on the main
  // thread
  if (NS_IsMainThread()) {
    process->ProcessComplete();
  } else {
    NS_DispatchToMainThread(NewRunnableMethod(
        "nsProcess::ProcessComplete", process, &nsProcess::ProcessComplete));
  }
}

void nsProcess::ProcessComplete() {
  if (mThread) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "xpcom-shutdown");
    }
    PR_JoinThread(mThread);
    mThread = nullptr;
  }

  const char* topic;
  if (mExitValue < 0) {
    topic = "process-failed";
  } else {
    topic = "process-finished";
  }

  mPid = -1;
  nsCOMPtr<nsIObserver> observer;
  if (mWeakObserver) {
    observer = do_QueryReferent(mWeakObserver);
  } else if (mObserver) {
    observer = mObserver;
  }
  mObserver = nullptr;
  mWeakObserver = nullptr;

  if (observer) {
    observer->Observe(NS_ISUPPORTS_CAST(nsIProcess*, this), topic, nullptr);
  }
}

// XXXldb |aArgs| has the wrong const-ness
NS_IMETHODIMP
nsProcess::Run(bool aBlocking, const char** aArgs, uint32_t aCount) {
  return CopyArgsAndRunProcess(aBlocking, aArgs, aCount, nullptr, false);
}

// XXXldb |aArgs| has the wrong const-ness
NS_IMETHODIMP
nsProcess::RunAsync(const char** aArgs, uint32_t aCount, nsIObserver* aObserver,
                    bool aHoldWeak) {
  return CopyArgsAndRunProcess(false, aArgs, aCount, aObserver, aHoldWeak);
}

nsresult nsProcess::CopyArgsAndRunProcess(bool aBlocking, const char** aArgs,
                                          uint32_t aCount,
                                          nsIObserver* aObserver,
                                          bool aHoldWeak) {
  // Add one to the aCount for the program name and one for null termination.
  char** my_argv = nullptr;
  my_argv = (char**)moz_xmalloc(sizeof(char*) * (aCount + 2));

  my_argv[0] = ToNewUTF8String(mTargetPath);

  for (uint32_t i = 0; i < aCount; ++i) {
    my_argv[i + 1] = const_cast<char*>(aArgs[i]);
  }

  my_argv[aCount + 1] = nullptr;

  nsresult rv = RunProcess(aBlocking, my_argv, aObserver, aHoldWeak, false);

  free(my_argv[0]);
  free(my_argv);
  return rv;
}

// XXXldb |aArgs| has the wrong const-ness
NS_IMETHODIMP
nsProcess::Runw(bool aBlocking, const char16_t** aArgs, uint32_t aCount) {
  return CopyArgsAndRunProcessw(aBlocking, aArgs, aCount, nullptr, false);
}

// XXXldb |aArgs| has the wrong const-ness
NS_IMETHODIMP
nsProcess::RunwAsync(const char16_t** aArgs, uint32_t aCount,
                     nsIObserver* aObserver, bool aHoldWeak) {
  return CopyArgsAndRunProcessw(false, aArgs, aCount, aObserver, aHoldWeak);
}

nsresult nsProcess::CopyArgsAndRunProcessw(bool aBlocking,
                                           const char16_t** aArgs,
                                           uint32_t aCount,
                                           nsIObserver* aObserver,
                                           bool aHoldWeak) {
  // Add one to the aCount for the program name and one for null termination.
  char** my_argv = nullptr;
  my_argv = (char**)moz_xmalloc(sizeof(char*) * (aCount + 2));

  my_argv[0] = ToNewUTF8String(mTargetPath);

  for (uint32_t i = 0; i < aCount; i++) {
    my_argv[i + 1] = ToNewUTF8String(nsDependentString(aArgs[i]));
  }

  my_argv[aCount + 1] = nullptr;

  nsresult rv = RunProcess(aBlocking, my_argv, aObserver, aHoldWeak, true);

  for (uint32_t i = 0; i <= aCount; ++i) {
    free(my_argv[i]);
  }
  free(my_argv);
  return rv;
}

nsresult nsProcess::RunProcess(bool aBlocking, char** aMyArgv,
                               nsIObserver* aObserver, bool aHoldWeak,
                               bool aArgsUTF8) {
  NS_WARNING_ASSERTION(!XRE_IsContentProcess(),
                       "No launching of new processes in the content process");

  if (NS_WARN_IF(!mExecutable)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (NS_WARN_IF(mThread)) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  if (aObserver) {
    if (aHoldWeak) {
      mWeakObserver = do_GetWeakReference(aObserver);
      if (!mWeakObserver) {
        return NS_NOINTERFACE;
      }
    } else {
      mObserver = aObserver;
    }
  }

  mExitValue = -1;
  mPid = -1;

#if defined(PROCESSMODEL_WINAPI)
  BOOL retVal;
  UniqueFreePtr<wchar_t> cmdLine;

  // |aMyArgv| is null-terminated and always starts with the program path. If
  // the second slot is non-null then arguments are being passed.
  if (aMyArgv[1] || mNoShell) {
    // Pass the executable path as argv[0] to the launched program when calling
    // CreateProcess().
    char** argv = mNoShell ? aMyArgv : aMyArgv + 1;

    wchar_t* assembledCmdLine = nullptr;
    if (assembleCmdLine(argv, &assembledCmdLine,
                        aArgsUTF8 ? CP_UTF8 : CP_ACP) == -1) {
      return NS_ERROR_FILE_EXECUTION_FAILED;
    }
    cmdLine.reset(assembledCmdLine);
  }

  // The program name in aMyArgv[0] is always UTF-8
  NS_ConvertUTF8toUTF16 wideFile(aMyArgv[0]);

  if (mNoShell) {
    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = mStartHidden ? SW_HIDE : SW_SHOWNORMAL;

    PROCESS_INFORMATION processInfo;
    retVal = CreateProcess(/* lpApplicationName = */ wideFile.get(),
                           /* lpCommandLine */ cmdLine.get(),
                           /* lpProcessAttributes = */ NULL,
                           /* lpThreadAttributes = */ NULL,
                           /* bInheritHandles = */ FALSE,
                           /* dwCreationFlags = */ 0,
                           /* lpEnvironment = */ NULL,
                           /* lpCurrentDirectory = */ NULL,
                           /* lpStartupInfo = */ &startupInfo,
                           /* lpProcessInformation */ &processInfo);

    if (!retVal) {
      return NS_ERROR_FILE_EXECUTION_FAILED;
    }

    CloseHandle(processInfo.hThread);

    mProcess = processInfo.hProcess;
  } else {
    SHELLEXECUTEINFOW sinfo;
    memset(&sinfo, 0, sizeof(SHELLEXECUTEINFOW));
    sinfo.cbSize = sizeof(SHELLEXECUTEINFOW);
    sinfo.hwnd = nullptr;
    sinfo.lpFile = wideFile.get();
    sinfo.nShow = mStartHidden ? SW_HIDE : SW_SHOWNORMAL;

    /* The SEE_MASK_NO_CONSOLE flag is important to prevent console windows
     * from appearing. This makes behavior the same on all platforms. The flag
     * will not have any effect on non-console applications.
     */
    sinfo.fMask =
        SEE_MASK_FLAG_DDEWAIT | SEE_MASK_NO_CONSOLE | SEE_MASK_NOCLOSEPROCESS;

    if (cmdLine) {
      sinfo.lpParameters = cmdLine.get();
    }

    retVal = ShellExecuteExW(&sinfo);
    if (!retVal) {
      return NS_ERROR_FILE_EXECUTION_FAILED;
    }

    mProcess = sinfo.hProcess;
  }

  mPid = GetProcessId(mProcess);
#elif defined(XP_MACOSX)
  // Note: |aMyArgv| is already null-terminated as required by posix_spawnp.
  pid_t newPid = 0;
  int result = posix_spawnp(&newPid, aMyArgv[0], nullptr, nullptr, aMyArgv,
                            *_NSGetEnviron());
  mPid = static_cast<int32_t>(newPid);

  if (result != 0) {
    return NS_ERROR_FAILURE;
  }
#elif defined(XP_UNIX)
  base::LaunchOptions options;
  std::vector<std::string> argvVec;
  for (char** arg = aMyArgv; *arg != nullptr; ++arg) {
    argvVec.push_back(*arg);
  }
  pid_t newPid;
  if (base::LaunchApp(argvVec, options, &newPid)) {
    static_assert(sizeof(pid_t) <= sizeof(int32_t),
                  "mPid is large enough to hold a pid");
    mPid = static_cast<int32_t>(newPid);
  } else {
    return NS_ERROR_FAILURE;
  }
#else
  mProcess = PR_CreateProcess(aMyArgv[0], aMyArgv, nullptr, nullptr);
  if (!mProcess) {
    return NS_ERROR_FAILURE;
  }
  struct MYProcess {
    uint32_t pid;
  };
  MYProcess* ptrProc = (MYProcess*)mProcess;
  mPid = ptrProc->pid;
#endif

  NS_ADDREF_THIS();
  mBlocking = aBlocking;
  if (aBlocking) {
    Monitor(this);
    if (mExitValue < 0) {
      return NS_ERROR_FILE_EXECUTION_FAILED;
    }
  } else {
    mThread =
        PR_CreateThread(PR_SYSTEM_THREAD, Monitor, this, PR_PRIORITY_NORMAL,
                        PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (!mThread) {
      NS_RELEASE_THIS();
      return NS_ERROR_FAILURE;
    }

    // It isn't a failure if we just can't watch for shutdown
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(this, "xpcom-shutdown", false);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsProcess::GetIsRunning(bool* aIsRunning) {
  if (mThread) {
    *aIsRunning = true;
  } else {
    *aIsRunning = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsProcess::GetStartHidden(bool* aStartHidden) {
  *aStartHidden = mStartHidden;
  return NS_OK;
}

NS_IMETHODIMP
nsProcess::SetStartHidden(bool aStartHidden) {
  mStartHidden = aStartHidden;
  return NS_OK;
}

NS_IMETHODIMP
nsProcess::GetNoShell(bool* aNoShell) {
  *aNoShell = mNoShell;
  return NS_OK;
}

NS_IMETHODIMP
nsProcess::SetNoShell(bool aNoShell) {
  mNoShell = aNoShell;
  return NS_OK;
}

NS_IMETHODIMP
nsProcess::GetPid(uint32_t* aPid) {
  if (!mThread) {
    return NS_ERROR_FAILURE;
  }
  if (mPid < 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  *aPid = mPid;
  return NS_OK;
}

NS_IMETHODIMP
nsProcess::Kill() {
  if (!mThread) {
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mLock);
#if defined(PROCESSMODEL_WINAPI)
    if (TerminateProcess(mProcess, 0) == 0) {
      return NS_ERROR_FAILURE;
    }
#elif defined(XP_UNIX)
    if (kill(mPid, SIGKILL) != 0) {
      return NS_ERROR_FAILURE;
    }
#else
    if (!mProcess || (PR_KillProcess(mProcess) != PR_SUCCESS)) {
      return NS_ERROR_FAILURE;
    }
#endif
  }

  // We must null out mThread if we want IsRunning to return false immediately
  // after this call.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, "xpcom-shutdown");
  }
  PR_JoinThread(mThread);
  mThread = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsProcess::GetExitValue(int32_t* aExitValue) {
  MutexAutoLock lock(mLock);

  *aExitValue = mExitValue;

  return NS_OK;
}

NS_IMETHODIMP
nsProcess::Observe(nsISupports* aSubject, const char* aTopic,
                   const char16_t* aData) {
  // Shutting down, drop all references
  if (mThread) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "xpcom-shutdown");
    }
    mThread = nullptr;
  }

  mObserver = nullptr;
  mWeakObserver = nullptr;

  MutexAutoLock lock(mLock);
  mShutdown = true;

  return NS_OK;
}
