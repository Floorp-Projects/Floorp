/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WindowsUIUtils_h__
#define mozilla_widget_WindowsUIUtils_h__

#include "nsIWindowsUIUtils.h"

enum TabletModeState {
  eTabletModeUnknown = 0,
  eTabletModeOff,
  eTabletModeOn
};

class WindowsUIUtils final : public nsIWindowsUIUtils {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOWSUIUTILS

  WindowsUIUtils();
protected:
  ~WindowsUIUtils();

  TabletModeState mInTabletMode;
};

#endif // mozilla_widget_WindowsUIUtils_h__
