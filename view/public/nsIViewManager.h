/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIViewManager_h___
#define nsIViewManager_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIView.h"

class nsIRegion;
class nsIEvent;
class nsIPresContext;
class nsIView;
class nsIWidget;
struct nsRect;

#define NS_IVIEWMANAGER_IID   \
{ 0x3a8863d0, 0xa7f3, 0x11d1, \
  { 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIViewManager : public nsISupports
{
public:
  /**
   * Initialize the ViewManager
   * Note: this instance does not hold a reference to the presentation
   * context because it holds a reference to this instance.
   * @param aPresContext the presentation context to use.
   * @result The result of the initialization, NS_OK if no errors
   */
  virtual nsresult Init(nsIPresContext* aPresContext) = 0;

  /**
   * Set the window for which the manager is responsible.
   * @param aRootWindow window to set as root
   */
  virtual void SetRootWindow(nsIWidget *aRootWindow) = 0;

  /**
   * Get the window for which the manager is responsible.
   * @result the root window
   */
  virtual nsIWidget *GetRootWindow() = 0;

  /**
   * Get the root of the view tree.
   * @result the root view
   */
  virtual nsIView *GetRootView() = 0;

  /**
   * Set the root of the view tree.
   * @param aView view to set as root
   */
  virtual void SetRootView(nsIView *aView) = 0;

  /**
   * Get the current framerate i.e. the rate at which timed
   * refreshes occur. A framerate of 0 indicates that timed refreshes
   * should not occur. framerate is in terms of frames per second
   * @result the update rate in terms of frames per second
   */
  virtual PRUint32 GetFrameRate() = 0;

  /**
   * Set the current framerate.
   * @param frameRate new update rate. see GetFrameRate for more info
   * @result the call may need to create a timer and the success
   *         status will be returned. NS_OK means all is well
   */
  virtual nsresult SetFrameRate(PRUint32 frameRate) = 0;

  /**
   * Get the dimensions of the root window. The dimensions are in
   * twips
   * @param width out parameter for width of window in twips
   * @param height out parameter for height of window in twips
   */
  virtual void GetWindowDimensions(nscoord *width, nscoord *height) = 0;

  /**
   * Set the dimensions of the root window.
   * Called if the root window is resized. The dimensions are in
   * twips
   * @param width of window in twips
   * @param height of window in twips
   */
  virtual void SetWindowDimensions(nscoord width, nscoord height) = 0;

  /**
   * Get the position of the root window relative to the
   * composited area. This indicates the scrolled position of the
   * root window.
   * @param xoffset out parameter for X scroll position of window in twips
   * @param yoffset out parameter for Y scroll position of window in twips
   */
  virtual void GetWindowOffsets(nscoord *xoffset, nscoord *yoffset) = 0;

  /**
   * Set the position of the root window relative to the
   * composited area. This indicates the scrolled position of the
   * root window. Called when the root window is scrolled.
   * @param xoffset X scroll position of window in twips
   * @param yoffset Y scroll position of window in twips
   */
  virtual void SetWindowOffsets(nscoord xoffset, nscoord yoffset) = 0;

  /**
   * Reset the state of scrollbars and the scrolling region
   */
  virtual void ResetScrolling(void) = 0;

  /**
   * Called to refresh an area of the root window. Often called in
   * response to a paint/redraw event from the native windowing system.
   * @param aContext rendering context to draw into
   * @param region nsIRegion to be updated
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  virtual void Refresh(nsIView *aView, nsIRenderingContext *aContext,
                       nsIRegion *region, PRUint32 aUpdateFlags) = 0;

  /**
   * Called to refresh an area of the root window. Often called in
   * response to a paint/redraw event from the native windowing system.
   * @param aView view to paint. should be root view
   * @param aContext rendering context to draw into
   * @param rect nsRect to be updated
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  virtual void Refresh(nsIView* aView, nsIRenderingContext *aContext,
                       nsRect *rect, PRUint32 aUpdateFlags) = 0;

  /**
   * Called to force a redrawing of any dirty areas.
   */
  virtual void Composite() = 0;

  /**
   * Called to inform the view manager that some portion of a view
   * is dirty and needs to be redrawn. The region passed in
   * should be in the view's coordinate space.
   * @param aView view to paint. should be root view
   * @param region region to mark as damaged, if nsnull, then entire
   *               view is marked as damaged
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  virtual void UpdateView(nsIView *aView, nsIRegion *aRegion,
                          PRUint32 aUpdateFlags) = 0;

  /**
   * Called to inform the view manager that some portion of a view
   * is dirty and needs to be redrawn. The rect passed in
   * should be in the view's coordinate space.
   * @param aView view to paint. should be root view
   * @param rect rect to mark as damaged
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  virtual void UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags) = 0;

  /**
   * Called to dispatch an event to the appropriate view. Often called
   * as a result of receiving a mouse or keyboard event from the widget
   * event system.
   * @param event event to dispatch
   * @result event handling status
   */
  virtual PRBool DispatchEvent(nsIEvent *event) = 0;

  /**
   * Used to grab/capture all mouse events for a specific view,
   * irrespective of the cursor position at which the
   * event occurred.
   * @param aView view to capture mouse events
   * @result event handling status
   */
  virtual PRBool GrabMouseEvents(nsIView *aView) = 0;

  /**
   * Used to grab/capture all keyboard events for a specific view,
   * irrespective of the cursor position at which the
   * event occurred.
   * @param aView view to capture keyboard events
   * @result event handling status
   */
  virtual PRBool GrabKeyEvents(nsIView *aView) = 0;

  /**
   * Get the current view, if any, that's capturing mouse events.
   * @result view that is capturing mouse events or nsnull
   */
  virtual nsIView* GetMouseEventGrabber() = 0;

  /**
   * Get the current view, if any, that's capturing keyboard events.
   * @result view that is capturing keyboard events or nsnull
   */
  virtual nsIView* GetKeyEventGrabber() = 0;

  /**
   * Given a parent view, insert another view as its child. If above
   * is PR_TRUE, the view is inserted above (in z-order) the sibling. If
   * it is PR_FALSE, the view is inserted below.
   * The view manager generates the appopriate dirty regions.
   * @param parent parent view
   * @param child child view
   * @param sibling sibling view
   * @param above boolean above or below state
   */
  virtual void InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                           PRBool above) = 0;

  /**
   * Given a parent view, insert another view as its child. The zindex
   * indicates where the child should be inserted relative to other
   * children of the parent.
   * The view manager generates the appopriate dirty regions.
   * @param parent parent view
   * @param child child view
   * @param zindex z depth of child
   */
  virtual void InsertChild(nsIView *parent, nsIView *child,
                           PRInt32 zindex) = 0;

  /**
   * Remove a specific child of a view.
   * The view manager generates the appopriate dirty regions.
   * @param parent parent view
   * @param child child view
   */
  virtual void RemoveChild(nsIView *parent, nsIView *child) = 0;

  /**
   * Move a view's position by the specified amount.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to move
   * @param x x offset to add to current view position
   * @param y y offset to add to current view position
   */
  virtual void MoveViewBy(nsIView *aView, nscoord x, nscoord y) = 0;

  /**
   * Move a view to the specified position,
   * provided in parent coordinates.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to move
   * @param x x value for new view position
   * @param y y value for new view position
   */
  virtual void MoveViewTo(nsIView *aView, nscoord x, nscoord y) = 0;

  /**
   * Resize a view to the specified width and height.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to move
   * @param width new view width
   * @param height new view height
   */
  virtual void ResizeView(nsIView *aView, nscoord width, nscoord height) = 0;

  /**
   * Set the clip of a view.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to to clip rect on
   * @param rect new clipping rect for view
   */
  virtual void SetViewClip(nsIView *aView, nsRect *rect) = 0;

  /**
   * Set the visibility of a view.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change visibility state of
   * @param visible new visibility state
   */
  virtual void SetViewVisibility(nsIView *aView, nsViewVisibility visible) = 0;

  /**
   * Set the z-index of a view. Positive z-indices mean that a view
   * is above its parent in z-order. Negative z-indices mean that a
   * view is below its parent.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param zindex new z depth
   */
  virtual void SetViewZindex(nsIView *aView, PRInt32 zindex) = 0;

  /**
   * Used to move a view above another in z-order.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param other view to move aView above
   */
  virtual void MoveViewAbove(nsIView *aView, nsIView *other) = 0;

  /**
   * Used to move a view below another in z-order.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param other view to move aView below
   */
  virtual void MoveViewBelow(nsIView *aView, nsIView *other) = 0;

  /**
   * Returns whether a view is actually shown (based on its visibility
   * and that of its ancestors).
   * @param aView view to query visibilty of
   * @result PR_TRUE if visible, else PR_FALSE
   */
  virtual PRBool IsViewShown(nsIView *aView) = 0;

  /**
   * Returns the clipping area of a view in absolute coordinates.
   * @param aView view to query clip rect of
   * @param rect to set with view's clipping rect
   * @result PR_TRUE if there is a clip rect, else PR_FALSE
   */
  virtual PRBool GetViewClipAbsolute(nsIView *aView, nsRect *rect) = 0;

  /**
   * Used set the transparency status of the content in a view. see
   * nsIView.HasTransparency().
   * @param aTransparent PR_TRUE if there are transparent areas, PR_FALSE otherwise.
   */
  virtual void SetViewContentTransparency(nsIView *aView, PRBool aTransparent) = 0;

  /**
   * Note: This didn't exist in 4.0. Called to set the opacity of a view. 
   * A value of 0.0 means completely transparent. A value of 1.0 means
   * completely opaque.
   * @param opacity new opacity value
   */
  virtual void SetViewOpacity(nsIView *aView, float aOpacity) = 0;

  /**
   * Get the presentation context associated with this manager
   * @result presentation context
   */
  virtual nsIPresContext* GetPresContext() = 0;

  /**
   * Set the area that the view manager considers to be "dirty"
   * to an empty state
   */
  virtual void ClearDirtyRegion() = 0;
};

//when the refresh happens, should it be double buffered?
#define NS_VMREFRESH_DOUBLE_BUFFER  0x0001
//is the damagerect in top-level window space?
#define NS_VMREFRESH_SCREEN_RECT    0x0002
//update view now?
#define NS_VMREFRESH_IMMEDIATE      0x0004
//prevent "sync painting"
#define NS_VMREFRESH_NO_SYNC        0x0008

#endif  // nsIViewManager_h___
