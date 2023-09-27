/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMABufLibWrapper.h"
#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#endif
#include "base/message_loop.h"  // for MessageLoop
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"
#include "WidgetUtilsGtk.h"
#include "gfxConfig.h"
#include "nsIGfxInfo.h"
#include "mozilla/Components.h"
#include "mozilla/ClearOnShutdown.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <mutex>
#include <unistd.h>
#include "gbm.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

bool sUseWebGLDmabufBackend = true;

#define GBMLIB_NAME "libgbm.so.1"
#define DRMLIB_NAME "libdrm.so.2"

// Use static lock to protect dri operation as
// gbm_dri.c is not thread safe.
// https://gitlab.freedesktop.org/mesa/mesa/-/issues/4422
mozilla::StaticMutex GbmLib::sDRILock MOZ_UNANNOTATED;

bool GbmLib::sLoaded = false;
void* GbmLib::sGbmLibHandle = nullptr;
void* GbmLib::sXf86DrmLibHandle = nullptr;
CreateDeviceFunc GbmLib::sCreateDevice;
DestroyDeviceFunc GbmLib::sDestroyDevice;
CreateFunc GbmLib::sCreate;
CreateWithModifiersFunc GbmLib::sCreateWithModifiers;
GetModifierFunc GbmLib::sGetModifier;
GetStrideFunc GbmLib::sGetStride;
GetFdFunc GbmLib::sGetFd;
DestroyFunc GbmLib::sDestroy;
MapFunc GbmLib::sMap;
UnmapFunc GbmLib::sUnmap;
GetPlaneCountFunc GbmLib::sGetPlaneCount;
GetHandleForPlaneFunc GbmLib::sGetHandleForPlane;
GetStrideForPlaneFunc GbmLib::sGetStrideForPlane;
GetOffsetFunc GbmLib::sGetOffset;
DeviceIsFormatSupportedFunc GbmLib::sDeviceIsFormatSupported;
DrmPrimeHandleToFDFunc GbmLib::sDrmPrimeHandleToFD;
CreateSurfaceFunc GbmLib::sCreateSurface;
DestroySurfaceFunc GbmLib::sDestroySurface;

bool GbmLib::IsLoaded() {
  return sCreateDevice != nullptr && sDestroyDevice != nullptr &&
         sCreate != nullptr && sCreateWithModifiers != nullptr &&
         sGetModifier != nullptr && sGetStride != nullptr &&
         sGetFd != nullptr && sDestroy != nullptr && sMap != nullptr &&
         sUnmap != nullptr && sGetPlaneCount != nullptr &&
         sGetHandleForPlane != nullptr && sGetStrideForPlane != nullptr &&
         sGetOffset != nullptr && sDeviceIsFormatSupported != nullptr &&
         sDrmPrimeHandleToFD != nullptr && sCreateSurface != nullptr &&
         sDestroySurface != nullptr;
}

