/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsViewManager_h___
#define nsViewManager_h___
#include "nsCOMPtr.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "nsIWidget.h"
#include "nsITimer.h"
#include "prtime.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsThreadUtils.h"
#include "nsIScrollableView.h"
#include "nsIRegion.h"
#include "nsIBlender.h"
#include "nsView.h"

class nsISupportsArray;
class BlendingBuffers;

//Uncomment the following line to enable generation of viewmanager performance data.
#ifdef MOZ_PERF_METRICS
//#define NS_VM_PERF_METRICS 1 
#endif

#ifdef NS_VM_PERF_METRICS
#include "nsTimer.h"
#endif

/**
   FIXED-POSITION FRAMES AND Z-ORDERING

   Fixed-position frames are special. They have TWO views. There is the "real" view, which is
   a child of the root view for the viewport (which is the root view of the view manager).
   There is also a "placeholder" view (of class nsZPlaceholderView) which never really
   participates in any view operations. It is a child of the view that would have contained
   the fixed-position element if it had not been fixed-position. The real view keeps track
   of the placeholder view and returns the placeholder view when you call GetZParent on the
   real view.

   (Although currently all views which have a placeholder view are themselves children of the
   root view, we don't want to depend on this. Later we might want to support views that
   are fixed relative to some container other than the viewport.)

   As we build the display list in CreateDisplayList, once we've processed the parent of
   real views (i.e., the root), we move those real views from their current position in the
   display list over to where their placeholder views are in the display list. This ensures that
   views get repainted in the order they would have been repainted in the absence of
   fixed-position frames.
 */

/**
   Invalidation model:

   1) Callers call into the view manager and ask it to update a view.
   
   2) The view manager finds the "right" widget for the view, henceforth called
      the root widget.

   3) The view manager traverses descendants of the root widget and for each
      one that needs invalidation either

      a)  Calls Invalidate() on the widget (no batching)
    or
      b)  Stores the rect to invalidate on the widget's view (batching)

   // XXXbz we want to change this a bit.  See bug 243726

   4) When batching, the call to end the batch either processes the pending
      Invalidate() calls on the widgets or posts an event to do so.

   It's important to note that widgets associated to views outside this view
   manager can end up being invalidated during step 3.  Therefore, the end of a
   view update batch really needs to traverse the entire view tree, to ensure
   that those invalidates happen.

   To cope with this, invalidate event processing and view update batch
   handling should only happen on the root viewmanager.  This means the root
   view manager is the only thing keeping track of mUpdateCnt.  As a result,
   Composite() calls should also be forwarded to the root view manager.
*/

class nsZPlaceholderView : public nsView
{
public:
  nsZPlaceholderView(nsViewManager* aViewManager) : nsView(aViewManager) {}

  void RemoveReparentedView() { mReparentedView = nsnull; }
  void SetReparentedView(nsView* aView) { mReparentedView = aView; }
  nsView* GetReparentedView() const { return mReparentedView; }

  virtual PRBool IsZPlaceholderView() const { return PR_TRUE; }

protected:
  virtual ~nsZPlaceholderView() {
    if (nsnull != mReparentedView) {
      mReparentedView->SetZParent(nsnull);
    }
  }

protected:
  nsView   *mReparentedView;
};

class nsViewManagerEvent : public nsRunnable {
public:
  nsViewManagerEvent(class nsViewManager *vm) : mViewManager(vm) {
    NS_ASSERTION(mViewManager, "null parameter");
  }
  void Revoke() { mViewManager = nsnull; }
protected:
  class nsViewManager *mViewManager;
};

