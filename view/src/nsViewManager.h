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

#ifndef nsViewManager_h___
#define nsViewManager_h___

#include "nsIViewManager.h"
#include "nsCRT.h"
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsITimer.h"
#include "prtime.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsIScrollableView.h"
#include "nsIRegion.h"
#include "nsIBlender.h"
#include "nsIEventQueue.h"
#include "nsView.h"
#include "nsIEventProcessor.h"

class nsISupportsArray;
struct DisplayListElement2;
struct DisplayZTreeNode;
class nsView;

//Uncomment the following line to enable generation of viewmanager performance data.
#ifdef MOZ_PERF_METRICS
//#define NS_VM_PERF_METRICS 1 
#endif

#ifdef NS_VM_PERF_METRICS
#include "nsTimer.h"
#endif

// compositor per-view flags
#define IS_PARENT_OF_REFRESHED_VIEW  0x00000001
#define IS_Z_PLACEHOLDER_VIEW        0x80000000

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
  nsView* GetReparentedView() { return mReparentedView; }

  NS_IMETHOD GetCompositorFlags(PRUint32 *aFlags)
  { nsView::GetCompositorFlags(aFlags); *aFlags |= IS_Z_PLACEHOLDER_VIEW; return NS_OK; }

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

  NS_IMETHOD  Init(nsIDeviceContext* aContext, nscoord aX = 0, nscoord aY = 0);

  NS_IMETHOD  GetRootView(nsIView *&aView);
  NS_IMETHOD  SetRootView(nsIView *aView, nsIWidget* aWidget=nsnull);

  NS_IMETHOD  GetWindowDimensions(nscoord *width, nscoord *height);
  NS_IMETHOD  SetWindowDimensions(nscoord width, nscoord height);

  NS_IMETHOD  ResetScrolling(void);

  NS_IMETHOD  Composite(void);

  NS_IMETHOD  UpdateView(nsIView *aView, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateAllViews(PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateViewAfterScroll(nsIView *aView, PRInt32 aDX, PRInt32 aDY);

  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent, nsEventStatus* aStatus);

  NS_IMETHOD  GrabMouseEvents(nsIView *aView, PRBool &aResult);
  NS_IMETHOD  GrabKeyEvents(nsIView *aView, PRBool &aresult);

  NS_IMETHOD  GetMouseEventGrabber(nsIView *&aView);
  NS_IMETHOD  GetKeyEventGrabber(nsIView *&aView);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                          PRBool above);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child,
                          PRInt32 zindex);

  NS_IMETHOD  InsertZPlaceholder(nsIView *aParent, nsIView *aZChild,
                                 PRInt32 aZIndex);

  NS_IMETHOD  RemoveChild(nsIView *parent, nsIView *child);

  NS_IMETHOD  MoveViewBy(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  ResizeView(nsIView *aView, nscoord aWidth, nscoord aHeight, PRBool aRepaintExposedAreaOnly = PR_FALSE);

  NS_IMETHOD  SetViewChildClip(nsIView *aView, nsRect *aRect);

  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible);

  NS_IMETHOD  SetViewZIndex(nsIView *aView, PRInt32 aZIndex);

  NS_IMETHOD  SetViewAutoZIndex(nsIView *aView, PRBool aAutoZIndex);

  NS_IMETHOD  MoveViewAbove(nsIView *aView, nsIView *aOther);
  NS_IMETHOD  MoveViewBelow(nsIView *aView, nsIView *aOther);

  NS_IMETHOD  SetViewContentTransparency(nsIView *aView, PRBool aTransparent);
  NS_IMETHOD  SetViewOpacity(nsIView *aView, float aOpacity);

  NS_IMETHOD  SetViewObserver(nsIViewObserver *aObserver);
  NS_IMETHOD  GetViewObserver(nsIViewObserver *&aObserver);

  NS_IMETHOD  GetDeviceContext(nsIDeviceContext *&aContext);

  NS_IMETHOD  ShowQuality(PRBool aShow);
  NS_IMETHOD  GetShowQuality(PRBool &aResult);
  NS_IMETHOD  SetQuality(nsContentQuality aQuality);

  NS_IMETHOD  DisableRefresh(void);
  NS_IMETHOD  EnableRefresh(PRUint32 aUpdateFlags);

  NS_IMETHOD  BeginUpdateViewBatch(void);
  NS_IMETHOD  EndUpdateViewBatch(PRUint32 aUpdateFlags);

  NS_IMETHOD  SetRootScrollableView(nsIScrollableView *aScrollable);
  NS_IMETHOD  GetRootScrollableView(nsIScrollableView **aScrollable);

  nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds);

  NS_IMETHOD Display(nsIView *aView, nscoord aX, nscoord aY, const nsRect& aClipRect);

  NS_IMETHOD AddCompositeListener(nsICompositeListener *aListener);
  NS_IMETHOD RemoveCompositeListener(nsICompositeListener *aListener);

  NS_IMETHOD GetWidgetForView(nsIView *aView, nsIWidget **aWidget);
  NS_IMETHOD GetWidget(nsIWidget **aWidget);
  NS_IMETHOD ForceUpdate();
  NS_IMETHOD GetOffset(nscoord *aX, nscoord *aY);
 
  NS_IMETHOD IsCachingWidgetChanges(PRBool* aCaching);
  NS_IMETHOD CacheWidgetChanges(PRBool aCache);
  NS_IMETHOD AllowDoubleBuffering(PRBool aDoubleBuffer);
  NS_IMETHOD IsPainting(PRBool& aIsPainting);
  NS_IMETHOD FlushPendingInvalidates();
  NS_IMETHOD SetDefaultBackgroundColor(nscolor aColor);
  NS_IMETHOD GetDefaultBackgroundColor(nscolor* aColor);
  NS_IMETHOD GetLastUserEventTime(PRUint32& aTime);
  nsresult ProcessInvalidateEvent();
  static PRInt32 GetViewManagerCount();
  static const nsVoidArray* GetViewManagerArray();
  static PRUint32 gLastUserEventTime;

  /**
	 * Determine if a rectangle specified in the view's coordinate system 
	 * is completely, or partially visible.
   * @param aView view that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to test for visibility 
   * @returns PR_TRUE if the rect is visible, PR_FALSE otherwise.
	 */
  NS_IMETHOD IsRectVisible(nsIView *aView, const nsRect &aRect, PRBool aMustBeFullyVisible, PRBool *isVisible);

