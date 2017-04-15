/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_gtk_ScreenHelperGTK_h
#define mozilla_widget_gtk_ScreenHelperGTK_h

#include "mozilla/widget/ScreenManager.h"

#include "prlink.h"
#include "gdk/gdk.h"
#ifdef MOZ_X11
#include <X11/Xlib.h>
#endif

namespace mozilla {
namespace widget {

class ScreenHelperGTK final : public ScreenManager::Helper
{
public:
  ScreenHelperGTK();
  ~ScreenHelperGTK() override;

  static gint GetGTKMonitorScaleFactor();

#ifdef MOZ_X11
  Atom NetWorkareaAtom() { return mNetWorkareaAtom; }
#endif

  // For internal use from signal callback functions
  void RefreshScreens();

private:
  PRLibrary* mXineramalib;
  GdkWindow* mRootWindow;
#ifdef MOZ_X11
  Atom mNetWorkareaAtom;
#endif
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_gtk_ScreenHelperGTK_h
