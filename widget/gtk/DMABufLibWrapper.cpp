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
#include <mutex>

using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

bool sUseWebGLDmabufBackend = true;

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

int nsDMABufDevice::GetDmabufFD(uint32_t aGEMHandle) {
  int fd;
  return nsGbmLib::DrmPrimeHandleToFD(mDRMFd, aGEMHandle, 0, &fd) < 0 ? -1 : fd;
}

gbm_device* nsDMABufDevice::GetGbmDevice() {
  std::call_once(mFlagGbmDevice, [&] {
    mGbmDevice = (mDRMFd != -1) ? nsGbmLib::CreateDevice(mDRMFd) : nullptr;
  });
  return mGbmDevice;
}

int nsDMABufDevice::OpenDRMFd() { return open(mDrmRenderNode.get(), O_RDWR); }

bool nsDMABufDevice::IsEnabled(nsACString& aFailureId) {
  if (mDRMFd == -1) {
    aFailureId = mFailureId;
  }
  return mDRMFd != -1;
}

nsDMABufDevice::nsDMABufDevice()
    : mXRGBFormat({true, false, GBM_FORMAT_XRGB8888, nullptr, 0}),
      mARGBFormat({true, true, GBM_FORMAT_ARGB8888, nullptr, 0}) {
  Configure();
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

void nsDMABufDevice::Configure() {
  LOGDMABUF(("nsDMABufDevice::Configure()"));

  if (!nsGbmLib::IsAvailable()) {
    LOGDMABUF(("nsGbmLib is not available!"));
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
  LOGDMABUF(("DMABuf is enabled"));
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
       "sUseWebGLDmabufBackend %d "
       "widget_dmabuf_webgl_enabled %d\n",
       gfx::gfxVars::UseDMABuf(), sUseWebGLDmabufBackend,
       StaticPrefs::widget_dmabuf_webgl_enabled()));
  return gfx::gfxVars::UseDMABuf() && sUseWebGLDmabufBackend &&
         StaticPrefs::widget_dmabuf_webgl_enabled();
}

void nsDMABufDevice::DisableDMABufWebGL() { sUseWebGLDmabufBackend = false; }

GbmFormat* nsDMABufDevice::GetGbmFormat(bool aHasAlpha) {
  GbmFormat* format = aHasAlpha ? &mARGBFormat : &mXRGBFormat;
  return format->mIsSupported ? format : nullptr;
}

nsDMABufDevice* GetDMABufDevice() {
  static nsDMABufDevice dmaBufDevice;
  return &dmaBufDevice;
}

}  // namespace widget
}  // namespace mozilla