protected:
  virtual ~nsViewManager();
  void ProcessPendingUpdates(nsView *aView);

private:
  nsIRenderingContext *CreateRenderingContext(nsView &aView);
  void AddRectToDirtyRegion(nsView* aView, const nsRect &aRect) const;
  void UpdateTransCnt(nsView *oldview, nsView *newview);

  PRBool UpdateAllCoveringWidgets(nsView *aView, nsView *aTarget, nsRect &aDamagedRect, PRBool aOnlyRepaintIfUnblittable);

  
  void UpdateViews(nsView *aView, PRUint32 aUpdateFlags);

  void Refresh(nsView *aView, nsIRenderingContext *aContext,
               nsIRegion *region, PRUint32 aUpdateFlags);
  void DefaultRefresh(nsView* aView, const nsRect* aRect);
  void RenderViews(nsView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect,
                   PRBool &aResult);

  void RenderView(nsView *aView, nsIRenderingContext &aRC,
                 const nsRect &aDamageRect, nsRect &aGlobalRect, PRBool &aResult);

  void RenderDisplayListElement(DisplayListElement2* element, nsIRenderingContext &aRC);

  void PaintView(nsView *aView, nsIRenderingContext &aRC, nscoord x, nscoord y,
                const nsRect &aDamageRect);
    
  nsresult CreateBlendingBuffers(nsIRenderingContext &aRC);
          
  PRBool CreateDisplayList(nsView *aView, PRBool aReparentedViewsPresent, DisplayZTreeNode* &aResult, nscoord aOriginX, nscoord aOriginY,
                           PRBool aInsideRealView, nsView *aRealView, const nsRect *aDamageRect,
                           nsView *aTopView, nscoord aX, nscoord aY, PRBool aPaintFloaters);
  PRBool AddToDisplayList(nsView *aView, DisplayZTreeNode* &aParent, nsRect &aClipRect, nsRect& aDirtyRect, PRUint32 aFlags, nscoord aAbsX, nscoord aAbsY);
  void ReapplyClipInstructions(PRBool aHaveClip, nsRect& aClipRect, PRInt32& aIndex);
  nsresult OptimizeDisplayList(const nsRect& aDamageRect, nsRect& aFinalTransparentRect);
    // Remove redundant PUSH/POP_CLIP pairs.
  void ComputeViewOffset(nsView *aView, nsPoint *aOrigin);

  void AddCoveringWidgetsToOpaqueRegion(nsIRegion* aRgn, nsIDeviceContext* aContext,
                                        nsView* aRootView);

  // Predicates
  PRBool DoesViewHaveNativeWidget(nsView* aView);
  PRBool IsClipView(nsView* aView);

  void PauseTimer(void);
  void RestartTimer(void);
  void OptimizeDisplayListClipping(PRBool aHaveClip, nsRect& aClipRect, PRInt32& aIndex,
                                   PRBool& aAnyRendered);
#ifdef NS_DEBUG
	void ShowDisplayList(PRInt32 flatlen);
