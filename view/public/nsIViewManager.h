/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIViewManager_h___
#define nsIViewManager_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsCoord.h"
#include "nsIView.h"
#include "nsColor.h"

class nsIRegion;
class nsIEvent;
class nsIPresContext;
class nsIView;
class nsIScrollableView;
class nsIWidget;
class nsICompositeListener;
struct nsRect;
class nsIDeviceContext;

enum nsContentQuality {
  nsContentQuality_kGood = 0,
  nsContentQuality_kFair,
  nsContentQuality_kPoor,
  nsContentQuality_kUnknown
};

#define NS_IVIEWMANAGER_IID   \
{ 0x3a8863d0, 0xa7f3, 0x11d1, \
  { 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

class nsIViewManager : public nsISupports
{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IVIEWMANAGER_IID; return iid; }
  /**
   * Initialize the ViewManager
   * Note: this instance does not hold a reference to the viewobserver
   * because it holds a reference to this instance.
   * @param aContext the device context to use.
   * @param aX X offset of the view manager's coordinate space in twips
   * @param aY Y offset of the view manager's coordinate space in twips
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD  Init(nsIDeviceContext* aContext, nscoord aX = 0, nscoord aY = 0) = 0;

  /**
   * Get the root of the view tree.
   * @result the root view
   */
  NS_IMETHOD  GetRootView(nsIView *&aView) = 0;

  /**
   * Set the root of the view tree. Does not destroy the current root view.
   * One of following must be true:
   * a) the aWidget parameter is an nsIWidget instance to render into 
   *    that is not owned by any view or
   * b) aView has a nsIWidget instance or
   * c) aView has a parent view managed by a different view manager 
   * @param aView view to set as root
   * @param aWidget widget to render into. (Can not be owned by a view)
   */
  NS_IMETHOD  SetRootView(nsIView *aView, nsIWidget* aWidget = nsnull) = 0;

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
   * Called to inform the view manager that the entire area of a view
   * is dirty and needs to be redrawn.
   * @param aView view to paint. should be root view
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateView(nsIView *aView, PRUint32 aUpdateFlags) = 0;

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
   * Called to inform the view manager that it should redraw all views.
   * @param aView view to paint. should be root view
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateAllViews(PRUint32 aUpdateFlags) = 0;

  /**
   * Called to inform the view manager that a view has scrolled.
   * The view manager will invalidate any widgets which may need
   * to be rerendered.
   * @param aView view to paint. should be root view
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateViewAfterScroll(nsIView *aView, PRInt32 aDX, PRInt32 aDY) = 0;

  /**
   * Called to dispatch an event to the appropriate view. Often called
   * as a result of receiving a mouse or keyboard event from the widget
   * event system.
   * @param event event to dispatch
   * @result event handling status
   */
  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent, nsEventStatus* aStatus) = 0;

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

  NS_IMETHOD  InsertZPlaceholder(nsIView *aParent, nsIView *aZChild,
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
   * @param RepaintExposedAreaOnly if PR_TRUE Repaint only the expanded or contracted region,
   *                               if PR_FALSE Repaint the union of the old and new rectangles.
   *                          
   */
  NS_IMETHOD  ResizeView(nsIView *aView, nscoord aWidth, nscoord aHeight, PRBool aRepaintExposedAreaOnly = PR_FALSE) = 0;

  /**
   * Set the clipping of a view's children
   * The view manager generates the appopriate dirty regions.
   * @param aView view set clip children rect on
   * @param rect new clipping rect for view's children
   */
  NS_IMETHOD  SetViewChildClip(nsIView *aView, nsRect *aRect) = 0;

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
   * Indicate that the z-index of a view is "auto". An "auto" z-index
   * means that the view does not define a new stacking context,
   * which means that the z-indicies of the view's children are
   * relative to the view's siblings.
   * The view manager generates the appopriate dirty regions.
   * @param aView view to change z depth of
   * @param zindex new z depth
   */
  NS_IMETHOD  SetViewAutoZIndex(nsIView *aView, PRBool aAutoZIndex) = 0;

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
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   * @return error status
   */
  NS_IMETHOD EnableRefresh(PRUint32 aUpdateFlags) = 0;

  /**
   * prevents the view manager from refreshing. allows UpdateView()
   * to notify widgets of damaged regions that should be repainted
   * when the batch is ended.
   * @return error status
   */
  NS_IMETHOD BeginUpdateViewBatch(void) = 0;

  /**
   * allow the view manager to refresh any damaged areas accumulated
   * after the BeginUpdateViewBatch() call.  this may cause a
   * synchronous paint to occur inside the call if aUpdateFlags NS_VMREFRESH_IMMEDIATE is set
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   * @return error status
   */
  NS_IMETHOD EndUpdateViewBatch(PRUint32 aUpdateFlags) = 0;

  /**
   * set the view that is is considered to be the root scrollable
   * view for the document.
   * @param aScrollable root scrollable view
   * @return error status
   */
  NS_IMETHOD SetRootScrollableView(nsIScrollableView *aScrollable) = 0;

  /**
   * get the view that is is considered to be the root scrollable
   * view for the document.
   * @param aScrollable out parameter for root scrollable view
   * @return error status
   */
  NS_IMETHOD GetRootScrollableView(nsIScrollableView **aScrollable) = 0;

  /**
   * Display the specified view. Used when printing.
   */
  NS_IMETHOD Display(nsIView *aView, nscoord aX, nscoord aY) = 0;

  /**
   * Add a listener to the view manager's composite listener list.
   * @param aListener - new listener
   * @result error status
   */
  NS_IMETHOD AddCompositeListener(nsICompositeListener *aListener) = 0;

  /**
   * Remove a listener from the view manager's composite listener list.
   * @param aListener - listener to remove
   * @result error status
   */
  NS_IMETHOD RemoveCompositeListener(nsICompositeListener *aListener) = 0;

  /**
   * Retrieve the widget that a view renders into.
   * @param aView the view to get the widget for 
   * @param aWidget the widget that aView renders into.
   * @result error status
   */

  NS_IMETHOD GetWidgetForView(nsIView *aView, nsIWidget **aWidget) = 0;

  /**
   * Retrieve the widget that a view manager renders into
   * @param aWidget the widget that aView renders into.
   * @result error status
   */

  NS_IMETHOD GetWidget(nsIWidget **aWidget) = 0;

  /**
   * Force update of view manager widget
   * Callers should use UpdateView(view, NS_VMREFRESH_IMMEDIATE) in most cases instead
   * @result error status
   */

  NS_IMETHOD ForceUpdate() = 0;

  
  /**
   * Get view manager offset specified in nsIViewManager::Init
   * @param aX x offset in twips
   * @param aY y offset in twips
   * @result error status
   */

  NS_IMETHOD GetOffset(nscoord *aX, nscoord *aY) = 0;

  /**
   * Turn widget on or off widget movement caching
   */
  NS_IMETHOD IsCachingWidgetChanges(PRBool* aCaching)=0;


  /**
   * Pass true to cache widget changes. pass false to stop. When false is passed
   * All widget changes will be applied.
   */
  NS_IMETHOD CacheWidgetChanges(PRBool aCache)=0;

  /**
   * Control double buffering of the display. If double buffering
   * is enabled the viewmanager is allowed to render to an offscreen
   * drawing surface before copying to the display in order to prevent
   * flicker. If it is disabled all rendering will appear directly on the
   * the display. The display is double buffered by default.
   * @param aDoubleBuffer PR_TRUE to enable double buffering
   *                      PR_FALSE to disable double buffering
   */
  NS_IMETHOD AllowDoubleBuffering(PRBool aDoubleBuffer)=0;

  /**
   * Indicate whether the viewmanager is currently painting
   *
   * @param aPainting PR_TRUE if the viewmanager is painting
   *                  PR_FALSE otherwise
   */
  NS_IMETHOD IsPainting(PRBool& aIsPainting)=0;


  /**
   * Flush pending invalidates which have been queued up
   * between DisableRefresh and EnableRefresh calls. 
   */
  NS_IMETHOD FlushPendingInvalidates()=0;


  /**
   * Set the default background color that the view manager should use
   * to paint otherwise unowned areas. If the color isn't known, just set
   * it to zero (which means 'transparent' since the color is RGBA).
   *
   * @param aColor the default background color
   */
  NS_IMETHOD SetDefaultBackgroundColor(nscolor aColor)=0;

  /**
   * Retrieve the default background color.
   *
   * @param aColor the default background color
   */
  NS_IMETHOD GetDefaultBackgroundColor(nscolor* aColor)=0;

  /**
   * Retrieve the time of the last user event. User events
   * include mouse and keyboard events. The viewmanager
   * saves the time of the last user event.
   *
   * @param aTime Last user event time in microseconds
   */
  NS_IMETHOD GetLastUserEventTime(PRUint32& aTime)=0;

  /**
	 * Determine if a rectangle specified in the view's coordinate system 
	 * is completely, or partially visible.
   * @param aView view that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to test for visibility 
   * @returns PR_TRUE if the rect is visible, PR_FALSE otherwise.
	 */
  NS_IMETHOD IsRectVisible(nsIView *aView, const nsRect &aRect, PRBool aMustBeFullyVisible, PRBool *isVisible)=0;
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
