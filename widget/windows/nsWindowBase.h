/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowBase_h_
#define nsWindowBase_h_

#include "nsBaseWidget.h"
#include <windows.h>

/*
 * nsWindowBase - Base class of common methods other classes need to access
 * in both win32 and winrt window classes.
 */

class nsWindowBase : public nsBaseWidget
{
public:
  /*
   * Return the HWND or null for this widget.
   */
  virtual HWND GetWindowHandle() MOZ_FINAL {
    return static_cast<HWND>(GetNativeData(NS_NATIVE_WINDOW));
  }

  /*
   * Init a standard gecko event for this widget.
   */
  virtual void InitEvent(nsGUIEvent& aEvent, nsIntPoint* aPoint = nullptr) = 0;

  /*
   * Dispatch a gecko event for this widget.
   */
  virtual bool DispatchWindowEvent(nsGUIEvent* aEvent) = 0;
};

#endif // nsWindowBase_h_
