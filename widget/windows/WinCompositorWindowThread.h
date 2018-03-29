/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_WinCompositorWindowThread_h
#define widget_windows_WinCompositorWindowThread_h

#include "base/thread.h"
#include "base/message_loop.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

namespace mozilla {

namespace layers {
class SynchronousTask;
}

namespace widget {

class WinCompositorWindowThread final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(WinCompositorWindowThread)

public:
  /// Can be called from any thread.
  static WinCompositorWindowThread* Get();

  /// Can only be called from the main thread.
  static void Start();

  /// Can only be called from the main thread.
  static void ShutDown();

  /// Can be called from any thread.
  static MessageLoop* Loop();

  /// Can be called from any thread.
  static bool IsInCompositorWindowThread();

  /// Can be called from any thread.
  static HWND CreateCompositorWindow(HWND aParentWnd);

  /// Can be called from any thread.
  static void DestroyCompositorWindow(HWND aWnd);

private:
  explicit WinCompositorWindowThread(base::Thread* aThread);
  ~WinCompositorWindowThread();

  void ShutDownTask(layers::SynchronousTask* aTask);

  base::Thread* const mThread;
};

} // namespace widget
} // namespace mozilla

#endif // widget_windows_WinCompositorWindowThread_h
