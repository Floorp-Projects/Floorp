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

typedef enum
{
  nsContentQuality_kGood = 0,
  nsContentQuality_kFair,
  nsContentQuality_kPoor
} nsContentQuality;

#define NS_IVIEWMANAGER_IID   \
{ 0x3a8863d0, 0xa7f3, 0x11d1, \
  { 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIViewManager : public nsISupports
{
public:
  /**
   * Initialize the ViewManager
   * Note: this instance does not hold a reference to the viewobserver
   * because it holds a reference to this instance.
   * @param aContext the device context to use.
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD  Init(nsIDeviceContext* aContext) = 0;

  /**
   * Get the root of the view tree.
   * @result the root view
   */
  NS_IMETHOD  GetRootView(nsIView *&aView) = 0;

  /**
   * Set the root of the view tree. Does not destroy the current root view.
   * @param aView view to set as root
   */
  NS_IMETHOD  SetRootView(nsIView *aView) = 0;

  /**
   * Get the current framerate i.e. the rate at which timed
   * refreshes occur. A framerate of 0 indicates that timed refreshes
   * should not occur. framerate is in terms of frames per second
   * @result the update rate in terms of frames per second
   */
  NS_IMETHOD  GetFrameRate(PRUint32& aRate) = 0;

  /**
   * Set the current framerate.
   * @param frameRate new update rate. see GetFrameRate for more info
   * @result the call may need to create a timer and the success
   *         status will be returned. NS_OK means all is well
   */
  NS_IMETHOD  SetFrameRate(PRUint32 frameRate) = 0;

  /**
   * Get the dimensions of the root window. The dimensions are in
   * twips
   * @param width out parameter for width of window in twips
   * @param height out parameter for height of window in twips
   */
  NS_IMETHOD  GetWindowDimensions(nscoord *width, nscoord *height) = 0;

  /**
   * Set the dimensions of the root window.
   * Called if the root window is resized. The dimensions are in
   * twips
   * @param width of window in twips
   * @param height of window in twips
   */
  NS_IMETHOD  SetWindowDimensions(nscoord width, nscoord height) = 0;

  /**
   * Reset the state of scrollbars and the scrolling region
   */
  NS_IMETHOD  ResetScrolling(void) = 0;

  /**
   * Called to force a redrawing of any dirty areas.
   */
  NS_IMETHOD  Composite(void) = 0;

  /**
   * Called to inform the view manager that some portion of a view
   * is dirty and needs to be redrawn. The region passed in
   * should be in the view's coordinate space.
   * @param aView view to paint. should be root view
   * @param region region to mark as damaged, if nsnull, then entire
   *               view is marked as damaged
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateView(nsIView *aView, nsIRegion *aRegion,
                         PRUint32 aUpdateFlags) = 0;

  /**
   * Called to inform the view manager that some portion of a view
   * is dirty and needs to be redrawn. The rect passed in
   * should be in the view's coordinate space.
   * @param aView view to paint. should be root view
   * @param rect rect to mark as damaged
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags) = 0;

  /**
   * Called to dispatch an event to the appropriate view. Often called
   * as a result of receiving a mouse or keyboard event from the widget
   * event system.
   * @param event event to dispatch
   * @result event handling status
   */
  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent, nsEventStatus& aStatus) = 0;

  /**
   * Used to grab/capture all mouse events for a specific view,
   * irrespective of the cursor position at which the
   * event occurred.
   * @param aView view to capture mouse events
   * @result event handling status
   */
  NS_IMETHOD  GrabMouseEvents(nsIView *aView, PRBool& aResult) = 0;

  /**
   * Used to grab/capture all keyboard events for a specific view,
   * irrespective of the cursor position at which the
   * event occurred.
   * @param aView view to capture keyboard events
   * @result event handling status
   */
  NS_IMETHOD  GrabKeyEvents(nsIView *aView, PRBool& aResult) = 0;

  /**
   * Get the current view, if any, that's capturing mouse events.
   * @result view that is capturing mouse events or nsnull
   */
  NS_IMETHOD  GetMouseEventGrabber(nsIView *&aView) = 0;

  /**
   * Get the current view, if any, that's capturing keyboard events.
   * @result view that is capturing keyboard events or nsnull
   */
  NS_IMETHOD  GetKeyEventGrabber(nsIView *&aView) = 0;

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
  NS_IMETHOD  InsertChild(nsIView *aParent, nsIView *aChild, nsIView *aSibling,
                          PRBool aAbove) = 0;

  /**
   * Given a parent view, insert another view as its child. The zindex
   * indicates where the child should be inserted relative to other
   * children of the parent.
   * The view manager generates the appopriate dirty regions.
   * @param parent parent view
   * @param child child view
   * @param zindex z depth of child
   */
  NS_IMETHOD  InsertChild(nsIView *aParent, nsIView *aChild,
                          PRInt32 aZIndex) = 0;

  /**
   * Remove a specific child of a view.
   * The view manager generates the appopriate dirty regions.
   * @param parent parent view
   * @param child child view
   */
  NS_IMETHOD  RemoveChild(nsIView *aParent, nsIView *aChild) = 0;

  /**
   * Move a view's position by the specified amount.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to move
   * @param x x offset to add to current view position
   * @param y y offset to add to current view position
   */
  NS_IMETHOD  MoveViewBy(nsIView *aView, nscoord aX, nscoord aY) = 0;

  /**
   * Move a view to the specified position,
   * provided in parent coordinates.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to move
   * @param x x value for new view position
   * @param y y value for new view position
   */
  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY) = 0;

  /**
   * Resize a view to the specified width and height.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to move
   * @param width new view width
   * @param height new view height
   */
  NS_IMETHOD  ResizeView(nsIView *aView, nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Set the clip of a view.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to to clip rect on
   * @param rect new clipping rect for view
   */
  NS_IMETHOD  SetViewClip(nsIView *aView, nsRect *aRect) = 0;

  /**
   * Set the visibility of a view.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change visibility state of
   * @param visible new visibility state
   */
  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible) = 0;

  /**
   * Set the z-index of a view. Positive z-indices mean that a view
   * is above its parent in z-order. Negative z-indices mean that a
   * view is below its parent.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param zindex new z depth
   */
  NS_IMETHOD  SetViewZIndex(nsIView *aView, PRInt32 aZindex) = 0;

  /**
   * Used to move a view above another in z-order.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param other view to move aView above
   */
  NS_IMETHOD  MoveViewAbove(nsIView *aView, nsIView *aOther) = 0;

  /**
   * Used to move a view below another in z-order.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param other view to move aView below
   */
  NS_IMETHOD  MoveViewBelow(nsIView *aView, nsIView *aOther) = 0;

  /**
   * Returns whether a view is actually shown (based on its visibility
   * and that of its ancestors).
   * @param aView view to query visibilty of
   * @result PR_TRUE if visible, else PR_FALSE
   */
  NS_IMETHOD  IsViewShown(nsIView *aView, PRBool &aResult) = 0;

  /**
   * Returns the clipping area of a view in absolute coordinates.
   * @param aView view to query clip rect of
   * @param rect to set with view's clipping rect
   * @result PR_TRUE if there is a clip rect, else PR_FALSE
   */
  NS_IMETHOD  GetViewClipAbsolute(nsIView *aView, nsRect *aRect, PRBool &aResult) = 0;

  /**
   * Used set the transparency status of the content in a view. see
   * nsIView.HasTransparency().
   * @param aTransparent PR_TRUE if there are transparent areas, PR_FALSE otherwise.
   */
  NS_IMETHOD  SetViewContentTransparency(nsIView *aView, PRBool aTransparent) = 0;

  /**
   * Note: This didn't exist in 4.0. Called to set the opacity of a view. 
   * A value of 0.0 means completely transparent. A value of 1.0 means
   * completely opaque.
   * @param opacity new opacity value
   */
  NS_IMETHOD  SetViewOpacity(nsIView *aView, float aOpacity) = 0;

  /**
   * Set the view observer associated with this manager
   * @param aObserver - new observer
   * @result error status
   */
  NS_IMETHOD SetViewObserver(nsIViewObserver *aObserver) = 0;

  /**
   * Get the view observer associated with this manager
   * @param aObserver - out parameter for observer
   * @result error status
   */
  NS_IMETHOD GetViewObserver(nsIViewObserver *&aObserver) = 0;

  /**
   * Get the device context associated with this manager
   * @result device context
   */
  NS_IMETHOD  GetDeviceContext(nsIDeviceContext *&aContext) = 0;

  /**
   * Select whether quality level should be displayed in root view
   * @param aShow if PR_TRUE, quality level will be displayed, else hidden
   */
  NS_IMETHOD  ShowQuality(PRBool aShow) = 0;

  /**
   * Query whether quality level should be displayed in view frame
   * @return if PR_TRUE, quality level will be displayed, else hidden
   */
  NS_IMETHOD  GetShowQuality(PRBool &aResult) = 0;

  /**
   * Select whether quality level should be displayed in root view
   * @param aShow if PR_TRUE, quality level will be displayed, else hidden
   */
  NS_IMETHOD  SetQuality(nsContentQuality aQuality) = 0;

  /**
   * prevent the view manager from refreshing.
   * @return error status
   */
  NS_IMETHOD DisableRefresh(void) = 0;

  /**
   * allow the view manager to refresh. this may cause a synchronous
   * paint to occur inside the call.
   * @return error status
   */
  NS_IMETHOD EnableRefresh(void) = 0;

  /**
   * Display the specified view. Used when printing.
   */
  NS_IMETHOD Display(nsIView *aView) = 0;
};

//when the refresh happens, should it be double buffered?
#define NS_VMREFRESH_DOUBLE_BUFFER      0x0001
//update view now?
#define NS_VMREFRESH_IMMEDIATE          0x0002
//prevent "sync painting"
#define NS_VMREFRESH_NO_SYNC            0x0004
//if the total damage area is greater than 25% of the
//area of the root view, use double buffering
#define NS_VMREFRESH_AUTO_DOUBLE_BUFFER 0x0008

#endif  // nsIViewManager_h___
