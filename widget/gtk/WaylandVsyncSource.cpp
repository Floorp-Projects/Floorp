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

static void WaylandVsyncSourceCallbackHandler(void* data,
                                              struct wl_callback* callback,
                                              uint32_t time) {
  WaylandVsyncSource::WaylandDisplay* context =
      (WaylandVsyncSource::WaylandDisplay*)data;
  wl_callback_destroy(callback);
  context->FrameCallback(time);
}

static const struct wl_callback_listener WaylandVsyncSourceCallbackListener = {
    WaylandVsyncSourceCallbackHandler};

WaylandVsyncSource::WaylandDisplay::WaylandDisplay(MozContainer* container)
    : mEnabledLock("WaylandVsyncEnabledLock"),
      mIsShutdown(false),
      mVsyncEnabled(false),
      mMonitorEnabled(false),
      mCallback(nullptr),
      mContainer(container),
      mLastVsyncTimeStamp(TimeStamp::Now()) {
  MOZ_ASSERT(NS_IsMainThread());

  // We store the display here so all the frame callbacks won't have to look it
  // up all the time.
  mDisplay = WaylandDisplayGetWLDisplay();

  mVsyncRate = TimeDuration::FromMilliseconds(1000.0 / 60.0);
}

void WaylandVsyncSource::WaylandDisplay::ClearFrameCallback() {
  if (mCallback) {
    wl_callback_destroy(mCallback);
    mCallback = nullptr;
  }
}

void WaylandVsyncSource::WaylandDisplay::Refresh() {
  TimeStamp outputTimestamp;
  {
    MutexAutoLock lock(mEnabledLock);
    if (!mMonitorEnabled || !mVsyncEnabled || mCallback) {
      // We don't need to do anything because:
      // * We are unwanted by our widget or monitor, or
      // * The last frame callback hasn't yet run to see that it had been shut
      //   down, so we can reuse it after having set mVsyncEnabled to true.
      return;
    }

    struct wl_surface* surface = moz_container_wayland_surface_lock(mContainer);
    if (!surface) {
      // The surface hasn't been created yet. Try again when the surface is
      // ready.
      RefPtr<WaylandVsyncSource::WaylandDisplay> self(this);
      moz_container_wayland_add_initial_draw_callback(
          mContainer, [self]() -> void { self->Refresh(); });
      return;
    }
    moz_container_wayland_surface_unlock(mContainer, &surface);

    // Vsync is enabled, but we don't have a callback configured. Set one up so
    // we can get to work.
    SetupFrameCallback();
    mLastVsyncTimeStamp = TimeStamp::Now();
    outputTimestamp = mLastVsyncTimeStamp + GetVsyncRate();
  }
  NotifyVsync(mLastVsyncTimeStamp, outputTimestamp);
}

void WaylandVsyncSource::WaylandDisplay::EnableMonitor() {
  {
    MutexAutoLock lock(mEnabledLock);
    if (mMonitorEnabled) {
      return;
    }
    mMonitorEnabled = true;
  }
  Refresh();
}

void WaylandVsyncSource::WaylandDisplay::DisableMonitor() {
  MutexAutoLock lock(mEnabledLock);
  if (!mMonitorEnabled) {
    return;
  }
  mMonitorEnabled = false;
  ClearFrameCallback();
}

void WaylandVsyncSource::WaylandDisplay::SetupFrameCallback() {
  MOZ_ASSERT(mCallback == nullptr);
  struct wl_surface* surface = moz_container_wayland_surface_lock(mContainer);
  if (!surface) {
    // We don't have a surface, either due to being called before it was made
    // available in the mozcontainer, or after it was destroyed. We're all done
    // regardless.
    ClearFrameCallback();
    return;
  }

  mCallback = wl_surface_frame(surface);
  wl_callback_add_listener(mCallback, &WaylandVsyncSourceCallbackListener,
                           this);
  wl_surface_commit(surface);
  wl_display_flush(mDisplay);
  moz_container_wayland_surface_unlock(mContainer, &surface);
}

void WaylandVsyncSource::WaylandDisplay::FrameCallback(uint32_t timestampTime) {
  TimeStamp outputTimestamp;
  {
    MutexAutoLock lock(mEnabledLock);
    mCallback = nullptr;

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
    outputTimestamp = vsyncTimestamp + GetVsyncRate();
  }
  NotifyVsync(mLastVsyncTimeStamp, outputTimestamp);
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
  {
    MutexAutoLock lock(mEnabledLock);
    if (mVsyncEnabled || mIsShutdown) {
      return;
    }
    mVsyncEnabled = true;
  }
  Refresh();
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
  MutexAutoLock lock(mEnabledLock);
  mIsShutdown = true;
  mVsyncEnabled = false;
  ClearFrameCallback();
}

}  // namespace mozilla

#endif  // MOZ_WAYLAND
