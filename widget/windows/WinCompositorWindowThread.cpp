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

namespace mozilla {
namespace widget {

static StaticRefPtr<WinCompositorWindowThread> sWinCompositorWindowThread;

WinCompositorWindowThread::WinCompositorWindowThread(base::Thread* aThread)
  : mThread(aThread)
{
}

WinCompositorWindowThread::~WinCompositorWindowThread()
{
  delete mThread;
}

/* static */ WinCompositorWindowThread*
WinCompositorWindowThread::Get()
{
  return sWinCompositorWindowThread;
}

/* static */ void
WinCompositorWindowThread::Start()
{
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

/* static */ void
WinCompositorWindowThread::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sWinCompositorWindowThread);

  layers::SynchronousTask task("WinCompositorWindowThread");
  RefPtr<Runnable> runnable = WrapRunnable(
    RefPtr<WinCompositorWindowThread>(sWinCompositorWindowThread.get()),
    &WinCompositorWindowThread::ShutDownTask,
    &task);
  sWinCompositorWindowThread->Loop()->PostTask(runnable.forget());
  task.Wait();

  sWinCompositorWindowThread = nullptr;
}

void
WinCompositorWindowThread::ShutDownTask(layers::SynchronousTask* aTask)
{
  layers::AutoCompleteTask complete(aTask);
  MOZ_ASSERT(IsInCompositorWindowThread());
}

/* static */ MessageLoop*
WinCompositorWindowThread::Loop()
{
  return sWinCompositorWindowThread ? sWinCompositorWindowThread->mThread->message_loop() : nullptr;
}

/* static */ bool
WinCompositorWindowThread::IsInCompositorWindowThread()
{
  return sWinCompositorWindowThread && sWinCompositorWindowThread->mThread->thread_id() == PlatformThread::CurrentId();
}

const wchar_t kClassNameCompositor[] = L"MozillaCompositorWindowClass";

ATOM g_compositor_window_class;

// This runs on the window owner thread.
void InitializeWindowClass() {

  if (g_compositor_window_class) {
    return;
  }

  WNDCLASSW wc;
  wc.style         = CS_OWNDC;
  wc.lpfnWndProc   = ::DefWindowProcW;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = GetModuleHandle(nullptr);
  wc.hIcon         = nullptr;
  wc.hCursor       = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName  = nullptr;
  wc.lpszClassName = kClassNameCompositor;
  g_compositor_window_class = ::RegisterClassW(&wc);
}

/* static */ HWND
WinCompositorWindowThread::CreateCompositorWindow(HWND aParentWnd)
{
  MOZ_ASSERT(Loop());
  MOZ_ASSERT(aParentWnd);

  if (!Loop()) {
    return nullptr;
  }

  layers::SynchronousTask task("Create compositor window");

  HWND compositorWnd = nullptr;

  RefPtr<Runnable> runnable =
    NS_NewRunnableFunction("WinCompositorWindowThread::CreateCompositorWindow::Runnable", [&]() {
      layers::AutoCompleteTask complete(&task);

      InitializeWindowClass();

      compositorWnd =
        ::CreateWindowEx(WS_EX_NOPARENTNOTIFY,
                         kClassNameCompositor,
                         nullptr,
                         WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE,
                         0, 0, 1, 1,
                         aParentWnd, 0, GetModuleHandle(nullptr), 0);
    });

  Loop()->PostTask(runnable.forget());

  task.Wait();

  return compositorWnd;
}

/* static */ void
WinCompositorWindowThread::DestroyCompositorWindow(HWND aWnd)
{
  MOZ_ASSERT(aWnd);
  MOZ_ASSERT(Loop());

  if (!Loop()) {
    return;
  }

  RefPtr<Runnable> runnable =
    NS_NewRunnableFunction("WinCompositorWidget::CreateNativeWindow::Runnable", [aWnd]() {
      ::DestroyWindow(aWnd);
  });

  Loop()->PostTask(runnable.forget());
}

} // namespace widget
} // namespace mozilla
