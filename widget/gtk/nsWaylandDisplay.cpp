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

static void global_registry_handler(void* data, wl_registry* registry,
                                    uint32_t id, const char* interface,
                                    uint32_t version) {
  auto* display = static_cast<nsWaylandDisplay*>(data);
  if (!display) {
    return;
  }

  nsDependentCString iface(interface);
  if (iface.EqualsLiteral("wl_shm")) {
    auto* shm = WaylandRegistryBind<wl_shm>(registry, id, &wl_shm_interface, 1);
    display->SetShm(shm);
  } else if (iface.EqualsLiteral("zwp_idle_inhibit_manager_v1")) {
    auto* idle_inhibit_manager =
        WaylandRegistryBind<zwp_idle_inhibit_manager_v1>(
            registry, id, &zwp_idle_inhibit_manager_v1_interface, 1);
    display->SetIdleInhibitManager(idle_inhibit_manager);
  } else if (iface.EqualsLiteral("zwp_relative_pointer_manager_v1")) {
    auto* relative_pointer_manager =
        WaylandRegistryBind<zwp_relative_pointer_manager_v1>(
            registry, id, &zwp_relative_pointer_manager_v1_interface, 1);
    display->SetRelativePointerManager(relative_pointer_manager);
  } else if (iface.EqualsLiteral("zwp_pointer_constraints_v1")) {
    auto* pointer_constraints = WaylandRegistryBind<zwp_pointer_constraints_v1>(
        registry, id, &zwp_pointer_constraints_v1_interface, 1);
    display->SetPointerConstraints(pointer_constraints);
  } else if (iface.EqualsLiteral("wl_compositor")) {
    // Requested wl_compositor version 4 as we need wl_surface_damage_buffer().
    auto* compositor = WaylandRegistryBind<wl_compositor>(
        registry, id, &wl_compositor_interface, 4);
    display->SetCompositor(compositor);
  } else if (iface.EqualsLiteral("wl_subcompositor")) {
    auto* subcompositor = WaylandRegistryBind<wl_subcompositor>(
        registry, id, &wl_subcompositor_interface, 1);
    display->SetSubcompositor(subcompositor);
  } else if (iface.EqualsLiteral("wp_viewporter")) {
    auto* viewporter = WaylandRegistryBind<wp_viewporter>(
        registry, id, &wp_viewporter_interface, 1);
    display->SetViewporter(viewporter);
  } else if (iface.EqualsLiteral("zwp_linux_dmabuf_v1") && version > 2) {
    auto* dmabuf = WaylandRegistryBind<zwp_linux_dmabuf_v1>(
        registry, id, &zwp_linux_dmabuf_v1_interface, 3);
    display->SetDmabuf(dmabuf);
  } else if (iface.EqualsLiteral("xdg_activation_v1")) {
    auto* activation = WaylandRegistryBind<xdg_activation_v1>(
        registry, id, &xdg_activation_v1_interface, 1);
    display->SetXdgActivation(activation);
  } else if (iface.EqualsLiteral("wl_seat")) {
    // Install keyboard handlers for main thread only
    auto* seat =
        WaylandRegistryBind<wl_seat>(registry, id, &wl_seat_interface, 1);
    KeymapWrapper::SetSeat(seat, id);
  } else if (iface.EqualsLiteral("wp_fractional_scale_manager_v1")) {
    auto* manager = WaylandRegistryBind<wp_fractional_scale_manager_v1>(
        registry, id, &wp_fractional_scale_manager_v1_interface, 1);
    display->SetFractionalScaleManager(manager);
  } else if (iface.EqualsLiteral("gtk_primary_selection_device_manager") ||
             iface.EqualsLiteral("zwp_primary_selection_device_manager_v1")) {
    display->EnablePrimarySelection();
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

  // See Bug 1826583 and Bug 1844653 for reference.
  // "warning: queue %p destroyed while proxies still attached" and variants
  // like "zwp_linux_dmabuf_feedback_v1@%d still attached" are exceptions on
  // Wayland and non-fatal. They are triggered in certain versions of Mesa or
  // the proprietary Nvidia driver and we don't want to crash because of them.
  if (strstr(error, "still attached")) {
    return;
  }

  MOZ_CRASH_UNSAFE(error);
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
