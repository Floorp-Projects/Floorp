/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
#include "nsIScrollableView.h"
#include "nsIRegion.h"
#include "nsIBlender.h"

class nsISupportsArray;

// Dont want to get rid of timer code, because we may want to use it
// if we go to a implementation where we have invalidates that have been queued
// within the view manager, instead of doing invalidates on the widget.
// The timer would be used to cause the paints to happen.
//#define NS_VIEWMANAGER_NEEDS_TIMER 1

class nsViewManager : public nsIViewManager {
public:
  nsViewManager();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsIDeviceContext* aContext, nscoord aX = 0, nscoord aY = 0);

  NS_IMETHOD  GetRootView(nsIView *&aView);
  NS_IMETHOD  SetRootView(nsIView *aView, nsIWidget* aWidget=nsnull);

  NS_IMETHOD  GetFrameRate(PRUint32 &aRate);
  NS_IMETHOD  SetFrameRate(PRUint32 frameRate);

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

  NS_IMETHOD  RemoveChild(nsIView *parent, nsIView *child);

  NS_IMETHOD  MoveViewBy(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  ResizeView(nsIView *aView, nscoord aWidth, nscoord aHeight);

  NS_IMETHOD  SetViewClip(nsIView *aView, nsRect *aRect);

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

  NS_IMETHOD Display(nsIView *aView);

  NS_IMETHOD AddCompositeListener(nsICompositeListener *aListener);
  NS_IMETHOD RemoveCompositeListener(nsICompositeListener *aListener);

  NS_IMETHOD GetWidgetForView(nsIView *aView, nsIWidget **aWidget);
  NS_IMETHOD GetWidget(nsIWidget **aWidget);
  NS_IMETHOD ForceUpdate();
  NS_IMETHOD GetOffset(nscoord *aX, nscoord *aY);
  
protected:
  virtual ~nsViewManager();

private:
	nsIRenderingContext *CreateRenderingContext(nsIView &aView);
	void AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const;
	void UpdateDirtyViews(nsIView *aView, nsRect *aParentRect) const;
	void UpdateTransCnt(nsIView *oldview, nsIView *newview);

	void ProcessPendingUpdates(nsIView *aView);
	void UpdateViews(nsIView *aView, PRUint32 aUpdateFlags);

	void Refresh(nsIView *aView, nsIRenderingContext *aContext,
	             nsIRegion *region, PRUint32 aUpdateFlags);
	void Refresh(nsIView* aView, nsIRenderingContext *aContext,
	             const nsRect *rect, PRUint32 aUpdateFlags);
	void RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect,
					 PRBool &aResult);
	void RenderView(nsIView *aView, nsIRenderingContext &aRC,
					const nsRect &aDamageRect, nsRect &aGlobalRect, PRBool &aResult);
	PRBool CreateDisplayList(nsIView *aView, PRInt32 *aIndex, nscoord aOriginX, nscoord aOriginY,
	                       nsIView *aRealView, const nsRect *aDamageRect = nsnull,
	                       nsIView *aTopView = nsnull, nsVoidArray *aArray = nsnull,
	                       nscoord aX = 0, nscoord aY = 0);
	PRBool AddToDisplayList(nsVoidArray *aArray, PRInt32 *aIndex,
	                      nsIView *aView, nsRect &aRect, PRUint32 aFlags);
	void ShowDisplayList(PRInt32 flatlen);
	void ComputeViewOffset(nsIView *aView, nscoord *aX, nscoord *aY, PRInt32 aFlag);
	PRBool DoesViewHaveNativeWidget(nsIView &aView);
	void PauseTimer(void);
	void RestartTimer(void);

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
  PRBool IsRectVisible(nsIView *aView, const nsRect &aRect);

private:
  nsIDeviceContext  *mContext;
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
  nsIScrollableView *mRootScrollable;

  //from here to public should be static and locked... MMP
  static PRInt32           mVMCount;        //number of viewmanagers
  static nsDrawingSurface  mDrawingSurface; //single drawing surface
  static nsRect            mDSBounds;       //for all VMs

  //blending buffers
  static nsDrawingSurface  gOffScreen;
  static nsDrawingSurface  gRed;
  static nsDrawingSurface  gBlue;
  static PRInt32           gBlendWidth;
  static PRInt32           gBlendHeight;

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

public:
  //these are public so that our timer callback can poke them.
#if NS_VIEWMANAGER_NEEDS_TIMER
  nsITimer          *mTimer;
#endif
  nsRect            mDirtyRect;
  nsIView           *mRootView;
  PRUint32          mFrameRate;
  PRUint32          mTrueFrameRate;

protected:
  nscoord           mX;
  nscoord           mY;
};

#endif

