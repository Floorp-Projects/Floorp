/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"

#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_widget.h"

namespace mozilla {
namespace widget {

// nsWaylandDisplay needs to be created for each calling thread(main thread,
// compositor thread and render thread)
#define MAX_DISPLAY_CONNECTIONS 10

// An array of active wayland displays. We need a display for every thread
// where is wayland interface used as we need to dispatch waylands events
// there.
static RefPtr<nsWaylandDisplay> gWaylandDisplays[MAX_DISPLAY_CONNECTIONS];
static StaticMutex gWaylandDisplayArrayWriteMutex;

// Dispatch events to Compositor/Render queues
void WaylandDispatchDisplays() {
  MOZ_ASSERT(NS_IsMainThread(),
             "WaylandDispatchDisplays() is supposed to run in main thread");
  for (auto& display : gWaylandDisplays) {
    if (display) {
      display->DispatchEventQueue();
    }
  }
}

void WaylandDisplayRelease() {
  StaticMutexAutoLock lock(gWaylandDisplayArrayWriteMutex);
  for (auto& display : gWaylandDisplays) {
    if (display) {
      display = nullptr;
    }
  }
}

// Get WaylandDisplay for given wl_display and actual calling thread.
RefPtr<nsWaylandDisplay> WaylandDisplayGet(GdkDisplay* aGdkDisplay) {
  wl_display* waylandDisplay = WaylandDisplayGetWLDisplay(aGdkDisplay);
  if (!waylandDisplay) {
    return nullptr;
  }

  // Search existing display connections for wl_display:thread combination.
  for (auto& display : gWaylandDisplays) {
    if (display && display->Matches(waylandDisplay)) {
      return display;
    }
  }

  StaticMutexAutoLock arrayLock(gWaylandDisplayArrayWriteMutex);
  for (auto& display : gWaylandDisplays) {
    if (display == nullptr) {
      display = new nsWaylandDisplay(waylandDisplay);
      return display;
    }
  }

  MOZ_CRASH("There's too many wayland display conections!");
  return nullptr;
}

wl_display* WaylandDisplayGetWLDisplay(GdkDisplay* aGdkDisplay) {
  if (!aGdkDisplay) {
    aGdkDisplay = gdk_display_get_default();
    if (!aGdkDisplay || GDK_IS_X11_DISPLAY(aGdkDisplay)) {
      return nullptr;
    }
  }

  return gdk_wayland_display_get_wl_display(aGdkDisplay);
}

void nsWaylandDisplay::SetShm(wl_shm* aShm) { mShm = aShm; }

void nsWaylandDisplay::SetCompositor(wl_compositor* aCompositor) {
  mCompositor = aCompositor;
}

void nsWaylandDisplay::SetSubcompositor(wl_subcompositor* aSubcompositor) {
  mSubcompositor = aSubcompositor;
}

void nsWaylandDisplay::SetDataDeviceManager(
    wl_data_device_manager* aDataDeviceManager) {
  mDataDeviceManager = aDataDeviceManager;
}

void nsWaylandDisplay::SetSeat(wl_seat* aSeat) { mSeat = aSeat; }

void nsWaylandDisplay::SetPrimarySelectionDeviceManager(
    gtk_primary_selection_device_manager* aPrimarySelectionDeviceManager) {
  mPrimarySelectionDeviceManagerGtk = aPrimarySelectionDeviceManager;
}

void nsWaylandDisplay::SetPrimarySelectionDeviceManager(
    zwp_primary_selection_device_manager_v1* aPrimarySelectionDeviceManager) {
  mPrimarySelectionDeviceManagerZwpV1 = aPrimarySelectionDeviceManager;
}

void nsWaylandDisplay::SetIdleInhibitManager(
    zwp_idle_inhibit_manager_v1* aIdleInhibitManager) {
  mIdleInhibitManager = aIdleInhibitManager;
}

static void global_registry_handler(void* data, wl_registry* registry,
                                    uint32_t id, const char* interface,
                                    uint32_t version) {
  auto* display = static_cast<nsWaylandDisplay*>(data);
  if (!display) {
    return;
  }

  if (strcmp(interface, "wl_shm") == 0) {
    auto* shm = WaylandRegistryBind<wl_shm>(registry, id, &wl_shm_interface, 1);
    wl_proxy_set_queue((struct wl_proxy*)shm, display->GetEventQueue());
    display->SetShm(shm);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    int data_device_manager_version = MIN(version, 3);
    auto* data_device_manager = WaylandRegistryBind<wl_data_device_manager>(
        registry, id, &wl_data_device_manager_interface,
        data_device_manager_version);
    wl_proxy_set_queue((struct wl_proxy*)data_device_manager,
                       display->GetEventQueue());
    display->SetDataDeviceManager(data_device_manager);
  } else if (strcmp(interface, "wl_seat") == 0) {
    auto* seat =
        WaylandRegistryBind<wl_seat>(registry, id, &wl_seat_interface, 1);
    wl_proxy_set_queue((struct wl_proxy*)seat, display->GetEventQueue());
    display->SetSeat(seat);
  } else if (strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    auto* primary_selection_device_manager =
        WaylandRegistryBind<gtk_primary_selection_device_manager>(
            registry, id, &gtk_primary_selection_device_manager_interface, 1);
    wl_proxy_set_queue((struct wl_proxy*)primary_selection_device_manager,
                       display->GetEventQueue());
    display->SetPrimarySelectionDeviceManager(primary_selection_device_manager);
  } else if (strcmp(interface, "zwp_primary_selection_device_manager_v1") ==
             0) {
    auto* primary_selection_device_manager =
        WaylandRegistryBind<gtk_primary_selection_device_manager>(
            registry, id, &zwp_primary_selection_device_manager_v1_interface,
            1);
    wl_proxy_set_queue((struct wl_proxy*)primary_selection_device_manager,
                       display->GetEventQueue());
    display->SetPrimarySelectionDeviceManager(primary_selection_device_manager);
  } else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0) {
    auto* idle_inhibit_manager =
        WaylandRegistryBind<zwp_idle_inhibit_manager_v1>(
            registry, id, &zwp_idle_inhibit_manager_v1_interface, 1);
    wl_proxy_set_queue((struct wl_proxy*)idle_inhibit_manager,
                       display->GetEventQueue());
    display->SetIdleInhibitManager(idle_inhibit_manager);
  } else if (strcmp(interface, "wl_compositor") == 0) {
    // Requested wl_compositor version 4 as we need wl_surface_damage_buffer().
    auto* compositor = WaylandRegistryBind<wl_compositor>(
        registry, id, &wl_compositor_interface, 4);
    wl_proxy_set_queue((struct wl_proxy*)compositor, display->GetEventQueue());
    display->SetCompositor(compositor);
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    auto* subcompositor = WaylandRegistryBind<wl_subcompositor>(
        registry, id, &wl_subcompositor_interface, 1);
    wl_proxy_set_queue((struct wl_proxy*)subcompositor,
                       display->GetEventQueue());
    display->SetSubcompositor(subcompositor);
  }
}

static void global_registry_remover(void* data, wl_registry* registry,
                                    uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

bool nsWaylandDisplay::DispatchEventQueue() {
  if (mEventQueue) {
    wl_display_dispatch_queue_pending(mDisplay, mEventQueue);
  }
  return true;
}

void nsWaylandDisplay::SyncEnd() {
  wl_callback_destroy(mSyncCallback);
  mSyncCallback = nullptr;
}

static void wayland_sync_callback(void* data, struct wl_callback* callback,
                                  uint32_t time) {
  auto display = static_cast<nsWaylandDisplay*>(data);
  display->SyncEnd();
}

static const struct wl_callback_listener sync_callback_listener = {
    .done = wayland_sync_callback};

void nsWaylandDisplay::SyncBegin() {
  WaitForSyncEnd();

  // Use wl_display_sync() to synchronize wayland events.
  // See dri2_wl_swap_buffers_with_damage() from MESA
  // or wl_display_roundtrip_queue() from wayland-client.
  struct wl_display* displayWrapper =
      static_cast<wl_display*>(wl_proxy_create_wrapper((void*)mDisplay));
  if (!displayWrapper) {
    NS_WARNING("Failed to create wl_proxy wrapper!");
    return;
  }

  wl_proxy_set_queue((struct wl_proxy*)displayWrapper, mEventQueue);
  mSyncCallback = wl_display_sync(displayWrapper);
  wl_proxy_wrapper_destroy((void*)displayWrapper);

  if (!mSyncCallback) {
    NS_WARNING("Failed to create wl_display_sync callback!");
    return;
  }

  wl_callback_add_listener(mSyncCallback, &sync_callback_listener, this);
  wl_display_flush(mDisplay);
}

void nsWaylandDisplay::QueueSyncBegin() {
  RefPtr<nsWaylandDisplay> self(this);
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("nsWaylandDisplay::QueueSyncBegin",
                             [self]() -> void { self->SyncBegin(); }));
}

void nsWaylandDisplay::WaitForSyncEnd() {
  // We're done here
  if (!mSyncCallback) {
    return;
  }

  while (mSyncCallback != nullptr) {
    // TODO: wl_display_dispatch_queue() should not be called while
    // glib main loop is iterated at nsAppShell::ProcessNextNativeEvent().
    if (wl_display_dispatch_queue(mDisplay, mEventQueue) == -1) {
      NS_WARNING("wl_display_dispatch_queue failed!");
      SyncEnd();
      return;
    }
  }
}

bool nsWaylandDisplay::Matches(wl_display* aDisplay) {
  return mThreadId == PR_GetCurrentThread() && aDisplay == mDisplay;
}

nsWaylandDisplay::nsWaylandDisplay(wl_display* aDisplay, bool aLighWrapper)
    : mThreadId(PR_GetCurrentThread()),
      mDisplay(aDisplay),
      mEventQueue(nullptr),
      mDataDeviceManager(nullptr),
      mCompositor(nullptr),
      mSubcompositor(nullptr),
      mSeat(nullptr),
      mShm(nullptr),
      mSyncCallback(nullptr),
      mPrimarySelectionDeviceManagerGtk(nullptr),
      mPrimarySelectionDeviceManagerZwpV1(nullptr),
      mIdleInhibitManager(nullptr),
      mRegistry(nullptr),
      mExplicitSync(false) {
  if (!aLighWrapper) {
    mRegistry = wl_display_get_registry(mDisplay);
    wl_registry_add_listener(mRegistry, &registry_listener, this);
  }

  if (!NS_IsMainThread()) {
    mEventQueue = wl_display_create_queue(mDisplay);
    wl_proxy_set_queue((struct wl_proxy*)mRegistry, mEventQueue);
  }

  if (!aLighWrapper) {
    if (mEventQueue) {
      wl_display_roundtrip_queue(mDisplay, mEventQueue);
      wl_display_roundtrip_queue(mDisplay, mEventQueue);
    } else {
      wl_display_roundtrip(mDisplay);
      wl_display_roundtrip(mDisplay);
    }
  }
}

nsWaylandDisplay::~nsWaylandDisplay() {
  wl_registry_destroy(mRegistry);
  mRegistry = nullptr;

  if (mEventQueue) {
    wl_event_queue_destroy(mEventQueue);
    mEventQueue = nullptr;
  }
  mDisplay = nullptr;
}

}  // namespace widget
}  // namespace mozilla
