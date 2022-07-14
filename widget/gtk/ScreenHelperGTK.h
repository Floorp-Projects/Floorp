/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_gtk_ScreenHelperGTK_h
#define mozilla_widget_gtk_ScreenHelperGTK_h

#include "mozilla/widget/ScreenManager.h"

#include "gdk/gdk.h"
#ifdef MOZ_X11
#  include <X11/Xlib.h>
#  include "X11UndefineNone.h"
#endif

class nsWindow;
struct wl_registry;

namespace mozilla {
namespace widget {

class ScreenGetter {
 public:
  ScreenGetter() = default;
  virtual ~ScreenGetter(){};

  virtual void Init(){};

  virtual void RefreshScreens(){};
  virtual RefPtr<nsIScreen> GetScreenForWindow(nsWindow* aWindow) {
    return nullptr;
  }
};

class ScreenGetterGtk : public ScreenGetter {
 public:
  ScreenGetterGtk();
  ~ScreenGetterGtk();

  void Init();

#ifdef MOZ_X11
  Atom NetWorkareaAtom() { return mNetWorkareaAtom; }
#endif

  // For internal use from signal callback functions
  void RefreshScreens();

 private:
  GdkWindow* mRootWindow;
#ifdef MOZ_X11
  Atom mNetWorkareaAtom;
#endif
};

class ScreenGetterWayland;
struct MonitorConfig;

#ifdef MOZ_WAYLAND
class ScreenGetterWayland : public ScreenGetter {
 public:
  ScreenGetterWayland();
  ~ScreenGetterWayland();

  void Init();

  MonitorConfig* AddMonitorConfig(int aId);
  bool RemoveMonitorConfig(int aId);
  already_AddRefed<Screen> MakeScreenWayland(gint aMonitor);

  RefPtr<nsIScreen> GetScreenForWindow(nsWindow* aWindow);

  // For internal use from signal callback functions
  void RefreshScreens();

 private:
  int GetMonitorForWindow(nsWindow* aWindow);
  bool MonitorUsesNonIntegerScale(int aMonitor);

 private:
  wl_registry* mRegistry = nullptr;
  // We use UniquePtr<> here to ensure that MonitorConfig is heap-allocated
  // so it's not invalidated by any change to mMonitors that could happen in the
  // meantime.
  AutoTArray<UniquePtr<MonitorConfig>, 4> mMonitors;
  AutoTArray<RefPtr<Screen>, 4> mScreenList;
};
#endif

class ScreenHelperGTK final : public ScreenManager::Helper {
 public:
  ScreenHelperGTK();
  ~ScreenHelperGTK();

  static gint GetGTKMonitorScaleFactor(gint aMonitorNum = 0);
  static RefPtr<nsIScreen> GetScreenForWindow(nsWindow* aWindow);
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_gtk_ScreenHelperGTK_h
