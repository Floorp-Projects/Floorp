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

class nsViewManager : public nsIViewManager
{
public:
  nsViewManager();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsIDeviceContext* aContext);

  NS_IMETHOD  GetRootView(nsIView *&aView);
  NS_IMETHOD  SetRootView(nsIView *aView);

  NS_IMETHOD  GetFrameRate(PRUint32 &aRate);
  NS_IMETHOD  SetFrameRate(PRUint32 frameRate);

  NS_IMETHOD  GetWindowDimensions(nscoord *width, nscoord *height);
  NS_IMETHOD  SetWindowDimensions(nscoord width, nscoord height);

  NS_IMETHOD  ResetScrolling(void);

  NS_IMETHOD  Composite(void);

  NS_IMETHOD  UpdateView(nsIView *aView, nsIRegion *aRegion,
                         PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags);

  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);

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
  NS_IMETHOD  EnableRefresh(void);

  NS_IMETHOD  BeginUpdateViewBatch(void);
  NS_IMETHOD  EndUpdateViewBatch(void);

  NS_IMETHOD SetRootScrollableView(nsIScrollableView *aScrollable);
  NS_IMETHOD GetRootScrollableView(nsIScrollableView **aScrollable);

  nsDrawingSurface GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds);

  NS_IMETHOD Display(nsIView *aView);

protected:
  virtual ~nsViewManager();

private:
  nsIRenderingContext *CreateRenderingContext(nsIView &aView);
  void AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const;
  void UpdateDirtyViews(nsIView *aView, nsRect *aParentRect) const;
  void UpdateTransCnt(nsIView *oldview, nsIView *newview);

  void Refresh(nsIView *aView, nsIRenderingContext *aContext,
               nsIRegion *region, PRUint32 aUpdateFlags);
  void Refresh(nsIView* aView, nsIRenderingContext *aContext,
               const nsRect *rect, PRUint32 aUpdateFlags);
  void RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect,
                   PRBool &aResult);
  void RenderView(nsIView *aView, nsIRenderingContext &aRC,
                  const nsRect &aDamageRect, nsRect &aGlobalRect, PRBool &aResult);
  PRBool CreateDisplayList(nsIView *aView, PRInt32 *aIndex, nscoord aOriginX, nscoord aOriginY,
                           nsIView *aRealView, nsIView *aTopView,
                           const nsRect *aDamageRect = nsnull,
                           nscoord aX = 0, nscoord aY = 0);
//  void FlattenViewTreeUp(nsIView *aView, PRInt32 *aIndex, const nsRect *aDamageRect,
//                         nsIView *aStartView, nsVoidArray *aArray);
  PRBool AddToDisplayList(PRInt32 *aIndex, nsIView *aView, nsRect &aRect, PRUint32 aFlags);
  void ShowDisplayList(PRInt32 flatlen);
  void ComputeViewOffset(nsIView *aView, nscoord *aX, nscoord *aY, PRInt32 aFlag);
  PRBool DoesViewHaveNativeWidget(nsIView &aView);
  void PauseTimer(void);
  void RestartTimer(void);

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
  static PRUint32          mVMCount;        //number of viewmanagers
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

public:
  //these are public so that our timer callback can poke them.
  nsITimer          *mTimer;
  nsRect            mDirtyRect;
  nsIView           *mRootView;
  PRUint32          mFrameRate;
  PRUint32          mTrueFrameRate;
};

#endif

