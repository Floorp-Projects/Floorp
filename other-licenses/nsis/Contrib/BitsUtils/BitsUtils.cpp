/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <bits.h>
#include <utility>

// Avoid conversions, we will only build Unicode anyway.
#if !(defined(UNICODE) && defined(_UNICODE))
#  error "Unicode required"
#endif

static HINSTANCE gHInst;

// ***** Section: ScopeExit
// Derived from mfbt mozilla::ScopeExit, I have removed the use of
// GuardObjectNotifier and annotations MOZ_STACK_CLASS and MOZ_MUST_USE.
template <typename ExitFunction>
class ScopeExit {
  ExitFunction mExitFunction;
  bool mExecuteOnDestruction;

 public:
  explicit ScopeExit(ExitFunction &&cleanup)
      : mExitFunction(cleanup), mExecuteOnDestruction(true) {}

  ScopeExit(ScopeExit &&rhs)
      : mExitFunction(std::move(rhs.mExitFunction)),
        mExecuteOnDestruction(rhs.mExecuteOnDestruction) {
    rhs.release();
  }

  ~ScopeExit() {
    if (mExecuteOnDestruction) {
      mExitFunction();
    }
  }

  void release() { mExecuteOnDestruction = false; }

 private:
  explicit ScopeExit(const ScopeExit &) = delete;
  ScopeExit &operator=(const ScopeExit &) = delete;
  ScopeExit &operator=(ScopeExit &&) = delete;
};

template <typename ExitFunction>
ScopeExit<ExitFunction> MakeScopeExit(ExitFunction &&exitFunction) {
  return ScopeExit<ExitFunction>(std::move(exitFunction));
}

// ***** Section: NSIS stack
typedef struct _stack_t {
  struct _stack_t *next;
  WCHAR text[1];  // this should be the length of g_stringsize when allocating
} stack_t;

static unsigned int g_stringsize;
static stack_t **g_stacktop;

static int popstringn(LPWSTR str, int maxlen) {
  stack_t *th;
  if (!g_stacktop || !*g_stacktop) return 1;
  th = (*g_stacktop);
  if (str) lstrcpynW(str, th->text, maxlen ? maxlen : g_stringsize);
  *g_stacktop = th->next;
  GlobalFree((HGLOBAL)th);
  return 0;
}

static void pushstring(LPCWSTR str) {
  stack_t *th;
  if (!g_stacktop) return;
  th = (stack_t *)GlobalAlloc(
      GPTR, (sizeof(stack_t) + (g_stringsize) * sizeof(*str)));
  lstrcpynW(th->text, str, g_stringsize);
  th->next = *g_stacktop;
  *g_stacktop = th;
}

// ***** Section: NSIS Plug-In API (from NSIS api.h)
// NSIS Plug-In Callback Messages
enum NSPIM {
  NSPIM_UNLOAD,     // This is the last message a plugin gets, do final cleanup
  NSPIM_GUIUNLOAD,  // Called after .onGUIEnd
};

// Prototype for callbacks registered with
// extra_parameters->RegisterPluginCallback() Return NULL for unknown messages
// Should always be __cdecl for future expansion possibilities
typedef UINT_PTR (*NSISPLUGINCALLBACK)(enum NSPIM);

#define NSISCALL __stdcall

typedef struct {
  LPVOID exec_flags;
  int(NSISCALL *ExecuteCodeSegment)(int, HWND);
  void(NSISCALL *validate_filename)(LPWSTR);
  int(NSISCALL *RegisterPluginCallback)(
      HMODULE, NSISPLUGINCALLBACK);  // returns 0 on success, 1 if already
                                     // registered and < 0 on errors
} extra_parameters;

// ***** Section: StartBitsThread
UINT_PTR __cdecl NSISPluginCallback(NSPIM msg);

static struct {
  HANDLE thread;
  bool shutdown_requested;
  CRITICAL_SECTION cs;
  CONDITION_VARIABLE cv;
} gStartBitsThread = {nullptr, false, 0, 0};

// This thread connects to the BackgroundCopyManager, which may take some time
// if the BITS service is not already running. It also holds open the connection
// until gStartBitsThread.shutdown_requested becomes true.
DWORD WINAPI StartBitsThreadProc(LPVOID) {
  EnterCriticalSection(&gStartBitsThread.cs);
  auto leaveCS =
      MakeScopeExit([] { LeaveCriticalSection(&gStartBitsThread.cs); });

  if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
    return 0;
  }
  auto coUninit = MakeScopeExit([] { CoUninitialize(); });

  IBackgroundCopyManager *bcm = nullptr;
  if (FAILED(CoCreateInstance(
          __uuidof(BackgroundCopyManager), nullptr, CLSCTX_LOCAL_SERVER,
          __uuidof(IBackgroundCopyManager), (LPVOID *)&bcm)) ||
      !bcm) {
    return 0;
  }

  do {
    SleepConditionVariableCS(&gStartBitsThread.cv, &gStartBitsThread.cs,
                             INFINITE);
  } while (!gStartBitsThread.shutdown_requested);

  bcm->Release();
  return 1;
}

