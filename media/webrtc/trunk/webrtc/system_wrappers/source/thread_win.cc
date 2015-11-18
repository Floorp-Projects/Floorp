/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/thread_win.h"

#include <process.h>
#include <stdio.h>
#include <windows.h>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace {
void CALLBACK RaiseFlag(ULONG_PTR param) {
  *reinterpret_cast<bool*>(param) = true;
}

// TODO(tommi): This is borrowed from webrtc/base/thread.cc, but we can't
// include thread.h from here since thread.h pulls in libjingle dependencies.
// Would be good to consolidate.

// As seen on MSDN.
// http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.71).aspx
#define MSDEV_SET_THREAD_NAME  0x406D1388
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;
  LPCSTR szName;
  DWORD dwThreadID;
  DWORD dwFlags;
} THREADNAME_INFO;

void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = szThreadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags = 0;

  __try {
    RaiseException(MSDEV_SET_THREAD_NAME, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<ULONG_PTR*>(&info));
  }
  __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}

}

// For use in ThreadWindowsUI callbacks
static UINT static_reg_windows_msg = RegisterWindowMessageW(L"WebrtcWindowsUIThreadEvent");
// timer id used in delayed callbacks
static const UINT_PTR kTimerId = 1;
static const wchar_t kThisProperty[] = L"ThreadWindowsUIPtr";
static const wchar_t kThreadWindow[] = L"WebrtcWindowsUIThread";

ThreadWindows::ThreadWindows(ThreadRunFunction func, void* obj,
                             const char* thread_name)
    : run_function_(func),
      obj_(obj),
      stop_(false),
      thread_(NULL),
      name_(thread_name ? thread_name : "webrtc") {
  DCHECK(func);
}

ThreadWindows::~ThreadWindows() {
  DCHECK(main_thread_.CalledOnValidThread());
  DCHECK(!thread_);
}

// static
uint32_t ThreadWrapper::GetThreadId() {
  return GetCurrentThreadId();
}

// static
DWORD WINAPI ThreadWindows::StartThread(void* param) {
  static_cast<ThreadWindows*>(param)->Run();
  return 0;
}

bool ThreadWindows::Start() {
  DCHECK(main_thread_.CalledOnValidThread());
  DCHECK(!thread_);

  stop_ = false;

  // See bug 2902 for background on STACK_SIZE_PARAM_IS_A_RESERVATION.
  // Set the reserved stack stack size to 1M, which is the default on Windows
  // and Linux.
  DWORD thread_id;
  thread_ = ::CreateThread(NULL, 1024 * 1024, &StartThread, this,
      STACK_SIZE_PARAM_IS_A_RESERVATION, &thread_id);
  if (!thread_ ) {
    DCHECK(false) << "CreateThread failed";
    return false;
  }

  return true;
}

bool ThreadWindows::Stop() {
  DCHECK(main_thread_.CalledOnValidThread());
  if (thread_) {
    // Set stop_ to |true| on the worker thread.
    QueueUserAPC(&RaiseFlag, thread_, reinterpret_cast<ULONG_PTR>(&stop_));
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = nullptr;
  }

  return true;
}

bool ThreadWindows::SetPriority(ThreadPriority priority) {
  DCHECK(main_thread_.CalledOnValidThread());
  return thread_ && SetThreadPriority(thread_, priority);
}

void ThreadWindows::Run() {
  if (!name_.empty())
    SetThreadName(static_cast<DWORD>(-1), name_.c_str());

  do {
    // The interface contract of Start/Stop is that for a successfull call to
    // Start, there should be at least one call to the run function.  So we
    // call the function before checking |stop_|.
    if (!run_function_(obj_))
      break;
    // Alertable sleep to permit RaiseFlag to run and update |stop_|.
    SleepEx(0, true);
  } while (!stop_);
}

bool ThreadWindowsUI::Stop() {
  DCHECK(main_thread_.CalledOnValidThread());

  // Shut down the dispatch loop and let the background thread exit.
  if (timerid_) {
    KillTimer(hwnd_, timerid_);
    timerid_ = 0;
  }

  PostMessage(hwnd_, WM_CLOSE, 0, 0);

  return ThreadWindows::Stop();
}

bool ThreadWindowsUI::InternalInit() {
  // Create an event window for use in generating callbacks to capture
  // objects.
  if (hwnd_ == NULL) {
    WNDCLASSW wc;
    HMODULE hModule = GetModuleHandle(NULL);
    if (!GetClassInfoW(hModule, kThreadWindow, &wc)) {
      ZeroMemory(&wc, sizeof(WNDCLASSW));
      wc.hInstance = hModule;
      wc.lpfnWndProc = EventWindowProc;
      wc.lpszClassName = kThreadWindow;
      RegisterClassW(&wc);
    }
    hwnd_ = CreateWindowW(kThreadWindow, L"",
                          0, 0, 0, 0, 0,
                          NULL, NULL, hModule, NULL);
    assert(hwnd_);
    SetPropW(hwnd_, kThisProperty, this);

    if (timeout_) {
      // if someone set the timer before we started
      RequestCallbackTimer(timeout_);
    }
  }
  return !!hwnd_;
}

void ThreadWindowsUI::RequestCallback() {
  assert(hwnd_);
  assert(static_reg_windows_msg);
  PostMessage(hwnd_, static_reg_windows_msg, 0, 0);
}

bool ThreadWindowsUI::RequestCallbackTimer(unsigned int milliseconds) {
  if (!hwnd_) {
    assert(!thread_);
    // set timer once thread starts
  } else {
    if (timerid_) {
      KillTimer(hwnd_, timerid_);
    }
    timerid_ = SetTimer(hwnd_, kTimerId, milliseconds, NULL);
  }
  timeout_ = milliseconds;
  return !!timerid_;
}

void ThreadWindowsUI::Run() {
  if (!InternalInit()) {
    assert(false);
  }

  if (!name_.empty())
    SetThreadName(static_cast<DWORD>(-1), name_.c_str());

  do {
    // The interface contract of Start/Stop is that for a successful call to
    // Start, there should be at least one call to the run function.  So we
    // call the function before checking |stop_|.
    if (!run_function_(obj_))
      break;

    // Alertable sleep to permit RaiseFlag to run and update |stop_|.
    if (MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT,
                                    MWMO_ALERTABLE | MWMO_INPUTAVAILABLE) ==
        WAIT_OBJECT_0) {
      MSG msg;
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          stop_ = true;
          break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

  } while (!stop_);

  // Don't need to DestroyWindow(hwnd_) due to WM_CLOSE->WM_DESTROY handling
};

void
ThreadWindowsUI::NativeEventCallback() {
  if (!run_function_) {
    stop_ = true;
    return;
  }
  stop_ = !run_function_(obj_);
}

/* static */
LRESULT CALLBACK
ThreadWindowsUI::EventWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_DESTROY) {
    RemovePropW(hwnd, kThisProperty);
    PostQuitMessage(0);
    return 0;
  }

  ThreadWindowsUI *twui = static_cast<ThreadWindowsUI*>(GetPropW(hwnd, kThisProperty));
  if (!twui) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }

  if ((uMsg == static_reg_windows_msg && uMsg != WM_NULL) ||
      (uMsg == WM_TIMER && wParam == kTimerId)) {
    twui->NativeEventCallback();
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

}  // namespace webrtc