#endif

  // Utilities

  /**
   * Returns the nearest parent view with an attached widget. Can be the
   * same view as passed-in.
   */
  nsView* GetWidgetView(nsView *aView) const;

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

  /**
	 * Installs event processor
	 */
  NS_IMETHOD SetEventProcessor(nsIEventProcessor* aEventProcessor) { mEventProcessor = aEventProcessor; return NS_OK; }

  // Utilities used to size the offscreen drawing surface

  /**
   * Determine the maximum and width and height of all of the
   * view manager's widgets.
   *
   * @param aMaxWidgetBounds the maximum width and height of all view managers
   * widgets on exit.
   */
  void GetMaxWidgetBounds(nsRect& aMaxWidgetBounds) const;

  /**
   * Determine if a rect's width and height will fit within a specified width and height
   * @param aRect rectangle to test
   * @param aWidth width to determine if the rectangle's width will fit within
   * @param aHeight height to determine if the rectangles height will fit within
   * @returns PR_TRUE if the rect width and height fits with aWidth, aHeight, PR_FALSE
   * otherwise.
   */
  PRBool RectFitsInside(nsRect& aRect, PRInt32 aWidth, PRInt32 aHeight) const;

  /**
   * Determine if two rectangles width and height will fit within a specified width and height
   * @param aRect1 first rectangle to test
   * @param aRect1 second rectangle to test
   * @param aWidth width to determine if both rectangle's width will fit within
   * @param aHeight height to determine if both rectangles height will fit within
   * @returns PR_TRUE if the rect1's and rect2's width and height fits with aWidth,
   * aHeight, PR_FALSE otherwise.
   */
  PRBool BothRectsFitInside(nsRect& aRect1, nsRect& aRect2, PRInt32 aWidth, PRInt32 aHeight, nsRect& aNewSize) const;
  
  /**
   * Return an offscreen surface size from a set of discrete surface sizes.
   * The smallest discrete surface size that can enclose both the Maximum widget 
   * size (@see GetMaxWidgetBounds) and the requested size is returned.
   *
   * @param aRequestedSize Requested size for the offscreen.
   * @param aSurfaceSize contains the surface size 
   */
  void CalculateDiscreteSurfaceSize(nsRect& aRequestedSize, nsRect& aSize) const;

  /**
   * Get the size of the offscreen drawing surface..
   *
   * @param aRequestedSize Desired size for the offscreen.
   * @param aSurfaceSize   Offscreen adjusted to a discrete size which encloses aRequestedSize.
   */
  void GetDrawingSurfaceSize(nsRect& aRequestedSize, nsRect& aSurfaceSize) const;

public: // NOT in nsIViewManager, so private to the view module
  nsView* GetRootView() const { return mRootView; }
  nsView* GetMouseEventGrabber() const { return mMouseGrabber; }
	nsView* GetKeyEventGrabber() const { return mKeyGrabber; }

private:
  nsIDeviceContext  *mContext;
  float             mTwipsToPixels;
  float             mPixelsToTwips;
  nsIViewObserver   *mObserver;
  nsIWidget         *mRootWindow;
  PRIntervalTime    mLastRefresh;
  PRInt32           mTransCnt;
  PRBool            mRefreshEnabled;
  PRBool            mPainting;
  PRBool            mRecursiveRefreshPending;
  nsView            *mMouseGrabber;
  nsView            *mKeyGrabber;
  PRInt32           mUpdateCnt;
  PRInt32           mUpdateBatchCnt;
  PRInt32           mDisplayListCount;
  nsAutoVoidArray   mDisplayList;
  PRInt32           mTranslucentViewCount;
  nsRect            mTranslucentArea;       // bounding box of all translucent views.
  nsIScrollableView *mRootScrollable;
  PRInt32           mCachingWidgetChanges;
  nscolor           mDefaultBackgroundColor;

  nsHashtable       mMapPlaceholderViewToZTreeNode;

  //from here to public should be static and locked... MMP
  static PRInt32           mVMCount;        //number of viewmanagers
  static nsDrawingSurface  mDrawingSurface; //single drawing surface
  static nsRect            mDSBounds;       //for all VMs

  //blending buffers
  static nsDrawingSurface  gOffScreen;
  static nsDrawingSurface  gBlack;
  static nsDrawingSurface  gWhite;
  static nsSize            gOffScreenSize;

  //Rendering context used to cleanup the blending buffers
  static nsIRenderingContext* gCleanupContext;

  // Largest requested offscreen size if larger than a full screen.
  static nsSize            gLargestRequestedSize;

  //list of view managers
  static nsVoidArray       *gViewManagers;

  //compositor regions
  nsIRegion         *mOpaqueRgn;
  nsIRegion         *mTmpRgn;

  nsIBlender        *mBlender;

  nsIRenderingContext *mOffScreenCX;
  nsIRenderingContext *mBlackCX;
  nsIRenderingContext *mWhiteCX;

  nsISupportsArray  *mCompositeListeners;
  void DestroyZTreeNode(DisplayZTreeNode* aNode);
protected:
  nsView            *mRootView;
  nscoord           mX;
  nscoord           mY;
  PRBool            mAllowDoubleBuffering;
  PRBool            mHasPendingInvalidates;
  PRBool            mPendingInvalidateEvent;
  nsCOMPtr<nsIEventQueue>  mEventQueue;
  void PostInvalidateEvent();

  nsCOMPtr<nsIEventProcessor> mEventProcessor;

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DECLARE(mWatch) //  Measures compositing+paint time for current document
#endif
};

#endif /* nsViewManager_h___ */
