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

#include "nsViewManager.h"
#include "nsUnitConversion.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScrollableView.h"
#include "nsView.h"
#include "nsIScrollbar.h"
#include "nsIClipView.h"
#include "nsISupportsArray.h"
#include "nsICompositeListener.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);

//#define GS_DEBUG

#define UPDATE_QUANTUM  1000 / 40

//#define NO_DOUBLE_BUFFER
#define NEW_COMPOSITOR
#define USE_DISPLAY_LIST_ELEMENTS

//used for debugging new compositor
//#define SHOW_RECTS
//#define SHOW_DISPLAYLIST

//display list flags
#define RENDER_VIEW   0x0000
#define VIEW_INCLUDED 0x0001
#define PUSH_CLIP     0x0002
#define POP_CLIP      0x0004

#define DISPLAYLIST_INC  1

//display list elements
struct DisplayListElement {
  nsIView*	mView;
  nsRect	mClip;
  PRUint32	mFlags;
};

#ifdef NS_VIEWMANAGER_NEEDS_TIMER

static void vm_timer_callback(nsITimer *aTimer, void *aClosure)
{
  nsViewManager *vm = (nsViewManager *)aClosure;
  printf("ViewManager timer callback\n");

  //restart the timer
  
  if (vm->mTrueFrameRate == vm->mFrameRate)
  {
    PRUint32 fr = vm->mFrameRate;

    vm->mFrameRate = 0;
    vm->SetFrameRate(fr);
  }
//printf("timer composite...\n");
#ifndef XP_MAC
	//XXX temporary: The Mac doesn't need the timer to repaint but
	// obviously this is not the good method to disable the thing.
	// It's that way for now because the proper solutions
	// (set UPDATE_QUANTUM to 0, or simply not create the timer)
	// don't work for now. We'll fix that and then disable the
	// Mac timers as we should.
  vm->Composite();
#endif
}
#endif

#if 0
static void blinkRect(nsIRenderingContext* context, const nsRect& r)
{
	context->InvertRect(r);
	::PR_Sleep(::PR_MillisecondsToInterval(100));
	context->InvertRect(r);
}
#endif

PRInt32 nsViewManager::mVMCount = 0;
nsDrawingSurface nsViewManager::mDrawingSurface = nsnull;
nsRect nsViewManager::mDSBounds = nsRect(0, 0, 0, 0);

nsDrawingSurface nsViewManager::gOffScreen = nsnull;
nsDrawingSurface nsViewManager::gRed = nsnull;
nsDrawingSurface nsViewManager::gBlue = nsnull;
PRInt32 nsViewManager::gBlendWidth = 0;
PRInt32 nsViewManager::gBlendHeight = 0;

static NS_DEFINE_IID(knsViewManagerIID, NS_IVIEWMANAGER_IID);

nsViewManager :: nsViewManager()
{
  NS_INIT_REFCNT();
  mVMCount++;
  mUpdateBatchCnt = 0;
  mCompositeListeners = nsnull;
  mX = 0;
  mY = 0;
}

nsViewManager :: ~nsViewManager()
{
#ifdef NS_VIEWMANAGER_NEEDS_TIMER
  if (nsnull != mTimer)
  {
    mTimer->Cancel();     //XXX this should not be necessary. MMP
    NS_RELEASE(mTimer);
  }
#endif

  NS_IF_RELEASE(mRootWindow);

  mRootScrollable = nsnull;

  NS_ASSERTION((mVMCount > 0), "underflow of viewmanagers");
  --mVMCount;

  if ((0 == mVMCount) &&
      ((nsnull != mDrawingSurface) || (nsnull != gOffScreen) ||
      (nsnull != gRed) || (nsnull != gBlue)))
  {
    nsIRenderingContext *rc;

    nsresult rv = nsComponentManager::CreateInstance(kRenderingContextCID, 
                                       nsnull, 
                                       NS_GET_IID(nsIRenderingContext), 
                                       (void **)&rc);

    if (NS_OK == rv)
    {
      if (nsnull != mDrawingSurface)
        rc->DestroyDrawingSurface(mDrawingSurface);

      if (nsnull != gOffScreen)
        rc->DestroyDrawingSurface(gOffScreen);

      if (nsnull != gRed)
        rc->DestroyDrawingSurface(gRed);

      if (nsnull != gBlue)
        rc->DestroyDrawingSurface(gBlue);

      NS_RELEASE(rc);
    }
    
    mDrawingSurface = nsnull;
    gOffScreen = nsnull;
    gRed = nsnull;
    gBlue = nsnull;
    gBlendWidth = 0;
    gBlendHeight = 0;
  }

  mObserver = nsnull;
  mContext = nsnull;

  if (nsnull != mDisplayList)
  {
	PRInt32 count = mDisplayList->Count();
	for (PRInt32 index = 0; index < count; index++) {
		DisplayListElement* element = (DisplayListElement*) mDisplayList->ElementAt(index);
		if (element != nsnull)
			delete element;
	}

    delete mDisplayList;
    mDisplayList = nsnull;
  }

  if (nsnull != mTransRgn)
  {
    if (nsnull != mTransRects)
      mTransRgn->FreeRects(mTransRects);

    NS_RELEASE(mTransRgn);
  }

  NS_IF_RELEASE(mBlender);
  NS_IF_RELEASE(mOpaqueRgn);
  NS_IF_RELEASE(mTRgn);
  NS_IF_RELEASE(mRCRgn);

  NS_IF_RELEASE(mOffScreenCX);
  NS_IF_RELEASE(mRedCX);
  NS_IF_RELEASE(mBlueCX);

  if (nsnull != mCompositeListeners) {
    mCompositeListeners->Clear();
    NS_RELEASE(mCompositeListeners);
  }
}

NS_IMPL_QUERY_INTERFACE(nsViewManager, knsViewManagerIID)

NS_IMPL_ADDREF(nsViewManager);

nsrefcnt nsViewManager::Release(void)
{
  /* Note funny ordering of use of mRefCnt. We were seeing a problem
     during the deletion of a view hierarchy where child views,
     while being destroyed, referenced this view manager and caused
     the Destroy part of this method to be re-entered. Waiting until
     the destruction has finished to decrement the refcount
     prevents that.
  */
  NS_LOG_RELEASE(this, mRefCnt - 1, "nsViewManager");
  if (mRefCnt == 1)
  {
    if (nsnull != mRootView) {
      // Destroy any remaining views
      mRootView->Destroy();
      mRootView = nsnull;
    }
    delete this;
    return 0;
  }
  mRefCnt--;
  return mRefCnt;
}

