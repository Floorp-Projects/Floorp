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
  virtual void GetScreenRectForWindow(nsWindow* aWindow, GdkRectangle* aRect){};
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

struct MonitorConfig {
  int id = 0;
  int x = 0;
  int y = 0;
  int width_mm = 0;
  int height_mm = 0;
  int width = 0;
  int height = 0;
  int scale = 0;

 public:
  explicit MonitorConfig(int aId) : id(aId){};
};

#ifdef MOZ_WAYLAND
class ScreenGetterWayland : public ScreenGetter {
 public:
  ScreenGetterWayland() : mRegistry(){};
  ~ScreenGetterWayland();

  void Init();

  MonitorConfig* AddMonitorConfig(int aId);
  bool RemoveMonitorConfig(int aId);
  already_AddRefed<Screen> MakeScreenWayland(gint aMonitorNum);

  RefPtr<nsIScreen> GetScreenForWindow(nsWindow* aWindow);
  void GetScreenRectForWindow(nsWindow* aWindow, GdkRectangle* aRect);

  // For internal use from signal callback functions
  void RefreshScreens();

 private:
  int GetMonitorForWindow(nsWindow* aWindow);

 private:
  void* mRegistry;
  AutoTArray<MonitorConfig, 4> mMonitors;
  AutoTArray<RefPtr<Screen>, 4> mScreenList;
};
#endif

class ScreenHelperGTK final : public ScreenManager::Helper {
 public:
  ScreenHelperGTK();
  ~ScreenHelperGTK();

  static gint GetGTKMonitorScaleFactor(gint aMonitorNum = 0);
  static RefPtr<nsIScreen> GetScreenForWindow(nsWindow* aWindow);
  static void GetScreenRectForWindow(nsWindow* aWindow, GdkRectangle* aRect);
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_gtk_ScreenHelperGTK_h
