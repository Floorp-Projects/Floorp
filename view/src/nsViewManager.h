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
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsITimer.h"
#include "prtime.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsIScrollableView.h"
#include "nsIRegion.h"
#include "nsIBlender.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsView.h"

class nsIRegion;
class nsIEvent;
class nsIPresContext;
class nsISupportsArray;
struct DisplayListElement2;
struct DisplayZTreeNode;
class BlendingBuffers;
struct PLArenaPool;
class nsHashtable;

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

class nsZPlaceholderView : public nsView
{
public:
  nsZPlaceholderView() : nsView() {}

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

class nsViewManager : public nsIViewManager {
public:
  nsViewManager();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsIDeviceContext* aContext);

  NS_IMETHOD  GetRootView(nsIView *&aView);
  NS_IMETHOD  SetRootView(nsIView *aView);

  NS_IMETHOD  GetWindowDimensions(nscoord *width, nscoord *height);
  NS_IMETHOD  SetWindowDimensions(nscoord width, nscoord height);

  NS_IMETHOD  ResetScrolling(void);

  NS_IMETHOD  Composite(void);

  NS_IMETHOD  UpdateView(nsIView *aView, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateAllViews(PRUint32 aUpdateFlags);

  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent, nsEventStatus* aStatus);

  NS_IMETHOD  GrabMouseEvents(nsIView *aView, PRBool &aResult);
  NS_IMETHOD  GrabKeyEvents(nsIView *aView, PRBool &aresult);

  NS_IMETHOD  GetMouseEventGrabber(nsIView *&aView);
  NS_IMETHOD  GetKeyEventGrabber(nsIView *&aView);

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

  NS_IMETHOD  SetViewChildClipRegion(nsIView *aView, const nsRegion *aRegion);

  NS_IMETHOD  SetViewBitBltEnabled(nsIView *aView, PRBool aEnable);

  NS_IMETHOD  SetViewCheckChildEvents(nsIView *aView, PRBool aEnable);

  NS_IMETHOD  SetViewFloating(nsIView *aView, PRBool aFloating);

  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible);

  NS_IMETHOD  SetViewZIndex(nsIView *aView, PRBool aAuto, PRInt32 aZIndex, PRBool aTopMost=PR_FALSE);
  NS_IMETHOD  SetViewContentTransparency(nsIView *aView, PRBool aTransparent);
  NS_IMETHOD  SetViewOpacity(nsIView *aView, float aOpacity);

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

  NS_IMETHOD AddCompositeListener(nsICompositeListener *aListener);
  NS_IMETHOD RemoveCompositeListener(nsICompositeListener *aListener);

  NS_IMETHOD GetWidget(nsIWidget **aWidget);
  nsIWidget* GetWidget() { return mRootView ? mRootView->GetWidget() : nsnull; }
  NS_IMETHOD ForceUpdate();
 
  NS_IMETHOD IsCachingWidgetChanges(PRBool* aCaching);
  NS_IMETHOD CacheWidgetChanges(PRBool aCache);
  NS_IMETHOD AllowDoubleBuffering(PRBool aDoubleBuffer);
  NS_IMETHOD IsPainting(PRBool& aIsPainting);
  NS_IMETHOD FlushPendingInvalidates();
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

protected:
  virtual ~nsViewManager();
  void ProcessPendingUpdates(nsView *aView);

