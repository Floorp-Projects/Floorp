/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWaylandDisplay.h"

#include <dlfcn.h>

#include "base/message_loop.h"    // for MessageLoop
#include "base/task.h"            // for NewRunnableMethod, etc
#include "mozilla/gfx/Logging.h"  // for gfxCriticalNote
#include "mozilla/StaticMutex.h"
#include "mozilla/Array.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/Sprintf.h"
#include "WidgetUtilsGtk.h"
#include "nsGtkKeyUtils.h"

namespace mozilla::widget {

GbmFormat nsWaylandDisplay::sXRGBFormat = {true, false, GBM_FORMAT_XRGB8888,
                                           nullptr, 0};
GbmFormat nsWaylandDisplay::sARGBFormat = {true, true, GBM_FORMAT_ARGB8888,
                                           nullptr, 0};

static nsWaylandDisplay* gWaylandDisplay;

void WaylandDisplayRelease() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "WaylandDisplay can be released in main thread only!");
  if (!gWaylandDisplay) {
    NS_WARNING("WaylandDisplayRelease(): Wayland display is missing!");
    return;
  }
  delete gWaylandDisplay;
  gWaylandDisplay = nullptr;
}

wl_display* WaylandDisplayGetWLDisplay() {
  GdkDisplay* disp = gdk_display_get_default();
  if (!GdkIsWaylandDisplay(disp)) {
    return nullptr;
  }
  return gdk_wayland_display_get_wl_display(disp);
}

nsWaylandDisplay* WaylandDisplayGet() {
  if (!gWaylandDisplay) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                       "WaylandDisplay can be created in main thread only!");
    wl_display* waylandDisplay = WaylandDisplayGetWLDisplay();
    if (!waylandDisplay) {
      return nullptr;
    }
    gWaylandDisplay = new nsWaylandDisplay(waylandDisplay);
  }
  return gWaylandDisplay;
}

void nsWaylandDisplay::SetShm(wl_shm* aShm) { mShm = aShm; }

void nsWaylandDisplay::SetCompositor(wl_compositor* aCompositor) {
  mCompositor = aCompositor;
}

void nsWaylandDisplay::SetSubcompositor(wl_subcompositor* aSubcompositor) {
  mSubcompositor = aSubcompositor;
}

void nsWaylandDisplay::SetIdleInhibitManager(
    zwp_idle_inhibit_manager_v1* aIdleInhibitManager) {
  mIdleInhibitManager = aIdleInhibitManager;
}

void nsWaylandDisplay::SetViewporter(wp_viewporter* aViewporter) {
  mViewporter = aViewporter;
}

void nsWaylandDisplay::SetRelativePointerManager(
    zwp_relative_pointer_manager_v1* aRelativePointerManager) {
  mRelativePointerManager = aRelativePointerManager;
}

void nsWaylandDisplay::SetPointerConstraints(
    zwp_pointer_constraints_v1* aPointerConstraints) {
  mPointerConstraints = aPointerConstraints;
}

void nsWaylandDisplay::SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf) {
  mDmabuf = aDmabuf;
}

void nsWaylandDisplay::SetXdgActivation(xdg_activation_v1* aXdgActivation) {
  mXdgActivation = aXdgActivation;
}

GbmFormat* nsWaylandDisplay::GetGbmFormat(bool aHasAlpha) {
  return aHasAlpha ? &sARGBFormat : &sXRGBFormat;
}

