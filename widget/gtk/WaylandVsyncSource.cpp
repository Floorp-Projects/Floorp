/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WAYLAND

#  include "WaylandVsyncSource.h"
#  include "nsThreadUtils.h"
#  include "nsISupportsImpl.h"
#  include "MainThreadUtils.h"

#  include <gdk/gdkwayland.h>

namespace mozilla {

static void WaylandVsyncSourceCallbackHandler(void* data,
                                              struct wl_callback* callback,
                                              uint32_t time) {
  WaylandVsyncSource::WaylandFrameCallbackContext* context =
      (WaylandVsyncSource::WaylandFrameCallbackContext*)data;
  wl_callback_destroy(callback);

  if (!context->mEnabled) {
    // We have been terminated, so just clean up the context and return.
    delete context;
    return;
  }

  context->mDisplay->FrameCallback();
}

static const struct wl_callback_listener WaylandVsyncSourceCallbackListener = {
    WaylandVsyncSourceCallbackHandler};

WaylandVsyncSource::WaylandDisplay::WaylandDisplay(MozContainer* container)
    : mThread("WaylandVsyncThread"),
      mTask(nullptr),
      mCallbackContext(nullptr),
      mNotifyThreadMonitor("WaylandVsyncNotifyThreadMonitor"),
      mEnabledLock("WaylandVsyncEnabledLock"),
      mVsyncEnabled(false),
      mMonitorEnabled(false),
      mShutdown(false),
      mContainer(container) {
  MOZ_ASSERT(NS_IsMainThread());

  // We store the display here so all the frame callbacks won't have to look it
  // up all the time.
  mDisplay = widget::WaylandDisplayGet()->GetDisplay();
}

void WaylandVsyncSource::WaylandDisplay::Loop() {
  MonitorAutoLock lock(mNotifyThreadMonitor);
  while (true) {
    lock.Wait();
    if (mShutdown) {
      return;
    }

    NotifyVsync(TimeStamp::Now());
  }
}

void WaylandVsyncSource::WaylandDisplay::ClearFrameCallback() {
  if (mCallbackContext) {
    mCallbackContext->mEnabled = false;
    mCallbackContext = nullptr;
  }
}

bool WaylandVsyncSource::WaylandDisplay::Setup() {
  MutexAutoLock lock(mEnabledLock);
  MOZ_ASSERT(!mTask);
  MOZ_ASSERT(!mShutdown);

  if (!mThread.Start()) {
    return false;
  }
  mTask = NewRunnableMethod("WaylandVsyncSource::WaylandDisplay::Loop", this,
                            &WaylandDisplay::Loop);
  RefPtr<Runnable> addrefedTask = mTask;
  mThread.message_loop()->PostTask(addrefedTask.forget());

  return true;
}

void WaylandVsyncSource::WaylandDisplay::EnableMonitor() {
  MutexAutoLock lock(mEnabledLock);
  if (mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = true;
  if (mVsyncEnabled && (!mCallbackContext || !mCallbackContext->mEnabled)) {
    // Vsync is enabled, but we don't have a callback configured. Set one up so
    // we can get to work.
    SetupFrameCallback();
  }
}

void WaylandVsyncSource::WaylandDisplay::DisableMonitor() {
  MutexAutoLock lock(mEnabledLock);
  if (!mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = false;
  ClearFrameCallback();
}

void WaylandVsyncSource::WaylandDisplay::Notify() {
  // Use our vsync thread to notify any vsync observers.
  MonitorAutoLock lock(mNotifyThreadMonitor);
  mNotifyThreadMonitor.NotifyAll();
}

void WaylandVsyncSource::WaylandDisplay::SetupFrameCallback() {
  struct wl_surface* surface = moz_container_get_wl_surface(mContainer);
  if (!surface) {
    // We don't have a surface, either due to being called before it was made
    // available in the mozcontainer, or after it was destroyed. We're all done
    // regardless.
    ClearFrameCallback();
    return;
  }

  if (mCallbackContext == nullptr) {
    // We don't have a context as we're not currently running, so allocate one.
    // The callback handler is responsible for deallocating the context, as
    // the callback might be called after we have been destroyed.
    mCallbackContext = new WaylandFrameCallbackContext(this);
  }

  struct wl_callback* callback = wl_surface_frame(surface);
  wl_callback_add_listener(callback, &WaylandVsyncSourceCallbackListener,
                           mCallbackContext);
  wl_surface_commit(surface);
  wl_display_flush(mDisplay);
}

void WaylandVsyncSource::WaylandDisplay::FrameCallback() {
  {
    MutexAutoLock lock(mEnabledLock);

    if (!mVsyncEnabled || !mMonitorEnabled) {
      // We are unwanted by either our creator or our consumer, so we just stop
      // here without setting up a new frame callback.
      return;
    }

    // Configure our next frame callback.
    SetupFrameCallback();
  }

  Notify();
}

void WaylandVsyncSource::WaylandDisplay::EnableVsync() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mEnabledLock);
  if (mVsyncEnabled) {
    return;
  }

  mVsyncEnabled = true;
  if (!mMonitorEnabled || (mCallbackContext && mCallbackContext->mEnabled)) {
    // We don't need to do anything because:
    // * We are unwanted by our widget, or
    // * The last frame callback hasn't yet run to see that it had been shut
    //   down, so we can reuse it after having set mVsyncEnabled to true.
    return;
  }

  // We don't have a callback configured, so set one up.
  SetupFrameCallback();
}

void WaylandVsyncSource::WaylandDisplay::DisableVsync() {
  MutexAutoLock lock(mEnabledLock);
  mVsyncEnabled = false;
  ClearFrameCallback();
}

bool WaylandVsyncSource::WaylandDisplay::IsVsyncEnabled() {
  MutexAutoLock lock(mEnabledLock);
  return mVsyncEnabled;
}

void WaylandVsyncSource::WaylandDisplay::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  DisableVsync();

  // Terminate our task by setting the shutdown flag and waking up the thread so
  // that it may check the flag.
  {
    MonitorAutoLock lock(mNotifyThreadMonitor);
    mShutdown = true;
    mNotifyThreadMonitor.NotifyAll();
  }

  // Stop the thread. Note that Stop() waits for tasks to terminate, so we must
  // manually ensure that we will break out of our thread loop first in order to
  // not block forever.
  mThread.Stop();
}

}  // namespace mozilla

#endif  // MOZ_WAYLAND
