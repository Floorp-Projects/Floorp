/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperGTK.h"

#ifdef MOZ_X11
#  include <gdk/gdkx.h>
#endif /* MOZ_X11 */
#ifdef MOZ_WAYLAND
#  include <gdk/gdkwayland.h>
#endif /* MOZ_WAYLAND */
#include <dlfcn.h>
#include <gtk/gtk.h>

#include "gfxPlatformGtk.h"
#include "mozilla/Logging.h"
#include "nsGtkUtils.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {

static LazyLogModule sScreenLog("WidgetScreen");

static void monitors_changed(GdkScreen* aScreen, gpointer aClosure) {
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Received monitors-changed event"));
  ScreenHelperGTK* self = static_cast<ScreenHelperGTK*>(aClosure);
  self->RefreshScreens();
}

static GdkFilterReturn root_window_event_filter(GdkXEvent* aGdkXEvent,
                                                GdkEvent* aGdkEvent,
                                                gpointer aClosure) {
#ifdef MOZ_X11
  ScreenHelperGTK* self = static_cast<ScreenHelperGTK*>(aClosure);
  XEvent* xevent = static_cast<XEvent*>(aGdkXEvent);

  switch (xevent->type) {
    case PropertyNotify: {
      XPropertyEvent* propertyEvent = &xevent->xproperty;
      if (propertyEvent->atom == self->NetWorkareaAtom()) {
        MOZ_LOG(sScreenLog, LogLevel::Debug, ("Work area size changed"));
        self->RefreshScreens();
      }
    } break;
    default:
      break;
  }
#endif

  return GDK_FILTER_CONTINUE;
}

ScreenHelperGTK::ScreenHelperGTK()
    : mRootWindow(nullptr)
#ifdef MOZ_X11
      ,
      mNetWorkareaAtom(0)
#endif
{
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("ScreenHelperGTK created"));
  GdkScreen* defaultScreen = gdk_screen_get_default();
  if (!defaultScreen) {
    // Sometimes we don't initial X (e.g., xpcshell)
    MOZ_LOG(sScreenLog, LogLevel::Debug,
            ("defaultScreen is nullptr, running headless"));
    return;
  }
  mRootWindow = gdk_get_default_root_window();
  MOZ_ASSERT(mRootWindow);

  g_object_ref(mRootWindow);

  // GDK_PROPERTY_CHANGE_MASK ==> PropertyChangeMask, for PropertyNotify
  gdk_window_set_events(mRootWindow,
                        GdkEventMask(gdk_window_get_events(mRootWindow) |
                                     GDK_PROPERTY_CHANGE_MASK));

  g_signal_connect(defaultScreen, "monitors-changed",
                   G_CALLBACK(monitors_changed), this);
#ifdef MOZ_X11
  gdk_window_add_filter(mRootWindow, root_window_event_filter, this);
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    mNetWorkareaAtom = XInternAtom(GDK_WINDOW_XDISPLAY(mRootWindow),
                                   "_NET_WORKAREA", X11False);
  }
#endif
  RefreshScreens();
}

ScreenHelperGTK::~ScreenHelperGTK() {
  if (mRootWindow) {
    g_signal_handlers_disconnect_by_func(
        gdk_screen_get_default(), FuncToGpointer(monitors_changed), this);

    gdk_window_remove_filter(mRootWindow, root_window_event_filter, this);
    g_object_unref(mRootWindow);
    mRootWindow = nullptr;
  }
}

gint ScreenHelperGTK::GetGTKMonitorScaleFactor(gint aMonitorNum) {
  // Since GDK 3.10
  static auto sGdkScreenGetMonitorScaleFactorPtr =
      (gint(*)(GdkScreen*, gint))dlsym(RTLD_DEFAULT,
                                       "gdk_screen_get_monitor_scale_factor");
  if (sGdkScreenGetMonitorScaleFactorPtr) {
    GdkScreen* screen = gdk_screen_get_default();
    return sGdkScreenGetMonitorScaleFactorPtr(screen, aMonitorNum);
  }
  return 1;
}

static uint32_t GetGTKPixelDepth() {
  GdkVisual* visual = gdk_screen_get_system_visual(gdk_screen_get_default());
  return gdk_visual_get_depth(visual);
}

static already_AddRefed<Screen> MakeScreen(GdkScreen* aScreen,
                                           gint aMonitorNum) {
  GdkRectangle monitor;
  GdkRectangle workarea;
  gdk_screen_get_monitor_geometry(aScreen, aMonitorNum, &monitor);
  gdk_screen_get_monitor_workarea(aScreen, aMonitorNum, &workarea);
  gint gdkScaleFactor = ScreenHelperGTK::GetGTKMonitorScaleFactor(aMonitorNum);

  // gdk_screen_get_monitor_geometry / workarea returns application pixels
  // (desktop pixels), so we need to convert it to device pixels with
  // gdkScaleFactor.
  LayoutDeviceIntRect rect(
      monitor.x * gdkScaleFactor, monitor.y * gdkScaleFactor,
      monitor.width * gdkScaleFactor, monitor.height * gdkScaleFactor);
  LayoutDeviceIntRect availRect(
      workarea.x * gdkScaleFactor, workarea.y * gdkScaleFactor,
      workarea.width * gdkScaleFactor, workarea.height * gdkScaleFactor);
  uint32_t pixelDepth = GetGTKPixelDepth();

  // Use per-monitor scaling factor in gtk/wayland, or 1.0 otherwise.
  DesktopToLayoutDeviceScale contentsScale(1.0);
#ifdef MOZ_WAYLAND
  GdkDisplay* gdkDisplay = gdk_display_get_default();
  if (!GDK_IS_X11_DISPLAY(gdkDisplay)) {
    contentsScale.scale = gdkScaleFactor;
  }
#endif

  CSSToLayoutDeviceScale defaultCssScale(gdkScaleFactor *
                                         gfxPlatformGtk::GetFontScaleFactor());

  float dpi = 96.0f;
  gint heightMM = gdk_screen_get_monitor_height_mm(aScreen, aMonitorNum);
  if (heightMM > 0) {
    dpi = rect.height / (heightMM / MM_PER_INCH_FLOAT);
  }

  MOZ_LOG(sScreenLog, LogLevel::Debug,
          ("New screen [%d %d %d %d (%d %d %d %d) %d %f %f %f]", rect.x, rect.y,
           rect.width, rect.height, availRect.x, availRect.y, availRect.width,
           availRect.height, pixelDepth, contentsScale.scale,
           defaultCssScale.scale, dpi));
  RefPtr<Screen> screen = new Screen(rect, availRect, pixelDepth, pixelDepth,
                                     contentsScale, defaultCssScale, dpi);
  return screen.forget();
}

void ScreenHelperGTK::RefreshScreens() {
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refreshing screens"));
  AutoTArray<RefPtr<Screen>, 4> screenList;

  GdkScreen* defaultScreen = gdk_screen_get_default();
  gint numScreens = gdk_screen_get_n_monitors(defaultScreen);
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("GDK reports %d screens", numScreens));

  for (gint i = 0; i < numScreens; i++) {
    screenList.AppendElement(MakeScreen(defaultScreen, i));
  }

  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.Refresh(std::move(screenList));
}

}  // namespace widget
}  // namespace mozilla