private:
  void ReparentChildWidgets(nsIView* aView, nsIWidget *aNewWidget);
  void ReparentWidgets(nsIView* aView, nsIView *aParent);
  already_AddRefed<nsIRenderingContext> CreateRenderingContext(nsView &aView);
  void AddRectToDirtyRegion(nsView* aView, const nsRect &aRect) const;

  PRBool UpdateWidgetArea(nsView *aWidgetView, const nsRect &aDamagedRect, nsView* aIgnoreWidgetView);

  void UpdateViews(nsView *aView, PRUint32 aUpdateFlags);

  void Refresh(nsView *aView, nsIRenderingContext *aContext,
               nsIRegion *region, PRUint32 aUpdateFlags);
  /**
   * Refresh aView (which must be non-null) with our default background color
   */
  void DefaultRefresh(nsView* aView, const nsRect* aRect);
  PRBool BuildRenderingDisplayList(nsIView* aRootView,
    const nsRegion& aRegion, nsVoidArray* aDisplayList, PLArenaPool &aPool);
  void RenderViews(nsView *aRootView, nsIRenderingContext& aRC,
                   const nsRegion& aRegion, nsDrawingSurface aRCSurface,
                   const nsVoidArray& aDisplayList);

  void RenderDisplayListElement(DisplayListElement2* element,
                                nsIRenderingContext* aRC);

  void PaintView(nsView *aView, nsIRenderingContext &aRC, nscoord x, nscoord y,
                 const nsRect &aDamageRect);

  void InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut, PRUint32 aUpdateFlags);
  void InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
                                          PRUint32 aUpdateFlags, nscoord aY1, nscoord aY2, PRBool aInCutOut);

  BlendingBuffers* CreateBlendingBuffers(nsIRenderingContext *aRC, PRBool aBorrowContext,
                                         nsDrawingSurface aBorrowSurface, PRBool aNeedAlpha,
                                         const nsRect& aArea);

  void ReparentViews(DisplayZTreeNode* aNode, nsHashtable &);
  void BuildDisplayList(nsView* aView, const nsRect& aRect, PRBool aEventProcessing,
                        PRBool aCaptured, nsVoidArray* aDisplayList, PLArenaPool &aPool);
  void BuildEventTargetList(nsVoidArray &aTargets, nsView* aView, nsGUIEvent* aEvent, PRBool aCaptured, PLArenaPool &aPool);

  PRBool CreateDisplayList(nsView *aView,
                           DisplayZTreeNode* &aResult,
                           nscoord aOriginX, nscoord aOriginY,
                           nsView *aRealView, const nsRect *aDamageRect,
                           nsView *aTopView, nscoord aX, nscoord aY,
                           PRBool aPaintFloats, PRBool aEventProcessing,
                           nsHashtable&, PLArenaPool &aPool);
  PRBool AddToDisplayList(nsView *aView,
                          DisplayZTreeNode* &aParent, nsRect &aClipRect,
                          nsRect& aDirtyRect, PRUint32 aFlags, nscoord aAbsX, nscoord aAbsY,
                          PRBool aAssumeIntersection, PLArenaPool &aPool);
  void OptimizeDisplayList(const nsVoidArray* aDisplayList, const nsRegion& aDirtyRegion,
                           nsRect& aFinalTransparentRect, nsRegion& aOpaqueRgn,
                           PRBool aTreatUniformAsOpaque);

  void AddCoveringWidgetsToOpaqueRegion(nsRegion &aRgn, nsIDeviceContext* aContext,
                                        nsView* aRootView);

  // Predicates
  PRBool DoesViewHaveNativeWidget(nsView* aView);

  void PauseTimer(void);
  void RestartTimer(void);
  void OptimizeDisplayListClipping(const nsVoidArray* aDisplayList, PRBool aHaveClip,
                                   nsRect& aClipRect, PRInt32& aIndex,
                                   PRBool& aAnyRendered);
  nsRect OptimizeTranslucentRegions(const nsVoidArray& aDisplayList,
                                    PRInt32* aIndex, nsRegion* aOpaqueRegion);

  void ShowDisplayList(const nsVoidArray* aDisplayList);

  // Utilities

  PRBool IsViewInserted(nsView *aView);

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

  nsresult ProcessWidgetChanges(nsView* aView);

  // Utilities used to size the offscreen drawing surface

  /**
   * Determine the maximum and width and height of all of the
   * view manager's widgets.
   *
   * @param aMaxWidgetBounds the maximum width and height of all view managers
   * widgets on exit.
   */
  void GetMaxWidgetBounds(nsRect& aMaxWidgetBounds) const;

public: // NOT in nsIViewManager, so private to the view module
  nsView* GetRootView() const { return mRootView; }
  nsView* GetMouseEventGrabber() const;
	nsView* GetKeyEventGrabber() const { return mKeyGrabber; }

  nsEventStatus HandleEvent(nsView* aView, nsGUIEvent* aEvent, PRBool aCaptured);

  /**
   * Called to inform the view manager that a view has scrolled.
   * The view manager will invalidate any widgets which may need
   * to be rerendered.
   * @param aView view to paint. should be root view
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  void UpdateViewAfterScroll(nsIView *aView, PRInt32 aDX, PRInt32 aDY);

  PRBool CanScrollWithBitBlt(nsView* aView);

  nsresult CreateRegion(nsIRegion* *result);

  // return the sum of all view offsets from aView right up to the
  // root of this view hierarchy (the view with no parent, which might
  // not be in this view manager).
  static nsPoint ComputeViewOffset(const nsView *aView);

private:
  nsIDeviceContext  *mContext;
  float             mTwipsToPixels;
  float             mPixelsToTwips;
  nsIViewObserver   *mObserver;
  nsView            *mMouseGrabber;
  nsView            *mKeyGrabber;
  PRInt32           mUpdateCnt;
  PRInt32           mUpdateBatchCnt;
  nsIScrollableView *mRootScrollable;
  PRInt32           mCachingWidgetChanges;
  nscolor           mDefaultBackgroundColor;
  nsPoint           mMouseLocation; // device units, relative to mRootView
  nsCOMPtr<nsIBlender> mBlender;
  nsISupportsArray  *mCompositeListeners;
  nsCOMPtr<nsIFactory> mRegionFactory;
  nsView            *mRootView;
  nsCOMPtr<nsIEventQueueService>  mEventQueueService;
  nsCOMPtr<nsIEventQueue>         mInvalidateEventQueue;
  nsCOMPtr<nsIEventQueue>         mSynthMouseMoveEventQueue;
  PRPackedBool      mRefreshEnabled;
  PRPackedBool      mPainting;
  PRPackedBool      mRecursiveRefreshPending;
  PRPackedBool      mAllowDoubleBuffering;
  PRPackedBool      mHasPendingInvalidates;

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