class nsViewManager : public nsIViewManager {
public:
  nsViewManager();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsIDeviceContext* aContext);

  NS_IMETHOD_(nsIView*) CreateView(const nsRect& aBounds,
                                   const nsIView* aParent,
                                   nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  NS_IMETHOD_(nsIScrollableView*) CreateScrollableView(const nsRect& aBounds,
                                                       const nsIView* aParent);

  NS_IMETHOD  GetRootView(nsIView *&aView);
  NS_IMETHOD  SetRootView(nsIView *aView);

  NS_IMETHOD  GetWindowDimensions(nscoord *width, nscoord *height);
  NS_IMETHOD  SetWindowDimensions(nscoord width, nscoord height);

  NS_IMETHOD  Composite(void);

  NS_IMETHOD  UpdateView(nsIView *aView, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateAllViews(PRUint32 aUpdateFlags);

  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent, nsEventStatus* aStatus);

  NS_IMETHOD  GrabMouseEvents(nsIView *aView, PRBool &aResult);

  NS_IMETHOD  GetMouseEventGrabber(nsIView *&aView);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                          PRBool above);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child,
                          PRInt32 zindex);

  NS_IMETHOD  InsertZPlaceholder(nsIView *parent, nsIView *child, nsIView *sibling,
                                 PRBool above);

  NS_IMETHOD  RemoveChild(nsIView *parent);

  NS_IMETHOD  MoveViewBy(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  ResizeView(nsIView *aView, const nsRect &aRect, PRBool aRepaintExposedAreaOnly = PR_FALSE);

  NS_IMETHOD  SetViewCheckChildEvents(nsIView *aView, PRBool aEnable);

  NS_IMETHOD  SetViewFloating(nsIView *aView, PRBool aFloating);

  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible);

  NS_IMETHOD  SetViewZIndex(nsIView *aView, PRBool aAuto, PRInt32 aZIndex, PRBool aTopMost=PR_FALSE);

  NS_IMETHOD  SetViewObserver(nsIViewObserver *aObserver);
  NS_IMETHOD  GetViewObserver(nsIViewObserver *&aObserver);

  NS_IMETHOD  GetDeviceContext(nsIDeviceContext *&aContext);

  NS_IMETHOD  DisableRefresh(void);
  NS_IMETHOD  EnableRefresh(PRUint32 aUpdateFlags);

  NS_IMETHOD  BeginUpdateViewBatch(void);
  NS_IMETHOD  EndUpdateViewBatch(PRUint32 aUpdateFlags);

  NS_IMETHOD  SetRootScrollableView(nsIScrollableView *aScrollable);
  NS_IMETHOD  GetRootScrollableView(nsIScrollableView **aScrollable);

  NS_IMETHOD Display(nsIView *aView, nscoord aX, nscoord aY, const nsRect& aClipRect);

  NS_IMETHOD RenderOffscreen(nsIView* aView, nsRect aRect, PRBool aUntrusted,
                             PRBool aIgnoreViewportScrolling,
                             nscolor aBackgroundColor,
                             nsIRenderingContext** aRenderedContext);

  NS_IMETHOD AddCompositeListener(nsICompositeListener *aListener);
  NS_IMETHOD RemoveCompositeListener(nsICompositeListener *aListener);

  NS_IMETHOD GetWidget(nsIWidget **aWidget);
  nsIWidget* GetWidget() { return mRootView ? mRootView->GetWidget() : nsnull; }
  NS_IMETHOD ForceUpdate();
 
  NS_IMETHOD AllowDoubleBuffering(PRBool aDoubleBuffer);
  NS_IMETHOD IsPainting(PRBool& aIsPainting);
  NS_IMETHOD SetDefaultBackgroundColor(nscolor aColor);
  NS_IMETHOD GetDefaultBackgroundColor(nscolor* aColor);
  NS_IMETHOD GetLastUserEventTime(PRUint32& aTime);
  void ProcessInvalidateEvent();
  static PRInt32 GetViewManagerCount();
  static const nsVoidArray* GetViewManagerArray();
  static PRUint32 gLastUserEventTime;

  /**
   * Determine if a rectangle specified in the view's coordinate system 
   * is completely, or partially visible.
   * @param aView view that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to test for visibility 
   * @param aMinTwips is the min. pixel rows or cols at edge of screen 
   *                  needed for object to be counted visible
   * @param aRectVisibility returns eVisible if the rect is visible, 
   *                        otherwise it returns an enum indicating why not
   */
  NS_IMETHOD GetRectVisibility(nsIView *aView, const nsRect &aRect, 
                               PRUint16 aMinTwips, 
                               nsRectVisibility *aRectVisibility);

  NS_IMETHOD SynthesizeMouseMove(PRBool aFromScroll);
  void ProcessSynthMouseMoveEvent(PRBool aFromScroll);

  /* Update the cached RootViewManager pointer on this view manager. */
  void InvalidateHierarchy();

protected:
  virtual ~nsViewManager();