bool GbmLib::Load() {
  static bool sTriedToLoad = false;
  if (sTriedToLoad) {
    return sLoaded;
  }

  sTriedToLoad = true;

  MOZ_ASSERT(!sGbmLibHandle);
  MOZ_ASSERT(!sLoaded);

  LOGDMABUF(("Loading DMABuf system library %s ...\n", GBMLIB_NAME));

  sGbmLibHandle = dlopen(GBMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
  if (!sGbmLibHandle) {
    LOGDMABUF(("Failed to load %s, dmabuf isn't available.\n", GBMLIB_NAME));
    return false;
  }

  sCreateDevice = (CreateDeviceFunc)dlsym(sGbmLibHandle, "gbm_create_device");
  sDestroyDevice =
      (DestroyDeviceFunc)dlsym(sGbmLibHandle, "gbm_device_destroy");
  sCreate = (CreateFunc)dlsym(sGbmLibHandle, "gbm_bo_create");
  sCreateWithModifiers = (CreateWithModifiersFunc)dlsym(
      sGbmLibHandle, "gbm_bo_create_with_modifiers");
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
  sCreateSurface =
      (CreateSurfaceFunc)dlsym(sGbmLibHandle, "gbm_surface_create");
  sDestroySurface =
      (DestroySurfaceFunc)dlsym(sGbmLibHandle, "gbm_surface_destroy");

  sXf86DrmLibHandle = dlopen(DRMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
  if (!sXf86DrmLibHandle) {
    LOGDMABUF(("Failed to load %s, dmabuf isn't available.\n", DRMLIB_NAME));
    return false;
  }
  sDrmPrimeHandleToFD =
      (DrmPrimeHandleToFDFunc)dlsym(sXf86DrmLibHandle, "drmPrimeHandleToFD");
  sLoaded = IsLoaded();
  if (!sLoaded) {
    LOGDMABUF(("Failed to load all symbols from %s\n", GBMLIB_NAME));
  }
  return sLoaded;
}

int DMABufDevice::GetDmabufFD(uint32_t aGEMHandle) {
  int fd;
  return GbmLib::DrmPrimeHandleToFD(mDRMFd, aGEMHandle, 0, &fd) < 0 ? -1 : fd;
}

gbm_device* DMABufDevice::GetGbmDevice() {
  std::call_once(mFlagGbmDevice, [&] {
    mGbmDevice = (mDRMFd != -1) ? GbmLib::CreateDevice(mDRMFd) : nullptr;
  });
  return mGbmDevice;
}

int DMABufDevice::OpenDRMFd() { return open(mDrmRenderNode.get(), O_RDWR); }

bool DMABufDevice::IsEnabled(nsACString& aFailureId) {
  if (mDRMFd == -1) {
    aFailureId = mFailureId;
  }
  return mDRMFd != -1;
}

DMABufDevice::DMABufDevice()
    : mXRGBFormat({true, false, GBM_FORMAT_XRGB8888, {}}),
      mARGBFormat({true, true, GBM_FORMAT_ARGB8888, {}}) {
  Configure();
}

DMABufDevice::~DMABufDevice() {
  if (mGbmDevice) {
    GbmLib::DestroyDevice(mGbmDevice);
    mGbmDevice = nullptr;
  }
  if (mDRMFd != -1) {
    close(mDRMFd);
    mDRMFd = -1;
  }
}

void DMABufDevice::Configure() {
  LOGDMABUF(("DMABufDevice::Configure()"));

  if (!GbmLib::IsAvailable()) {
    LOGDMABUF(("GbmLib is not available!"));
    mFailureId = "FEATURE_FAILURE_NO_LIBGBM";
    return;
  }

  mDrmRenderNode = nsAutoCString(getenv("MOZ_DRM_DEVICE"));
  if (mDrmRenderNode.IsEmpty()) {
    mDrmRenderNode.Assign(gfx::gfxVars::DrmRenderDevice());
  }
  if (mDrmRenderNode.IsEmpty()) {
    LOGDMABUF(("We're missing DRM render device!\n"));
    mFailureId = "FEATURE_FAILURE_NO_DRM_DEVICE";
    return;
  }

  LOGDMABUF(("Using DRM device %s", mDrmRenderNode.get()));
  mDRMFd = open(mDrmRenderNode.get(), O_RDWR);
  if (mDRMFd < 0) {
    LOGDMABUF(("Failed to open drm render node %s error %s\n",
               mDrmRenderNode.get(), strerror(errno)));
    mFailureId = "FEATURE_FAILURE_NO_DRM_DEVICE";
    return;
  }

#ifdef MOZ_WAYLAND
  LoadFormatModifiers();
#endif

  LOGDMABUF(("DMABuf is enabled"));
}

#ifdef NIGHTLY_BUILD
bool DMABufDevice::IsDMABufTexturesEnabled() {
  return gfx::gfxVars::UseDMABuf() &&
         StaticPrefs::widget_dmabuf_textures_enabled();
}
#else
bool DMABufDevice::IsDMABufTexturesEnabled() { return false; }
#endif
bool DMABufDevice::IsDMABufWebGLEnabled() {
  LOGDMABUF(
      ("DMABufDevice::IsDMABufWebGLEnabled: UseDMABuf %d "
       "sUseWebGLDmabufBackend %d "
       "widget_dmabuf_webgl_enabled %d\n",
       gfx::gfxVars::UseDMABuf(), sUseWebGLDmabufBackend,
       StaticPrefs::widget_dmabuf_webgl_enabled()));
  return gfx::gfxVars::UseDMABuf() && sUseWebGLDmabufBackend &&
         StaticPrefs::widget_dmabuf_webgl_enabled();
}

#ifdef MOZ_WAYLAND
void DMABufDevice::SetModifiersToGfxVars() {
  gfxVars::SetDMABufModifiersXRGB(mXRGBFormat.mModifiers);
  gfxVars::SetDMABufModifiersARGB(mARGBFormat.mModifiers);
}

void DMABufDevice::GetModifiersFromGfxVars() {
  mXRGBFormat.mModifiers = gfxVars::DMABufModifiersXRGB().Clone();
  mARGBFormat.mModifiers = gfxVars::DMABufModifiersARGB().Clone();
}
#endif

void DMABufDevice::DisableDMABufWebGL() { sUseWebGLDmabufBackend = false; }

GbmFormat* DMABufDevice::GetGbmFormat(bool aHasAlpha) {
  GbmFormat* format = aHasAlpha ? &mARGBFormat : &mXRGBFormat;
  return format->mIsSupported ? format : nullptr;
}

#ifdef MOZ_WAYLAND
void DMABufDevice::AddFormatModifier(bool aHasAlpha, int aFormat,
                                     uint32_t mModifierHi,
                                     uint32_t mModifierLo) {
  GbmFormat* format = aHasAlpha ? &mARGBFormat : &mXRGBFormat;
  format->mIsSupported = true;
  format->mHasAlpha = aHasAlpha;
  format->mFormat = aFormat;
  format->mModifiers.AppendElement(((uint64_t)mModifierHi << 32) | mModifierLo);
}

static void dmabuf_modifiers(void* data,
                             struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                             uint32_t format, uint32_t modifier_hi,
                             uint32_t modifier_lo) {
  // skip modifiers marked as invalid
  if (modifier_hi == (DRM_FORMAT_MOD_INVALID >> 32) &&
      modifier_lo == (DRM_FORMAT_MOD_INVALID & 0xffffffff)) {
    return;
  }

  auto* device = static_cast<DMABufDevice*>(data);
  switch (format) {
    case GBM_FORMAT_ARGB8888:
      device->AddFormatModifier(true, format, modifier_hi, modifier_lo);
      break;
    case GBM_FORMAT_XRGB8888:
      device->AddFormatModifier(false, format, modifier_hi, modifier_lo);
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
  if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0 && version > 2) {
    auto* dmabuf = WaylandRegistryBind<zwp_linux_dmabuf_v1>(
        registry, id, &zwp_linux_dmabuf_v1_interface, 3);
    LOGDMABUF(("zwp_linux_dmabuf_v1 is available."));
    zwp_linux_dmabuf_v1_add_listener(dmabuf, &dmabuf_listener, data);
  } else if (strcmp(interface, "wl_drm") == 0) {
    LOGDMABUF(("wl_drm is available."));
  }
}

static void global_registry_remover(void* data, wl_registry* registry,
                                    uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

void DMABufDevice::LoadFormatModifiers() {
  if (!GdkIsWaylandDisplay()) {
    return;
  }
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(NS_IsMainThread());
    wl_display* display = WaylandDisplayGetWLDisplay();
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);
    SetModifiersToGfxVars();
  } else {
    GetModifiersFromGfxVars();
  }
}
#endif

DMABufDevice* GetDMABufDevice() {
  static StaticAutoPtr<DMABufDevice> sDmaBufDevice;
  static std::once_flag onceFlag;
  std::call_once(onceFlag, [] {
    sDmaBufDevice = new DMABufDevice();
    if (NS_IsMainThread()) {
      ClearOnShutdown(&sDmaBufDevice);
    } else {
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "ClearDmaBufDevice", [] { ClearOnShutdown(&sDmaBufDevice); }));
    }
  });
  return sDmaBufDevice.get();
}

}  // namespace widget
}  // namespace mozilla
