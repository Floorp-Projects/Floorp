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
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/Logging.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "nsGtkUtils.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {

#ifdef MOZ_LOGGING
static LazyLogModule sScreenLog("WidgetScreen");
#  define LOG_SCREEN(args) MOZ_LOG(sScreenLog, LogLevel::Debug, args)
#else
#  define LOG_SCREEN(args)
#endif /* MOZ_LOGGING */

using GdkMonitor = struct _GdkMonitor;

static UniquePtr<ScreenGetter> gScreenGetter;

static void monitors_changed(GdkScreen* aScreen, gpointer aClosure) {
  LOG_SCREEN(("Received monitors-changed event"));
  ScreenGetterGtk* self = static_cast<ScreenGetterGtk*>(aClosure);
  self->RefreshScreens();
}

static void screen_resolution_changed(GdkScreen* aScreen, GParamSpec* aPspec,
                                      ScreenGetterGtk* self) {
  self->RefreshScreens();
}

static GdkFilterReturn root_window_event_filter(GdkXEvent* aGdkXEvent,
                                                GdkEvent* aGdkEvent,
                                                gpointer aClosure) {
#ifdef MOZ_X11
  ScreenGetterGtk* self = static_cast<ScreenGetterGtk*>(aClosure);
  XEvent* xevent = static_cast<XEvent*>(aGdkXEvent);

  switch (xevent->type) {
    case PropertyNotify: {
      XPropertyEvent* propertyEvent = &xevent->xproperty;
      if (propertyEvent->atom == self->NetWorkareaAtom()) {
        LOG_SCREEN(("Work area size changed"));
        self->RefreshScreens();
      }
    } break;
    default:
      break;
  }
#endif

  return GDK_FILTER_CONTINUE;
}

ScreenGetterGtk::ScreenGetterGtk()
    : mRootWindow(nullptr)
#ifdef MOZ_X11
      ,
      mNetWorkareaAtom(0)
#endif
{
}

void ScreenGetterGtk::Init() {
  LOG_SCREEN(("ScreenGetterGtk created"));
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
  // Use _after to ensure this callback is run after gfxPlatformGtk.cpp's
  // handler.
  g_signal_connect_after(defaultScreen, "notify::resolution",
                         G_CALLBACK(screen_resolution_changed), this);
#ifdef MOZ_X11
  gdk_window_add_filter(mRootWindow, root_window_event_filter, this);
  if (GdkIsX11Display()) {
    mNetWorkareaAtom = XInternAtom(GDK_WINDOW_XDISPLAY(mRootWindow),
                                   "_NET_WORKAREA", X11False);
  }
#endif
  RefreshScreens();
}

ScreenGetterGtk::~ScreenGetterGtk() {
  if (mRootWindow) {
    g_signal_handlers_disconnect_by_data(gdk_screen_get_default(), this);

    gdk_window_remove_filter(mRootWindow, root_window_event_filter, this);
    g_object_unref(mRootWindow);
    mRootWindow = nullptr;
  }
}

static uint32_t GetGTKPixelDepth() {
  GdkVisual* visual = gdk_screen_get_system_visual(gdk_screen_get_default());
  return gdk_visual_get_depth(visual);
}

static bool IsGNOMECompositor() {
  const char* currentDesktop = getenv("XDG_CURRENT_DESKTOP");
  return currentDesktop && strstr(currentDesktop, "GNOME") != nullptr;
}