void nsWaylandDisplay::AddFormatModifier(bool aHasAlpha, int aFormat,
                                         uint32_t aModifierHi,
                                         uint32_t aModifierLo) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  GbmFormat* format = aHasAlpha ? &sARGBFormat : &sXRGBFormat;
  format->mIsSupported = true;
  format->mHasAlpha = aHasAlpha;
  format->mFormat = aFormat;
  format->mModifiersCount++;
  format->mModifiers =
      (uint64_t*)realloc(format->mModifiers,
                         format->mModifiersCount * sizeof(*format->mModifiers));
  format->mModifiers[format->mModifiersCount - 1] =
      ((uint64_t)aModifierHi << 32) | aModifierLo;
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
  switch (format) {
    case GBM_FORMAT_ARGB8888:
      nsWaylandDisplay::AddFormatModifier(true, format, modifier_hi,
                                          modifier_lo);
      break;
    case GBM_FORMAT_XRGB8888:
      nsWaylandDisplay::AddFormatModifier(false, format, modifier_hi,
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
  auto* display = static_cast<nsWaylandDisplay*>(data);
  if (!display) {
    return;
  }

  if (strcmp(interface, "wl_shm") == 0) {
    auto* shm = WaylandRegistryBind<wl_shm>(registry, id, &wl_shm_interface, 1);
    display->SetShm(shm);
  } else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0) {
    auto* idle_inhibit_manager =
        WaylandRegistryBind<zwp_idle_inhibit_manager_v1>(
            registry, id, &zwp_idle_inhibit_manager_v1_interface, 1);
    display->SetIdleInhibitManager(idle_inhibit_manager);
  } else if (strcmp(interface, "zwp_relative_pointer_manager_v1") == 0) {
    auto* relative_pointer_manager =
        WaylandRegistryBind<zwp_relative_pointer_manager_v1>(
            registry, id, &zwp_relative_pointer_manager_v1_interface, 1);
    display->SetRelativePointerManager(relative_pointer_manager);
  } else if (strcmp(interface, "zwp_pointer_constraints_v1") == 0) {
    auto* pointer_constraints = WaylandRegistryBind<zwp_pointer_constraints_v1>(
        registry, id, &zwp_pointer_constraints_v1_interface, 1);
    display->SetPointerConstraints(pointer_constraints);
  } else if (strcmp(interface, "wl_compositor") == 0) {
    // Requested wl_compositor version 4 as we need wl_surface_damage_buffer().
    auto* compositor = WaylandRegistryBind<wl_compositor>(
        registry, id, &wl_compositor_interface, 4);
    display->SetCompositor(compositor);
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    auto* subcompositor = WaylandRegistryBind<wl_subcompositor>(
        registry, id, &wl_subcompositor_interface, 1);
    display->SetSubcompositor(subcompositor);
  } else if (strcmp(interface, "wp_viewporter") == 0) {
    auto* viewporter = WaylandRegistryBind<wp_viewporter>(
        registry, id, &wp_viewporter_interface, 1);
    display->SetViewporter(viewporter);
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0 && version > 2) {
    auto* dmabuf = WaylandRegistryBind<zwp_linux_dmabuf_v1>(
        registry, id, &zwp_linux_dmabuf_v1_interface, 3);
    display->SetDmabuf(dmabuf);
    if (NS_IsMainThread()) {
      zwp_linux_dmabuf_v1_add_listener(dmabuf, &dmabuf_listener, nullptr);
    }
  } else if (strcmp(interface, "xdg_activation_v1") == 0) {
    auto* activation = WaylandRegistryBind<xdg_activation_v1>(
        registry, id, &xdg_activation_v1_interface, 1);
    display->SetXdgActivation(activation);
    // Install keyboard handlers for main thread only
  } else if (NS_IsMainThread() && strcmp(interface, "wl_seat") == 0) {
    auto* seat =
        WaylandRegistryBind<wl_seat>(registry, id, &wl_seat_interface, 1);
    KeymapWrapper::SetSeat(seat, id);
  }
}

static void global_registry_remover(void* data, wl_registry* registry,
                                    uint32_t id) {
  KeymapWrapper::ClearSeat(id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler, global_registry_remover};

nsWaylandDisplay::~nsWaylandDisplay() {}

static void WlLogHandler(const char* format, va_list args) {
  char error[1000];
  VsprintfLiteral(error, format, args);
  gfxCriticalNote << "Wayland protocol error: " << error;
}

nsWaylandDisplay::nsWaylandDisplay(wl_display* aDisplay)
    : mThreadId(PR_GetCurrentThread()), mDisplay(aDisplay) {
  // GTK sets the log handler on display creation, thus we overwrite it here
  // in a similar fashion
  wl_log_set_handler_client(WlLogHandler);

  mRegistry = wl_display_get_registry(mDisplay);
  wl_registry_add_listener(mRegistry, &registry_listener, this);
  wl_display_roundtrip(mDisplay);
  wl_display_roundtrip(mDisplay);

  // Check we have critical Wayland interfaces.
  // Missing ones indicates a compositor bug and we can't continue.
  MOZ_DIAGNOSTIC_ASSERT(GetShm(), "We're missing shm interface!");
  MOZ_DIAGNOSTIC_ASSERT(GetCompositor(), "We're missing compositor interface!");
  MOZ_DIAGNOSTIC_ASSERT(GetSubcompositor(),
                        "We're missing subcompositor interface!");
}

}  // namespace mozilla::widget