static nsresult CreateRegion(nsIComponentManager* componentManager, nsIRegion* *result)
{
	*result = nsnull;
	nsIRegion* region = nsnull;
	nsresult rv = componentManager->CreateInstance(kRegionCID, nsnull, NS_GET_IID(nsIRegion), (void**)&region);
	if (NS_SUCCEEDED(rv)) {
		rv = region->Init();
		*result = region;
	}
	return rv;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
NS_IMETHODIMP nsViewManager :: Init(nsIDeviceContext* aContext, nscoord aX, nscoord aY)
{
	nsresult rv;

	NS_PRECONDITION(nsnull != aContext, "null ptr");

	if (nsnull == aContext) {
		return NS_ERROR_NULL_POINTER;
	}
	if (nsnull != mContext) {
		return NS_ERROR_ALREADY_INITIALIZED;
	}
	mContext = aContext;
#ifdef NS_VIEWMANAGER_NEEDS_TIMER
	mTimer = nsnull;
#endif
	mFrameRate = 0;
	mTrueFrameRate = 0;
	mTransCnt = 0;

	rv = SetFrameRate(UPDATE_QUANTUM);

	mLastRefresh = PR_IntervalNow();

	mRefreshEnabled = PR_TRUE;

	mMouseGrabber = nsnull;
	mKeyGrabber = nsnull;

	// create regions
	nsIComponentManager* componentManager = nsnull;
	rv = NS_GetGlobalComponentManager(&componentManager);
	if (NS_SUCCEEDED(rv)) {
		rv = CreateRegion(componentManager, &mTransRgn);
		rv = CreateRegion(componentManager, &mOpaqueRgn);
		rv = CreateRegion(componentManager, &mTRgn);
		rv = CreateRegion(componentManager, &mRCRgn);
	}

  mX = aX;
  mY = aY;
  
	return rv;
}

NS_IMETHODIMP nsViewManager :: GetRootView(nsIView *&aView)
{
  aView = mRootView;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetRootView(nsIView *aView, nsIWidget* aWidget)
{
  UpdateTransCnt(mRootView, aView);
  // Do NOT destroy the current root view. It's the caller's responsibility
  // to destroy it
  mRootView = aView;

  //now get the window too.
  NS_IF_RELEASE(mRootWindow);

  // The window must be specified through one of the following:
  //* a) The aView has a nsIWidget instance or
  //* b) the aWidget parameter is an nsIWidget instance to render into 
  //*    that is not owned by a view.
  //* c) aView has a parent view managed by a different view manager or

  if (nsnull != aWidget) {
    mRootWindow = aWidget;
    NS_ADDREF(mRootWindow);
    return NS_OK;
  }

  // case b) The aView has a nsIWidget instance
  if (nsnull != mRootView) {
    mRootView->GetWidget(mRootWindow);
    if (nsnull != mRootWindow) {
      return NS_OK;
    }
  }

  // case c)  aView has a parent view managed by a different view manager

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetFrameRate(PRUint32 &aRate)
{
  aRate = mTrueFrameRate;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetFrameRate(PRUint32 aFrameRate)
{
  nsresult  rv = NS_OK;

  if (aFrameRate != mFrameRate)
  {
#ifdef NS_VIEWMANAGER_NEEDS_TIMER
     //XXX: Reimplement using a repeating timer
    if (nsnull != mTimer)
    {
      mTimer->Cancel();     //XXX this should not be necessary. MMP
      NS_RELEASE(mTimer);
    }
#endif

    mFrameRate = aFrameRate;
    mTrueFrameRate = aFrameRate;

    if (mFrameRate != 0)
    {
#ifdef NS_VIEWMANAGER_NEEDS_TIMER
      rv = NS_NewTimer(&mTimer);

      if (NS_OK == rv)
        mTimer->Init(vm_timer_callback, this, 1000 / mFrameRate);
#endif
    }
  }

  return rv;
}

NS_IMETHODIMP nsViewManager :: GetWindowDimensions(nscoord *width, nscoord *height)
{
  if (nsnull != mRootView)
    mRootView->GetDimensions(width, height);
  else
  {
    *width = 0;
    *height = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetWindowDimensions(nscoord width, nscoord height)
{
  // Resize the root view
  if (nsnull != mRootView)
    mRootView->SetDimensions(width, height);

//printf("new dims: %d %d\n", width, height);
  // Inform the presentation shell that we've been resized
  if (nsnull != mObserver)
    mObserver->ResizeReflow(mRootView, width, height);
//printf("reflow done\n");

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: ResetScrolling(void)
{
  if (nsnull != mRootScrollable)
    mRootScrollable->ComputeScrollOffsets(PR_TRUE);

  return NS_OK;
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, nsIRegion *region, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nsDrawingSurface    ds = nsnull;

  if (PR_FALSE == mRefreshEnabled)
    return;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

//printf("refreshing region...\n");
  //force double buffering because of non-opaque views?

  if (mTransCnt > 0)
    aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

#ifdef NO_DOUBLE_BUFFER
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
#endif

  if (nsnull == aContext)
  {
    localcx = CreateRenderingContext(*aView);

    //couldn't get rendering context. this is ok at init time atleast
    if (nsnull == localcx) {
      mPainting = PR_FALSE;
      return;
    }
  }
  else
    localcx = aContext;

  // notify the listeners.
  if (nsnull != mCompositeListeners) {
    PRUint32 listenerCount;
    if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
      nsICompositeListener* listener;
      for (PRUint32 i = 0; i < listenerCount; i++) {
        if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), (void**)&listener))) {
          listener->WillRefreshRegion(this, aView, aContext, region, aUpdateFlags);
          NS_RELEASE(listener);
        }
      }
    }
  }

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsIWidget*  widget;

    aView->GetWidget(widget);
    widget->GetClientBounds(wrect);

    wrect.x = wrect.y = 0;

    NS_RELEASE(widget);

    ds = GetDrawingSurface(*localcx, wrect);
  }

  PRBool  result;
  nsRect  trect;

  if (nsnull != region)
    localcx->SetClipRegion(*region, nsClipCombine_kUnion, result);

  aView->GetBounds(trect);

  localcx->SetClipRect(trect, nsClipCombine_kIntersect, result);

  RenderViews(aView, *localcx, trect, result);

  if ((aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER) && ds)
#ifdef NEW_COMPOSITOR
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, 0);
#else
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
#endif

  if (localcx != aContext)
    NS_RELEASE(localcx);

  // Subtract the area we just painted from the dirty region
  if ((nsnull != region) && !region->IsEmpty())
  {
    nsRect  pixrect = trect;
    float   t2p;

    mContext->GetAppUnitsToDevUnits(t2p);

    pixrect.ScaleRoundIn(t2p);
    region->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
  }

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;

  // notify the listeners.
  if (nsnull != mCompositeListeners) {
    PRUint32 listenerCount;
    if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
      nsICompositeListener* listener;
      for (PRUint32 i = 0; i < listenerCount; i++) {
        if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), (void**)&listener))) {
          listener->DidRefreshRegion(this, aView, aContext, region, aUpdateFlags);
          NS_RELEASE(listener);
        }
      }
    }
  }
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, const nsRect *rect, PRUint32 aUpdateFlags)
{
  nsRect              wrect, brect;
  nsIRenderingContext *localcx = nsnull;
  nsDrawingSurface    ds = nsnull;

  if (PR_FALSE == mRefreshEnabled)
    return;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

  //force double buffering because of non-opaque views?

//printf("refreshing rect... ");
//stdout << *rect;
//printf("\n");
  if (mTransCnt > 0)
    aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

#ifdef NO_DOUBLE_BUFFER
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
#endif

  if (nsnull == aContext)
  {
    localcx = CreateRenderingContext(*aView);

    //couldn't get rendering context. this is ok if at startup
    if (nsnull == localcx) {
      mPainting = PR_FALSE;
      return;
    }
  }
  else
    localcx = aContext;

  // notify the listeners.
  if (nsnull != mCompositeListeners) {
    PRUint32 listenerCount;
    if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
      nsICompositeListener* listener;
      for (PRUint32 i = 0; i < listenerCount; i++) {
        if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), (void**)&listener))) {
          listener->WillRefreshRect(this, aView, aContext, rect, aUpdateFlags);
          NS_RELEASE(listener);
        }
      }
    }
  }

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsIWidget*  widget;

    aView->GetWidget(widget);
    widget->GetClientBounds(wrect);

    brect = wrect;
    wrect.x = wrect.y = 0;

    NS_RELEASE(widget);

    ds = GetDrawingSurface(*localcx, wrect);
  }

  nsRect trect = *rect;

  PRBool  result;

  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  RenderViews(aView, *localcx, trect, result);

  if ((aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER) && ds)
#ifdef NEW_COMPOSITOR
  {
#ifdef XP_MAC
    // localcx->SelectOffScreenDrawingSurface(nsnull);
    // blinkRect(localcx, trect);
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, 0);
#else
#if defined(XP_UNIX) || defined(XP_OS2)
    localcx->SetClipRect(trect, nsClipCombine_kReplace, result);
#endif //unix
    localcx->CopyOffScreenBits(ds, brect.x, brect.y, brect, 0);
#endif //mac
  }
#else
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
#endif


  if (localcx != aContext)
    NS_RELEASE(localcx);

#if 0
  // Subtract the area we just painted from the dirty region
  nsIRegion *dirtyRegion;
  aView->GetDirtyRegion(dirtyRegion);

  if ((nsnull != dirtyRegion) && !dirtyRegion->IsEmpty())
  {
    nsRect  pixrect = trect;
    float   t2p;

    mContext->GetAppUnitsToDevUnits(t2p);

    pixrect.ScaleRoundIn(t2p);
    dirtyRegion->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
    NS_RELEASE(dirtyRegion);
  }
#endif

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;

  // notify the listeners.
  if (nsnull != mCompositeListeners) {
    PRUint32 listenerCount;
    if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
      nsICompositeListener* listener;
      for (PRUint32 i = 0; i < listenerCount; i++) {
        if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), (void**)&listener))) {
          listener->DidRefreshRect(this, aView, aContext, rect, aUpdateFlags);
          NS_RELEASE(listener);
        }
      }
    }
  }
}

//states

typedef enum
{
  FRONT_TO_BACK_RENDER =    1,
  FRONT_TO_BACK_ACCUMULATE,
  BACK_TO_FRONT_TRANS,
  BACK_TO_FRONT_OPACITY,
  FRONT_TO_BACK_CLEANUP,
  FRONT_TO_BACK_POP_SEARCH,
  COMPOSITION_DONE
} nsCompState;

//bit shifts
#define TRANS_PROPERTY_TRANS      0
#define TRANS_PROPERTY_OPACITY    1

#ifdef SHOW_RECTS
static PRInt32 evenodd = 0;
#endif

void nsViewManager :: RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect, PRBool &aResult)
{
#ifdef NEW_COMPOSITOR

	PRBool isFloatingView = PR_FALSE;
	if (NS_SUCCEEDED(aRootView->GetFloating(isFloatingView)) && isFloatingView) {
		// floating views are rendered locally (and act globally).
		// Paint the view. The clipping rect was set above set don't clip again.
		aRootView->Paint(aRC, aRect, NS_VIEW_FLAG_CLIP_SET, aResult);
		return;
	}

#define SET_STATE(x)  { prevstate = state; state = (x); }

  PRInt32           flatlen = 0, cnt;
  nsCompState       state = FRONT_TO_BACK_RENDER, prevstate;
  PRInt32           transprop = 0;
  PRInt32           increment = DISPLAYLIST_INC;
  PRInt32           loopstart = 0, backstart;
  PRInt32           loopend;
  PRInt32           accumstart;
  float             t2p, p2t;
  PRInt32           rgnrect;
  nsRect            localrect, trect;
  PRBool            useopaque = PR_FALSE;
  nsRegionRectSet   onerect, *rectset;
  
  nsCOMPtr<nsIRegion> paintedRgn;
  if (NS_FAILED(nsComponentManager::CreateInstance(kRegionCID, nsnull, NS_GET_IID(nsIRegion), (void**) getter_AddRefs(paintedRgn))))
    return;
  paintedRgn->Init();

  //printf("-------------begin paint------------\n");
  if (aRootView && mRootView) {
    nscoord ox = 0, oy = 0;

    ComputeViewOffset(aRootView, &ox, &oy, 1);
    CreateDisplayList(mRootView, &flatlen, ox, oy, aRootView, &aRect);
  }

  loopend = flatlen;

  mContext->GetAppUnitsToDevUnits(t2p);
  mContext->GetDevUnitsToAppUnits(p2t);

#ifdef SHOW_DISPLAYLIST
  ShowDisplayList(flatlen);
#endif

  while (state != COMPOSITION_DONE)
  {
    nsIView   *curview;
    nsRect    *currect;
    PRUint32  curflags;
    PRBool    pushing;
    PRInt32   pushcnt = 0;
    PRBool    clipstate;

    for (cnt = loopstart; (increment > 0) ? (cnt < loopend) : (cnt > loopend); cnt += increment)
    {
      DisplayListElement* curelement = (DisplayListElement*) mDisplayList->ElementAt(cnt);
      if (nsnull == curelement)
        continue;
      curview = curelement->mView;
      currect = &curelement->mClip;
      curflags = curelement->mFlags;
    
      if ((nsnull != curview) && (nsnull != currect))
      {
        PRBool  hasWidget = DoesViewHaveNativeWidget(*curview);
        PRBool  isBottom = cnt == (flatlen - DISPLAYLIST_INC);

        if (curflags & PUSH_CLIP)
        {
          if (state == BACK_TO_FRONT_OPACITY)
          {
            mOffScreenCX->PopState(clipstate);
            mRedCX->PopState(clipstate);
            mBlueCX->PopState(clipstate);

            // permanently remove any painted opaque views.
            mOffScreenCX->SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
            mRedCX->SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
            mRedCX->SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);

            --pushcnt;
            NS_ASSERTION((pushcnt >= 0), "underflow");
          }
          else if (state == BACK_TO_FRONT_TRANS)
          {
            aRC.PopState(clipstate);
            --pushcnt;
            NS_ASSERTION((pushcnt >= 0), "underflow");

            // permanently remove any painted opaque views.
            aRC.SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
          }
          else
          {
            pushing = FRONT_TO_BACK_RENDER;

            aRC.PushState();
            aRC.SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);

            ++pushcnt;
          }
        }
        else if (curflags & POP_CLIP)
        {
          if (state == BACK_TO_FRONT_OPACITY)
          {
            pushing = BACK_TO_FRONT_OPACITY;

            mOffScreenCX->PushState();
            mRedCX->PushState();
            mBlueCX->PushState();

            mOffScreenCX->SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);
            mRedCX->SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);
            mBlueCX->SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);

            ++pushcnt;
          }
          else if (state == BACK_TO_FRONT_TRANS)
          {
            pushing = FRONT_TO_BACK_RENDER;

            aRC.PushState();
            aRC.SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);

            ++pushcnt;
          }
          else
          {
            aRC.PopState(clipstate);
            --pushcnt;
            NS_ASSERTION((pushcnt >= 0), "underflow");

            // permanently remove any painted opaque views.
            aRC.SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
          }

          if (state == FRONT_TO_BACK_POP_SEARCH)
          {
            aRC.SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);

            if (!clipstate)
              state = prevstate;
            else if (pushcnt == 0)
              aResult = clipstate;
          }
        }
        else
