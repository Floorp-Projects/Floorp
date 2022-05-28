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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

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

gbm_device* nsDMABufDevice::GetGbmDevice() {
  return IsDMABufEnabled() ? mGbmDevice : nullptr;
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

  auto* device = static_cast<nsDMABufDevice*>(data);
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
  auto* device = static_cast<nsDMABufDevice*>(data);
  if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0 && version > 2) {
    auto* dmabuf = WaylandRegistryBind<zwp_linux_dmabuf_v1>(
        registry, id, &zwp_linux_dmabuf_v1_interface, 3);
    LOGDMABUF(("zwp_linux_dmabuf_v1 is available."));
    device->ResetFormatsModifiers();
    zwp_linux_dmabuf_v1_add_listener(dmabuf, &dmabuf_listener, data);
  } else if (strcmp(interface, "wl_drm") == 0) {
    LOGDMABUF(("wl_drm is available."));
  }
}

static void global_registry_remover(void* data, wl_registry* registry,
                                    uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

nsDMABufDevice::nsDMABufDevice()
    : mUseWebGLDmabufBackend(true),
      mXRGBFormat({true, false, GBM_FORMAT_XRGB8888, nullptr, 0}),
      mARGBFormat({true, true, GBM_FORMAT_ARGB8888, nullptr, 0}),
      mDRMFd(-1),
      mGbmDevice(nullptr),
      mInitialized(false) {
  if (GdkIsWaylandDisplay()) {
    wl_display* display = WaylandDisplayGetWLDisplay();
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);
  }

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
  LOGDMABUF(("nsDMABufDevice::Configure()"));

  MOZ_ASSERT(!mInitialized);
  mInitialized = true;

  bool isDMABufUsed = (
#ifdef NIGHTLY_BUILD
      StaticPrefs::widget_dmabuf_textures_enabled() ||
#endif
      StaticPrefs::widget_dmabuf_webgl_enabled());

  if (!isDMABufUsed) {
    // Disabled by user, just quit.
    LOGDMABUF(("IsDMABufEnabled(): Disabled by preferences."));
    aFailureId = "FEATURE_FAILURE_NO_PREFS_ENABLED";
    return false;
  }

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

bool nsDMABufDevice::IsDMABufEnabled() {
  if (!mInitialized) {
    MOZ_ASSERT(!XRE_IsParentProcess());
    nsCString failureId;
    return Configure(failureId);
  }
  return !!mGbmDevice;
}

#ifdef NIGHTLY_BUILD
bool nsDMABufDevice::IsDMABufTexturesEnabled() {
  return gfx::gfxVars::UseDMABuf() && IsDMABufEnabled() &&
         StaticPrefs::widget_dmabuf_textures_enabled();
}
#else
bool nsDMABufDevice::IsDMABufTexturesEnabled() { return false; }
#endif
bool nsDMABufDevice::IsDMABufVAAPIEnabled() {
  LOGDMABUF(
      ("nsDMABufDevice::IsDMABufVAAPIEnabled: EGL %d "
       "media_ffmpeg_vaapi_enabled %d CanUseHardwareVideoDecoding %d "
       "XRE_IsRDDProcess %d\n",
       gfx::gfxVars::UseEGL(), StaticPrefs::media_ffmpeg_vaapi_enabled(),
       gfx::gfxVars::CanUseHardwareVideoDecoding(), XRE_IsRDDProcess()));
  return gfx::gfxVars::UseVAAPI() && XRE_IsRDDProcess() &&
         gfx::gfxVars::CanUseHardwareVideoDecoding();
}
bool nsDMABufDevice::IsDMABufWebGLEnabled() {
  LOGDMABUF(
      ("nsDMABufDevice::IsDMABufWebGLEnabled: EGL %d mUseWebGLDmabufBackend %d "
       "DMABufEnabled %d  "
       "widget_dmabuf_webgl_enabled %d\n",
       gfx::gfxVars::UseEGL(), mUseWebGLDmabufBackend, IsDMABufEnabled(),
       StaticPrefs::widget_dmabuf_webgl_enabled()));
  return gfx::gfxVars::UseDMABuf() && mUseWebGLDmabufBackend &&
         IsDMABufEnabled() && StaticPrefs::widget_dmabuf_webgl_enabled();
}

void nsDMABufDevice::DisableDMABufWebGL() { mUseWebGLDmabufBackend = false; }

GbmFormat* nsDMABufDevice::GetGbmFormat(bool aHasAlpha) {
  GbmFormat* format = aHasAlpha ? &mARGBFormat : &mXRGBFormat;
  return format->mIsSupported ? format : nullptr;
}

GbmFormat* nsDMABufDevice::GetExactGbmFormat(int aFormat) {
  if (aFormat == mARGBFormat.mFormat) {
    return &mARGBFormat;
  } else if (aFormat == mXRGBFormat.mFormat) {
    return &mXRGBFormat;
  }

  return nullptr;
}

void nsDMABufDevice::AddFormatModifier(bool aHasAlpha, int aFormat,
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

void nsDMABufDevice::ResetFormatsModifiers() {
  mARGBFormat.mModifiersCount = 0;
  free(mARGBFormat.mModifiers);
  mARGBFormat.mModifiers = nullptr;

  mXRGBFormat.mModifiersCount = 0;
  free(mXRGBFormat.mModifiers);
  mXRGBFormat.mModifiers = nullptr;
}

nsDMABufDevice* GetDMABufDevice() {
  static nsDMABufDevice dmaBufDevice;
  return &dmaBufDevice;
}

}  // namespace widget
}  // namespace mozilla
