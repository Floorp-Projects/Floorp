/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/message_loop.h"  // for MessageLoop
#include "nsWaylandDisplay.h"
#include "DMABufLibWrapper.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"
#include "WidgetUtilsGtk.h"
#include "gfxConfig.h"
#include "nsIGfxInfo.h"
#include "mozilla/Components.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

#define GBMLIB_NAME "libgbm.so.1"
#define DRMLIB_NAME "libdrm.so.2"

// Use static lock to protect dri operation as
// gbm_dri.c is not thread safe.
// https://gitlab.freedesktop.org/mesa/mesa/-/issues/4422
mozilla::StaticMutex nsGbmLib::sDRILock MOZ_UNANNOTATED;

bool nsGbmLib::sLoaded = false;
void* nsGbmLib::sGbmLibHandle = nullptr;
void* nsGbmLib::sXf86DrmLibHandle = nullptr;
CreateDeviceFunc nsGbmLib::sCreateDevice;
DestroyDeviceFunc nsGbmLib::sDestroyDevice;
CreateFunc nsGbmLib::sCreate;
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
CreateSurfaceFunc nsGbmLib::sCreateSurface;
DestroySurfaceFunc nsGbmLib::sDestroySurface;

bool nsGbmLib::IsLoaded() {
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

bool nsGbmLib::Load() {
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

gbm_device* nsDMABufDevice::GetGbmDevice() { return mGbmDevice; }

nsDMABufDevice::nsDMABufDevice()
    : mUseWebGLDmabufBackend(true),
      mDRMFd(-1),
      mGbmDevice(nullptr),
      mInitialized(false) {
  nsAutoCString drm_render_node(getenv("MOZ_DRM_DEVICE"));
  if (drm_render_node.IsEmpty()) {
    drm_render_node.Assign(gfx::gfxVars::DrmRenderDevice());
  }

  if (!drm_render_node.IsEmpty()) {
    LOGDMABUF(("Using DRM device %s", drm_render_node.get()));
    mDRMFd = open(drm_render_node.get(), O_RDWR);
    if (mDRMFd < 0) {
      LOGDMABUF(("Failed to open drm render node %s error %s\n",
                 drm_render_node.get(), strerror(errno)));
    }
  } else {
    LOGDMABUF(("We're missing DRM render device!\n"));
  }
}

nsDMABufDevice::~nsDMABufDevice() {
  if (mGbmDevice) {
    nsGbmLib::DestroyDevice(mGbmDevice);
    mGbmDevice = nullptr;
  }
  if (mDRMFd != -1) {
    close(mDRMFd);
    mDRMFd = -1;
  }
}

int nsDMABufDevice::GetDRMFd() { return mDRMFd; }

bool nsDMABufDevice::Configure(nsACString& aFailureId) {
  if (mInitialized) {
    return true;
  }

  LOGDMABUF(("nsDMABufDevice::Configure()"));
  mInitialized = true;

  if (!nsGbmLib::IsAvailable()) {
    LOGDMABUF(("nsGbmLib is not available!"));
    aFailureId = "FEATURE_FAILURE_NO_LIBGBM";
    return false;
  }

  // fd passed to gbm_create_device() should be kept open until
  // gbm_device_destroy() is called.
  mGbmDevice = nsGbmLib::CreateDevice(GetDRMFd());
  if (!mGbmDevice) {
    LOGDMABUF(("Failed to create drm render device"));
    aFailureId = "FEATURE_FAILURE_BAD_DRM_RENDER_NODE";
    return false;
  }

  LOGDMABUF(("DMABuf is enabled"));
  return true;
}

#ifdef NIGHTLY_BUILD
bool nsDMABufDevice::IsDMABufTexturesEnabled() {
  return gfx::gfxVars::UseDMABuf() &&
         StaticPrefs::widget_dmabuf_textures_enabled();
}
#else
bool nsDMABufDevice::IsDMABufTexturesEnabled() { return false; }
#endif
bool nsDMABufDevice::IsDMABufWebGLEnabled() {
  LOGDMABUF(
      ("nsDMABufDevice::IsDMABufWebGLEnabled: UseDMABuf %d "
       "mUseWebGLDmabufBackend %d "
       "widget_dmabuf_webgl_enabled %d\n",
       gfx::gfxVars::UseDMABuf(), mUseWebGLDmabufBackend,
       StaticPrefs::widget_dmabuf_webgl_enabled()));
  return gfx::gfxVars::UseDMABuf() && mUseWebGLDmabufBackend &&
         StaticPrefs::widget_dmabuf_webgl_enabled();
}

void nsDMABufDevice::DisableDMABufWebGL() { mUseWebGLDmabufBackend = false; }

// TODO: Make private or make sure it's configured
nsDMABufDevice* GetDMABufDevice() {
  static nsDMABufDevice dmaBufDevice;
  return &dmaBufDevice;
}

nsDMABufDevice* GetAndConfigureDMABufDevice() {
  nsCString failureId;
  if (GetDMABufDevice()->Configure(failureId)) {
    return GetDMABufDevice();
  }
  return nullptr;
}

}  // namespace widget
}  // namespace mozilla