static already_AddRefed<Screen> MakeScreenGtk(GdkScreen* aScreen,
                                              gint aMonitorNum) {
  gint gdkScaleFactor = ScreenHelperGTK::GetGTKMonitorScaleFactor(aMonitorNum);

  // gdk_screen_get_monitor_geometry / workarea returns application pixels
  // (desktop pixels), so we need to convert it to device pixels with
  // gdkScaleFactor on X11.
  // GNOME/Wayland reports scales differently (Bug 1732682).
  gint geometryScaleFactor = 1;
  if (GdkIsX11Display() || (GdkIsWaylandDisplay() && !IsGNOMECompositor())) {
    geometryScaleFactor = gdkScaleFactor;
  }

  LayoutDeviceIntRect rect;

  GdkRectangle workarea;
  gdk_screen_get_monitor_workarea(aScreen, aMonitorNum, &workarea);
  LayoutDeviceIntRect availRect(workarea.x * geometryScaleFactor,
                                workarea.y * geometryScaleFactor,
                                workarea.width * geometryScaleFactor,
                                workarea.height * geometryScaleFactor);
  if (GdkIsX11Display()) {
    GdkRectangle monitor;
    gdk_screen_get_monitor_geometry(aScreen, aMonitorNum, &monitor);
    rect = LayoutDeviceIntRect(monitor.x * geometryScaleFactor,
                               monitor.y * geometryScaleFactor,
                               monitor.width * geometryScaleFactor,
                               monitor.height * geometryScaleFactor);
  } else {
    // We use Gtk workarea on Wayland as it matches our needs (Bug 1732682).
    rect = availRect;
  }

  uint32_t pixelDepth = GetGTKPixelDepth();

  // Use per-monitor scaling factor in gtk/wayland, or 1.0 otherwise.
  DesktopToLayoutDeviceScale contentsScale(1.0);
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
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

  LOG_SCREEN(
      ("New monitor %d size [%d,%d -> %d x %d] depth %d scale %f CssScale %f  "
       "DPI %f ]",
       aMonitorNum, rect.x, rect.y, rect.width, rect.height, pixelDepth,
       contentsScale.scale, defaultCssScale.scale, dpi));
  RefPtr<Screen> screen = new Screen(rect, availRect, pixelDepth, pixelDepth,
                                     contentsScale, defaultCssScale, dpi);
  return screen.forget();
}

void ScreenGetterGtk::RefreshScreens() {
  LOG_SCREEN(("Refreshing screens"));
  AutoTArray<RefPtr<Screen>, 4> screenList;

  GdkScreen* defaultScreen = gdk_screen_get_default();
  gint numScreens = gdk_screen_get_n_monitors(defaultScreen);
  LOG_SCREEN(("GDK reports %d screens", numScreens));

  for (gint i = 0; i < numScreens; i++) {
    screenList.AppendElement(MakeScreenGtk(defaultScreen, i));
  }

  ScreenManager::Refresh(std::move(screenList));
}

#ifdef MOZ_WAYLAND
static void output_handle_geometry(void* data, struct wl_output* wl_output,
                                   int x, int y, int physical_width,
                                   int physical_height, int subpixel,
                                   const char* make, const char* model,
                                   int32_t transform) {
  MonitorConfig* monitor = (MonitorConfig*)data;
  LOG_SCREEN(("wl_output: geometry position %d %d physical size %d %d", x, y,
              physical_width, physical_height));
  monitor->x = x;
  monitor->y = y;
  monitor->width_mm = physical_width;
  monitor->height_mm = physical_height;
}

static void output_handle_done(void* data, struct wl_output* wl_output) {
  LOG_SCREEN(("done"));
  gScreenGetter->RefreshScreens();
}

static void output_handle_scale(void* data, struct wl_output* wl_output,
                                int32_t scale) {
  MonitorConfig* monitor = (MonitorConfig*)data;
  LOG_SCREEN(("wl_output: scale %d", scale));
  monitor->scale = scale;
}

static void output_handle_mode(void* data, struct wl_output* wl_output,
                               uint32_t flags, int width, int height,
                               int refresh) {
  MonitorConfig* monitor = (MonitorConfig*)data;

  LOG_SCREEN(("wl_output: mode output size %d x %d refresh %d", width, height,
              refresh));

  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0) return;

  monitor->width = width;
  monitor->height = height;
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
    output_handle_done,
    output_handle_scale,
};

