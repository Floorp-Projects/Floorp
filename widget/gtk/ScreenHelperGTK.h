/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_gtk_ScreenHelperGTK_h
#define mozilla_widget_gtk_ScreenHelperGTK_h

#include "mozilla/widget/ScreenManager.h"

#include "gdk/gdk.h"

class nsWindow;

namespace mozilla::widget {

class ScreenHelperGTK final : public ScreenManager::Helper {
 public:
  ScreenHelperGTK();
  ~ScreenHelperGTK();

  static int GetMonitorCount();
  static gint GetGTKMonitorScaleFactor(gint aMonitorNum = 0);
  static RefPtr<widget::Screen> GetScreenForWindow(nsWindow* aWindow);
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_gtk_ScreenHelperGTK_h
