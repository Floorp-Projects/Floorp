/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowBase_h_
#define nsWindowBase_h_

#include "mozilla/MiscEvents.h"
#include "nsBaseWidget.h"
#include "npapi.h"
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
   * Return the parent window, if it exists.
   */
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) = 0;

  /*
   * Return true if this is a top level widget.
   */
  virtual bool IsTopLevelWidget() = 0;

  /*
   * Init a standard gecko event for this widget.
   * @param aEvent the event to initialize.
   * @param aPoint message position in physical coordinates.
   */
  virtual void InitEvent(mozilla::WidgetGUIEvent& aEvent,
                         nsIntPoint* aPoint = nullptr) = 0;

  /*
   * Dispatch a gecko event for this widget.
   * Returns true if it's consumed.  Otherwise, false.
   */
  virtual bool DispatchWindowEvent(mozilla::WidgetGUIEvent* aEvent) = 0;

  /*
   * Dispatch a gecko keyboard event for this widget. This
   * is called by KeyboardLayout to dispatch gecko events.
   * Returns true if it's consumed.  Otherwise, false.
   */
  virtual bool DispatchKeyboardEvent(mozilla::WidgetGUIEvent* aEvent) = 0;

  /*
   * Default dispatch of a plugin event.
   */
  virtual bool DispatchPluginEvent(const MSG &aMsg)
  {
    if (!PluginHasFocus()) {
      return false;
    }
    mozilla::WidgetPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, this);
    nsIntPoint point(0, 0);
    InitEvent(pluginEvent, &point);
    NPEvent npEvent;
    npEvent.event = aMsg.message;
    npEvent.wParam = aMsg.wParam;
    npEvent.lParam = aMsg.lParam;
    pluginEvent.pluginEvent = (void *)&npEvent;
    pluginEvent.retargetToFocusedDocument = true;
    return DispatchWindowEvent(&pluginEvent);
  }

  /*
   * Returns true if a plugin has focus on this widget.  Otherwise, false.
   */
  virtual bool PluginHasFocus() const MOZ_FINAL
  {
    return (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN);
  }

protected:
  InputContext mInputContext;
};

#endif // nsWindowBase_h_
