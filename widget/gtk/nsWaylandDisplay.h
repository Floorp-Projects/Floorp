/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_WAYLAND_REGISTRY_H__
#define __MOZ_WAYLAND_REGISTRY_H__

#include "mozwayland/mozwayland.h"
#include "wayland/gtk-primary-selection-client-protocol.h"

namespace mozilla {
namespace widget {

// TODO: Bug 1467125 - We need to integrate wl_display_dispatch_queue_pending()
// with compositor event loop.
#define EVENT_LOOP_DELAY (1000 / 240)

// Our general connection to Wayland display server,
// holds our display connection and runs event loop.
class nsWaylandDisplay : public nsISupports {
  NS_DECL_THREADSAFE_ISUPPORTS

 public:
  explicit nsWaylandDisplay(wl_display* aDisplay);

  bool DisplayLoop();
  bool Matches(wl_display* aDisplay);

  wl_display* GetDisplay() { return mDisplay; };
  wl_event_queue* GetEventQueue() { return mEventQueue; };
  wl_subcompositor* GetSubcompositor(void) { return mSubcompositor; };
  wl_data_device_manager* GetDataDeviceManager(void) {
    return mDataDeviceManager;
  };
  wl_seat* GetSeat(void) { return mSeat; };
  wl_shm* GetShm(void) { return mShm; };
  gtk_primary_selection_device_manager* GetPrimarySelectionDeviceManager(void) {
    return mPrimarySelectionDeviceManager;
  };

 public:
  void SetShm(wl_shm* aShm);
  void SetSubcompositor(wl_subcompositor* aSubcompositor);
  void SetDataDeviceManager(wl_data_device_manager* aDataDeviceManager);
  void SetSeat(wl_seat* aSeat);
  void SetPrimarySelectionDeviceManager(
      gtk_primary_selection_device_manager* aPrimarySelectionDeviceManager);

 private:
  virtual ~nsWaylandDisplay();

  PRThread* mThreadId;
  wl_display* mDisplay;
  wl_event_queue* mEventQueue;
  wl_data_device_manager* mDataDeviceManager;
  wl_subcompositor* mSubcompositor;
  wl_seat* mSeat;
  wl_shm* mShm;
  gtk_primary_selection_device_manager* mPrimarySelectionDeviceManager;
};

nsWaylandDisplay* WaylandDisplayGet(GdkDisplay* aGdkDisplay = nullptr);
void WaylandDisplayRelease(nsWaylandDisplay* aDisplay);

}  // namespace widget
}  // namespace mozilla

#endif  // __MOZ_WAYLAND_REGISTRY_H__
