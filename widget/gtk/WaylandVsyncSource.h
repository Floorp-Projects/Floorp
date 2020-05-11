/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WaylandVsyncSource_h_
#define _WaylandVsyncSource_h_

#include "mozilla/RefPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "mozcontainer.h"
#include "VsyncSource.h"
#include "base/thread.h"
#include "nsWaylandDisplay.h"

namespace mozilla {

/*
 * WaylandVsyncSource
 *
 * This class provides a per-widget VsyncSource under Wayland, emulated using
 * frame callbacks on the widget surface with empty surface commits.
 *
 * Wayland does not expose vsync/vblank, as it considers that an implementation
 * detail the clients should not concern themselves with. Instead, frame
 * callbacks are provided whenever the compositor believes it is a good time to
 * start drawing the next frame for a particular surface, giving us as much
 * time as possible to do so.
 *
 * Note that the compositor sends frame callbacks only when it sees fit, and
 * when that may be is entirely up to the compositor. One cannot expect a
 * certain rate of callbacks, or any callbacks at all. Examples of common
 * variations would be surfaces moved between outputs with different refresh
 * rates, and surfaces that are hidden and therefore do not receieve any
 * callbacks at all. Other hypothetical scenarios of variation could be
 * throttling to conserve power, or because a user has requested it.
 *
 */
class WaylandVsyncSource final : public gfx::VsyncSource {
 public:
  explicit WaylandVsyncSource(MozContainer* container) {
    MOZ_ASSERT(NS_IsMainThread());
    mGlobalDisplay = new WaylandDisplay(container);
  }

  virtual ~WaylandVsyncSource() { MOZ_ASSERT(NS_IsMainThread()); }

  virtual Display& GetGlobalDisplay() override { return *mGlobalDisplay; }

  struct WaylandFrameCallbackContext;

  class WaylandDisplay final : public mozilla::gfx::VsyncSource::Display {
   public:
    explicit WaylandDisplay(MozContainer* container);

    bool Setup();
    void EnableMonitor();
    void DisableMonitor();

    void FrameCallback();
    void Notify();

    virtual void EnableVsync() override;

    virtual void DisableVsync() override;

    virtual bool IsVsyncEnabled() override;

    virtual void Shutdown() override;

   private:
    virtual ~WaylandDisplay() = default;
    void Loop();
    void SetupFrameCallback();
    void ClearFrameCallback();

    base::Thread mThread;
    RefPtr<Runnable> mTask;
    WaylandFrameCallbackContext* mCallbackContext;
    Monitor mNotifyThreadMonitor;
    Mutex mEnabledLock;
    bool mVsyncEnabled;
    bool mMonitorEnabled;
    bool mShutdown;
    struct wl_display* mDisplay;
    MozContainer* mContainer;
  };

  // The WaylandFrameCallbackContext is a context owned by the frame callbacks.
  // It is created by the display, but deleted by the frame callbacks on the
  // next callback after being disabled.
  struct WaylandFrameCallbackContext {
    explicit WaylandFrameCallbackContext(
        WaylandVsyncSource::WaylandDisplay* aDisplay)
        : mEnabled(true), mDisplay(aDisplay) {}
    bool mEnabled;
    WaylandVsyncSource::WaylandDisplay* mDisplay;
  };

 private:
  // We need a refcounted VsyncSource::Display to use chromium IPC runnables.
  RefPtr<WaylandDisplay> mGlobalDisplay;
};

}  // namespace mozilla

#endif  // _WaylandVsyncSource_h_