private:
  void FlushPendingInvalidates();
  void ProcessPendingUpdates(nsView *aView, PRBool aDoInvalidate);
  void ReparentChildWidgets(nsIView* aView, nsIWidget *aNewWidget);
  void ReparentWidgets(nsIView* aView, nsIView *aParent);
  already_AddRefed<nsIRenderingContext> CreateRenderingContext(nsView &aView);
  void UpdateWidgetArea(nsView *aWidgetView, const nsRegion &aDamagedRegion,
                        nsView* aIgnoreWidgetView);

  void UpdateViews(nsView *aView, PRUint32 aUpdateFlags);

  void Refresh(nsView *aView, nsIRenderingContext *aContext,
               nsIRegion *region, PRUint32 aUpdateFlags);
  /**
   * Refresh aView (which must be non-null) with our default background color
   */
  void DefaultRefresh(nsView* aView, nsIRenderingContext *aContext, const nsRect* aRect);
  void RenderViews(nsView *aRootView, nsIRenderingContext& aRC,
                   const nsRegion& aRegion, nsIDrawingSurface* aRCSurface);

  void InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut, PRUint32 aUpdateFlags);
  void InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
                                          PRUint32 aUpdateFlags, nscoord aY1, nscoord aY2, PRBool aInCutOut);

  virtual BlendingBuffers* CreateBlendingBuffers(nsIRenderingContext *aRC, PRBool aBorrowContext,
                                                 nsIDrawingSurface* aBorrowSurface, PRBool aNeedAlpha,
                                                 const nsRect& aArea);
  virtual nsIBlender* GetBlender() { return mBlender; }

  void AddCoveringWidgetsToOpaqueRegion(nsRegion &aRgn, nsIDeviceContext* aContext,
                                        nsView* aRootView);

  // Predicates
  PRBool DoesViewHaveNativeWidget(nsView* aView);

  // Utilities

  PRBool IsViewInserted(nsView *aView);

  /**
   * Function to recursively call Update() on all widgets belonging to
   * a view or its kids.
   */
  void UpdateWidgetsForView(nsView* aView);

  /**
   * Returns the nearest parent view with an attached widget. Can be the
   * same view as passed-in.
   */
  static nsView* GetWidgetView(nsView *aView);

  /**
   * Transforms a rectangle from specified view's coordinate system to
   * the first parent that has an attached widget.
   */
  void ViewToWidget(nsView *aView, nsView* aWidgetView, nsRect &aRect) const;

  /**
   * Transforms a rectangle from specified view's coordinate system to
   * an absolute coordinate rectangle which can be compared against the
   * rectangle returned by GetVisibleRect to determine visibility.
   * @param aView view that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to convert to absolute coordinates
   * @param aAbsRect rectangle in absolute coorindates.
   * @returns NS_OK if successful otherwise, NS_ERROR_FAILURE 
   */

  nsresult GetAbsoluteRect(nsView *aView, const nsRect &aRect, 
                           nsRect& aAbsRect);
  /**
   * Determine the visible rect 
   * @param aVisibleRect visible rectangle in twips
   * @returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
   */

  nsresult GetVisibleRect(nsRect& aVisibleRect);

  // Utilities used to size the offscreen drawing surface

  /**
   * Determine the maximum and width and height of all of the
   * view manager's widgets.
   *
   * @param aMaxWidgetBounds the maximum width and height of all view managers
   * widgets on exit.
   */
  void GetMaxWidgetBounds(nsRect& aMaxWidgetBounds) const;

  void DoSetWindowDimensions(nscoord aWidth, nscoord aHeight)
  {
    nsRect oldDim;
    nsRect newDim(0, 0, aWidth, aHeight);
    mRootView->GetDimensions(oldDim);
    if (oldDim != newDim) {
      // Don't resize the widget. It is already being set elsewhere.
      mRootView->SetDimensions(newDim, PR_TRUE, PR_FALSE);
      if (mObserver)
        mObserver->ResizeReflow(mRootView, aWidth, aHeight);
    }
  }

  // Safety helpers
  void IncrementUpdateCount() {
    NS_ASSERTION(IsRootVM(),
                 "IncrementUpdateCount called on non-root viewmanager");
    ++mUpdateCnt;
  }

  void DecrementUpdateCount() {
    NS_ASSERTION(IsRootVM(),
                 "DecrementUpdateCount called on non-root viewmanager");
    --mUpdateCnt;
  }

  PRInt32 UpdateCount() const {
    NS_ASSERTION(IsRootVM(),
                 "DecrementUpdateCount called on non-root viewmanager");
    return mUpdateCnt;
  }

  void ClearUpdateCount() {
    NS_ASSERTION(IsRootVM(),
                 "DecrementUpdateCount called on non-root viewmanager");
    mUpdateCnt = 0;
  }

  PRBool IsPainting() const {
    return RootViewManager()->mPainting;
  }

  void SetPainting(PRBool aPainting) {
    RootViewManager()->mPainting = aPainting;
  }