//        if (isBottom)
//        if (!hasWidget || isBottom)
        {
          nsPoint *point;

          curview->GetScratchPoint(&point);

          if (!hasWidget || (hasWidget && point->x))
          {
            PRBool  trans = PR_FALSE;
            float   opacity = 1.0f;
            PRBool  translucent = PR_FALSE;

            if (!isBottom)
            {
              curview->HasTransparency(trans);
              curview->GetOpacity(opacity);

              translucent = opacity < 1.0f;
            }

            switch (state)
            {
              case FRONT_TO_BACK_RENDER:
                if (trans || translucent)
                {
                  if (mTransRgn && mOpaqueRgn)
                  {
                    //need to finish using back to front till this point
                    SET_STATE(FRONT_TO_BACK_ACCUMULATE)

                    if (pushcnt == 0)
                      accumstart = cnt;
                    else
                      accumstart = 0;

                    mTransRgn->SetTo(0, 0, 0, 0);
                    mOpaqueRgn->SetTo(0, 0, 0, 0);
                  }

                  //falls through
                }
                else
                {
                  RenderView(curview, aRC, aRect, *currect, aResult);
                  
                  // accumulate region of all rendered opaque views.
                  trect.IntersectRect(*currect, aRect), trect *= t2p;
                  paintedRgn->Union(trect.x, trect.y, trect.width, trect.height);

                  if (aResult == PR_FALSE)
                  {
                    aRC.SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);

                    if (clipstate)
                    {
                      if (pushcnt > 0)
                        SET_STATE(FRONT_TO_BACK_POP_SEARCH)
                      else
                        aResult = clipstate;
                    }
                  }

                  break;
                }

              case FRONT_TO_BACK_ACCUMULATE:
              {
                trect.IntersectRect(*currect, aRect);
                trect *= t2p;

                if (trans || translucent ||
                    mTransRgn->ContainsRect(trect.x, trect.y, trect.width, trect.height) ||
                    mOpaqueRgn->ContainsRect(trect.x, trect.y, trect.width, trect.height))
                {
                  transprop |= (trans << TRANS_PROPERTY_TRANS) | (translucent << TRANS_PROPERTY_OPACITY);
                  curelement->mFlags = (VIEW_INCLUDED | curflags);

                  if (!isBottom)
                  {
//printf("adding %d %d %d %d\n", trect.x, trect.y, trect.width, trect.height);
                    if (trans || translucent)
                      mTransRgn->Union(trect.x, trect.y, trect.width, trect.height);
                    else
                      mOpaqueRgn->Union(trect.x, trect.y, trect.width, trect.height);
                  }
                }
                else
                {
                  RenderView(curview, aRC, aRect, *currect, aResult);

                  // accumulate region of all rendered opaque views.
                  trect.IntersectRect(*currect, aRect), trect *= t2p;
                  paintedRgn->Union(trect.x, trect.y, trect.width, trect.height);

                  if (aResult == PR_FALSE)
                  {
                    aRC.SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);

                    if (clipstate)
                    {
                      if (pushcnt > 0)
                        SET_STATE(FRONT_TO_BACK_POP_SEARCH)
                      else
                        aResult = clipstate;
                    }
                  }
                }
  
                break;
              }

              case FRONT_TO_BACK_POP_SEARCH:
                break;

              case BACK_TO_FRONT_TRANS:
                if ((curflags & VIEW_INCLUDED) && localrect.Intersects(*currect))
                {
                  RenderView(curview, aRC, localrect, *currect, clipstate);
                }

                break;

              case FRONT_TO_BACK_CLEANUP:
                if ((curflags & VIEW_INCLUDED) && !trans &&
                    !translucent && localrect.Intersects(*currect))
                {
                  RenderView(curview, aRC, localrect, *currect, aResult);

                  if (aResult == PR_FALSE)
                  {
                    aRC.SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);

                    if (clipstate)
                    {
                      if (pushcnt > 0)
                        SET_STATE(FRONT_TO_BACK_POP_SEARCH)
                      else
                        aResult = clipstate;
                    }
                  }
                }

                break;

              case BACK_TO_FRONT_OPACITY:
                if (curflags & VIEW_INCLUDED)
                {
                  nsRect blendrect, pixrect;

                  if (blendrect.IntersectRect(*currect, localrect))
                  {
                    pixrect = blendrect;

                    pixrect.x -= localrect.x;
                    pixrect.y -= localrect.y;

                    pixrect *= t2p;

                    //if there is nothing to render in terms of pixels,
                    //just bag it right here.

                    if ((pixrect.width == 0) || (pixrect.height == 0))
                      break;

                    if (!translucent)
                      RenderView(curview, *mOffScreenCX, localrect, *currect, clipstate);
                    else
                    {
                      if (trans)
                      {
                        mRedCX->SetColor(NS_RGB(255, 0, 0));
                        mRedCX->FillRect(blendrect);
#ifdef SHOW_RECTS
                        mRedCX->SetColor(NS_RGB(255 - evenodd, evenodd, 255 - evenodd));
                        mRedCX->DrawLine(blendrect.x, blendrect.y, blendrect.x + blendrect.width, blendrect.y + blendrect.height);
#endif
                      }

                      RenderView(curview, *mRedCX, localrect, *currect, clipstate);

                      if (trans)
                      {
                        mBlueCX->SetColor(NS_RGB(0, 0, 255));
                        mBlueCX->FillRect(blendrect);
#ifdef SHOW_RECTS
                        mBlueCX->SetColor(NS_RGB(255 - evenodd, evenodd, 255 - evenodd));
                        mBlueCX->DrawLine(blendrect.x, blendrect.y, blendrect.x + blendrect.width, blendrect.y + blendrect.height);
#endif
                        RenderView(curview, *mBlueCX, localrect, *currect, clipstate);
                      }

                      if (nsnull == mBlender)
                      {
                        if (NS_OK == nsComponentManager::CreateInstance(kBlenderCID, nsnull, NS_GET_IID(nsIBlender), (void **)&mBlender))
                          mBlender->Init(mContext);
                      }

                      if (nsnull != mBlender)
                        mBlender->Blend(pixrect.x, pixrect.y,
                                        pixrect.width, pixrect.height,
                                        mRedCX, mOffScreenCX,
                                        pixrect.x, pixrect.y,
                                        opacity, trans ? mBlueCX : nsnull,
                                        NS_RGB(255, 0, 0), NS_RGB(0, 0, 255));
                    }
                  }
                }

                break;

              default:
                break;
            }
          }
          else
          {
            if (state == BACK_TO_FRONT_OPACITY)
            {
              mOffScreenCX->SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);
              mRedCX->SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);
              mBlueCX->SetClipRect(*currect, nsClipCombine_kSubtract, clipstate);
            }
            else
              aRC.SetClipRect(*currect, nsClipCombine_kSubtract, aResult);
          }
        }

        if (aResult == PR_TRUE)
          break;
      }
    }

    NS_ASSERTION((pushcnt >= 0), "underflow");

    while (pushcnt--)
    {
      NS_ASSERTION((pushcnt >= 0), "underflow");

      if (pushing == BACK_TO_FRONT_OPACITY)
      {
        mOffScreenCX->PopState(clipstate);
        mRedCX->PopState(clipstate);
        mBlueCX->PopState(clipstate);

        // permanently remove any painted opaque views.
        mOffScreenCX->SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
        mRedCX->SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
        mBlueCX->SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
      } else {
        aRC.PopState(clipstate);
        
        // permanently remove any painted opaque views.
        aRC.SetClipRegion(*paintedRgn, nsClipCombine_kSubtract, clipstate);
      }
    }

    switch (state)
    {
      case FRONT_TO_BACK_ACCUMULATE:
        SET_STATE((transprop & (1 << TRANS_PROPERTY_OPACITY)) ? BACK_TO_FRONT_OPACITY : BACK_TO_FRONT_TRANS)

        if ((curflags & (POP_CLIP | PUSH_CLIP)) || (state == BACK_TO_FRONT_OPACITY))
          backstart = flatlen - DISPLAYLIST_INC;
        else
        {
          backstart = flatlen - (DISPLAYLIST_INC << 1);

          //render the bottom most view
          RenderView(curview, aRC, aRect, *currect, aResult);
        }

        //get a snapshot of the current clip so that we can exclude areas
        //already excluded in it from the transparency region
        aRC.GetClipRegion(&mRCRgn);

        if (!mOpaqueRgn->IsEmpty())
          useopaque = PR_TRUE;

        if (mTRgn)
        {
          PRInt32 x, y, w, h;

          mTransRgn->GetBoundingBox(&x, &y, &w, &h);

          mTRgn->SetTo(x, y, w, h);
          mTRgn->Subtract(*mRCRgn);

          mTransRgn->Subtract(*mTRgn);
          mTransRgn->GetBoundingBox(&x, &y, &w, &h);

          // permanently remove any opaque painted views.
          mTransRgn->Subtract(*paintedRgn);

          aRC.SetClipRegion(*mTransRgn, nsClipCombine_kReplace, aResult);

          //did that finish everything?
          if (aResult == PR_TRUE)
          {
            SET_STATE(COMPOSITION_DONE)
            continue;
          }

          mTransRgn->GetRects(&mTransRects);

          //see if it might be better to ignore the rects and just do everything as one operation...

          if ((state == BACK_TO_FRONT_TRANS) &&
              (mTransRects->mNumRects > 1) &&
              (mTransRects->mArea >= PRUint32(w * h * 0.9f)))
          {
            rectset = &onerect;

            rectset->mNumRects = 1;

            rectset->mRects[0].x = x;
            rectset->mRects[0].y = y;
            rectset->mRects[0].width = w;
            rectset->mRects[0].height = h;
          }
          else
            rectset = mTransRects;

          rgnrect = 0;

#ifdef SHOW_RECTS
          evenodd = 255 - evenodd;
#endif
        }
        //falls through

      case BACK_TO_FRONT_TRANS:
      case BACK_TO_FRONT_OPACITY:
        if (rgnrect > 0)
        {
          if (state == BACK_TO_FRONT_OPACITY)
          {
#ifdef SHOW_RECTS
            mOffScreenCX->SetColor(NS_RGB(255 - evenodd, evenodd, 255 - evenodd));
            mOffScreenCX->DrawRect(localrect);
#endif
            mOffScreenCX->Translate(localrect.x, localrect.y);
            mRedCX->Translate(localrect.x, localrect.y);
            mBlueCX->Translate(localrect.x, localrect.y);

            //buffer swap.
            aRC.CopyOffScreenBits(gOffScreen, 0, 0, localrect, NS_COPYBITS_XFORM_DEST_VALUES | NS_COPYBITS_TO_BACK_BUFFER);
          }
          else
          {
#ifdef SHOW_RECTS
          aRC.SetColor(NS_RGB(255 - evenodd, evenodd, 255 - evenodd));
          aRC.DrawRect(localrect);

//          for (int xxxxx = 0; xxxxx < 50000000; xxxxx++);
#endif
          }
          //if this is the last rect, we don't need to subtract it, cause
          //we're done with this region.
          if (rgnrect < (PRInt32)rectset->mNumRects)
            aRC.SetClipRect(localrect, nsClipCombine_kSubtract, aResult);
        }

        if (rgnrect < (PRInt32)rectset->mNumRects)
        {
          loopstart = backstart;
          loopend = accumstart - DISPLAYLIST_INC;
          increment = -DISPLAYLIST_INC;

          localrect.x = rectset->mRects[rgnrect].x;
          localrect.y = rectset->mRects[rgnrect].y;
          localrect.width = rectset->mRects[rgnrect].width;
          localrect.height = rectset->mRects[rgnrect].height;

          if (state == BACK_TO_FRONT_OPACITY)
          {
            //prepare offscreen buffers

            if ((localrect.width > gBlendWidth) || (localrect.height > gBlendHeight))
            {
              nsRect  bitrect = nsRect(0, 0, localrect.width, localrect.height);

              NS_IF_RELEASE(mOffScreenCX);
              NS_IF_RELEASE(mRedCX);
              NS_IF_RELEASE(mBlueCX);

              if (nsnull != gOffScreen)
              {
                aRC.DestroyDrawingSurface(gOffScreen);
                gOffScreen = nsnull;
              }

              aRC.CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gOffScreen);

              if (nsnull != gRed)
              {
                aRC.DestroyDrawingSurface(gRed);
                gRed = nsnull;
              }

              aRC.CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gRed);

              if (nsnull != gBlue)
              {
                aRC.DestroyDrawingSurface(gBlue);
                gBlue = nsnull;
              }

              aRC.CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gBlue);

              gBlendWidth = localrect.width;
              gBlendHeight = localrect.height;
//printf("offscr: %d, %d (%d, %d)\n", w, h, accumrect.width, accumrect.height);
            }

            // bug 5062:  recreate the local blending contexts if necessary, since global drawing surfaces may have
            // been created while viewing another page, have to make sure local contexts exist.
            if (mOffScreenCX == NULL) {
              if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, NS_GET_IID(nsIRenderingContext), (void **)&mOffScreenCX))
                mOffScreenCX->Init(mContext, gOffScreen);
            }
            if (mRedCX == NULL) {
              if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, NS_GET_IID(nsIRenderingContext), (void **)&mRedCX))
                mRedCX->Init(mContext, gRed);
            }
            if (mBlueCX == NULL) {
              if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, NS_GET_IID(nsIRenderingContext), (void **)&mBlueCX))
                mBlueCX->Init(mContext, gBlue);
            }

            if ((nsnull == gOffScreen) || (nsnull == gRed))
              SET_STATE(BACK_TO_FRONT_TRANS)
          }