// Start up the thread
// returns true on success
bool StartBitsServiceBackgroundThreadImpl(extra_parameters *extra_params) {
  EnterCriticalSection(&gStartBitsThread.cs);
  auto leaveCS =
      MakeScopeExit([] { LeaveCriticalSection(&gStartBitsThread.cs); });

  if (gStartBitsThread.thread) {
    // Thread is already started, assumed to be still running.
    return true;
  }

  // Ensure the callback is registered so the thread can be stopped, and also so
  // NSIS doesn't unload this DLL.
  extra_params->RegisterPluginCallback(gHInst, NSISPluginCallback);

  gStartBitsThread.shutdown_requested = false;

  gStartBitsThread.thread =
      CreateThread(nullptr, 0, StartBitsThreadProc, nullptr, 0, 0);
  if (!gStartBitsThread.thread) {
    return false;
  }

  return true;
}

// Shut down the Start BITS thread, if it was started.
void ShutdownStartBitsThread() {
  EnterCriticalSection(&gStartBitsThread.cs);
  if (gStartBitsThread.thread) {
    gStartBitsThread.shutdown_requested = true;
    WakeAllConditionVariable(&gStartBitsThread.cv);
    LeaveCriticalSection(&gStartBitsThread.cs);

    // Give the thread a little time to clean up.
    if (WaitForSingleObject(gStartBitsThread.thread, 1000) == WAIT_OBJECT_0) {
      EnterCriticalSection(&gStartBitsThread.cs);
      gStartBitsThread.thread = nullptr;
      LeaveCriticalSection(&gStartBitsThread.cs);
    } else {
      // Don't attempt to recover if we didn't see the thread end,
      // the process will be exiting soon anyway.
    }

  } else {
    LeaveCriticalSection(&gStartBitsThread.cs);
  }
}

// ***** Section: CancelBitsJobsByName
#define MAX_JOB_NAME 256

bool CancelBitsJobsByNameImpl(LPWSTR matchJobName) {
  if (FAILED(CoInitialize(nullptr))) {
    return false;
  }
  auto coUninit = MakeScopeExit([] { CoUninitialize(); });

  IBackgroundCopyManager *bcm = nullptr;
  if (FAILED(CoCreateInstance(
          __uuidof(BackgroundCopyManager), nullptr, CLSCTX_LOCAL_SERVER,
          __uuidof(IBackgroundCopyManager), (LPVOID *)&bcm)) ||
      !bcm) {
    return false;
  }
  auto bcmRelease = MakeScopeExit([bcm] { bcm->Release(); });

  IEnumBackgroundCopyJobs *enumerator = nullptr;
  // Attempt to enumerate jobs for all users. If that fails,
  // try for only the current user.
  if (FAILED(bcm->EnumJobs(BG_JOB_ENUM_ALL_USERS, &enumerator))) {
    enumerator = nullptr;
    if (FAILED(bcm->EnumJobs(0, &enumerator))) {
      return false;
    }
  }
  if (!enumerator) {
    return false;
  }
  auto enumeratorRelease =
      MakeScopeExit([enumerator] { enumerator->Release(); });

  bool success = true;

  IBackgroundCopyJob *job = nullptr;
  HRESULT nextResult;
  while ((nextResult = enumerator->Next(1, &job, nullptr),
          SUCCEEDED(nextResult))) {
    if (nextResult == S_FALSE) {
      break;
    }
    if (!job) {
      success = false;
      break;
    }

    LPWSTR curJobName = nullptr;

    if (SUCCEEDED(job->GetDisplayName(&curJobName)) && curJobName) {
      if (lstrcmpW(curJobName, matchJobName) == 0) {
        if (!SUCCEEDED(job->Cancel())) {
          // If we can't cancel we can still try the other jobs.
          success = false;
        }
      }
      CoTaskMemFree((LPVOID)curJobName);
      curJobName = nullptr;
    } else {
      // We may not be able to access certain jobs, keep trying the rest.
      success = false;
    }

    job->Release();
    job = nullptr;
  }

  if (!SUCCEEDED(nextResult)) {
    success = false;
  }

  return success;
}

// ***** Section: DLL entry points
extern "C" {
// Cancel all BITS jobs with the given name.
void __declspec(dllexport)
    CancelBitsJobsByName(HWND hwndParent, int string_size, char *variables,
                         stack_t **stacktop, extra_parameters *) {
  g_stacktop = stacktop;
  g_stringsize = string_size;

  WCHAR matchJobName[MAX_JOB_NAME + 1];
  matchJobName[0] = L'\0';

  if (!popstringn(matchJobName, sizeof(matchJobName) / sizeof(WCHAR))) {
    if (CancelBitsJobsByNameImpl(matchJobName)) {
      pushstring(L"ok");
      return;
    }
  }

  pushstring(L"error");
}

// Start the BITS service in the background, and hold a reference to it until
// the (un)installer exits.
// Does not provide any feedback or touch the stack.
void __declspec(dllexport)
    StartBitsServiceBackground(HWND, int, char *, stack_t **,
                               extra_parameters *extra_params) {
  StartBitsServiceBackgroundThreadImpl(extra_params);
}
}

// Handle messages from NSIS
UINT_PTR __cdecl NSISPluginCallback(NSPIM msg) {
  if (msg == NSPIM_UNLOAD) {
    ShutdownStartBitsThread();
  }
  return 0;
}

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    gHInst = instance;
    InitializeConditionVariable(&gStartBitsThread.cv);
    InitializeCriticalSection(&gStartBitsThread.cs);
  }
  return TRUE;
}