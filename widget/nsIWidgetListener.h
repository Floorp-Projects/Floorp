/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIWidgetListener_h__
#define nsIWidgetListener_h__

#include <stdint.h>

#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"

class nsView;
class nsIntRegion;
class nsIPresShell;
class nsIWidget;
class nsIXULWindow;

/**
 * sizemode is an adjunct to widget size
 */
enum nsSizeMode
{
  nsSizeMode_Normal = 0,
  nsSizeMode_Minimized,
  nsSizeMode_Maximized,
  nsSizeMode_Fullscreen,
  nsSizeMode_Invalid
};

/**
 * different types of (top-level) window z-level positioning
 */
enum nsWindowZ
{
  nsWindowZTop = 0,   // on top
  nsWindowZBottom,    // on bottom
  nsWindowZRelative   // just below some specified widget
};

class nsIWidgetListener
{
public:

  /**
   * If this listener is for an nsIXULWindow, return it. If this is null, then
   * this is likely a listener for a view, which can be determined using
   * GetView. If both methods return null, this will be an nsWebBrowser.
   */
  virtual nsIXULWindow* GetXULWindow();

  /**
   * If this listener is for an nsView, return it.
   */
  virtual nsView* GetView();

  /**
   * Return the presshell for this widget listener.
   */
  virtual nsIPresShell* GetPresShell();

  /**
   * Called when a window is moved to location (x, y). Returns true if the
   * notification was handled. Coordinates are outer window screen coordinates.
   */
  virtual bool WindowMoved(nsIWidget* aWidget, int32_t aX, int32_t aY);

  /**
   * Called when a window is resized to (width, height). Returns true if the
   * notification was handled. Coordinates are outer window screen coordinates.
   */
  virtual bool WindowResized(nsIWidget* aWidget,
                             int32_t aWidth, int32_t aHeight);

  /**
   * Called when the size mode (minimized, maximized, fullscreen) is changed.
   */
  virtual void SizeModeChanged(nsSizeMode aSizeMode);

  /**
   * Called when the z-order of the window is changed. Returns true if the
   * notification was handled. aPlacement indicates the new z order. If
   * placement is nsWindowZRelative, then aRequestBelow should be the
   * window to place below. On return, aActualBelow will be set to the
   * window actually behind. This generally only applies to Windows.
   */
  virtual bool ZLevelChanged(bool aImmediate, nsWindowZ* aPlacement,
                             nsIWidget* aRequestBelow,
                             nsIWidget** aActualBelow);

  /**
   * Called when the window entered or left the fullscreen state.
   */
  virtual void FullscreenChanged(bool aInFullscreen);

  /**
   * Called when the window is activated and focused.
   */
  virtual void WindowActivated();

  /**
   * Called when the window is deactivated and no longer focused.
   */
  virtual void WindowDeactivated();

  /**
   * Called when the show/hide toolbar button on the Mac titlebar is pressed.
   */
  virtual void OSToolbarButtonPressed();

  /**
   * Called when a request is made to close the window. Returns true if the
   * notification was handled. Returns true if the notification was handled.
   */
  virtual bool RequestWindowClose(nsIWidget* aWidget);

  /*
   * Indicate that a paint is about to occur on this window. This is called
   * at a time when it's OK to change the geometry of this widget or of
   * other widgets. Must be called before every call to PaintWindow.
   */
  virtual void WillPaintWindow(nsIWidget* aWidget);

  /**
   * Paint the specified region of the window. Returns true if the
   * notification was handled.
   * This is called at a time when it is not OK to change the geometry of
   * this widget or of other widgets.
   */
  virtual bool PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion);

  /**
   * Indicates that a paint occurred.
   * This is called at a time when it is OK to change the geometry of
   * this widget or of other widgets.
   * Must be called after every call to PaintWindow.
   */
  virtual void DidPaintWindow();

  virtual void DidCompositeWindow(const mozilla::TimeStamp& aCompositeStart,
                                  const mozilla::TimeStamp& aCompositeEnd);

  /**
   * Request that layout schedules a repaint on the next refresh driver tick.
   */
  virtual void RequestRepaint();

  /**
   * Handle an event.
   */
  virtual nsEventStatus HandleEvent(mozilla::WidgetGUIEvent* aEvent,
                                    bool aUseAttachedEvents);
};

#endif
