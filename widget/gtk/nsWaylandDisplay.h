/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_WAYLAND_DISPLAY_H__
#define __MOZ_WAYLAND_DISPLAY_H__

#include "mozilla/widget/mozwayland.h"
#include "mozilla/widget/gtk-primary-selection-client-protocol.h"
#include "mozilla/widget/idle-inhibit-unstable-v1-client-protocol.h"

#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc
#include "mozilla/StaticMutex.h"

#include "mozilla/widget/gbm.h"
#include "mozilla/widget/linux-dmabuf-unstable-v1-client-protocol.h"

namespace mozilla {
namespace widget {

// Our general connection to Wayland display server,
// holds our display connection and runs event loop.
// We have a global nsWaylandDisplay object for each thread,
// recently we have three for main, compositor and render one.
class nsWaylandDisplay {
 public:
  // Create nsWaylandDisplay object on top of native Wayland wl_display
  // connection. When aLighWrapper is set we don't get wayland registry
  // objects and only event loop is provided.
  explicit nsWaylandDisplay(wl_display* aDisplay, bool aLighWrapper = false);
  virtual ~nsWaylandDisplay();

  bool DispatchEventQueue();

  void SyncBegin();
  void SyncEnd();
  void WaitForSyncEnd();

  bool Matches(wl_display* aDisplay);

  wl_display* GetDisplay() { return mDisplay; };
  wl_event_queue* GetEventQueue() { return mEventQueue; };
  wl_compositor* GetCompositor(void) { return mCompositor; };
  wl_subcompositor* GetSubcompositor(void) { return mSubcompositor; };
  wl_data_device_manager* GetDataDeviceManager(void) {
    return mDataDeviceManager;
  };
  wl_seat* GetSeat(void) { return mSeat; };
  wl_shm* GetShm(void) { return mShm; };
  gtk_primary_selection_device_manager* GetPrimarySelectionDeviceManager(void) {
    return mPrimarySelectionDeviceManager;
  };
  zwp_idle_inhibit_manager_v1* GetIdleInhibitManager(void) {
    return mIdleInhibitManager;
  }

  bool IsMainThreadDisplay() { return mEventQueue == nullptr; }

  void SetShm(wl_shm* aShm);
  void SetCompositor(wl_compositor* aCompositor);
  void SetSubcompositor(wl_subcompositor* aSubcompositor);
  void SetDataDeviceManager(wl_data_device_manager* aDataDeviceManager);
  void SetSeat(wl_seat* aSeat);
  void SetPrimarySelectionDeviceManager(
      gtk_primary_selection_device_manager* aPrimarySelectionDeviceManager);
  void SetIdleInhibitManager(zwp_idle_inhibit_manager_v1* aIdleInhibitManager);

  MessageLoop* GetThreadLoop() { return mThreadLoop; }
  void ShutdownThreadLoop();

  void SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf);
  zwp_linux_dmabuf_v1* GetDmabuf() { return mDmabuf; };
  bool IsExplicitSyncEnabled() { return mExplicitSync; }

 private:
  MessageLoop* mThreadLoop;
  PRThread* mThreadId;
  wl_display* mDisplay;
  wl_event_queue* mEventQueue;
  wl_data_device_manager* mDataDeviceManager;
  wl_compositor* mCompositor;
  wl_subcompositor* mSubcompositor;
  wl_seat* mSeat;
  wl_shm* mShm;
  wl_callback* mSyncCallback;
  gtk_primary_selection_device_manager* mPrimarySelectionDeviceManager;
  zwp_idle_inhibit_manager_v1* mIdleInhibitManager;
  wl_registry* mRegistry;
  zwp_linux_dmabuf_v1* mDmabuf;
  bool mExplicitSync;
};

void WaylandDispatchDisplays();
void WaylandDisplayShutdown();
nsWaylandDisplay* WaylandDisplayGet(GdkDisplay* aGdkDisplay = nullptr);
wl_display* WaylandDisplayGetWLDisplay(GdkDisplay* aGdkDisplay = nullptr);

}  // namespace widget
}  // namespace mozilla

#endif  // __MOZ_WAYLAND_DISPLAY_H__
