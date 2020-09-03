/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"
#include "mozilla/StaticPrefs_widget.h"
#include "DMABufLibWrapper.h"

namespace mozilla {
namespace widget {

wl_display* WaylandDisplayGetWLDisplay(GdkDisplay* aGdkDisplay) {
  if (!aGdkDisplay) {
    aGdkDisplay = gdk_display_get_default();
    if (!aGdkDisplay || GDK_IS_X11_DISPLAY(aGdkDisplay)) {
      return nullptr;
    }
  }

  return gdk_wayland_display_get_wl_display(aGdkDisplay);
}

// nsWaylandDisplay needs to be created for each calling thread(main thread,
// compositor thread and render thread)
#define MAX_DISPLAY_CONNECTIONS 5

static nsWaylandDisplay* gWaylandDisplays[MAX_DISPLAY_CONNECTIONS];
static StaticMutex gWaylandDisplayArrayMutex;
static StaticMutex gWaylandThreadLoopMutex;

void WaylandDisplayShutdown() {
  StaticMutexAutoLock lock(gWaylandDisplayArrayMutex);
  for (auto& display : gWaylandDisplays) {
    if (display) {
      display->ShutdownThreadLoop();
    }
  }
}

static void ReleaseDisplaysAtExit() {
  StaticMutexAutoLock lock(gWaylandDisplayArrayMutex);
  for (int i = 0; i < MAX_DISPLAY_CONNECTIONS; i++) {
    delete gWaylandDisplays[i];
    gWaylandDisplays[i] = nullptr;
  }
}

static void DispatchDisplay(nsWaylandDisplay* aDisplay) {
  aDisplay->DispatchEventQueue();
}

// Each thread which is using wayland connection (wl_display) has to operate
// its own wl_event_queue. Main Firefox thread wl_event_queue is handled
// by Gtk main loop, other threads/wl_event_queue has to be handled by us.
//
// nsWaylandDisplay is our interface to wayland compositor. It provides wayland
// global objects as we need (wl_display, wl_shm) and operates wl_event_queue on
// compositor (not the main) thread.
void WaylandDispatchDisplays() {
  StaticMutexAutoLock arrayLock(gWaylandDisplayArrayMutex);
  for (auto& display : gWaylandDisplays) {
    if (display) {
      StaticMutexAutoLock loopLock(gWaylandThreadLoopMutex);
      MessageLoop* loop = display->GetThreadLoop();
      if (loop) {
        loop->PostTask(NewRunnableFunction("WaylandDisplayDispatch",
                                           &DispatchDisplay, display));
      }
    }
  }
}

// Get WaylandDisplay for given wl_display and actual calling thread.
static nsWaylandDisplay* WaylandDisplayGetLocked(GdkDisplay* aGdkDisplay,
                                                 const StaticMutexAutoLock&) {
  wl_display* waylandDisplay = WaylandDisplayGetWLDisplay(aGdkDisplay);

  // Search existing display connections for wl_display:thread combination.
  for (auto& display : gWaylandDisplays) {
    if (display && display->Matches(waylandDisplay)) {
      return display;
    }
  }

  for (auto& display : gWaylandDisplays) {
    if (display == nullptr) {
      display = new nsWaylandDisplay(waylandDisplay);
      atexit(ReleaseDisplaysAtExit);
      return display;
    }
  }

  MOZ_CRASH("There's too many wayland display conections!");
  return nullptr;
}

nsWaylandDisplay* WaylandDisplayGet(GdkDisplay* aGdkDisplay) {
  if (!aGdkDisplay) {
    aGdkDisplay = gdk_display_get_default();
    if (!aGdkDisplay || GDK_IS_X11_DISPLAY(aGdkDisplay)) {
      return nullptr;
    }
  }

  StaticMutexAutoLock lock(gWaylandDisplayArrayMutex);
  return WaylandDisplayGetLocked(aGdkDisplay, lock);
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
  mPrimarySelectionDeviceManager = aPrimarySelectionDeviceManager;
}

void nsWaylandDisplay::SetIdleInhibitManager(
    zwp_idle_inhibit_manager_v1* aIdleInhibitManager) {
  mIdleInhibitManager = aIdleInhibitManager;
}

void nsWaylandDisplay::SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf) {
  mDmabuf = aDmabuf;
}

static void dmabuf_modifiers(void* data,
                             struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                             uint32_t format, uint32_t modifier_hi,
                             uint32_t modifier_lo) {
  switch (format) {
    case GBM_FORMAT_ARGB8888:
      GetDMABufDevice()->AddFormatModifier(true, format, modifier_hi,
                                           modifier_lo);
      break;
    case GBM_FORMAT_XRGB8888:
      GetDMABufDevice()->AddFormatModifier(false, format, modifier_hi,
                                           modifier_lo);
      break;
    default:
      break;
  }
}

static void dmabuf_format(void* data,
                          struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                          uint32_t format) {
  // XXX: deprecated
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
    dmabuf_format, dmabuf_modifiers};

static void global_registry_handler(void* data, wl_registry* registry,
                                    uint32_t id, const char* interface,
                                    uint32_t version) {
  auto display = reinterpret_cast<nsWaylandDisplay*>(data);
  if (!display) return;

  if (strcmp(interface, "wl_shm") == 0) {
    auto shm = static_cast<wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, 1));
    wl_proxy_set_queue((struct wl_proxy*)shm, display->GetEventQueue());
    display->SetShm(shm);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    int data_device_manager_version = MIN(version, 3);
    auto data_device_manager = static_cast<wl_data_device_manager*>(
        wl_registry_bind(registry, id, &wl_data_device_manager_interface,
                         data_device_manager_version));
    wl_proxy_set_queue((struct wl_proxy*)data_device_manager,
                       display->GetEventQueue());
    display->SetDataDeviceManager(data_device_manager);
  } else if (strcmp(interface, "wl_seat") == 0) {
    auto seat = static_cast<wl_seat*>(
        wl_registry_bind(registry, id, &wl_seat_interface, 1));
    wl_proxy_set_queue((struct wl_proxy*)seat, display->GetEventQueue());
    display->SetSeat(seat);
  } else if (strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    auto primary_selection_device_manager =
        static_cast<gtk_primary_selection_device_manager*>(wl_registry_bind(
            registry, id, &gtk_primary_selection_device_manager_interface, 1));
    wl_proxy_set_queue((struct wl_proxy*)primary_selection_device_manager,
                       display->GetEventQueue());
    display->SetPrimarySelectionDeviceManager(primary_selection_device_manager);
  } else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0) {
    auto idle_inhibit_manager =
        static_cast<zwp_idle_inhibit_manager_v1*>(wl_registry_bind(
            registry, id, &zwp_idle_inhibit_manager_v1_interface, 1));
    wl_proxy_set_queue((struct wl_proxy*)idle_inhibit_manager,
                       display->GetEventQueue());
    display->SetIdleInhibitManager(idle_inhibit_manager);
  } else if (strcmp(interface, "wl_compositor") == 0) {
    // Requested wl_compositor version 4 as we need wl_surface_damage_buffer().
    auto compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, 4));
    wl_proxy_set_queue((struct wl_proxy*)compositor, display->GetEventQueue());
    display->SetCompositor(compositor);
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    auto subcompositor = static_cast<wl_subcompositor*>(
        wl_registry_bind(registry, id, &wl_subcompositor_interface, 1));
    wl_proxy_set_queue((struct wl_proxy*)subcompositor,
                       display->GetEventQueue());
    display->SetSubcompositor(subcompositor);
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0 && version > 2) {
    auto dmabuf = static_cast<zwp_linux_dmabuf_v1*>(
        wl_registry_bind(registry, id, &zwp_linux_dmabuf_v1_interface, 3));
    LOGDMABUF(("zwp_linux_dmabuf_v1 is available."));
    display->SetDmabuf(dmabuf);
    // Get formats for main thread display only
    if (display->IsMainThreadDisplay()) {
      GetDMABufDevice()->ResetFormatsModifiers();
      zwp_linux_dmabuf_v1_add_listener(dmabuf, &dmabuf_listener, data);
    }
  } else if (strcmp(interface, "wl_drm") == 0) {
    LOGDMABUF(("wl_drm is available."));
  }
}