static void screen_registry_handler(void* data, wl_registry* registry,
                                    uint32_t id, const char* interface,
                                    uint32_t version) {
  ScreenGetterWayland* getter = static_cast<ScreenGetterWayland*>(data);
  if (strcmp(interface, "wl_output") == 0 && version > 1) {
    auto* output =
        WaylandRegistryBind<wl_output>(registry, id, &wl_output_interface, 2);
    wl_output_add_listener(output, &output_listener,
                           getter->AddMonitorConfig(id));
  }
}

static void screen_registry_remover(void* data, struct wl_registry* registry,
                                    uint32_t id) {
  auto* getter = static_cast<ScreenGetterWayland*>(data);
  if (getter->RemoveMonitorConfig(id)) {
    getter->RefreshScreens();
  }
  /* TODO: the object needs to be destroyed here, we're leaking */
}

static const struct wl_registry_listener screen_registry_listener = {
    screen_registry_handler, screen_registry_remover};

void ScreenGetterWayland::Init() {
  MOZ_ASSERT(GdkIsWaylandDisplay());
  LOG_SCREEN(("ScreenGetterWayland created"));
  wl_display* display = WaylandDisplayGetWLDisplay();
  mRegistry = wl_display_get_registry(display);
  wl_registry_add_listener((wl_registry*)mRegistry, &screen_registry_listener,
                           this);
  wl_display_roundtrip(display);
  wl_display_roundtrip(display);
}

MonitorConfig* ScreenGetterWayland::AddMonitorConfig(int aId) {
  mMonitors.EmplaceBack(aId);
  LOG_SCREEN(("Add Monitor ID %d num %d", aId, (int)(mMonitors.Length() - 1)));
  return &(mMonitors[mMonitors.Length() - 1]);
}

bool ScreenGetterWayland::RemoveMonitorConfig(int aId) {
  for (unsigned int i = 0; i < mMonitors.Length(); i++) {
    if (mMonitors[i].id == aId) {
      LOG_SCREEN(("Remove Monitor ID %d num %d", aId, i));
      mMonitors.RemoveElementAt(i);
      return true;
    }
  }
  return false;
}

ScreenGetterWayland::~ScreenGetterWayland() {
  g_clear_pointer(&mRegistry, wl_registry_destroy);
}

static bool GdkMonitorGetWorkarea(GdkMonitor* monitor, GdkRectangle* workarea) {
  static auto s_gdk_monitor_get_workarea =
      (void (*)(GdkMonitor*, GdkRectangle*))dlsym(RTLD_DEFAULT,
                                                  "gdk_monitor_get_workarea");
  if (!s_gdk_monitor_get_workarea) {
    return false;
  }

  s_gdk_monitor_get_workarea(monitor, workarea);
  return true;
}

already_AddRefed<Screen> ScreenGetterWayland::MakeScreenWayland(gint aMonitor) {
  MonitorConfig monitor = mMonitors[aMonitor];

  // On GNOME/Mutter we use results from wl_output directly
  LayoutDeviceIntRect rect(monitor.x, monitor.y, monitor.width, monitor.height);

  uint32_t pixelDepth = GetGTKPixelDepth();

  // Use per-monitor scaling factor in gtk/wayland, or 1.0 otherwise.
  DesktopToLayoutDeviceScale contentsScale(1.0);
  contentsScale.scale = monitor.scale;

  CSSToLayoutDeviceScale defaultCssScale(monitor.scale *
                                         gfxPlatformGtk::GetFontScaleFactor());

  float dpi = 96.0f;
  gint heightMM = monitor.height_mm;
  if (heightMM > 0) {
    dpi = rect.height / (heightMM / MM_PER_INCH_FLOAT);
  }

  LOG_SCREEN(
      ("Monitor %d [%d %d -> %d x %d depth %d content scale %f css scale %f "
       "DPI %f]",
       aMonitor, rect.x, rect.y, rect.width, rect.height, pixelDepth,
       contentsScale.scale, defaultCssScale.scale, dpi));
  RefPtr<Screen> screen = new Screen(rect, rect, pixelDepth, pixelDepth,
                                     contentsScale, defaultCssScale, dpi);
  return screen.forget();
}

