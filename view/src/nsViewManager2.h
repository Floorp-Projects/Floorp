/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: true; c-basic-offset: 4 -*- */
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

#ifndef nsViewManager2_h___
#define nsViewManager2_h___

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
#include "nsIEventQueue.h"

class nsISupportsArray;
struct DisplayListElement2;

//Uncomment the following line to enable generation of viewmanager performance data.
#ifdef MOZ_PERF_METRICS
//#define NS_VM_PERF_METRICS 1 
#endif

#ifdef NS_VM_PERF_METRICS
#include "nsTimer.h"
#endif

class nsViewManager2 : public nsIViewManager {
public:
  nsViewManager2();

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

  NS_IMETHOD  IsViewShown(nsIView *aView, PRBool& aResult);

  NS_IMETHOD  GetViewClipAbsolute(nsIView *aView, nsRect *aRect, PRBool &aResult);

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

  NS_IMETHOD Display(nsIView *aView, nscoord aX, nscoord aY);

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
protected:
  virtual ~nsViewManager2();
  void ProcessPendingUpdates(nsIView *aView);
 
private:
	nsIRenderingContext *CreateRenderingContext(nsIView &aView);
 
	void AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const;
	void InvalidateChildWidgets(nsIView *aView, nsRect& aDirtyRect) const;
	void UpdateTransCnt(nsIView *oldview, nsIView *newview);

	void UpdateViews(nsIView *aView, PRUint32 aUpdateFlags);

	void Refresh(nsIView *aView, nsIRenderingContext *aContext,
	             nsIRegion *region, PRUint32 aUpdateFlags);
	void Refresh(nsIView* aView, nsIRenderingContext *aContext,
	             const nsRect *rect, PRUint32 aUpdateFlags);
	void RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect,
					 PRBool &aResult);

	void RenderView(nsIView *aView, nsIRenderingContext &aRC,
					const nsRect &aDamageRect, nsRect &aGlobalRect, PRBool &aResult);

    void RenderDisplayListElement(DisplayListElement2* element, nsIRenderingContext &aRC);

	void PaintView(nsIView *aView, nsIRenderingContext &aRC, nscoord x, nscoord y,
				  const nsRect &aDamageRect);
    
    nsresult CreateBlendingBuffers(nsIRenderingContext &aRC);
					
	PRBool CreateDisplayList(nsIView *aView, PRInt32 *aIndex, nscoord aOriginX, nscoord aOriginY,
	                       nsIView *aRealView, const nsRect *aDamageRect = nsnull,
	                       nsIView *aTopView = nsnull, nscoord aX = 0, nscoord aY = 0);
	PRBool AddToDisplayList(PRInt32 *aIndex, nsIView *aView, nsRect &aClipRect, nsRect& aDirtyRect, PRUint32 aFlags, nscoord aAbsX, nscoord aAbsY);
	nsresult OptimizeDisplayList(const nsRect& aDamageRect);
	void ShowDisplayList(PRInt32 flatlen);
	void ComputeViewOffset(nsIView *aView, nsPoint *aOrigin, PRInt32 aFlag);

	// Predicates
	PRBool DoesViewHaveNativeWidget(nsIView* aView);
	PRBool IsClipView(nsIView* aView);

	// Utilities

	/**
	 * Returns the nearest parent view with an attached widget. Can be the
	 * same view as passed-in.
	 */
	nsIView* GetWidgetView(nsIView *aView) const;

	/**
	 * Transforms a rectangle from specified view's coordinate system to
	 * the first parent that has an attached widget.
	 */
	void ViewToWidget(nsIView *aView, nsIView* aWidgetView, nsRect &aRect) const;
	// void WidgetToView(nsIView* aView, nsRect &aWidgetRect);

  /**
	 * Transforms a rectangle from specified view's coordinate system to
	 * an absolute coordinate rectangle which can be compared against the
   * rectangle returned by GetVisibleRect to determine visibility.
   * @param aView view that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to convert to absolute coordinates
   * @param aAbsRect rectangle in absolute coorindates.
   * @returns NS_OK if successful otherwise, NS_ERROR_FAILURE 
	 */

  nsresult GetAbsoluteRect(nsIView *aView, const nsRect &aRect, 
                           nsRect& aAbsRect);
  /**
	 * Determine the visible rect 
   * @param aVisibleRect visible rectangle in twips
   * @returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
   */

  nsresult GetVisibleRect(nsRect& aVisibleRect);

  /**
	 * Determine if a rectangle specified in the view's coordinate system 
	 * is completely, or partially visible.
   * @param aView view that aRect coordinates are specified relative to
   * @param aRect rectangle in twips to test for visibility 
   * @returns PR_TRUE if the rect is visible, PR_FALSE otherwise.
	 */
  NS_IMETHOD IsRectVisible(nsIView *aView, const nsRect &aRect, PRBool aMustBeFullyVisible, PRBool *isVisible);

  nsresult ProcessWidgetChanges(nsIView* aView);

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

  

private:
  nsIDeviceContext  *mContext;
  float				mTwipsToPixels;
  float				mPixelsToTwips;
  nsIViewObserver   *mObserver;
  nsIWidget         *mRootWindow;
  PRIntervalTime    mLastRefresh;
  PRInt32           mTransCnt;
  PRBool            mRefreshEnabled;
  PRBool            mPainting;
  nsIView           *mMouseGrabber;
  nsIView           *mKeyGrabber;
  PRInt32           mUpdateCnt;
  PRInt32           mUpdateBatchCnt;
  nsVoidArray       *mDisplayList;
  PRInt32			mDisplayListCount;
  PRInt32			mOpaqueViewCount;
  PRInt32			mTranslucentViewCount;
  nsRect            mTranslucentArea;       // bounding box of all translucent views.
  nsSize			mTranslucentSize;       // size of the blending buffers.
  nsIScrollableView *mRootScrollable;
  PRInt32     mCachingWidgetChanges;

  //from here to public should be static and locked... MMP
  static PRInt32           mVMCount;        //number of viewmanagers
    //list of view managers
  static nsVoidArray       *gViewManagers;
  static nsDrawingSurface  mDrawingSurface; //single drawing surface
  static nsRect            mDSBounds;       //for all VMs

  //blending buffers
  static nsDrawingSurface  gOffScreen;
  static nsDrawingSurface  gRed;
  static nsDrawingSurface  gBlue;
  static nsSize            gOffScreenSize;
  static nsSize            gBlendSize;

  //Rendering context used to cleanup the blending buffers
  static nsIRenderingContext* gCleanupContext;

  // Largest requested offscreen size if larger than a full screen.
  static nsSize            gLargestRequestedSize;

 
  //compositor regions
  nsIRegion         *mTransRgn;
  nsIRegion         *mOpaqueRgn;
  nsIRegion         *mRCRgn;
  nsIRegion         *mTRgn;
  nsRegionRectSet   *mTransRects;

  nsIBlender        *mBlender;

  nsIRenderingContext *mOffScreenCX;
  nsIRenderingContext *mRedCX;
  nsIRenderingContext *mBlueCX;

  nsISupportsArray  *mCompositeListeners;

protected:
  nsRect            mDirtyRect;
  nsIView           *mRootView;
  nscoord           mX;
  nscoord           mY;
  PRBool            mAllowDoubleBuffering;
  PRBool            mHasPendingInvalidates;
  PRBool            mPendingInvalidateEvent;
  nsCOMPtr<nsIEventQueue>  mEventQueue;
  void PostInvalidateEvent();

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DECLARE(mWatch) //  Measures compositing+paint time for current document
#endif
};

#endif /* nsViewManager2_h___ */