static void global_registry_remover(void* data, wl_registry* registry,
                                    uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

bool nsWaylandDisplay::DispatchEventQueue() {
  wl_display_dispatch_queue_pending(mDisplay, mEventQueue);
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

void nsWaylandDisplay::WaitForSyncEnd() {
  // We're done here
  if (!mSyncCallback) {
    return;
  }

  while (mSyncCallback != nullptr) {
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

class nsWaylandDisplayLoopObserver : public MessageLoop::DestructionObserver {
 public:
  explicit nsWaylandDisplayLoopObserver(nsWaylandDisplay* aWaylandDisplay)
      : mDisplay(aWaylandDisplay){};
  virtual void WillDestroyCurrentMessageLoop() override {
    mDisplay->ShutdownThreadLoop();
    mDisplay = nullptr;
    delete this;
  }

 private:
  nsWaylandDisplay* mDisplay;
};

nsWaylandDisplay::nsWaylandDisplay(wl_display* aDisplay, bool aLighWrapper)
    : mThreadLoop(nullptr),
      mThreadId(PR_GetCurrentThread()),
      mDisplay(aDisplay),
      mEventQueue(nullptr),
      mDataDeviceManager(nullptr),
      mCompositor(nullptr),
      mSubcompositor(nullptr),
      mSeat(nullptr),
      mShm(nullptr),
      mSyncCallback(nullptr),
      mPrimarySelectionDeviceManager(nullptr),
      mIdleInhibitManager(nullptr),
      mRegistry(nullptr),
      mDmabuf(nullptr),
      mExplicitSync(false) {
  if (!aLighWrapper) {
    mRegistry = wl_display_get_registry(mDisplay);
    wl_registry_add_listener(mRegistry, &registry_listener, this);
  }

  if (!NS_IsMainThread()) {
    mThreadLoop = MessageLoop::current();
    if (mThreadLoop) {
      auto observer = new nsWaylandDisplayLoopObserver(this);
      mThreadLoop->AddDestructionObserver(observer);
    }
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

void nsWaylandDisplay::ShutdownThreadLoop() {
  StaticMutexAutoLock lock(gWaylandThreadLoopMutex);
  mThreadLoop = nullptr;
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