void ScreenGetterWayland::RefreshScreens() {
  LOG_SCREEN(("Refreshing screens"));
  AutoTArray<RefPtr<Screen>, 4> managerScreenList;

  mScreenList.Clear();
  const gint numScreens = mMonitors.Length();
  for (gint i = 0; i < numScreens; i++) {
    RefPtr<Screen> screen = MakeScreenWayland(i);
    mScreenList.AppendElement(screen);
    managerScreenList.AppendElement(screen);
  }

  ScreenManager::Refresh(std::move(managerScreenList));
}

int ScreenGetterWayland::GetMonitorForWindow(nsWindow* aWindow) {
  LOG_SCREEN(("GetMonitorForWindow() [%p]", aWindow));

  static auto s_gdk_display_get_monitor_at_window =
      (GdkMonitor * (*)(GdkDisplay*, GdkWindow*))
          dlsym(RTLD_DEFAULT, "gdk_display_get_monitor_at_window");

  if (!s_gdk_display_get_monitor_at_window) {
    LOG_SCREEN(("  failed, missing Gtk helpers"));
    return -1;
  }

  GdkWindow* gdkWindow = gtk_widget_get_window(aWindow->GetGtkWidget());
  if (!gdkWindow) {
    LOG_SCREEN(("  failed, can't get GdkWindow"));
    return -1;
  }

  GdkMonitor* monitor =
      s_gdk_display_get_monitor_at_window(gdk_display_get_default(), gdkWindow);
  if (!monitor) {
    LOG_SCREEN(("  failed, can't get monitor for GdkWindow"));
    return -1;
  }

  GdkRectangle workArea;
  if (!GdkMonitorGetWorkarea(monitor, &workArea)) {
    return -1;
  }

  for (unsigned int i = 0; i < mMonitors.Length(); i++) {
    // Although Gtk/Mutter is very creative in reporting various screens sizes
    // we can rely on Gtk work area start position to match wl_output.
    if (mMonitors[i].x == workArea.x && mMonitors[i].y == workArea.y) {
      LOG_SCREEN((" monitor %d values %d %d -> %d x %d", i, mMonitors[i].x,
                  mMonitors[i].y, mMonitors[i].width, mMonitors[i].height));
      return i;
    }
  }

  return -1;
}

RefPtr<nsIScreen> ScreenGetterWayland::GetScreenForWindow(nsWindow* aWindow) {
  if (mScreenList.IsEmpty()) {
    return nullptr;
  }

  int monitor = GetMonitorForWindow(aWindow);
  if (monitor < 0) {
    return nullptr;
  }
  return mScreenList[monitor];
}
#endif

RefPtr<nsIScreen> ScreenHelperGTK::GetScreenForWindow(nsWindow* aWindow) {
  return gScreenGetter->GetScreenForWindow(aWindow);
}

gint ScreenHelperGTK::GetGTKMonitorScaleFactor(gint aMonitorNum) {
  GdkScreen* screen = gdk_screen_get_default();
  return gdk_screen_get_monitor_scale_factor(screen, aMonitorNum);
}

ScreenHelperGTK::ScreenHelperGTK() {
#ifdef MOZ_WAYLAND
  // Use ScreenGetterWayland on Gnome/Mutter only. It uses additional wl_output
  // to track screen size changes (which are wrongly reported by mutter)
  // and causes issues on Sway (Bug 1730476).
  // https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/3941
  if (GdkIsWaylandDisplay() && IsGNOMECompositor()) {
    gScreenGetter = mozilla::MakeUnique<ScreenGetterWayland>();
  }
#endif
  if (!gScreenGetter) {
    gScreenGetter = mozilla::MakeUnique<ScreenGetterGtk>();
  }
  gScreenGetter->Init();
}

ScreenHelperGTK::~ScreenHelperGTK() { gScreenGetter = nullptr; }

}  // namespace widget
}  // namespace mozilla
