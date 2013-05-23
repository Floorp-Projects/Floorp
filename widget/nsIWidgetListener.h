/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozila.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIWidgetListener_h__
#define nsIWidgetListener_h__

#include "nscore.h"
#include "nsGUIEvent.h"
#include "nsIXULWindow.h"

class nsView;
class nsIPresShell;

class nsIWidgetListener
{
public:

  /**
   * If this listener is for an nsIXULWindow, return it. If this is null, then
   * this is likely a listener for a view, which can be determined using
   * GetView. If both methods return null, this will be an nsWebBrowser.
   */
  virtual nsIXULWindow* GetXULWindow() { return nullptr; }

  /**
   * If this listener is for an nsView, return it.
   */
  virtual nsView* GetView() { return nullptr; }

  /**
   * Return the presshell for this widget listener.
   */
  virtual nsIPresShell* GetPresShell() { return nullptr; }

  /**
   * Called when a window is moved to location (x, y). Returns true if the
   * notification was handled. Coordinates are outer window screen coordinates.
   */
  virtual bool WindowMoved(nsIWidget* aWidget, int32_t aX, int32_t aY) { return false; }

  /**
   * Called when a window is resized to (width, height). Returns true if the
   * notification was handled. Coordinates are outer window screen coordinates.
   */
  virtual bool WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight) { return false; }

  /**
   * Called when the size mode (minimized, maximized, fullscreen) is changed.
   */
  virtual void SizeModeChanged(nsSizeMode sizeMode) { }

  /**
   * Called when the z-order of the window is changed. Returns true if the
   * notification was handled. aPlacement indicates the new z order. If
   * placement is nsWindowZRelative, then aRequestBelow should be the
   * window to place below. On return, aActualBelow will be set to the
   * window actually behind. This generally only applies to Windows.
   */
  virtual bool ZLevelChanged(bool aImmediate, nsWindowZ *aPlacement,
                             nsIWidget* aRequestBelow, nsIWidget** aActualBelow) { return false; }

  /**
   * Called when the window is activated and focused.
   */
  virtual void WindowActivated() { }

  /**
   * Called when the window is deactivated and no longer focused.
   */
  virtual void WindowDeactivated() { }

  /**
   * Called when the show/hide toolbar button on the Mac titlebar is pressed.
   */
  virtual void OSToolbarButtonPressed() { }

  /**
   * Called when a request is made to close the window. Returns true if the
   * notification was handled. Returns true if the notification was handled.
   */
  virtual bool RequestWindowClose(nsIWidget* aWidget) { return false; }

  /*
   * Indicate that a paint is about to occur on this window. This is called
   * at a time when it's OK to change the geometry of this widget or of
   * other widgets. Must be called before every call to PaintWindow.
   */
  virtual void WillPaintWindow(nsIWidget* aWidget) { }

  /**
   * Paint the specified region of the window. Returns true if the
   * notification was handled.
   * This is called at a time when it is not OK to change the geometry of
   * this widget or of other widgets.
   */
  virtual bool PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion) { return false; }

  /**
   * Indicates that a paint occurred.
   * This is called at a time when it is OK to change the geometry of
   * this widget or of other widgets.
   * Must be called after every call to PaintWindow.
   */
  virtual void DidPaintWindow() { }

  /**
   * Request that layout schedules a repaint on the next refresh driver tick.
   */
  virtual void RequestRepaint() { }

  /**
   * Handle an event.
   */
  virtual nsEventStatus HandleEvent(nsGUIEvent* event, bool useAttachedEvents)
  {
    return nsEventStatus_eIgnore;
  }
};

#endif