//printf("rect: %d %d %d %d\n", localrect.x, localrect.y, localrect.width, localrect.height);
          localrect *= p2t;
          rgnrect++;

          if (state == BACK_TO_FRONT_OPACITY)
          {
            mOffScreenCX->Translate(-localrect.x, -localrect.y);
            mRedCX->Translate(-localrect.x, -localrect.y);
            mBlueCX->Translate(-localrect.x, -localrect.y);

            mOffScreenCX->SetClipRect(localrect, nsClipCombine_kReplace, clipstate);
            mRedCX->SetClipRect(localrect, nsClipCombine_kReplace, clipstate);
            mBlueCX->SetClipRect(localrect, nsClipCombine_kReplace, clipstate);
          }
        }
        else
        {
          if (useopaque)
          {
            mOpaqueRgn->Subtract(*mTransRgn);

            if (!mOpaqueRgn->IsEmpty())
            {
              SET_STATE(FRONT_TO_BACK_CLEANUP)

              loopstart = accumstart;
              loopend = flatlen;
              increment = DISPLAYLIST_INC;

              mOpaqueRgn->GetBoundingBox(&localrect.x, &localrect.y, &localrect.width, &localrect.height);

              localrect *= p2t;

              // permanently remove any opaque painted views.
              mOpaqueRgn->Subtract(*paintedRgn);

              aRC.SetClipRegion(*mOpaqueRgn, nsClipCombine_kReplace, aResult);

              //did that finish everything?
              if (aResult == PR_TRUE)
                SET_STATE(COMPOSITION_DONE)

              break;
            }
          }

          SET_STATE(COMPOSITION_DONE)
        }

        break;

      default:
      case FRONT_TO_BACK_CLEANUP:
      case FRONT_TO_BACK_RENDER:
        SET_STATE(COMPOSITION_DONE)
        break;
    }
  }

  if (nsnull != aRootView)
    ComputeViewOffset(aRootView, nsnull, nsnull, 0);

#undef SET_STATE

#else

  // Paint the view. The clipping rect was set above set don't clip again.
  aRootView->Paint(aRC, aRect, NS_VIEW_FLAG_CLIP_SET, aResult);

#endif
}

void nsViewManager :: RenderView(nsIView *aView, nsIRenderingContext &aRC, const nsRect &aDamageRect, nsRect &aGlobalRect, PRBool &aResult)
{
  nsRect  drect;

  NS_ASSERTION(!(nsnull == aView), "no view");

  aRC.PushState();

  aRC.Translate(aGlobalRect.x, aGlobalRect.y);

  drect.IntersectRect(aDamageRect, aGlobalRect);

  drect.x -= aGlobalRect.x;
  drect.y -= aGlobalRect.y;

  aView->Paint(aRC, drect, NS_VIEW_FLAG_JUST_PAINT, aResult);

  aRC.PopState(aResult);
}

void nsViewManager::UpdateDirtyViews(nsIView *aView, nsRect *aParentRect) const
{
  nsRect bounds;
  aView->GetBounds(bounds);

  // translate parent rect into child coords.
  nsRect parDamage;
  if (nsnull != aParentRect) {
    parDamage = *aParentRect;
    parDamage.IntersectRect(bounds, parDamage);
    parDamage.MoveBy(-bounds.x, -bounds.y);
  } else
    parDamage = bounds;

  if (PR_FALSE == parDamage.IsEmpty()) {
    nsIWidget *widget;
    aView->GetWidget(widget);
    if (nsnull != widget) {
      float scale;
      nsRect pixrect = parDamage;

      mContext->GetAppUnitsToDevUnits(scale);
      pixrect.ScaleRoundOut(scale);

//printf("invalidating: view %x (pix) %d, %d\n", aView, pixrect.width, pixrect.height);
      widget->Invalidate(pixrect, PR_FALSE);

      NS_RELEASE(widget);
    }
  }

  // Check our child views
  nsIView *child;

  aView->GetChild(0, child);

  while (nsnull != child) {
    UpdateDirtyViews(child, &parDamage);
    child->GetNextSibling(child);
  }
}

