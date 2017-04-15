/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScreenHelperGTK.h"

#ifdef MOZ_X11
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
// from Xinerama.h
typedef struct {
   int   screen_number;
   short x_org;
   short y_org;
   short width;
   short height;
} XineramaScreenInfo;
// prototypes from Xinerama.h
typedef Bool (*_XnrmIsActive_fn)(Display *dpy);
typedef XineramaScreenInfo* (*_XnrmQueryScreens_fn)(Display *dpy, int *number);
#endif
#include <dlfcn.h>
#include <gtk/gtk.h>

#include "gfxPlatformGtk.h"
#include "mozilla/Logging.h"
#include "nsGtkUtils.h"
#include "nsTArray.h"

#define SCREEN_MANAGER_LIBRARY_LOAD_FAILED ((PRLibrary*)1)

namespace mozilla {
namespace widget {

static LazyLogModule sScreenLog("WidgetScreen");

static void
monitors_changed(GdkScreen* aScreen, gpointer aClosure)
{
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Received monitors-changed event"));
  ScreenHelperGTK* self = static_cast<ScreenHelperGTK*>(aClosure);
  self->RefreshScreens();
}

static GdkFilterReturn
root_window_event_filter(GdkXEvent* aGdkXEvent, GdkEvent* aGdkEvent,
                         gpointer aClosure)
{
#ifdef MOZ_X11
  ScreenHelperGTK* self = static_cast<ScreenHelperGTK*>(aClosure);
  XEvent *xevent = static_cast<XEvent*>(aGdkXEvent);

  switch (xevent->type) {
    case PropertyNotify:
      {
        XPropertyEvent *propertyEvent = &xevent->xproperty;
        if (propertyEvent->atom == self->NetWorkareaAtom()) {
          MOZ_LOG(sScreenLog, LogLevel::Debug, ("Work area size changed"));
          self->RefreshScreens();
        }
      }
      break;
    default:
      break;
  }
#endif

  return GDK_FILTER_CONTINUE;
}

ScreenHelperGTK::ScreenHelperGTK()
  : mXineramalib(nullptr)
  , mRootWindow(nullptr)
  , mNetWorkareaAtom(0)
{
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("ScreenHelperGTK created"));
  GdkScreen *defaultScreen = gdk_screen_get_default();
  if (!defaultScreen) {
    // Sometimes we don't initial X (e.g., xpcshell)
    MOZ_LOG(sScreenLog, LogLevel::Debug, ("mRootWindow is nullptr, running headless"));
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
    mNetWorkareaAtom =
      XInternAtom(GDK_WINDOW_XDISPLAY(mRootWindow), "_NET_WORKAREA", False);
  }
#endif
  RefreshScreens();
}

ScreenHelperGTK::~ScreenHelperGTK()
{
  if (mRootWindow) {
    g_signal_handlers_disconnect_by_func(gdk_screen_get_default(),
                                         FuncToGpointer(monitors_changed),
                                         this);

    gdk_window_remove_filter(mRootWindow, root_window_event_filter, this);
    g_object_unref(mRootWindow);
    mRootWindow = nullptr;
  }

  /* XineramaIsActive() registers a callback function close_display()
   * in X, which is to be called in XCloseDisplay(). This is the case
   * if Xinerama is active, even if only with one screen.
   *
   * We can't unload libXinerama.so.1 here because this will make
   * the address of close_display() registered in X to be invalid and
   * it will crash when XCloseDisplay() is called later. */
}

gint
ScreenHelperGTK::GetGTKMonitorScaleFactor()
{
#if (MOZ_WIDGET_GTK >= 3)
  // Since GDK 3.10
  static auto sGdkScreenGetMonitorScaleFactorPtr = (gint (*)(GdkScreen*, gint))
    dlsym(RTLD_DEFAULT, "gdk_screen_get_monitor_scale_factor");
  if (sGdkScreenGetMonitorScaleFactorPtr) {
    // FIXME: In the future, we'll want to fix this for GTK on Wayland which
    // supports a variable scale factor per display.
    GdkScreen *screen = gdk_screen_get_default();
    return sGdkScreenGetMonitorScaleFactorPtr(screen, 0);
  }
#endif
  return 1;
}

static float
GetDefaultCssScale()
{
  return ScreenHelperGTK::GetGTKMonitorScaleFactor() * gfxPlatformGtk::GetDPIScale();
}

static uint32_t
GetGTKPixelDepth()
{
  GdkVisual * visual = gdk_screen_get_system_visual(gdk_screen_get_default());
  return gdk_visual_get_depth(visual);
}

static already_AddRefed<Screen>
MakeScreen(GdkWindow* aRootWindow)
{
  RefPtr<Screen> screen;

  gint scale = ScreenHelperGTK::GetGTKMonitorScaleFactor();
  gint width = gdk_screen_width() * scale;
  gint height = gdk_screen_height() * scale;
  uint32_t pixelDepth = GetGTKPixelDepth();
  DesktopToLayoutDeviceScale contentsScale(1.0);
  CSSToLayoutDeviceScale defaultCssScale(GetDefaultCssScale());

  LayoutDeviceIntRect rect;
  LayoutDeviceIntRect availRect;
  rect = availRect = LayoutDeviceIntRect(0, 0, width, height);

#ifdef MOZ_X11
  // We need to account for the taskbar, etc in the available rect.
  // See http://freedesktop.org/Standards/wm-spec/index.html#id2767771

  // XXX do we care about _NET_WM_STRUT_PARTIAL?  That will
  // add much more complexity to the code here (our screen
  // could have a non-rectangular shape), but should
  // lead to greater accuracy.

  long *workareas;
  GdkAtom type_returned;
  int format_returned;
  int length_returned;

  GdkAtom cardinal_atom = gdk_x11_xatom_to_atom(XA_CARDINAL);

  gdk_error_trap_push();

  // gdk_property_get uses (length + 3) / 4, hence G_MAXLONG - 3 here.
  if (!gdk_property_get(aRootWindow,
                        gdk_atom_intern ("_NET_WORKAREA", FALSE),
                        cardinal_atom,
                        0, G_MAXLONG - 3, FALSE,
                        &type_returned,
                        &format_returned,
                        &length_returned,
                        (guchar **) &workareas)) {
    // This window manager doesn't support the freedesktop standard.
    // Nothing we can do about it, so assume full screen size.
    MOZ_LOG(sScreenLog, LogLevel::Debug, ("New screen [%d %d %d %d %d %f]",
                                          rect.x, rect.y, rect.width, rect.height,
                                          pixelDepth, defaultCssScale.scale));
    screen = new Screen(rect, availRect,
                        pixelDepth, pixelDepth,
                        contentsScale, defaultCssScale);
    return screen.forget();
  }

  // Flush the X queue to catch errors now.
  gdk_flush();

  if (!gdk_error_trap_pop() &&
      type_returned == cardinal_atom &&
      length_returned && (length_returned % 4) == 0 &&
      format_returned == 32) {
    int num_items = length_returned / sizeof(long);

    for (int i = 0; i < num_items; i += 4) {
      LayoutDeviceIntRect workarea(workareas[i],     workareas[i + 1],
                                   workareas[i + 2], workareas[i + 3]);
      if (!rect.Contains(workarea)) {
        // Note that we hit this when processing screen size changes,
        // since we'll get the configure event before the toolbars have
        // been moved.  We'll end up cleaning this up when we get the
        // change notification to the _NET_WORKAREA property.  However,
        // we still want to listen to both, so we'll handle changes
        // properly for desktop environments that don't set the
        // _NET_WORKAREA property.
        NS_WARNING("Invalid bounds");
        continue;
      }

      availRect.IntersectRect(availRect, workarea);
    }
  }
  g_free(workareas);
#endif
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("New screen [%d %d %d %d %d %f]",
                                        rect.x, rect.y, rect.width, rect.height,
                                        pixelDepth, defaultCssScale.scale));
  screen = new Screen(rect, availRect,
                      pixelDepth, pixelDepth,
                      contentsScale, defaultCssScale);
  return screen.forget();
}