public: // NOT in nsIViewManager, so private to the view module
  nsView* GetRootView() const { return mRootView; }
  nsView* GetMouseEventGrabber() const {
    return RootViewManager()->mMouseGrabber;
  }
  nsViewManager* RootViewManager() const { return mRootViewManager; }
  PRBool IsRootVM() const { return this == RootViewManager(); }

  nsEventStatus HandleEvent(nsView* aView, nsPoint aPoint, nsGUIEvent* aEvent,
                            PRBool aCaptured);

  /**
   * Called to inform the view manager that a view is about to bit-blit.
   * @param aView the view that will bit-blit
   * @param aScrollAmount how much aView will scroll by
   * @return always returns NS_OK
   * @note
   * This method used to return void, but MSVC 6.0 SP5 (without the
   * Processor Pack) and SP6, and the MS eMbedded Visual C++ 4.0 SP4
   * (for WINCE) hit an internal compiler error when compiling this
   * method:
   *
   * @par
@verbatim
       fatal error C1001: INTERNAL COMPILER ERROR
                   (compiler file 'E:\8966\vc98\p2\src\P2\main.c', line 494)
@endverbatim
   *
   * @par
   * Making the method return nsresult worked around the internal
   * compiler error.  See Bugzilla bug 281158.  (The WINCE internal
   * compiler error was addressed by the patch in bug 291229 comment
   * 14 although the bug report did not mention the problem.)
   */
  nsresult WillBitBlit(nsView* aView, nsPoint aScrollAmount);
  
  /**
   * Called to inform the view manager that a view has scrolled.
   * The view manager will invalidate any widgets which may need
   * to be rerendered.
   * @param aView view to paint. should be the nsScrollPortView that
   * got scrolled.
   * @param aUpdateRegion ensure that this part of the view is repainted
   */
  void UpdateViewAfterScroll(nsView *aView, const nsRegion& aUpdateRegion);

  /**
   * Asks whether we can scroll a view using bitblt. If we say 'yes', we
   * return in aUpdateRegion an area that must be updated (relative to aView
   * after it has been scrolled).
   */
  PRBool CanScrollWithBitBlt(nsView* aView, nsPoint aDelta, nsRegion* aUpdateRegion);

  nsresult CreateRegion(nsIRegion* *result);

  // return the sum of all view offsets from aView right up to the
  // root of this view hierarchy (the view with no parent, which might
  // not be in this view manager).
  static nsPoint ComputeViewOffset(const nsView *aView);

  PRBool IsRefreshEnabled() { return RootViewManager()->mRefreshEnabled; }

  nsIViewObserver* GetViewObserver() { return mObserver; }

  // Call this when you need to let the viewmanager know that it now has
  // pending updates.
  void PostPendingUpdate() { RootViewManager()->mHasPendingUpdates = PR_TRUE; }
private:
  nsIDeviceContext  *mContext;
  nsIViewObserver   *mObserver;
  nsIScrollableView *mRootScrollable;
  nscolor           mDefaultBackgroundColor;
  nsPoint           mMouseLocation; // device units, relative to mRootView

  // The size for a resize that we delayed until the root view becomes
  // visible again.
  nsSize            mDelayedResize;

  nsCOMPtr<nsIBlender> mBlender;
  nsISupportsArray  *mCompositeListeners;
  nsCOMPtr<nsIFactory> mRegionFactory;
  nsView            *mRootView;
  // mRootViewManager is a strong ref unless it equals |this|.  It's
  // never null (if we have no ancestors, it will be |this|).
  nsViewManager     *mRootViewManager;
  PRPackedBool      mAllowDoubleBuffering;

  nsRevocableEventPtr<nsViewManagerEvent> mSynthMouseMoveEvent;
  nsRevocableEventPtr<nsViewManagerEvent> mInvalidateEvent;

  // The following members should not be accessed directly except by
  // the root view manager.  Some have accessor functions to enforce
  // this, as noted.
  
  // Use GrabMouseEvents() and GetMouseEventGrabber() to access mMouseGrabber.
  nsView            *mMouseGrabber;
  // Use IncrementUpdateCount(), DecrementUpdateCount(), UpdateCount(),
  // ClearUpdateCount() on the root viewmanager to access mUpdateCnt.
  PRInt32           mUpdateCnt;
  PRInt32           mUpdateBatchCnt;
  PRUint32          mUpdateBatchFlags;
  PRInt32           mScrollCnt;
  // Use IsRefreshEnabled() to check the value of mRefreshEnabled.
  PRPackedBool      mRefreshEnabled;
  // Use IsPainting() and SetPainting() to access mPainting.
  PRPackedBool      mPainting;
  PRPackedBool      mRecursiveRefreshPending;
  PRPackedBool      mHasPendingUpdates;
  PRPackedBool      mInScroll;

  //from here to public should be static and locked... MMP
  static PRInt32           mVMCount;        //number of viewmanagers

  //Rendering context used to cleanup the blending buffers
  static nsIRenderingContext* gCleanupContext;

  //list of view managers
  static nsVoidArray       *gViewManagers;

  void PostInvalidateEvent();

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DECLARE(mWatch) //  Measures compositing+paint time for current document
#endif
};

//when the refresh happens, should it be double buffered?
#define NS_VMREFRESH_DOUBLE_BUFFER      0x0001

#endif /* nsViewManager_h___ */
