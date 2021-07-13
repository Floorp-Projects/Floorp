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

using namespace mozilla::widget;

namespace mozilla {

static void WaylandVsyncSourceCallbackHandler(void* aData,
                                              struct wl_callback* aCallback,
                                              uint32_t aTime) {
  WaylandVsyncSource::WaylandDisplay* context =
      (WaylandVsyncSource::WaylandDisplay*)aData;
  wl_callback_destroy(aCallback);
  context->FrameCallback(aTime);
}

static void WaylandVsyncSourceCallbackHandler(void* aData, uint32_t aTime) {
  WaylandVsyncSource::WaylandDisplay* context =
      (WaylandVsyncSource::WaylandDisplay*)aData;
  context->FrameCallback(aTime);
}

static const struct wl_callback_listener WaylandVsyncSourceCallbackListener = {
    WaylandVsyncSourceCallbackHandler};

WaylandVsyncSource::WaylandDisplay::WaylandDisplay()
    : mMutex("WaylandVsyncSource"),
      mIsShutdown(false),
      mVsyncEnabled(false),
      mMonitorEnabled(false),
      mCallbackRequested(false),
      mDisplay(WaylandDisplayGetWLDisplay()),
      mContainer(nullptr),
      mVsyncRate(TimeDuration::FromMilliseconds(1000.0 / 60.0)),
      mLastVsyncTimeStamp(TimeStamp::Now()) {
  MOZ_ASSERT(NS_IsMainThread());
}

void WaylandVsyncSource::WaylandDisplay::MaybeUpdateSource(
    MozContainer* aContainer) {
  MutexAutoLock lock(mMutex);

  if (aContainer == mContainer) {
    return;
  }

  mNativeLayerRoot = nullptr;
  mContainer = aContainer;

  if (mMonitorEnabled) {
    mCallbackRequested = false;
    Refresh();
  }
}

void WaylandVsyncSource::WaylandDisplay::MaybeUpdateSource(
    const RefPtr<NativeLayerRootWayland>& aNativeLayerRoot) {
  MutexAutoLock lock(mMutex);

  if (aNativeLayerRoot == mNativeLayerRoot) {
    return;
  }

  mNativeLayerRoot = aNativeLayerRoot;
  mContainer = nullptr;

  if (mMonitorEnabled) {
    mCallbackRequested = false;
    Refresh();
  }
}

void WaylandVsyncSource::WaylandDisplay::Refresh() {
  mMutex.AssertCurrentThreadOwns();

  if (!(mContainer || mNativeLayerRoot) || !mMonitorEnabled || !mVsyncEnabled ||
      mCallbackRequested) {
    // We don't need to do anything because:
    // * We are unwanted by our widget or monitor, or
    // * The last frame callback hasn't yet run to see that it had been shut
    //   down, so we can reuse it after having set mVsyncEnabled to true.
    return;
  }

  if (mContainer) {
    struct wl_surface* surface = moz_container_wayland_surface_lock(mContainer);
    if (!surface) {
      // The surface hasn't been created yet. Try again when the surface is
      // ready.
      RefPtr<WaylandVsyncSource::WaylandDisplay> self(this);
      moz_container_wayland_add_initial_draw_callback(
          mContainer, [self]() -> void {
            MutexAutoLock lock(self->mMutex);
            self->Refresh();
          });
      return;
    }
    moz_container_wayland_surface_unlock(mContainer, &surface);
  }

  // Vsync is enabled, but we don't have a callback configured. Set one up so
  // we can get to work.
  SetupFrameCallback();
  mLastVsyncTimeStamp = TimeStamp::Now();
  TimeStamp outputTimestamp = mLastVsyncTimeStamp + GetVsyncRate();

  {
    MutexAutoUnlock unlock(mMutex);
    NotifyVsync(mLastVsyncTimeStamp, outputTimestamp);
  }
}

void WaylandVsyncSource::WaylandDisplay::EnableMonitor() {
  MutexAutoLock lock(mMutex);
  if (mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = true;
  Refresh();
}

void WaylandVsyncSource::WaylandDisplay::DisableMonitor() {
  MutexAutoLock lock(mMutex);
  if (!mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = false;
  mCallbackRequested = false;
}

void WaylandVsyncSource::WaylandDisplay::SetupFrameCallback() {
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mCallbackRequested);

  if (mNativeLayerRoot) {
    mNativeLayerRoot->RequestFrameCallback(&WaylandVsyncSourceCallbackHandler,
                                           this);
  } else {
    struct wl_surface* surface = moz_container_wayland_surface_lock(mContainer);
    if (!surface) {
      // We don't have a surface, either due to being called before it was made
      // available in the mozcontainer, or after it was destroyed. We're all
      // done regardless.
      mCallbackRequested = false;
      return;
    }

    wl_callback* callback = wl_surface_frame(surface);
    wl_callback_add_listener(callback, &WaylandVsyncSourceCallbackListener,
                             this);
    wl_surface_commit(surface);
    wl_display_flush(mDisplay);
    moz_container_wayland_surface_unlock(mContainer, &surface);
  }

  mCallbackRequested = true;
}

void WaylandVsyncSource::WaylandDisplay::FrameCallback(uint32_t timestampTime) {
  MutexAutoLock lock(mMutex);
  mCallbackRequested = false;

  if (!mVsyncEnabled || !mMonitorEnabled) {
    // We are unwanted by either our creator or our consumer, so we just stop
    // here without setting up a new frame callback.
    return;
  }

  // Configure our next frame callback.
  SetupFrameCallback();

  int64_t tick =
      BaseTimeDurationPlatformUtils::TicksFromMilliseconds(timestampTime);
  TimeStamp callbackTimeStamp = TimeStamp::FromSystemTime(tick);
  double duration = (TimeStamp::Now() - callbackTimeStamp).ToMilliseconds();

  TimeStamp vsyncTimestamp;
  if (duration < 50 && duration > -50) {
    vsyncTimestamp = callbackTimeStamp;
  } else {
    vsyncTimestamp = TimeStamp::Now();
  }

  CalculateVsyncRate(vsyncTimestamp);
  mLastVsyncTimeStamp = vsyncTimestamp;
  TimeStamp outputTimestamp = vsyncTimestamp + GetVsyncRate();

  {
    MutexAutoUnlock unlock(mMutex);
    NotifyVsync(mLastVsyncTimeStamp, outputTimestamp);
  }
}

TimeDuration WaylandVsyncSource::WaylandDisplay::GetVsyncRate() {
  return mVsyncRate;
}

void WaylandVsyncSource::WaylandDisplay::CalculateVsyncRate(
    TimeStamp vsyncTimestamp) {
  double duration = (vsyncTimestamp - mLastVsyncTimeStamp).ToMilliseconds();
  double curVsyncRate = mVsyncRate.ToMilliseconds();
  double correction;

  if (duration > curVsyncRate) {
    correction = fmin(curVsyncRate, (duration - curVsyncRate) / 10);
    mVsyncRate += TimeDuration::FromMilliseconds(correction);
  } else {
    correction = fmin(curVsyncRate / 2, (curVsyncRate - duration) / 10);
    mVsyncRate -= TimeDuration::FromMilliseconds(correction);
  }
}

void WaylandVsyncSource::WaylandDisplay::EnableVsync() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  if (mVsyncEnabled || mIsShutdown) {
    return;
  }
  mVsyncEnabled = true;
  Refresh();
}

void WaylandVsyncSource::WaylandDisplay::DisableVsync() {
  MutexAutoLock lock(mMutex);
  mVsyncEnabled = false;
  mCallbackRequested = false;
}

bool WaylandVsyncSource::WaylandDisplay::IsVsyncEnabled() {
  MutexAutoLock lock(mMutex);
  return mVsyncEnabled;
}

void WaylandVsyncSource::WaylandDisplay::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  mIsShutdown = true;
  mVsyncEnabled = false;
  mCallbackRequested = false;
}

}  // namespace mozilla

#endif  // MOZ_WAYLAND
