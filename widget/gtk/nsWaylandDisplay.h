/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_WAYLAND_DISPLAY_H__
#define __MOZ_WAYLAND_DISPLAY_H__

#include "DMABufLibWrapper.h"

#include "mozilla/widget/mozwayland.h"
#include "mozilla/widget/gbm.h"
#include "mozilla/widget/gtk-primary-selection-client-protocol.h"
#include "mozilla/widget/idle-inhibit-unstable-v1-client-protocol.h"
#include "mozilla/widget/relative-pointer-unstable-v1-client-protocol.h"
#include "mozilla/widget/pointer-constraints-unstable-v1-client-protocol.h"
#include "mozilla/widget/linux-dmabuf-unstable-v1-client-protocol.h"
#include "mozilla/widget/primary-selection-unstable-v1-client-protocol.h"
#include "mozilla/widget/viewporter-client-protocol.h"

namespace mozilla {
namespace widget {

// Our general connection to Wayland display server,
// holds our display connection and runs event loop.
// We have a global nsWaylandDisplay object for each thread.
class nsWaylandDisplay {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsWaylandDisplay)

  // Create nsWaylandDisplay object on top of native Wayland wl_display
  // connection. When aLighWrapper is set we don't get wayland registry
  // objects and only event loop is provided.
  explicit nsWaylandDisplay(wl_display* aDisplay, bool aLighWrapper = false);

  bool DispatchEventQueue();

  void SyncBegin();
  void QueueSyncBegin();
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
  wl_seat* GetSeat(void);
  wl_shm* GetShm(void) { return mShm; };
  gtk_primary_selection_device_manager* GetPrimarySelectionDeviceManagerGtk(
      void) {
    return mPrimarySelectionDeviceManagerGtk;
  };
  zwp_primary_selection_device_manager_v1*
  GetPrimarySelectionDeviceManagerZwpV1(void) {
    return mPrimarySelectionDeviceManagerZwpV1;
  };
  zwp_idle_inhibit_manager_v1* GetIdleInhibitManager(void) {
    return mIdleInhibitManager;
  }
  wp_viewporter* GetViewporter(void) { return mViewporter; };
  zwp_relative_pointer_manager_v1* GetRelativePointerManager(void) {
    return mRelativePointerManager;
  }
  zwp_pointer_constraints_v1* GetPointerConstraints(void) {
    return mPointerConstraints;
  }

  bool IsMainThreadDisplay() { return mEventQueue == nullptr; }

  void SetShm(wl_shm* aShm);
  void SetCompositor(wl_compositor* aCompositor);
  void SetSubcompositor(wl_subcompositor* aSubcompositor);
  void SetDataDeviceManager(wl_data_device_manager* aDataDeviceManager);
  void SetPrimarySelectionDeviceManager(
      gtk_primary_selection_device_manager* aPrimarySelectionDeviceManager);
  void SetPrimarySelectionDeviceManager(
      zwp_primary_selection_device_manager_v1* aPrimarySelectionDeviceManager);
  void SetIdleInhibitManager(zwp_idle_inhibit_manager_v1* aIdleInhibitManager);
  void SetViewporter(wp_viewporter* aViewporter);
  void SetRelativePointerManager(
      zwp_relative_pointer_manager_v1* aRelativePointerManager);
  void SetPointerConstraints(zwp_pointer_constraints_v1* aPointerConstraints);

  bool IsExplicitSyncEnabled() { return mExplicitSync; }

 private:
  ~nsWaylandDisplay();

  PRThread* mThreadId;
  wl_display* mDisplay;
  wl_event_queue* mEventQueue;
  wl_data_device_manager* mDataDeviceManager;
  wl_compositor* mCompositor;
  wl_subcompositor* mSubcompositor;
  wl_shm* mShm;
  wl_callback* mSyncCallback;
  gtk_primary_selection_device_manager* mPrimarySelectionDeviceManagerGtk;
  zwp_primary_selection_device_manager_v1* mPrimarySelectionDeviceManagerZwpV1;
  zwp_idle_inhibit_manager_v1* mIdleInhibitManager;
  zwp_relative_pointer_manager_v1* mRelativePointerManager;
  zwp_pointer_constraints_v1* mPointerConstraints;
  wl_registry* mRegistry;
  wp_viewporter* mViewporter;
  bool mExplicitSync;
};

void WaylandDispatchDisplays();
void WaylandDisplayRelease();

RefPtr<nsWaylandDisplay> WaylandDisplayGet(GdkDisplay* aGdkDisplay = nullptr);
wl_display* WaylandDisplayGetWLDisplay(GdkDisplay* aGdkDisplay = nullptr);

}  // namespace widget
}  // namespace mozilla

template <class T>
static inline T* WaylandRegistryBind(struct wl_registry* wl_registry,
                                     uint32_t name,
                                     const struct wl_interface* interface,
                                     uint32_t version) {
  struct wl_proxy* id;

  // When libwayland-client does not provide this symbol, it will be
  // linked to the fallback in libmozwayland, which returns NULL.
  id = wl_proxy_marshal_constructor_versioned(
      (struct wl_proxy*)wl_registry, WL_REGISTRY_BIND, interface, version, name,
      interface->name, version, nullptr);

  if (id == nullptr) {
    id = wl_proxy_marshal_constructor((struct wl_proxy*)wl_registry,
                                      WL_REGISTRY_BIND, interface, name,
                                      interface->name, version, nullptr);
  }

  return reinterpret_cast<T*>(id);
}

#endif  // __MOZ_WAYLAND_DISPLAY_H__
