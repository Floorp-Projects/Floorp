/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"

namespace mozilla {
namespace widget {

#define GBMLIB_NAME "libgbm.so.1"
#define DRMLIB_NAME "libdrm.so.2"

#define DMABUF_PREF "widget.wayland_dmabuf_backend.enabled"
// See WindowSurfaceWayland::RenderingCacheMode for details.
#define CACHE_MODE_PREF "widget.wayland_cache_mode"

bool nsWaylandDisplay::mIsDMABufEnabled = false;
// -1 mean the pref was not loaded yet
int nsWaylandDisplay::mIsDMABufPrefState = -1;
bool nsWaylandDisplay::mIsDMABufConfigured = false;
int nsWaylandDisplay::mRenderingCacheModePref = -1;

wl_display* WaylandDisplayGetWLDisplay(GdkDisplay* aGdkDisplay) {
  if (!aGdkDisplay) {
    aGdkDisplay = gdk_display_get_default();
  }

  // Available as of GTK 3.8+
  static auto sGdkWaylandDisplayGetWlDisplay = (wl_display * (*)(GdkDisplay*))
      dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_display");
  return sGdkWaylandDisplayGetWlDisplay(aGdkDisplay);
}

// nsWaylandDisplay needs to be created for each calling thread(main thread,
// compositor thread and render thread)
#define MAX_DISPLAY_CONNECTIONS 3

static nsWaylandDisplay* gWaylandDisplays[MAX_DISPLAY_CONNECTIONS];
static StaticMutex gWaylandDisplaysMutex;

void WaylandDisplayShutdown() {
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  for (auto& display : gWaylandDisplays) {
    if (display) {
      display->Shutdown();
    }
  }
}

static void ReleaseDisplaysAtExit() {
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
  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
  for (auto& display : gWaylandDisplays) {
    if (display && display->GetDispatcherThreadLoop()) {
      display->GetDispatcherThreadLoop()->PostTask(NewRunnableFunction(
          "WaylandDisplayDispatch", &DispatchDisplay, display));
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
    if (!aGdkDisplay) {
      return nullptr;
    }
  }

  StaticMutexAutoLock lock(gWaylandDisplaysMutex);
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

void nsWaylandDisplay::SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf) {
  mDmabuf = aDmabuf;
}

GbmFormat* nsWaylandDisplay::GetGbmFormat(bool aHasAlpha) {
  GbmFormat* format = aHasAlpha ? &mARGBFormat : &mXRGBFormat;
  return format->mIsSupported ? format : nullptr;
}

GbmFormat* nsWaylandDisplay::GetExactGbmFormat(int aFormat) {
  if (aFormat == mARGBFormat.mFormat) {
    return &mARGBFormat;
  } else if (aFormat == mXRGBFormat.mFormat) {
    return &mXRGBFormat;
  }

  return nullptr;
}

void nsWaylandDisplay::AddFormatModifier(bool aHasAlpha, int aFormat,
                                         uint32_t mModifierHi,
                                         uint32_t mModifierLo) {
  GbmFormat* format = aHasAlpha ? &mARGBFormat : &mXRGBFormat;
  format->mIsSupported = true;
  format->mHasAlpha = aHasAlpha;
  format->mFormat = aFormat;
  format->mModifiersCount++;
  format->mModifiers =
      (uint64_t*)realloc(format->mModifiers,
                         format->mModifiersCount * sizeof(*format->mModifiers));
  format->mModifiers[format->mModifiersCount - 1] =
      ((uint64_t)mModifierHi << 32) | mModifierLo;
}

static void dmabuf_modifiers(void* data,
                             struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                             uint32_t format, uint32_t modifier_hi,
                             uint32_t modifier_lo) {
  auto display = reinterpret_cast<nsWaylandDisplay*>(data);
  switch (format) {
    case GBM_FORMAT_ARGB8888:
      display->AddFormatModifier(true, format, modifier_hi, modifier_lo);
      break;
    case GBM_FORMAT_XRGB8888:
      display->AddFormatModifier(false, format, modifier_hi, modifier_lo);
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
    display->SetDmabuf(dmabuf);
    zwp_linux_dmabuf_v1_add_listener(dmabuf, &dmabuf_listener, data);
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

bool nsWaylandDisplay::ConfigureGbm() {
  if (!nsGbmLib::IsAvailable()) {
    return false;
  }

  // TODO - Better DRM device detection/configuration.
  const char* drm_render_node = getenv("MOZ_WAYLAND_DRM_DEVICE");
  if (!drm_render_node) {
    drm_render_node = "/dev/dri/renderD128";
  }

  mGbmFd = open(drm_render_node, O_RDWR);
  if (mGbmFd < 0) {
    NS_WARNING(
        nsPrintfCString("Failed to open drm render node %s\n", drm_render_node)
            .get());
    return false;
  }

  mGbmDevice = nsGbmLib::CreateDevice(mGbmFd);
  if (mGbmDevice == nullptr) {
    NS_WARNING(nsPrintfCString("Failed to create drm render device %s\n",
                               drm_render_node)
                   .get());
    close(mGbmFd);
    mGbmFd = -1;
    return false;
  }

  return true;
}

gbm_device* nsWaylandDisplay::GetGbmDevice() {
  if (!mGdmConfigured) {
    ConfigureGbm();
    mGdmConfigured = true;
  }
  return mGbmDevice;
}

int nsWaylandDisplay::GetGbmDeviceFd() {
  if (!mGdmConfigured) {
    ConfigureGbm();
    mGdmConfigured = true;
  }
  return mGbmFd;
}

nsWaylandDisplay::nsWaylandDisplay(wl_display* aDisplay)
    : mDispatcherThreadLoop(nullptr),
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
      mRegistry(nullptr),
      mDmabuf(nullptr),
      mGbmDevice(nullptr),
      mGbmFd(-1),
      mXRGBFormat({false, false, -1, nullptr, 0}),
      mARGBFormat({false, false, -1, nullptr, 0}),
      mGdmConfigured(false),
      mExplicitSync(false) {
  mRegistry = wl_display_get_registry(mDisplay);
  wl_registry_add_listener(mRegistry, &registry_listener, this);

  if (NS_IsMainThread()) {
    // We can't load the preference from compositor/render thread
    // so load all Wayland prefs here.
    if (mIsDMABufPrefState == -1) {
      mIsDMABufPrefState = Preferences::GetBool(DMABUF_PREF, false);
    }
    if (mRenderingCacheModePref == -1) {
      mRenderingCacheModePref = Preferences::GetInt(CACHE_MODE_PREF, 0);
    }

    // Use default event queue in main thread operated by Gtk+.
    mEventQueue = nullptr;
    wl_display_roundtrip(mDisplay);
    wl_display_roundtrip(mDisplay);
  } else {
    mDispatcherThreadLoop = MessageLoop::current();
    mEventQueue = wl_display_create_queue(mDisplay);
    wl_proxy_set_queue((struct wl_proxy*)mRegistry, mEventQueue);
    wl_display_roundtrip_queue(mDisplay, mEventQueue);
    wl_display_roundtrip_queue(mDisplay, mEventQueue);
  }
}

void nsWaylandDisplay::Shutdown() { mDispatcherThreadLoop = nullptr; }

nsWaylandDisplay::~nsWaylandDisplay() {
  // Owned by Gtk+, we don't need to release
  mDisplay = nullptr;

  wl_registry_destroy(mRegistry);
  mRegistry = nullptr;

  if (mEventQueue) {
    wl_event_queue_destroy(mEventQueue);
    mEventQueue = nullptr;
  }
}

bool nsWaylandDisplay::IsDMABufEnabled() {
  if (mIsDMABufConfigured) {
    return mIsDMABufEnabled;
  }

  // WaylandDisplayGet() sets mIsDMABufPrefState
  nsWaylandDisplay* display = WaylandDisplayGet();
  if (!display) {
    NS_WARNING("Failed to get nsWaylandDisplay, called too early?");
    return false;
  }

  if (nsWaylandDisplay::mIsDMABufPrefState == -1) {
    MOZ_ASSERT(false,
               "We're missing nsWaylandDisplay preference configuration!");
    return false;
  }

  mIsDMABufConfigured = true;
  if (!nsWaylandDisplay::mIsDMABufPrefState) {
    // Disabled by user, just quit.
    return false;
  }

  if (!display->ConfigureGbm()) {
    NS_WARNING("Failed to create GbmDevice, DMABUF/DRM won't be available!");
    return false;
  }

  if (!display->GetGbmFormat(/* aHasAlpha */ false) ||
      !display->GetGbmFormat(/* aHasAlpha */ true)) {
    NS_WARNING(
        "Failed to create obtain pixel format, DMABUF/DRM won't be available!");
    return false;
  }

  mIsDMABufEnabled = true;
  return true;
}

void* nsGbmLib::sGbmLibHandle = nullptr;
void* nsGbmLib::sXf86DrmLibHandle = nullptr;
bool nsGbmLib::sLibLoaded = false;
CreateDeviceFunc nsGbmLib::sCreateDevice;
CreateFunc nsGbmLib::sCreate;
ImportFunc nsGbmLib::sImport;
CreateWithModifiersFunc nsGbmLib::sCreateWithModifiers;
GetModifierFunc nsGbmLib::sGetModifier;
GetStrideFunc nsGbmLib::sGetStride;
GetFdFunc nsGbmLib::sGetFd;
DestroyFunc nsGbmLib::sDestroy;
MapFunc nsGbmLib::sMap;
UnmapFunc nsGbmLib::sUnmap;
GetPlaneCountFunc nsGbmLib::sGetPlaneCount;
GetHandleForPlaneFunc nsGbmLib::sGetHandleForPlane;
GetStrideForPlaneFunc nsGbmLib::sGetStrideForPlane;
GetOffsetFunc nsGbmLib::sGetOffset;
DeviceIsFormatSupportedFunc nsGbmLib::sDeviceIsFormatSupported;
DrmPrimeHandleToFDFunc nsGbmLib::sDrmPrimeHandleToFD;

bool nsGbmLib::IsAvailable() {
  if (!Load()) {
    return false;
  }
  return sCreateDevice != nullptr && sCreate != nullptr &&
         sCreateWithModifiers != nullptr && sImport != nullptr &&
         sGetModifier != nullptr && sGetStride != nullptr &&
         sGetFd != nullptr && sDestroy != nullptr && sMap != nullptr &&
         sUnmap != nullptr && sGetPlaneCount != nullptr &&
         sGetHandleForPlane != nullptr && sGetStrideForPlane != nullptr &&
         sGetOffset != nullptr && sDeviceIsFormatSupported != nullptr &&
         sDrmPrimeHandleToFD != nullptr;
}

bool nsGbmLib::IsModifierAvailable() {
  // Disable the modifiers for now. We may use modifiers for 3D rendering
  // only but not for cairo/skia backends which are used now.
  return false;
}

bool nsGbmLib::Load() {
  if (!sGbmLibHandle && !sLibLoaded) {
    sLibLoaded = true;

    sGbmLibHandle = dlopen(GBMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
    if (!sGbmLibHandle) {
      NS_WARNING(nsPrintfCString("Failed to load %s, dmabuf isn't available.\n",
                                 GBMLIB_NAME)
                     .get());
      return false;
    }

    sCreateDevice = (CreateDeviceFunc)dlsym(sGbmLibHandle, "gbm_create_device");
    sCreate = (CreateFunc)dlsym(sGbmLibHandle, "gbm_bo_create");
    sCreateWithModifiers = (CreateWithModifiersFunc)dlsym(
        sGbmLibHandle, "gbm_bo_create_with_modifiers");
    sImport = (ImportFunc)dlsym(sGbmLibHandle, "gbm_bo_import");
    sGetModifier = (GetModifierFunc)dlsym(sGbmLibHandle, "gbm_bo_get_modifier");
    sGetStride = (GetStrideFunc)dlsym(sGbmLibHandle, "gbm_bo_get_stride");
    sGetFd = (GetFdFunc)dlsym(sGbmLibHandle, "gbm_bo_get_fd");
    sDestroy = (DestroyFunc)dlsym(sGbmLibHandle, "gbm_bo_destroy");
    sMap = (MapFunc)dlsym(sGbmLibHandle, "gbm_bo_map");
    sUnmap = (UnmapFunc)dlsym(sGbmLibHandle, "gbm_bo_unmap");
    sGetPlaneCount =
        (GetPlaneCountFunc)dlsym(sGbmLibHandle, "gbm_bo_get_plane_count");
    sGetHandleForPlane = (GetHandleForPlaneFunc)dlsym(
        sGbmLibHandle, "gbm_bo_get_handle_for_plane");
    sGetStrideForPlane = (GetStrideForPlaneFunc)dlsym(
        sGbmLibHandle, "gbm_bo_get_stride_for_plane");
    sGetOffset = (GetOffsetFunc)dlsym(sGbmLibHandle, "gbm_bo_get_offset");
    sDeviceIsFormatSupported = (DeviceIsFormatSupportedFunc)dlsym(
        sGbmLibHandle, "gbm_device_is_format_supported");

    sXf86DrmLibHandle = dlopen(DRMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
    if (sXf86DrmLibHandle) {
      sDrmPrimeHandleToFD = (DrmPrimeHandleToFDFunc)dlsym(sXf86DrmLibHandle,
                                                          "drmPrimeHandleToFD");
      if (!sDrmPrimeHandleToFD) {
        NS_WARNING(nsPrintfCString(
                       "Failed to load %s, gbm modifiers are not available.\n",
                       DRMLIB_NAME)
                       .get());
      }
    }
  }

  return sGbmLibHandle;
}

}  // namespace widget
}  // namespace mozilla