static already_AddRefed<Screen>
MakeScreen(const XineramaScreenInfo& aScreenInfo)
{
  LayoutDeviceIntRect xineRect(aScreenInfo.x_org, aScreenInfo.y_org,
                               aScreenInfo.width, aScreenInfo.height);
  uint32_t pixelDepth = GetGTKPixelDepth();
  DesktopToLayoutDeviceScale contentsScale(1.0);
  CSSToLayoutDeviceScale defaultCssScale(GetDefaultCssScale());

  MOZ_LOG(sScreenLog, LogLevel::Debug, ("New screen [%d %d %d %d %d %f]",
                                        xineRect.x, xineRect.y,
                                        xineRect.width, xineRect.height,
                                        pixelDepth, defaultCssScale.scale));
  RefPtr<Screen> screen = new Screen(xineRect, xineRect,
                                     pixelDepth, pixelDepth,
                                     contentsScale, defaultCssScale);
  return screen.forget();
}

void
ScreenHelperGTK::RefreshScreens()
{
  MOZ_LOG(sScreenLog, LogLevel::Debug, ("Refreshing screens"));
  AutoTArray<RefPtr<Screen>, 4> screenList;
#ifdef MOZ_X11
  XineramaScreenInfo *screenInfo = nullptr;
  int numScreens;

  bool useXinerama = GDK_IS_X11_DISPLAY(gdk_display_get_default());

  if (useXinerama && !mXineramalib) {
    mXineramalib = PR_LoadLibrary("libXinerama.so.1");
    if (!mXineramalib) {
      mXineramalib = SCREEN_MANAGER_LIBRARY_LOAD_FAILED;
    }
  }
  if (mXineramalib && mXineramalib != SCREEN_MANAGER_LIBRARY_LOAD_FAILED) {
    _XnrmIsActive_fn _XnrmIsActive = (_XnrmIsActive_fn)
        PR_FindFunctionSymbol(mXineramalib, "XineramaIsActive");

    _XnrmQueryScreens_fn _XnrmQueryScreens = (_XnrmQueryScreens_fn)
        PR_FindFunctionSymbol(mXineramalib, "XineramaQueryScreens");

    // get the number of screens via xinerama
    Display *display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (_XnrmIsActive && _XnrmQueryScreens && _XnrmIsActive(display)) {
      screenInfo = _XnrmQueryScreens(display, &numScreens);
    }
  }

  // screenInfo == nullptr if either Xinerama couldn't be loaded or
  // isn't running on the current display
  if (!screenInfo || numScreens == 1) {
    numScreens = 1;
#endif
    MOZ_LOG(sScreenLog, LogLevel::Debug, ("Find only one screen available"));
    // Get primary screen
    screenList.AppendElement(MakeScreen(mRootWindow));
#ifdef MOZ_X11
  }
  // If Xinerama is enabled and there's more than one screen, fill
  // in the info for all of the screens.  If that's not the case
  // then defaults to the screen width + height
  else {
    MOZ_LOG(sScreenLog, LogLevel::Debug,
            ("Xinerama enabled for %d screens", numScreens));
    for (int i = 0; i < numScreens; ++i) {
      screenList.AppendElement(MakeScreen(screenInfo[i]));
    }
  }

  if (screenInfo) {
    XFree(screenInfo);
  }
#endif
  ScreenManager& screenManager = ScreenManager::GetSingleton();
  screenManager.Refresh(Move(screenList));
}

} // namespace widget
} // namespace mozilla