void nsViewManager::ProcessPendingUpdates(nsIView* aView)
{
	PRBool hasWidget;
	aView->HasWidget(&hasWidget);
	if (hasWidget) {
		nsCOMPtr<nsIRegion> dirtyRegion;
		aView->GetDirtyRegion(*getter_AddRefs(dirtyRegion));
		if (dirtyRegion != nsnull && !dirtyRegion->IsEmpty()) {
			nsCOMPtr<nsIWidget> widget;
			aView->GetWidget(*getter_AddRefs(widget));
			if (widget) {
				widget->InvalidateRegion(dirtyRegion, PR_FALSE);
			}
			dirtyRegion->Init();
		}
	}

	// process pending updates in child view.
	nsIView* childView = nsnull;
	aView->GetChild(0, childView);
	while (nsnull != childView)	{
		ProcessPendingUpdates(childView);
		childView->GetNextSibling(childView);
	}
}

NS_IMETHODIMP nsViewManager :: Composite()
{
  if (mUpdateCnt > 0)
  {
    if (nsnull != mRootWindow)
      mRootWindow->Update();

    mUpdateCnt = 0;
    PauseTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, PRUint32 aUpdateFlags)
{
	// Mark the entire view as damaged
	nsRect bounds;
	aView->GetBounds(bounds);
	bounds.x = bounds.y = 0;
	return UpdateView(aView, bounds, aUpdateFlags);
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
	NS_PRECONDITION(nsnull != aView, "null view");

   // If the rectangle is not visible then abort
   // without invalidating. This is a performance 
   // enhancement since invalidating a native widget
   // can be expensive.
  if (! IsRectVisible(aView, aRect)) {
    return NS_OK;
  }

	if (!mRefreshEnabled) {
		// accumulate this rectangle in the view's dirty region, so we can process it later.
		if (aRect.width != 0 && aRect.height != 0) {
			AddRectToDirtyRegion(aView, aRect);
			++mUpdateCnt;
		}
		return NS_OK;
	}

	// Ignore any silly requests...
	if ((aRect.width == 0) || (aRect.height == 0))
		return NS_OK;

	// is this view even visible?
	nsViewVisibility  visibility;
	aView->GetVisibility(visibility);
	if (visibility == nsViewVisibility_kHide)
		return NS_OK; 

	// Find the nearest view (including this view) that has a widget
	nsIView *widgetView = GetWidgetView(aView);
	if (nsnull != widgetView) {
		if (0 == mUpdateCnt)
			RestartTimer();

		mUpdateCnt++;

#if 0
		// Transform damaged rect to widgetView's coordinate system.
		nsRect  widgetRect = aRect;
		nsIView *parentView = aView;
		while (parentView != widgetView) {
			nscoord x, y;
			parentView->GetPosition(&x, &y);
			widgetRect.x += x;
			widgetRect.y += y;
			parentView->GetParent(parentView);
		}

		// Add this rect to the widgetView's dirty region.
		if (nsnull != widgetView)
			UpdateDirtyViews(widgetView, &widgetRect);
#else
		// Go ahead and invalidate the entire rectangular area.
		// regardless of parentage.
		nsRect  widgetRect = aRect;
		ViewToWidget(aView, widgetView, widgetRect);
		nsCOMPtr<nsIWidget> widget;
		widgetView->GetWidget(*getter_AddRefs(widget));
		widget->Invalidate(widgetRect, PR_FALSE);
#endif

		// See if we should do an immediate refresh or wait
		if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
			Composite();
		} else if ((mTrueFrameRate > 0) && !(aUpdateFlags & NS_VMREFRESH_NO_SYNC)) {
			// or if a sync paint is allowed and it's time for the compositor to
			// do a refresh
			PRInt32 deltams = PR_IntervalToMilliseconds(PR_IntervalNow() - mLastRefresh);
			if (deltams > (1000 / (PRInt32)mTrueFrameRate))
				Composite();
		}
	}

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateAllViews(PRUint32 aUpdateFlags)
{
	UpdateViews(mRootView, aUpdateFlags);
	return NS_OK;
}

void nsViewManager::UpdateViews(nsIView *aView, PRUint32 aUpdateFlags)
{
	// update this view.
	UpdateView(aView, aUpdateFlags);

	// update all children as well.
	nsIView* childView = nsnull;
	aView->GetChild(0, childView);
	while (nsnull != childView)	{
		UpdateViews(childView, aUpdateFlags);
		childView->GetNextSibling(childView);
	}
}

NS_IMETHODIMP nsViewManager :: DispatchEvent(nsGUIEvent *aEvent, nsEventStatus *aStatus)
{
  *aStatus = nsEventStatus_eIgnore;

  switch(aEvent->message)
  {
    case NS_SIZE:
    {
      nsIView*      view = nsView::GetViewFor(aEvent->widget);

      if (nsnull != view)
      {
        nscoord width = ((nsSizeEvent*)aEvent)->windowSize->width;
        nscoord height = ((nsSizeEvent*)aEvent)->windowSize->height;
        width = ((nsSizeEvent*)aEvent)->mWinWidth;
        height = ((nsSizeEvent*)aEvent)->mWinHeight;

        // The root view may not be set if this is the resize associated with
        // window creation

        if (view == mRootView)
        {
          // Convert from pixels to twips
          float p2t;
          mContext->GetDevUnitsToAppUnits(p2t);

//printf("resize: (pix) %d, %d\n", width, height);
          SetWindowDimensions(NSIntPixelsToTwips(width, p2t),
                              NSIntPixelsToTwips(height, p2t));
          *aStatus = nsEventStatus_eConsumeNoDefault;
        }
      }

      break;
    }

    case NS_PAINT:
    {
      nsIView *view = nsView::GetViewFor(aEvent->widget);

      if (nsnull != view)
      {
        // The rect is in device units, and it's in the coordinate space of its
        // associated window.
        nsRect  damrect = *((nsPaintEvent*)aEvent)->rect;

        float   p2t;
        mContext->GetDevUnitsToAppUnits(p2t);
        damrect.ScaleRoundOut(p2t);

        // Do an immediate refresh
        if (nsnull != mContext)
        {
          nsRect  viewrect;
          float   varea;

          // Check that there's actually something to paint
          view->GetBounds(viewrect);
          viewrect.x = viewrect.y = 0;
          varea = (float)viewrect.width * viewrect.height;

          if (varea > 0.0000001f)
          {
            // nsRect     arearect;
            PRUint32   updateFlags = 0;

            // Auto double buffering logic.
            // See if the paint region is greater than .25 the area of our view.
            // If so, enable double buffered painting.
             
            // XXX These two lines cause a lot of flicker for drag-over re-drawing - rods
            //arearect.IntersectRect(damrect, viewrect);
  
            //if ((((float)arearect.width * arearect.height) / varea) >  0.25f)
            // XXX rods
              updateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

//printf("refreshing: view: %x, %d, %d, %d, %d\n", view, trect.x, trect.y, trect.width, trect.height);
            // Refresh the view
            Refresh(view, ((nsPaintEvent*)aEvent)->renderingContext, &damrect, updateFlags);
          }
        }

        *aStatus = nsEventStatus_eConsumeNoDefault;
      }

      break;
    }

    case NS_DESTROY:
      *aStatus = nsEventStatus_eConsumeNoDefault;
      break;

    default:
    {
      nsIView* baseView;
      nsIView* view;
      nsPoint offset;
      nsIScrollbar* sb;

      //Find the view whose coordinates system we're in.
      baseView = nsView::GetViewFor(aEvent->widget);
        
      //Find the view to which we're initially going to send the event 
      //for hittesting.
      if (nsnull != mMouseGrabber && NS_IS_MOUSE_EVENT(aEvent)) {
        view = mMouseGrabber;
      }
      else if (nsnull != mKeyGrabber && NS_IS_KEY_EVENT(aEvent)) {
        view = mKeyGrabber;
      }
      else if (NS_OK == aEvent->widget->QueryInterface(NS_GET_IID(nsIScrollbar), (void**)&sb)) {
        view = baseView;
        NS_RELEASE(sb);
      }
      else {
        view = mRootView;
      }

      if (nsnull != view) {
        //Calculate the proper offset for the view we're going to
        offset.x = offset.y = 0;
        if (baseView != view) {
          //Get offset from root of baseView
          nsIView *parent;
          nsRect bounds;

          parent = baseView;
          while (nsnull != parent) {
            parent->GetBounds(bounds);
            offset.x += bounds.x;
            offset.y += bounds.y;
            parent->GetParent(parent);
          }

          //Subtract back offset from root of view
          parent = view;
          while (nsnull != parent) {
            parent->GetBounds(bounds);
            offset.x -= bounds.x;
            offset.y -= bounds.y;
            parent->GetParent(parent);
          }
      
        }

        //Dispatch the event
        float p2t, t2p;

        mContext->GetDevUnitsToAppUnits(p2t);
        mContext->GetAppUnitsToDevUnits(t2p);

        //Before we start mucking with coords, make sure we know our baseline
        aEvent->refPoint.x = aEvent->point.x;
        aEvent->refPoint.y = aEvent->point.y;

        aEvent->point.x = NSIntPixelsToTwips(aEvent->point.x, p2t);
        aEvent->point.y = NSIntPixelsToTwips(aEvent->point.y, p2t);

        aEvent->point.x += offset.x;
        aEvent->point.y += offset.y;
 
        PRBool handled = PR_FALSE;
        view->HandleEvent(aEvent, NS_VIEW_FLAG_CHECK_CHILDREN | 
                                  NS_VIEW_FLAG_CHECK_PARENT |
                                  NS_VIEW_FLAG_CHECK_SIBLINGS,
                          aStatus,
                          handled);

        aEvent->point.x -= offset.x;
        aEvent->point.y -= offset.y;

        aEvent->point.x = NSTwipsToIntPixels(aEvent->point.x, t2p);
        aEvent->point.y = NSTwipsToIntPixels(aEvent->point.y, t2p);

		//
		// if the event is an nsTextEvent, we need to map the reply back into platform coordinates
		//
		if (aEvent->message==NS_TEXT_EVENT) {
			((nsTextEvent*)aEvent)->theReply.mCursorPosition.x=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.x, t2p);
			((nsTextEvent*)aEvent)->theReply.mCursorPosition.y=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.y, t2p);
			((nsTextEvent*)aEvent)->theReply.mCursorPosition.width=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.width, t2p);
			((nsTextEvent*)aEvent)->theReply.mCursorPosition.height=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.height, t2p);
		}
		if((aEvent->message==NS_COMPOSITION_START) ||
		   (aEvent->message==NS_COMPOSITION_QUERY)) {
			((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.x=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.x,t2p);
	        ((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.y=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.y,t2p);
			((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.width=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.width,t2p);
	        ((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.height=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.height,t2p);
		}
	  }
    
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GrabMouseEvents(nsIView *aView, PRBool &aResult)
{
  mMouseGrabber = aView;
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GrabKeyEvents(nsIView *aView, PRBool &aResult)
{
  mKeyGrabber = aView;
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetMouseEventGrabber(nsIView *&aView)
{
  aView = mMouseGrabber;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetKeyEventGrabber(nsIView *&aView)
{
  aView = mKeyGrabber;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                                           PRBool above)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    nsIView *kid;
    nsIView *prev = nsnull;

    //verify that the sibling exists...

    parent->GetChild(0, kid);

    while (nsnull != kid)
    {
      if (kid == sibling)
        break;

      //get the next sibling view

      prev = kid;
      kid->GetNextSibling(kid);
    }

    if (nsnull != kid)
    {
      //it's there, so do the insertion

      if (PR_TRUE == above)
        parent->InsertChild(child, prev);
      else
        parent->InsertChild(child, sibling);
    }

    UpdateTransCnt(nsnull, child);

    // if the parent view is marked as "floating", make the newly added view float as well.
    PRBool isFloating = PR_FALSE;
    parent->GetFloating(isFloating);
   	child->SetFloating(isFloating);

    //and mark this area as dirty if the view is visible...

    nsViewVisibility  visibility;
    child->GetVisibility(visibility);

    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, NS_VMREFRESH_NO_SYNC);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: InsertChild(nsIView *parent, nsIView *child, PRInt32 zindex)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    nsIView *kid;
    nsIView *prev = nsnull;

    //find the right insertion point...

    parent->GetChild(0, kid);

    while (nsnull != kid)
    {
      PRInt32 idx;

      kid->GetZIndex(idx);

      if (zindex >= idx)
        break;

      //get the next sibling view

      prev = kid;
      kid->GetNextSibling(kid);
    }

    //in case this hasn't been set yet... maybe we should not do this? MMP

    child->SetZIndex(zindex);
    parent->InsertChild(child, prev);

    UpdateTransCnt(nsnull, child);

    // if the parent view is marked as "floating", make the newly added view float as well.
    PRBool isFloating = PR_FALSE;
    parent->GetFloating(isFloating);
   	child->SetFloating(isFloating);

    //and mark this area as dirty if the view is visible...
    nsViewVisibility  visibility;
    child->GetVisibility(visibility);

    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, NS_VMREFRESH_NO_SYNC);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: RemoveChild(nsIView *parent, nsIView *child)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    UpdateTransCnt(child, nsnull);
    UpdateView(child, NS_VMREFRESH_NO_SYNC);
    parent->RemoveChild(child);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: MoveViewBy(nsIView *aView, nscoord aX, nscoord aY)
{
  nscoord x, y;

  aView->GetPosition(&x, &y);
  MoveViewTo(aView, aX + x, aY + y);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
	nscoord oldX, oldY;
	aView->GetPosition(&oldX, &oldY);
	aView->SetPosition(aX, aY);

	// only do damage control if the view is visible

	if ((aX != oldX) || (aY != oldY)) {
		nsViewVisibility  visibility;
		aView->GetVisibility(visibility);
		if (visibility != nsViewVisibility_kHide) {
			nsRect  bounds;
			aView->GetBounds(bounds);
			nsRect oldArea(oldX, oldY, bounds.width, bounds.height);
			nsIView* parentView;
			aView->GetParent(parentView);
			UpdateView(parentView, oldArea, NS_VMREFRESH_NO_SYNC);
			nsRect newArea(aX, aY, bounds.width, bounds.height); 
			UpdateView(parentView, newArea, NS_VMREFRESH_NO_SYNC);
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::ResizeView(nsIView *aView, nscoord width, nscoord height)
{
	nscoord oldWidth, oldHeight;
	aView->GetDimensions(&oldWidth, &oldHeight);
	if ((width != oldWidth) || (height != oldHeight)) {
		nscoord x = 0, y = 0;
		nsIView* parentView = nsnull;
	    aView->GetParent(parentView);
	    if (parentView != nsnull)
			aView->GetPosition(&x, &y);
		else
			parentView = aView;

		// resize the view.
		aView->SetDimensions(width, height);

#if 1
		// refresh the bounding box of old and new areas.
		nscoord maxWidth = (oldWidth < width ? width : oldWidth);
		nscoord maxHeight = (oldHeight < height ? height : oldHeight);
		nsRect boundingArea(x, y, maxWidth, maxHeight);
		UpdateView(parentView, boundingArea, NS_VMREFRESH_NO_SYNC);
#else
		// brute force, invalidate old and new areas. I don't understand
		// why just refreshing the bounding box is insufficient.
		nsRect oldBounds(x, y, oldWidth, oldHeight);
		UpdateView(parentView, oldBounds, NS_VMREFRESH_NO_SYNC);
		UpdateView(parentView, NS_VMREFRESH_NO_SYNC);
#endif
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewClip(nsIView *aView, nsRect *aRect)
{
  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aRect), "no clip");

  aView->SetClip(aRect->x, aRect->y, aRect->XMost(), aRect->YMost());

  UpdateView(aView, *aRect, NS_VMREFRESH_NO_SYNC);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewVisibility(nsIView *aView, nsViewVisibility aVisible)
{
  nsViewVisibility  oldVisible;
  aView->GetVisibility(oldVisible);
  if (aVisible != oldVisible) {
    aView->SetVisibility(aVisible);
    if (nsViewVisibility_kHide == aVisible) {
      nsIView* parentView = nsnull;
      aView->GetParent(parentView);
      if (parentView) {
        nsRect  bounds;
        aView->GetBounds(bounds);
        UpdateView(parentView, bounds, NS_VMREFRESH_NO_SYNC);
      }
    }
    else {
      UpdateView(aView, NS_VMREFRESH_NO_SYNC);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewZIndex(nsIView *aView, PRInt32 aZIndex)
{
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "no view");

  PRInt32 oldidx;

  aView->GetZIndex(oldidx);

  if (oldidx != aZIndex)
  {
    nsIView *parent;

    aView->GetParent(parent);

    if (nsnull != parent)
    {
      //we don't just call the view manager's RemoveChild()
      //so that we can avoid two trips trough the UpdateView()
      //code (one for removal, one for insertion). MMP

      parent->RemoveChild(aView);
      UpdateTransCnt(aView, nsnull);
      rv = InsertChild(parent, aView, aZIndex);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager::SetViewAutoZIndex(nsIView *aView, PRBool aAutoZIndex)
{
	return aView->SetAutoZIndex(aAutoZIndex);
}

NS_IMETHODIMP nsViewManager :: MoveViewAbove(nsIView *aView, nsIView *aOther)
{
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aOther), "no view");

  nsIView *nextview;

  aView->GetNextSibling(nextview);
 
  if (nextview != aOther)
  {
    nsIView *parent;

    aView->GetParent(parent);

    if (nsnull != parent)
    {
      //we don't just call the view manager's RemoveChild()
      //so that we can avoid two trips trough the UpdateView()
      //code (one for removal, one for insertion). MMP

      parent->RemoveChild(aView);
      UpdateTransCnt(aView, nsnull);
      rv = InsertChild(parent, aView, aOther, PR_TRUE);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager :: MoveViewBelow(nsIView *aView, nsIView *aOther)
{
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aOther), "no view");

  nsIView *nextview;

  aOther->GetNextSibling(nextview);
 
  if (nextview != aView)
  {
    nsIView *parent;

    aView->GetParent(parent);

    if (nsnull != parent)
    {
      //we don't just call the view manager's RemoveChild()
      //so that we can avoid two trips trough the UpdateView()
      //code (one for removal, one for insertion). MMP

      parent->RemoveChild(aView);
      UpdateTransCnt(aView, nsnull);
      rv = InsertChild(parent, aView, aOther, PR_FALSE);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP nsViewManager :: IsViewShown(nsIView *aView, PRBool &aResult)
{
  aResult = PR_TRUE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager :: GetViewClipAbsolute(nsIView *aView, nsRect *rect, PRBool &aResult)
{
  aResult = PR_TRUE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager :: SetViewContentTransparency(nsIView *aView, PRBool aTransparent)
{
  PRBool trans;

  aView->HasTransparency(trans);

  if (trans != aTransparent)
  {
    UpdateTransCnt(aView, nsnull);
    aView->SetContentTransparency(aTransparent);
    UpdateTransCnt(nsnull, aView);
    UpdateView(aView, NS_VMREFRESH_NO_SYNC);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewOpacity(nsIView *aView, float aOpacity)
{
  float opacity;

  aView->GetOpacity(opacity);

  if (opacity != aOpacity)
  {
    UpdateTransCnt(aView, nsnull);
    aView->SetOpacity(aOpacity);
    UpdateTransCnt(nsnull, aView);
    UpdateView(aView, NS_VMREFRESH_NO_SYNC);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetViewObserver(nsIViewObserver *aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetViewObserver(nsIViewObserver *&aObserver)
{
  if (nsnull != mObserver)
  {
    aObserver = mObserver;
    NS_ADDREF(mObserver);
    return NS_OK;
  }
  else
    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP nsViewManager :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

#ifndef max
#define max(a, b) ((a) < (b) ? (b) : (a))
#endif

nsDrawingSurface nsViewManager :: GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds)
{
  if ((nsnull == mDrawingSurface)
  		|| (mDSBounds.width < aBounds.width)
  		|| (mDSBounds.height < aBounds.height))
  {
		nsRect newBounds;
		newBounds.MoveTo(aBounds.x, aBounds.y);
		newBounds.width = max(aBounds.width, mDSBounds.width);
		newBounds.height = max(aBounds.height, mDSBounds.height);

    if (mDrawingSurface) {
      //destroy existing DS
      aContext.DestroyDrawingSurface(mDrawingSurface);
      mDrawingSurface = nsnull;
    }

    nsresult rv = aContext.CreateDrawingSurface(&newBounds, 0, mDrawingSurface);

		if (NS_SUCCEEDED(rv)) {
	    mDSBounds = newBounds;
			aContext.SelectOffScreenDrawingSurface(mDrawingSurface);
	  }
	  else {
	  	mDSBounds.SetRect(0,0,0,0);
	  	mDrawingSurface = nsnull;
	  }
  }
  else {
		aContext.SelectOffScreenDrawingSurface(mDrawingSurface);

		float p2t;
	  mContext->GetDevUnitsToAppUnits(p2t);
	  nsRect bounds = aBounds;
	  bounds *= p2t;

		PRBool clipEmpty;
		aContext.SetClipRect(bounds, nsClipCombine_kReplace, clipEmpty);

		nscolor col = NS_RGB(255,255,255);
		aContext.SetColor(col);
		//aContext.FillRect(bounds);
  }

  return mDrawingSurface;
}

NS_IMETHODIMP nsViewManager :: ShowQuality(PRBool aShow)
{
  if (nsnull != mRootScrollable)
    mRootScrollable->ShowQuality(aShow);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetShowQuality(PRBool &aResult)
{
  if (nsnull != mRootScrollable)
    mRootScrollable->GetShowQuality(aResult);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetQuality(nsContentQuality aQuality)
{
  if (nsnull != mRootScrollable)
    mRootScrollable->SetQuality(aQuality);

  return NS_OK;
}

nsIRenderingContext * nsViewManager :: CreateRenderingContext(nsIView &aView)
{
  nsIView             *par = &aView;
  nsIWidget           *win;
  nsIRenderingContext *cx = nsnull;
  nscoord             x, y, ax = 0, ay = 0;

  do
  {
    par->GetWidget(win);

    if (nsnull != win)
      break;

    //get absolute coordinates of view, but don't
    //add in view pos since the first thing you ever
    //need to do when painting a view is to translate
    //the rendering context by the views pos and other parts
    //of the code do this for us...

    if (par != &aView)
    {
      par->GetPosition(&x, &y);

      ax += x;
      ay += y;
    }

    par->GetParent(par);
  }
  while (nsnull != par);

  if (nsnull != win)
  {
    mContext->CreateRenderingContext(&aView, cx);

    if (nsnull != cx)
      cx->Translate(ax, ay);

    NS_RELEASE(win);
  }

  return cx;
}

void nsViewManager::AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const
{
	// Find a view with an associated widget. We'll transform this rect from the
	// current view's coordinate system to a "heavyweight" parent view, then convert
	// the rect to pixel coordinates, and accumulate the rect into that view's dirty region.
	nsIView* widgetView = GetWidgetView(aView);
	if (widgetView != nsnull) {
		nsRect widgetRect = aRect;
		ViewToWidget(aView, widgetView, widgetRect);

		// Get the dirty region associated with the widget view
		nsCOMPtr<nsIRegion> dirtyRegion;
		if (NS_SUCCEEDED(widgetView->GetDirtyRegion(*getter_AddRefs(dirtyRegion)))) {
			// add this rect to the widget view's dirty region.
			dirtyRegion->Union(widgetRect.x, widgetRect.y, widgetRect.width, widgetRect.height);
		}
	}
}

void nsViewManager::UpdateTransCnt(nsIView *oldview, nsIView *newview)
{
  if (nsnull != oldview)
  {
    PRBool  hasTransparency;
    float   opacity;

    oldview->HasTransparency(hasTransparency);
    oldview->GetOpacity(opacity);

    if (hasTransparency || (1.0f != opacity))
      mTransCnt--;
  }

  if (nsnull != newview)
  {
    PRBool  hasTransparency;
    float   opacity;

    newview->HasTransparency(hasTransparency);
    newview->GetOpacity(opacity);

    if (hasTransparency || (1.0f != opacity))
      mTransCnt++;
  }
}

NS_IMETHODIMP nsViewManager :: DisableRefresh(void)
{
  if (mUpdateBatchCnt > 0)
    return NS_OK;

  mRefreshEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: EnableRefresh(PRUint32 aUpdateFlags)
{
  if (mUpdateBatchCnt > 0)
    return NS_OK;

  mRefreshEnabled = PR_TRUE;

  if (mUpdateCnt > 0)
    ProcessPendingUpdates(mRootView);

  if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
   
    if (mTrueFrameRate > 0)
    {
      PRInt32 deltams = PR_IntervalToMilliseconds(PR_IntervalNow() - mLastRefresh);

      if (deltams > (1000 / (PRInt32)mTrueFrameRate))
        Composite();
    }
  }


  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: BeginUpdateViewBatch(void)
{
  nsresult result = NS_OK;
  
  if (mUpdateBatchCnt == 0)
    result = DisableRefresh();

  if (NS_SUCCEEDED(result))
    ++mUpdateBatchCnt;

  return result;
}

NS_IMETHODIMP nsViewManager :: EndUpdateViewBatch(PRUint32 aUpdateFlags)
{
  nsresult result = NS_OK;

  --mUpdateBatchCnt;

  NS_ASSERTION(mUpdateBatchCnt >= 0, "Invalid batch count!");

  if (mUpdateBatchCnt < 0)
  {
    mUpdateBatchCnt = 0;
    return NS_ERROR_FAILURE;
  }

  if (mUpdateBatchCnt == 0)
    result = EnableRefresh(aUpdateFlags);

  return result;
}

NS_IMETHODIMP nsViewManager :: SetRootScrollableView(nsIScrollableView *aScrollable)
{
  mRootScrollable = aScrollable;

  //XXX this needs to go away when layout start setting this bit on it's own. MMP
  if (mRootScrollable)
    mRootScrollable->SetScrollProperties(NS_SCROLL_PROPERTY_ALWAYS_BLIT);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetRootScrollableView(nsIScrollableView **aScrollable)
{
  *aScrollable = mRootScrollable;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: Display(nsIView* aView)
{
  nsIRenderingContext *localcx = nsnull;
  nsRect              trect;

  if (PR_FALSE == mRefreshEnabled)
    return NS_OK;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

  mContext->CreateRenderingContext(localcx);

  //couldn't get rendering context. this is ok if at startup
  if (nsnull == localcx)
  {
    mPainting = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  aView->GetBounds(trect);
  nscoord x = trect.x, y = trect.y;

  // XXX Temporarily reset the position to (0, 0), that way when we paint
  // we won't end up translating incorrectly
  aView->SetPosition(0, 0);

  PRBool  result;

  trect.x = trect.y = 0;
  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  // Paint the view. The clipping rect was set above set don't clip again.
  aView->Paint(*localcx, trect, NS_VIEW_FLAG_CLIP_SET, result);

  // XXX Reset the view's origin
  aView->SetPosition(x, y);

  NS_RELEASE(localcx);

  mPainting = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: AddCompositeListener(nsICompositeListener* aListener)
{
	if (nsnull == mCompositeListeners) {
		nsresult rv = NS_NewISupportsArray(&mCompositeListeners);
		if (NS_FAILED(rv))
			return rv;
	}
	return mCompositeListeners->AppendElement(aListener);
}

NS_IMETHODIMP nsViewManager :: RemoveCompositeListener(nsICompositeListener* aListener)
{
	if (nsnull != mCompositeListeners) {
		return mCompositeListeners->RemoveElement(aListener);
	}
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsViewManager::GetWidgetForView(nsIView *aView, nsIWidget **aWidget)
{
  *aWidget = nsnull;
  nsIView *view = aView;
  PRBool hasWidget = PR_FALSE;
  while (!hasWidget && view)
  {
	  view->HasWidget(&hasWidget);
    if (!hasWidget)
      view->GetParent(view);
  }

  if (hasWidget) {
      // Widget was found in the view hierarchy
    view->GetWidget(*aWidget);
  } else {
      // No widget was found in the view hierachy, so use try to use the mRootWindow
    if (nsnull != mRootWindow) {
#ifdef NS_DEBUG
      nsCOMPtr<nsIViewManager> vm;
      nsCOMPtr<nsIViewManager> thisInstance(this);
      aView->GetViewManager(*getter_AddRefs(vm));
      NS_ASSERTION(thisInstance == vm, "Must use the view instances view manager when calling GetWidgetForView");
#endif
      *aWidget = mRootWindow;
      NS_ADDREF(mRootWindow);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP nsViewManager::GetWidget(nsIWidget **aWidget)
{
  NS_IF_ADDREF(mRootWindow);
  *aWidget = mRootWindow;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::ForceUpdate()
{
  if (mRootWindow) {
    mRootWindow->Update();
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetOffset(nscoord *aX, nscoord *aY)
{
  NS_ASSERTION(aX != nsnull, "aX pointer is null");
  NS_ASSERTION(aY != nsnull, "aY pointer is null");
  *aX = mX;
  *aY = mY;
  return NS_OK;
}


PRBool nsViewManager :: CreateDisplayList(nsIView *aView, PRInt32 *aIndex,
                                          nscoord aOriginX, nscoord aOriginY, nsIView *aRealView,
                                          const nsRect *aDamageRect, nsIView *aTopView,
                                          nsVoidArray *aArray, nscoord aX, nscoord aY)
{
  PRInt32     numkids, zindex;
  PRBool      hasWidget, retval = PR_FALSE;
  nsIClipView *clipper = nsnull;
  nsPoint     *point;
  nsIView     *child = nsnull;

  NS_ASSERTION(!(!aView), "no view");
  NS_ASSERTION(!(!aIndex), "no index");

  if (!aArray)
  {
    if (!mDisplayList)
      mDisplayList = new nsVoidArray(8);

    aArray = mDisplayList;

    if (!aArray)
      return PR_TRUE;
  }

  if (!aTopView)
    aTopView = aView;

  nsRect lrect;

  aView->GetBounds(lrect);

  if (aView == aTopView)
  {
    lrect.x = 0;
    lrect.y = 0;
  }

  lrect.x += aX;
  lrect.y += aY;

  aView->QueryInterface(NS_GET_IID(nsIClipView), (void **)&clipper);
  aView->GetChildCount(numkids);
  aView->GetScratchPoint(&point);

  hasWidget = DoesViewHaveNativeWidget(*aView);

  if (numkids > 0)
  {
    if (clipper && (!hasWidget || (hasWidget && point->x)))
    {
      lrect.x -= aOriginX;
      lrect.y -= aOriginY;

      retval = AddToDisplayList(aArray, aIndex, aView, lrect, PUSH_CLIP);

      if (retval)
        return retval;

      lrect.x += aOriginX;
      lrect.y += aOriginY;
    }

    if (!hasWidget || (hasWidget && point->x))
//    if ((aView == aTopView) || (aView == aRealView))
//    if ((aView == aTopView) || !hasWidget || (aView == aRealView))
//    if ((aView == aTopView) || !(hasWidget && clipper) || (aView == aRealView))
    {
      for (aView->GetChild(0, child); nsnull != child; child->GetNextSibling(child))
      {
        child->GetZIndex(zindex);
        if (zindex < 0)
          break;
        retval = CreateDisplayList(child, aIndex, aOriginX, aOriginY, aRealView, aDamageRect, aTopView, aArray, lrect.x, lrect.y);
        if (retval)
          break;
      }
    }
  }

  lrect.x -= aOriginX;
  lrect.y -= aOriginY;

//  if (clipper)
  if (clipper && (!hasWidget || (hasWidget && point->x)))
  {
    if (numkids > 0)
      retval = AddToDisplayList(aArray, aIndex, aView, lrect, POP_CLIP);
  }
  else if (!retval)
  {
    nsViewVisibility  vis;
    float             opacity;
    PRBool            overlap;
    PRBool            trans;
    nsRect            irect;

    aView->GetVisibility(vis);
    aView->GetOpacity(opacity);
    aView->HasTransparency(trans);

    if (aDamageRect)
      overlap = irect.IntersectRect(lrect, *aDamageRect);
    else
      overlap = PR_TRUE;

    if ((nsViewVisibility_kShow == vis) && (opacity > 0.0f) && overlap)
    {
      retval = AddToDisplayList(aArray, aIndex, aView, lrect, 0);

      if (retval || !trans && (opacity == 1.0f) && (irect == *aDamageRect))
        retval = PR_TRUE;
    }
    
    // any children with negative z-indices?
    if (!retval && nsnull != child) {
      lrect.x += aOriginX;
      lrect.y += aOriginY;
      for (; nsnull != child; child->GetNextSibling(child)) {
        retval = CreateDisplayList(child, aIndex, aOriginX, aOriginY, aRealView, aDamageRect, aTopView, aArray, lrect.x, lrect.y);
        if (retval)
          break;
	  }
    }
  }
  
  return retval;
}

PRBool nsViewManager :: AddToDisplayList(nsVoidArray *aArray, PRInt32 *aIndex, nsIView *aView, nsRect &aRect, PRUint32 aFlags)
{
  PRInt32 index = (*aIndex)++;
  DisplayListElement* element = (DisplayListElement*) mDisplayList->ElementAt(index);
  if (element == nsnull) {
    element = new DisplayListElement;
    if (element == nsnull) {
      *aIndex = index;
      return PR_TRUE;
    }
    mDisplayList->ReplaceElementAt(element, index);
  }
  
  element->mView = aView;
  element->mClip = aRect;
  element->mFlags = aFlags;
  
  return PR_FALSE;
}

void nsViewManager::ShowDisplayList(PRInt32 flatlen)
{
  char     nest[400];
  PRInt32  newnestcnt, nestcnt = 0, cnt;

  for (cnt = 0; cnt < 400; cnt++)
    nest[cnt] = ' ';

  float t2p;
  mContext->GetAppUnitsToDevUnits(t2p);

  printf("### display list length=%d ###\n", flatlen);

  for (cnt = 0; cnt < flatlen; cnt += DISPLAYLIST_INC)
  {
    nsIView   *view, *parent;
    nsRect    rect;
    PRUint32  flags;
    PRInt32   zindex;

    DisplayListElement* element = (DisplayListElement*) mDisplayList->ElementAt(cnt);
    view = element->mView;
    rect = element->mClip;
    flags = element->mFlags;

    nest[nestcnt << 1] = 0;

    view->GetParent(parent);
    view->GetZIndex(zindex);
    rect *= t2p;
    printf("%snsIView@%p [z=%d, x=%d, y=%d, w=%d, h=%d, p=%p]\n",
           nest, view, zindex,
           rect.x, rect.y, rect.width, rect.height, parent);

    newnestcnt = nestcnt;

    if (flags)
    {
      printf("%s", nest);

      if (flags & POP_CLIP) {
        printf("POP_CLIP ");
        newnestcnt--;
      }

      if (flags & PUSH_CLIP) {
        printf("PUSH_CLIP ");
        newnestcnt++;
      }

      if (flags & VIEW_INCLUDED)
        printf("VIEW_INCLUDED ");

      printf("\n");
    }

    nest[nestcnt << 1] = ' ';

    nestcnt = newnestcnt;
  }
}

void nsViewManager :: ComputeViewOffset(nsIView *aView, nscoord *aX, nscoord *aY, PRInt32 aFlag)
{
  nsIView *parent;
  nsRect  bounds;
  nsPoint *point;

  aView->GetScratchPoint(&point);

  point->x = aFlag;

  aView->GetBounds(bounds);

  if (aX && aY)
  {
    *aX += bounds.x;
    *aY += bounds.y;
  }

  aView->GetParent(parent);

  if (parent)
    ComputeViewOffset(parent, aX, aY, aFlag);
}

PRBool nsViewManager :: DoesViewHaveNativeWidget(nsIView &aView)
{
  nsIWidget *widget;
  PRBool    retval = PR_FALSE;

  aView.GetWidget(widget);

  if (nsnull != widget)
  {
    void  *nativewidget;

    nativewidget = widget->GetNativeData(NS_NATIVE_WIDGET);
    NS_RELEASE(widget);

    if (nsnull != nativewidget)
      retval = PR_TRUE;
  }

  return retval;
}

void nsViewManager :: PauseTimer(void)
{
  PRUint32 oldframerate = mTrueFrameRate;
  SetFrameRate(0);
  mTrueFrameRate = oldframerate;
}

void nsViewManager :: RestartTimer(void)
{
  SetFrameRate(mTrueFrameRate);
}

nsIView* nsViewManager::GetWidgetView(nsIView *aView) const
{
	while (aView != nsnull) {
		PRBool hasWidget;
		aView->HasWidget(&hasWidget);
		if (hasWidget)
			return aView;
		aView->GetParent(aView);
	}
	return nsnull;
}

void nsViewManager::ViewToWidget(nsIView *aView, nsIView* aWidgetView, nsRect &aRect) const
{
	while (aView != aWidgetView) {
		nscoord x, y;
		aView->GetPosition(&x, &y);
		aRect.MoveBy(x, y);
		aView->GetParent(aView);
	}
	
	// intersect aRect with bounds of aWidgetView, to prevent generating any illegal rectangles.
	nsRect bounds;
	aWidgetView->GetBounds(bounds);
	bounds.x = bounds.y = 0;
	aRect.IntersectRect(aRect, bounds);
	
	// finally, convert to device coordinates.
	float t2p;
	mContext->GetAppUnitsToDevUnits(t2p);
	aRect.ScaleRoundOut(t2p);
}


nsresult nsViewManager::GetVisibleRect(nsRect& aVisibleRect)
{
  nsresult rv = NS_OK;

  // Get the viewport scroller
  nsIScrollableView* scrollingView;
  GetRootScrollableView(&scrollingView);

  if (scrollingView) {     
    // Determine the visible rect in the scrolled view's coordinate space.
    // The size of the visible area is the clip view size
    const nsIView*  clipView;
 
    scrollingView->GetScrollPosition(aVisibleRect.x, aVisibleRect.y);
    scrollingView->GetClipView(&clipView);
    clipView->GetDimensions(&aVisibleRect.width, &aVisibleRect.height);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsViewManager::GetAbsoluteRect(nsIView *aView, const nsRect &aRect, 
                                         nsRect& aAbsRect)
{
  nsIScrollableView* scrollingView = nsnull;
  nsIView* scrolledView = nsnull;
  GetRootScrollableView(&scrollingView);
  if (nsnull == scrollingView) { 
    return NS_ERROR_FAILURE;
  }

  scrollingView->GetScrolledView(scrolledView);

   // Calculate the absolute coordinates of the aRect passed in.
   // aRects values are relative to aView
  aAbsRect = aRect;
  nsIView *parentView = aView;
  while ((parentView != nsnull) && (parentView != scrolledView)) {
    nscoord x, y;
    parentView->GetPosition(&x, &y);
    aAbsRect.MoveBy(x, y);
    parentView->GetParent(parentView);
  }

  if (parentView != scrolledView) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


PRBool nsViewManager::IsRectVisible(nsIView *aView, const nsRect &aRect)
{
   // Calculate the absolute coordinates for the visible rectangle   
  nsRect visibleRect;
  if (GetVisibleRect(visibleRect) == NS_ERROR_FAILURE) {
    return PR_TRUE;
  }

   // Calculate the absolute coordinates of the aRect passed in.
   // aRects values are relative to aView
  nsRect absRect;
  if ((GetAbsoluteRect(aView, aRect, absRect)) == NS_ERROR_FAILURE) {
    return PR_TRUE;
  }
 
    // Compare the visible rect against the rect passed in.
  PRBool overlaps = absRect.IntersectRect(absRect, visibleRect);

#if 0
  // Debugging code
  static int toggle = 0;
  for (int i = 0; i < toggle; i++) {
    printf(" ");
  }
  if (toggle == 10) {
    toggle = 0;
  } else {
   toggle++;
  }
  printf("***overlaps %d\n", overlaps);
#endif

  return overlaps;
}



