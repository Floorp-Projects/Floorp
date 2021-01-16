/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"
#include "DMABufLibWrapper.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

namespace mozilla {
namespace widget {

#define GBMLIB_NAME "libgbm.so.1"
#define DRMLIB_NAME "libdrm.so.2"

void* nsGbmLib::sGbmLibHandle = nullptr;
void* nsGbmLib::sXf86DrmLibHandle = nullptr;
bool nsGbmLib::sLibLoaded = false;
CreateDeviceFunc nsGbmLib::sCreateDevice;
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

bool nsGbmLib::IsLoaded() {
  return sCreateDevice != nullptr && sCreate != nullptr &&
         sCreateWithModifiers != nullptr && sGetModifier != nullptr &&
         sGetStride != nullptr && sGetFd != nullptr && sDestroy != nullptr &&
         sMap != nullptr && sUnmap != nullptr && sGetPlaneCount != nullptr &&
         sGetHandleForPlane != nullptr && sGetStrideForPlane != nullptr &&
         sGetOffset != nullptr && sDeviceIsFormatSupported != nullptr &&
         sDrmPrimeHandleToFD != nullptr;
}

bool nsGbmLib::IsAvailable() {
  if (!Load()) {
    return false;
  }
  return IsLoaded();
}

bool nsGbmLib::Load() {
  if (!sGbmLibHandle && !sLibLoaded) {
    sLibLoaded = true;

    sGbmLibHandle = dlopen(GBMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
    if (!sGbmLibHandle) {
      LOGDMABUF(("Failed to load %s, dmabuf isn't available.\n", GBMLIB_NAME));
      return false;
    }

    sCreateDevice = (CreateDeviceFunc)dlsym(sGbmLibHandle, "gbm_create_device");
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

    sXf86DrmLibHandle = dlopen(DRMLIB_NAME, RTLD_LAZY | RTLD_LOCAL);
    if (!sXf86DrmLibHandle) {
      LOGDMABUF(("Failed to load %s, dmabuf isn't available.\n", DRMLIB_NAME));
      return false;
    }
    sDrmPrimeHandleToFD =
        (DrmPrimeHandleToFDFunc)dlsym(sXf86DrmLibHandle, "drmPrimeHandleToFD");
    if (!IsLoaded()) {
      LOGDMABUF(("Failed to load all symbols from %s\n", GBMLIB_NAME));
    }
  }

  return sGbmLibHandle;
}

gbm_device* nsDMABufDevice::GetGbmDevice() {
  return IsDMABufEnabled() ? mGbmDevice : nullptr;
}

int nsDMABufDevice::GetGbmDeviceFd() { return IsDMABufEnabled() ? mGbmFd : -1; }

static void dmabuf_modifiers(void* data,
                             struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                             uint32_t format, uint32_t modifier_hi,
                             uint32_t modifier_lo) {
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
    : mXRGBFormat({true, false, GBM_FORMAT_XRGB8888, nullptr, 0}),
      mARGBFormat({true, true, GBM_FORMAT_ARGB8888, nullptr, 0}),
      mGbmDevice(nullptr),
      mGbmFd(-1) {
  if (gdk_display_get_default() &&
      !GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    wl_display* display = WaylandDisplayGetWLDisplay();
    mRegistry = (void*)wl_display_get_registry(display);
    wl_registry_add_listener((wl_registry*)mRegistry, &registry_listener, this);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);
  }
}

nsDMABufDevice::~nsDMABufDevice() {
  if (mRegistry) {
    wl_registry_destroy((wl_registry*)mRegistry);
    mRegistry = nullptr;
  }
}

bool nsDMABufDevice::Configure() {
  bool isDMABufUsed = (
#ifdef NIGHTLY_BUILD
      StaticPrefs::widget_dmabuf_textures_enabled() ||
#endif
      StaticPrefs::widget_dmabuf_webgl_enabled() ||
      StaticPrefs::media_ffmpeg_vaapi_enabled() ||
      StaticPrefs::media_ffmpeg_vaapi_drm_display_enabled());

  if (!isDMABufUsed) {
    // Disabled by user, just quit.
    LOGDMABUF(("IsDMABufEnabled(): Disabled by preferences."));
    return false;
  }

  if (!nsGbmLib::IsAvailable()) {
    LOGDMABUF(("nsGbmLib is not available!"));
    return false;
  }

  // TODO - Better DRM device detection/configuration.
  const char* drm_render_node = getenv("MOZ_WAYLAND_DRM_DEVICE");
  if (!drm_render_node) {
    drm_render_node = "/dev/dri/renderD128";
  }

  mGbmFd = open(drm_render_node, O_RDWR);
  if (mGbmFd < 0) {
    LOGDMABUF(("Failed to open drm render node %s\n", drm_render_node));
    return false;
  }

  mGbmDevice = nsGbmLib::CreateDevice(mGbmFd);
  if (mGbmDevice == nullptr) {
    LOGDMABUF(("Failed to create drm render device %s\n", drm_render_node));
    close(mGbmFd);
    mGbmFd = -1;
    return false;
  }

  LOGDMABUF(("GBM device initialized"));
  return true;
}

bool nsDMABufDevice::IsDMABufEnabled() {
  static bool isDMABufEnabled = Configure();
  return isDMABufEnabled;
}

#ifdef NIGHTLY_BUILD
bool nsDMABufDevice::IsDMABufTexturesEnabled() {
  return gfx::gfxVars::UseEGL() && IsDMABufEnabled() &&
         StaticPrefs::widget_dmabuf_textures_enabled();
}
#else
bool nsDMABufDevice::IsDMABufTexturesEnabled() { return false; }
#endif
bool nsDMABufDevice::IsDMABufVAAPIEnabled() {
  return gfx::gfxVars::UseEGL() && IsDMABufEnabled() &&
         StaticPrefs::media_ffmpeg_vaapi_enabled() &&
         gfx::gfxVars::CanUseHardwareVideoDecoding() && !XRE_IsRDDProcess();
}
bool nsDMABufDevice::IsDMABufWebGLEnabled() {
  return gfx::gfxVars::UseEGL() && IsDMABufEnabled() &&
         StaticPrefs::widget_dmabuf_webgl_enabled();
}

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
