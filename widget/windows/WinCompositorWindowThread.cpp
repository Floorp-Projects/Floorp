/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/platform_thread.h"
#include "WinCompositorWindowThread.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/StaticPtr.h"
#include "mtransport/runnable_utils.h"
#include "mozilla/StaticPrefs_apz.h"

#if WINVER < 0x0602
#  define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

namespace mozilla {
namespace widget {

static StaticRefPtr<WinCompositorWindowThread> sWinCompositorWindowThread;

WinCompositorWindowThread::WinCompositorWindowThread(base::Thread* aThread)
    : mThread(aThread) {}

WinCompositorWindowThread::~WinCompositorWindowThread() { delete mThread; }

/* static */
WinCompositorWindowThread* WinCompositorWindowThread::Get() {
  return sWinCompositorWindowThread;
}

/* static */
void WinCompositorWindowThread::Start() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sWinCompositorWindowThread);

  base::Thread* thread = new base::Thread("WinCompositor");

  base::Thread::Options options;
  // HWND requests ui thread.
  options.message_loop_type = MessageLoop::TYPE_UI;

  if (!thread->StartWithOptions(options)) {
    delete thread;
    return;
  }

  sWinCompositorWindowThread = new WinCompositorWindowThread(thread);
}

/* static */
void WinCompositorWindowThread::ShutDown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sWinCompositorWindowThread);

  layers::SynchronousTask task("WinCompositorWindowThread");
  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<WinCompositorWindowThread>(sWinCompositorWindowThread.get()),
      &WinCompositorWindowThread::ShutDownTask, &task);
  sWinCompositorWindowThread->Loop()->PostTask(runnable.forget());
  task.Wait();

  sWinCompositorWindowThread = nullptr;
}

void WinCompositorWindowThread::ShutDownTask(layers::SynchronousTask* aTask) {
  layers::AutoCompleteTask complete(aTask);
  MOZ_ASSERT(IsInCompositorWindowThread());
}

/* static */
MessageLoop* WinCompositorWindowThread::Loop() {
  return sWinCompositorWindowThread
             ? sWinCompositorWindowThread->mThread->message_loop()
             : nullptr;
}

/* static */
bool WinCompositorWindowThread::IsInCompositorWindowThread() {
  return sWinCompositorWindowThread &&
         sWinCompositorWindowThread->mThread->thread_id() ==
             PlatformThread::CurrentId();
}

const wchar_t kClassNameCompositorInitalParent[] =
    L"MozillaCompositorInitialParentClass";
const wchar_t kClassNameCompositor[] = L"MozillaCompositorWindowClass";

ATOM g_compositor_inital_parent_window_class;
ATOM g_compositor_window_class;

// This runs on the window owner thread.
void InitializeInitialParentWindowClass() {
  if (g_compositor_inital_parent_window_class) {
    return;
  }

  WNDCLASSW wc;
  wc.style = 0;
  wc.lpfnWndProc = ::DefWindowProcW;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hIcon = nullptr;
  wc.hCursor = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = kClassNameCompositorInitalParent;
  g_compositor_inital_parent_window_class = ::RegisterClassW(&wc);
}

// This runs on the window owner thread.
void InitializeWindowClass() {
  if (g_compositor_window_class) {
    return;
  }

  WNDCLASSW wc;
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = ::DefWindowProcW;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hIcon = nullptr;
  wc.hCursor = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = kClassNameCompositor;
  g_compositor_window_class = ::RegisterClassW(&wc);
}

/* static */
WinCompositorWnds WinCompositorWindowThread::CreateCompositorWindow() {
  MOZ_ASSERT(Loop());

  if (!Loop()) {
    return WinCompositorWnds(nullptr, nullptr);
  }

  layers::SynchronousTask task("Create compositor window");

  HWND initialParentWnd = nullptr;
  HWND compositorWnd = nullptr;

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "WinCompositorWindowThread::CreateCompositorWindow::Runnable", [&]() {
        layers::AutoCompleteTask complete(&task);

        InitializeInitialParentWindowClass();
        InitializeWindowClass();

        // Create initial parent window.
        // We could not directly create a compositor window with a main window
        // as parent window, so instead create it with a temporary placeholder
        // parent. Its parent is set as main window in UI process.
        initialParentWnd =
            ::CreateWindowEx(WS_EX_TOOLWINDOW, kClassNameCompositorInitalParent,
                             nullptr, WS_POPUP | WS_DISABLED, 0, 0, 1, 1,
                             nullptr, 0, GetModuleHandle(nullptr), 0);

        DWORD extendedStyle = WS_EX_NOPARENTNOTIFY | WS_EX_NOREDIRECTIONBITMAP;

        if (!StaticPrefs::apz_windows_force_disable_direct_manipulation()) {
          extendedStyle |= WS_EX_LAYERED;
        }

        compositorWnd = ::CreateWindowEx(
            extendedStyle, kClassNameCompositor, nullptr,
            WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE, 0, 0, 1, 1,
            initialParentWnd, 0, GetModuleHandle(nullptr), 0);
      });

  Loop()->PostTask(runnable.forget());

  task.Wait();

  return WinCompositorWnds(compositorWnd, initialParentWnd);
}

/* static */
void WinCompositorWindowThread::DestroyCompositorWindow(
    WinCompositorWnds aWnds) {
  MOZ_ASSERT(aWnds.mCompositorWnd);
  MOZ_ASSERT(aWnds.mInitialParentWnd);
  MOZ_ASSERT(Loop());

  if (!Loop()) {
    return;
  }

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "WinCompositorWidget::CreateNativeWindow::Runnable", [aWnds]() {
        ::DestroyWindow(aWnds.mCompositorWnd);
        ::DestroyWindow(aWnds.mInitialParentWnd);
      });

  Loop()->PostTask(runnable.forget());
}

}  // namespace widget
}  // namespace mozilla
