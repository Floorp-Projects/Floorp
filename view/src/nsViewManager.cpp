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

#include "nsViewManager.h"
#include "nsUnitConversion.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScrollableView.h"
#include "nsView.h"
#include "nsIScrollbar.h"
#include "nsIClipView.h"

static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIScrollbarIID, NS_ISCROLLBAR_IID);
static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);
static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);
static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kIClipViewIID, NS_ICLIPVIEW_IID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

static const PRBool gsDebug = PR_FALSE;

#define UPDATE_QUANTUM  1000 / 40

//#define NO_DOUBLE_BUFFER
#define NEW_COMPOSITOR

//used for debugging new compositor
//#define SHOW_RECTS
//#define SHOW_DISPLAYLIST

//number of entries per view in display list
#define DISPLAYLIST_INC  3

//display list flags
#define RENDER_VIEW   0x0000
#define VIEW_INCLUDED 0x0001
#define PUSH_CLIP     0x0002
#define POP_CLIP      0x0004

//display list slots
#define VIEW_SLOT     0
#define RECT_SLOT     1
#define FLAGS_SLOT    2

static void vm_timer_callback(nsITimer *aTimer, void *aClosure)
{
  nsViewManager *vm = (nsViewManager *)aClosure;

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

PRUint32 nsViewManager::mVMCount = 0;
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
  mVMCount++;
}

nsViewManager :: ~nsViewManager()
{
  if (nsnull != mTimer)
  {
    mTimer->Cancel();     //XXX this should not be necessary. MMP
    NS_RELEASE(mTimer);
  }

  NS_IF_RELEASE(mRootWindow);

  mRootScrollable = nsnull;

  --mVMCount;

  NS_ASSERTION(!(mVMCount < 0), "underflow of viewmanagers");

  if ((0 == mVMCount) &&
      ((nsnull != mDrawingSurface) || (nsnull != gOffScreen) ||
      (nsnull != gRed) || (nsnull != gBlue)))
  {
    nsIRenderingContext *rc;

    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

    nsresult rv = nsComponentManager::CreateInstance(kRenderingContextCID, 
                                       nsnull, 
                                       kIRenderingContextIID, 
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
    PRInt32 cnt = mDisplayList->Count(), idx;

    for (idx = RECT_SLOT; idx < cnt; idx += DISPLAYLIST_INC)
    {
      nsRect *rect = (nsRect *)mDisplayList->ElementAt(idx);

      if (nsnull != rect)
        delete rect;
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
}

NS_IMPL_QUERY_INTERFACE(nsViewManager, knsViewManagerIID)

nsrefcnt nsViewManager::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsViewManager::Release(void)
{
  mRefCnt--;

  if (mRefCnt == 0)
  {
    if (nsnull != mRootView) {
      // Destroy any remaining views
      mRootView->Destroy();
      mRootView = nsnull;
    }
    delete this;
    return 0;
  }
  return mRefCnt;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
NS_IMETHODIMP nsViewManager :: Init(nsIDeviceContext* aContext)
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

  mDSBounds.Empty();
  mTimer = nsnull;
  mFrameRate = 0;
  mTrueFrameRate = 0;
  mTransCnt = 0;

  rv = SetFrameRate(UPDATE_QUANTUM);

  mLastRefresh = PR_IntervalNow();

  mRefreshEnabled = PR_TRUE;

  mMouseGrabber = nsnull;
  mKeyGrabber = nsnull;

  //create regions
  nsComponentManager::CreateInstance(kRegionCID, nsnull, kIRegionIID, (void **)&mTransRgn);
  nsComponentManager::CreateInstance(kRegionCID, nsnull, kIRegionIID, (void **)&mOpaqueRgn);
  nsComponentManager::CreateInstance(kRegionCID, nsnull, kIRegionIID, (void **)&mTRgn);
  nsComponentManager::CreateInstance(kRegionCID, nsnull, kIRegionIID, (void **)&mRCRgn);

  if (nsnull != mTransRgn)
    mTransRgn->Init();

  if (nsnull != mOpaqueRgn)
    mOpaqueRgn->Init();

  if (nsnull != mTRgn)
    mTRgn->Init();

  if (nsnull != mRCRgn)
    mRCRgn->Init();

  return rv;
}

NS_IMETHODIMP nsViewManager :: GetRootView(nsIView *&aView)
{
  aView = mRootView;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetRootView(nsIView *aView)
{
  UpdateTransCnt(mRootView, aView);
  // Do NOT destroy the current root view. It's the caller's responsibility
  // to destroy it
  mRootView = aView;

  //now get the window too.
  NS_IF_RELEASE(mRootWindow);

  if (nsnull != mRootView)
    mRootView->GetWidget(mRootWindow);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: GetFrameRate(PRUint32 &aRate)
{
  aRate = mTrueFrameRate;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetFrameRate(PRUint32 aFrameRate)
{
  nsresult  rv;

  if (aFrameRate != mFrameRate)
  {
    if (nsnull != mTimer)
    {
      mTimer->Cancel();     //XXX this should not be necessary. MMP
      NS_RELEASE(mTimer);
    }

    mFrameRate = aFrameRate;
    mTrueFrameRate = aFrameRate;

    if (mFrameRate != 0)
    {
      rv = NS_NewTimer(&mTimer);

      if (NS_OK == rv)
        mTimer->Init(vm_timer_callback, this, 1000 / mFrameRate);
    }
    else
      rv = NS_OK;
  }
  else
    rv = NS_OK;

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
    if (nsnull == localcx)
      return;
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsIWidget*  widget;

    aView->GetWidget(widget);
    widget->GetClientBounds(wrect);

    wrect.x = wrect.y = 0;

    NS_RELEASE(widget);

    ds = GetDrawingSurface(*localcx, wrect);

    if (ds)
      localcx->SelectOffScreenDrawingSurface(ds);
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
    if (nsnull == localcx)
      return;
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsIWidget*  widget;

    aView->GetWidget(widget);
    widget->GetClientBounds(wrect);

    brect = wrect;
    wrect.x = wrect.y = 0;

    NS_RELEASE(widget);

    ds = GetDrawingSurface(*localcx, wrect);

    if (ds)
      localcx->SelectOffScreenDrawingSurface(ds);
  }

  nsRect trect = *rect;

  PRBool  result;

  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  RenderViews(aView, *localcx, trect, result);

  if ((aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER) && ds)
#ifdef NEW_COMPOSITOR
  {
#ifdef XP_UNIX
    localcx->SetClipRect(trect, nsClipCombine_kReplace, result);
#endif
    localcx->CopyOffScreenBits(ds, brect.x, brect.y, brect, 0);
  }
#else
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
#endif

  if (localcx != aContext)
    NS_RELEASE(localcx);

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

  mLastRefresh = PR_IntervalNow();

  mPainting = PR_FALSE;
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

static evenodd = 0;

void nsViewManager :: RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect, PRBool &aResult)
{
#ifdef NEW_COMPOSITOR

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
  nsRect            localrect;
  PRBool            useopaque = PR_FALSE;
  nsRegionRectSet   onerect, *rectset;

//printf("begin paint\n");
  if (nsnull != aRootView)
  {
    nscoord ox = 0, oy = 0;

    ComputeViewOffset(aRootView, &ox, &oy, 1);
    CreateDisplayList(mRootView, &flatlen, ox, oy, aRootView, &aRect);
  }

  loopend = flatlen;

  mContext->GetAppUnitsToDevUnits(t2p);
  mContext->GetDevUnitsToAppUnits(p2t);

#ifdef SHOW_DISPLAYLIST
{
  char      nest[400];
  PRUint32  newnestcnt, nestcnt = 0;

  for (cnt = 0; cnt < 400; cnt++)
    nest[cnt] = ' ';

  printf("flatlen %d\n", flatlen);

  for (cnt = 0; cnt < flatlen; cnt += DISPLAYLIST_INC)
  {
    nsRect    *rect;
    PRUint32  flags;

    nest[nestcnt << 1] = 0;

    printf("%sview: %x\n", nest, mDisplayList->ElementAt(cnt + VIEW_SLOT));

    rect = (nsRect *)mDisplayList->ElementAt(cnt + RECT_SLOT);

    if (nsnull != rect)
      printf("%srect: %d, %d, %d, %d\n", nest, rect->x, rect->y, rect->width, rect->height);
    else
      printf("%srect: null\n", nest);

    flags = (PRUint32)mDisplayList->ElementAt(cnt + FLAGS_SLOT);

    newnestcnt = nestcnt;

    if (flags)
    {
      printf("%s", nest);

      if (flags & POP_CLIP)
      {
        printf("POP_CLIP ");
        newnestcnt--;
      }

      if (flags & PUSH_CLIP)
      {
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
#endif

  while (state != COMPOSITION_DONE)
  {
    nsIView   *curview;
    nsRect    *currect;
    PRUint32  curflags;
    PRBool    pushing;
    PRUint32  pushcnt = 0;

    for (cnt = loopstart; (increment > 0) ? (cnt < loopend) : (cnt > loopend); cnt += increment)
    {
      curview = (nsIView *)mDisplayList->ElementAt(cnt + VIEW_SLOT);
      currect = (nsRect *)mDisplayList->ElementAt(cnt + RECT_SLOT);
      curflags = (PRUint32)mDisplayList->ElementAt(cnt + FLAGS_SLOT);
    
      if ((nsnull != curview) && (nsnull != currect))
      {
        PRBool  hasWidget = DoesViewHaveNativeWidget(*curview);
        PRBool  isBottom = cnt == (flatlen - DISPLAYLIST_INC);

        if (curflags & PUSH_CLIP)
        {
          PRBool  clipstate;

          if (state == BACK_TO_FRONT_OPACITY)
          {
            mOffScreenCX->PopState(clipstate);
            mRedCX->PopState(clipstate);
            mBlueCX->PopState(clipstate);

            pushcnt--;
            NS_ASSERTION(!((PRInt32)pushcnt < 0), "underflow");
          }
          else if (state == BACK_TO_FRONT_TRANS)
          {
            aRC.PopState(clipstate);
            pushcnt--;
            NS_ASSERTION(!((PRInt32)pushcnt < 0), "underflow");
          }
          else
          {
            pushing = FRONT_TO_BACK_RENDER;

            aRC.PushState();
            aRC.SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);

            pushcnt++;
          }
        }
        else if (curflags & POP_CLIP)
        {
          PRBool  clipstate;

          if (state == BACK_TO_FRONT_OPACITY)
          {
            pushing = BACK_TO_FRONT_OPACITY;

            mOffScreenCX->PushState();
            mRedCX->PushState();
            mBlueCX->PushState();

            mOffScreenCX->SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);
            mRedCX->SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);
            mBlueCX->SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);

            pushcnt++;
          }
          else if (state == BACK_TO_FRONT_TRANS)
          {
            pushing = FRONT_TO_BACK_RENDER;

            aRC.PushState();
            aRC.SetClipRect(*currect, nsClipCombine_kIntersect, clipstate);

            pushcnt++;
          }
          else
          {
            aRC.PopState(clipstate);
            pushcnt--;
            NS_ASSERTION(!((PRInt32)pushcnt < 0), "underflow");
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

                  if (aResult == PR_FALSE)
                  {
                    PRBool clipstate;

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
                nsRect  trect;

                trect.IntersectRect(*currect, aRect);
                trect *= t2p;

                if (trans || translucent ||
                    mTransRgn->ContainsRect(trect.x, trect.y, trect.width, trect.height) ||
                    mOpaqueRgn->ContainsRect(trect.x, trect.y, trect.width, trect.height))
                {
                  transprop |= (trans << TRANS_PROPERTY_TRANS) | (translucent << TRANS_PROPERTY_OPACITY);
                  mDisplayList->ReplaceElementAt((void *)(VIEW_INCLUDED | curflags), cnt + FLAGS_SLOT);

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

                  if (aResult == PR_FALSE)
                  {
                    PRBool clipstate;

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
                  PRBool clipstate;

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
                    PRBool clipstate;

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
                    PRBool clipstate;

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
                        if (NS_OK == nsComponentManager::CreateInstance(kBlenderCID, nsnull, kIBlenderIID, (void **)&mBlender))
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
              PRBool clipstate;

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

    NS_ASSERTION(!((PRInt32)pushcnt < 0), "underflow");

    while (pushcnt--)
    {
      PRBool  clipstate;

      NS_ASSERTION(!((PRInt32)pushcnt < 0), "underflow");

      if (pushing == BACK_TO_FRONT_OPACITY)
      {
        mOffScreenCX->PopState(clipstate);
        mRedCX->PopState(clipstate);
        mBlueCX->PopState(clipstate);
      }
      else
        aRC.PopState(clipstate);
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

              if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kIRenderingContextIID, (void **)&mOffScreenCX))
                mOffScreenCX->Init(mContext, gOffScreen);

              if (nsnull != gRed)
              {
                aRC.DestroyDrawingSurface(gRed);
                gRed = nsnull;
              }

              aRC.CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gRed);

              if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kIRenderingContextIID, (void **)&mRedCX))
                mRedCX->Init(mContext, gRed);

              if (nsnull != gBlue)
              {
                aRC.DestroyDrawingSurface(gBlue);
                gBlue = nsnull;
              }

              aRC.CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gBlue);

              if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kIRenderingContextIID, (void **)&mBlueCX))
                mBlueCX->Init(mContext, gBlue);

              gBlendWidth = localrect.width;
              gBlendHeight = localrect.height;
//printf("offscr: %d, %d (%d, %d)\n", w, h, accumrect.width, accumrect.height);
            }

            if ((nsnull == gOffScreen) || (nsnull == gRed))
              SET_STATE(BACK_TO_FRONT_TRANS)
          }

//printf("rect: %d %d %d %d\n", localrect.x, localrect.y, localrect.width, localrect.height);
          localrect *= p2t;
          rgnrect++;

          if (state == BACK_TO_FRONT_OPACITY)
          {
            PRBool  clipstate;

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

void nsViewManager :: UpdateDirtyViews(nsIView *aView, nsRect *aParentRect) const
{
  nsRect    pardamage;
  nsRect    bounds;
  PRUint32   flags;

  aView->GetBounds(bounds);

  //translate parent rect into child coords.

  if (nsnull != aParentRect)
  {
    pardamage = *aParentRect;

    pardamage.IntersectRect(bounds, pardamage);
    pardamage.MoveBy(-bounds.x, -bounds.y);
  }
  else
    pardamage = bounds;

  if (PR_FALSE == pardamage.IsEmpty())
  {
    nsIWidget *widget;

    aView->GetWidget(widget);

    if (nsnull != widget)
    {
      float scale;
      nsRect pixrect = pardamage;

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

  while (nsnull != child)
  {
    UpdateDirtyViews(child, &pardamage);
    child->GetNextSibling(child);
  }
}

#if 0

  nsRect    bounds;
  nsIRegion *dirtyRegion, *damager;
  PRInt32   posx, posy;

  aView->GetBounds(bounds);

  //translate parent region into child coords.

  if (nsnull != aParentDamage)
  {
    float     scale;

    mContext->GetAppUnitsToDevUnits(scale);

    posx = NSTwipsToIntPixels(bounds.x, scale);
    posy = NSTwipsToIntPixels(bounds.y, scale);

    aParentDamage->Offset(-posx, -posy);
  }

  // See if the view has a non-empty dirty region

  aView->GetDirtyRegion(dirtyRegion);

  if (nsnull != dirtyRegion)
  {
    if (PR_FALSE == dirtyRegion->IsEmpty())
    {
      if (nsnull != aParentDamage)
        dirtyRegion->Union(*aParentDamage);

      damager = dirtyRegion;
    }
    else
      damager = aParentDamage;
  }
  else
    damager = aParentDamage;

  nsIWidget *widget;

  aView->GetWidget(widget);

  if (nsnull != widget)
  {
    //have we not yet figured out the rootmost
    //view for the damage repair? if not, keep
    //recording widgets as we find them as the
    //topmost widget containing the topmost
    //damaged view.

    if (nsnull == *aTopDamaged)
      *aTopWidget = widget;

    if (nsnull != damager)
    {
      nsRegionComplexity cplx;

      damager->GetRegionComplexity(cplx);

      if (cplx == eRegionComplexity_rect)
      {
        nsRect trect;

        damager->GetBoundingBox(&trect.x, &trect.y, &trect.width, &trect.height);
        widget->Invalidate(trect, PR_FALSE);
      }
      else if (cplx == eRegionComplexity_complex)
        widget->Invalidate(*damager, PR_FALSE);
    }

    NS_RELEASE(widget);
  }

  //record the topmost damaged view

  if ((nsnull == *aTopDamaged) && (damager == dirtyRegion))
    *aTopDamaged = aView;

  // Check our child views
  nsIView *child;

  aView->GetChild(0, child);

  while (nsnull != child)
  {
    UpdateDirtyViews(child, damager, aTopDamaged, aTopWidget);
    child->GetNextSibling(child);
  }

  //translate the parent damage region back to where it used to be

  if (nsnull != aParentDamage)
    aParentDamage->Offset(posx, posy);

  if (nsnull != dirtyRegion)
  {
    // Clear our dirty region

    dirtyRegion->SetTo(0, 0, 0, 0);
    NS_RELEASE(dirtyRegion);
  }

#endif

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

NS_IMETHODIMP nsViewManager :: UpdateView(nsIView *aView, nsIRegion *aRegion, PRUint32 aUpdateFlags)
{
  // XXX Huh. What about the case where aRegion isn't nsull?
  // XXX yeah? what about it?
  if (aRegion == nsnull)
  {
    nsRect  trect;

    // Mark the entire view as damaged
    aView->GetBounds(trect);
    trect.x = trect.y = 0;
    UpdateView(aView, trect, aUpdateFlags);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
  NS_PRECONDITION(nsnull != aView, "null view");

  // Ignore any silly requests...
  if ((aRect.width == 0) || (aRect.height == 0))
    return NS_OK;

  if (gsDebug)
  {
    printf("ViewManager::UpdateView: %p, rect ", aView);
    stdout << aRect;
    printf("\n");
  }

  // Find the nearest view (including this view) that has a widget
  nsRect  trect = aRect;
  nsIView *par = aView;
  nscoord x, y;
  nsIView *widgetView = aView;

  do
  {
    nsIWidget *widget;

    // See if this view has a widget
    widgetView->GetWidget(widget);

    if (nsnull != widget)
    {
      NS_RELEASE(widget);
      break;
    }

    // Get the parent view
    widgetView->GetParent(widgetView);
  } while (nsnull != widgetView);
//  NS_ASSERTION(nsnull != widgetView, "no widget");  this assertion not valid for printing...MMP

  if (nsnull != widgetView)
  {
    if (0 == mUpdateCnt)
      RestartTimer();

    mUpdateCnt++;

    // Convert damage rect to coordinate space of containing view with
    // a widget
    // XXX Consolidate this code with the code above...
//    if (aView != widgetView)
//    {
      do
      {
        par->GetPosition(&x, &y);
        trect.x += x;
        trect.y += y;

        if (par != widgetView)
          par->GetParent(par);
      }
      while ((nsnull != par) && (par != widgetView));
//    }

    // Add this rect to the widgetView's dirty region.

    if (nsnull != widgetView)
      UpdateDirtyViews(widgetView, &trect);

    // See if we should do an immediate refresh or wait

    if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE)
    {
      Composite();
    }
    else if ((mTrueFrameRate > 0) && !(aUpdateFlags & NS_VMREFRESH_NO_SYNC))
    {
      // or if a sync paint is allowed and it's time for the compositor to
      // do a refresh

      PRInt32 deltams = PR_IntervalToMilliseconds(PR_IntervalNow() - mLastRefresh);

      if (deltams > (1000 / (PRInt32)mTrueFrameRate))
        Composite();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus)
{
  aStatus = nsEventStatus_eIgnore;

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
          aStatus = nsEventStatus_eConsumeNoDefault;
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
          varea = (float)viewrect.width * viewrect.height;

          if (varea > 0.0000001f)
          {
            nsRect     arearect;
            PRUint32   updateFlags = 0;

            // Auto double buffering logic.
            // See if the paint region is greater than .25 the area of our view.
            // If so, enable double buffered painting.

            arearect.IntersectRect(damrect, viewrect);
  
            if ((((float)arearect.width * arearect.height) / varea) >  0.25f)
              updateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

//printf("refreshing: view: %x, %d, %d, %d, %d\n", view, trect.x, trect.y, trect.width, trect.height);
            // Refresh the view
            Refresh(view, ((nsPaintEvent*)aEvent)->renderingContext, &damrect, updateFlags);
          }
        }

        aStatus = nsEventStatus_eConsumeNoDefault;
      }

      break;
    }

    case NS_DESTROY:
      aStatus = nsEventStatus_eConsumeNoDefault;
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
      else if (NS_OK == aEvent->widget->QueryInterface(kIScrollbarIID, (void**)&sb)) {
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

        aEvent->point.x = NSIntPixelsToTwips(aEvent->point.x, p2t);
        aEvent->point.y = NSIntPixelsToTwips(aEvent->point.y, p2t);

        aEvent->point.x += offset.x;
        aEvent->point.y += offset.y;

        view->HandleEvent(aEvent, NS_VIEW_FLAG_CHECK_CHILDREN | 
                                  NS_VIEW_FLAG_CHECK_PARENT |
                                  NS_VIEW_FLAG_CHECK_SIBLINGS,
                          aStatus);

        aEvent->point.x -= offset.x;
        aEvent->point.y -= offset.y;

        aEvent->point.x = NSTwipsToIntPixels(aEvent->point.x, t2p);
        aEvent->point.y = NSTwipsToIntPixels(aEvent->point.y, t2p);
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

    //and mark this area as dirty if the view is visible...

    nsViewVisibility  visibility;
    child->GetVisibility(visibility);

    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
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

    //and mark this area as dirty if the view is visible...
    nsViewVisibility  visibility;

    child->GetVisibility(visibility);

    if (nsViewVisibility_kHide != visibility)
      UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
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
    UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
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

NS_IMETHODIMP nsViewManager :: MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
  nscoord oldX;
  nscoord oldY;

  aView->GetPosition(&oldX, &oldY);
  aView->SetPosition(aX, aY);

  // only do damage control if the view is visible

  if ((aX != oldX) || (aY != oldY))
  {
    nsViewVisibility  visibility;
    aView->GetVisibility(visibility);
    if (visibility != nsViewVisibility_kHide)
    {
      nsRect  bounds;
      aView->GetBounds(bounds);
    	nsRect oldArea(oldX, oldY, bounds.width, bounds.height);
    	nsIView* parent;
      aView->GetParent(parent);
  	  UpdateView(parent, oldArea, NS_VMREFRESH_NO_SYNC);
  	  nsRect newArea(aX, aY, bounds.width, bounds.height); 
  	  UpdateView(parent, newArea, NS_VMREFRESH_NO_SYNC);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: ResizeView(nsIView *aView, nscoord width, nscoord height)
{
  nscoord twidth, theight, left, top, right, bottom, x, y;

  aView->GetPosition(&x, &y);
  aView->GetDimensions(&twidth, &theight);

  if (width < twidth)
  {
    left = width;
    right = twidth;
  }
  else
  {
    left = twidth;
    right = width;
  }

  if (height < theight)
  {
    top = height;
    bottom = theight;
  }
  else
  {
    top = theight;
    bottom = height;
  }

  aView->SetDimensions(width, height);

	nsIView *parent;
  aView->GetParent(parent);

  if (nsnull == parent)
  {
    parent = aView;
    x = y = 0;
  }

  //now damage the right edge of the view,
  //and the bottom edge of the view,

  nsRect  trect;

  //right edge...

  trect.x = left;
  trect.y = y;
  trect.width = right - left;
  trect.height = bottom - y;

  UpdateView(parent, trect, NS_VMREFRESH_NO_SYNC);

  //bottom edge...

  trect.x = x;
  trect.y = top;
  trect.width = right - x;
  trect.height = bottom - top;

  UpdateView(parent, trect, NS_VMREFRESH_NO_SYNC);
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
  aView->SetVisibility(aVisible);
  UpdateView(aView, nsnull, NS_VMREFRESH_NO_SYNC);
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
    UpdateView(aView, nsnull, NS_VMREFRESH_NO_SYNC);
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
    UpdateView(aView, nsnull, NS_VMREFRESH_NO_SYNC);
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

nsDrawingSurface nsViewManager :: GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds)
{
  if ((nsnull == mDrawingSurface) ||
      (mDSBounds.width < aBounds.width) || (mDSBounds.height < aBounds.height))
  {
	if (aBounds.width < mDSBounds.width)
		aBounds.width = mDSBounds.width;
	if (aBounds.height < mDSBounds.height)
		aBounds.height = mDSBounds.height;

    if (nsnull != mDrawingSurface)
    {
      //destroy existing DS
      aContext.DestroyDrawingSurface(mDrawingSurface);
      mDrawingSurface = nsnull;
    }
    aContext.CreateDrawingSurface(&aBounds, 0, mDrawingSurface);
    mDSBounds = aBounds;
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

void nsViewManager :: AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const
{
  // Get the dirty region associated with the view
  nsIRegion *dirtyRegion;

  aView->GetDirtyRegion(dirtyRegion);

  if (nsnull == dirtyRegion)
  {
    static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
    static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);

    // The view doesn't have a dirty region so create one
    nsresult rv = nsComponentManager::CreateInstance(kRegionCID, 
                                       nsnull, 
                                       kIRegionIID, 
                                       (void **)&dirtyRegion);

    dirtyRegion->Init();
    aView->SetDirtyRegion(dirtyRegion);
  }

  // Dirty regions are in device units, and aRect is in app units so
  // we need to convert to device units
  nsRect  trect = aRect;
  float   t2p;

  mContext->GetAppUnitsToDevUnits(t2p);

  trect.ScaleRoundOut(t2p);
  dirtyRegion->Union(trect.x, trect.y, trect.width, trect.height);
  NS_IF_RELEASE(dirtyRegion);
}

void nsViewManager :: UpdateTransCnt(nsIView *oldview, nsIView *newview)
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
  mRefreshEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager :: EnableRefresh(void)
{
  mRefreshEnabled = PR_TRUE;

  if (mTrueFrameRate > 0)
  {
    PRInt32 deltams = PR_IntervalToMilliseconds(PR_IntervalNow() - mLastRefresh);

    if (deltams > (1000 / (PRInt32)mTrueFrameRate))
      Composite();
  }

  return NS_OK;
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
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nsDrawingSurface    ds = nsnull;
  nsRect              trect;

  if (PR_FALSE == mRefreshEnabled)
    return NS_OK;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

  mContext->CreateRenderingContext(localcx);

  //couldn't get rendering context. this is ok if at startup
  if (nsnull == localcx)
  {
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

PRBool nsViewManager :: CreateDisplayList(nsIView *aView, PRInt32 *aIndex,
                                          nscoord aOriginX, nscoord aOriginY, nsIView *aRealView,
                                          const nsRect *aDamageRect, nsIView *aTopView,
                                          nsVoidArray *aArray, nscoord aX, nscoord aY)
{
  PRInt32     numkids, cnt;
  PRBool      hasWidget, retval = PR_FALSE;
  nsIClipView *clipper = nsnull;
  nsPoint     *point;

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

  aView->QueryInterface(kIClipViewIID, (void **)&clipper);
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
      for (cnt = 0; cnt < numkids; cnt++)
      {
        nsIView *child;

        aView->GetChild(cnt, child);
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
  }

#if 0
  if ((aTopView == aView) &&
      (trans || (opacity < 1.0f) ||
      (nsViewVisibility_kHide == vis) ||
      !overlap))
  {
    nsIView *parent;

    //the root view that we were given is transparent, translucent or
    //hidden, so we need to walk up the tree and add intervening parents
    //to the view list until we find the root view or something opaque,
    //non-transparent and not hidden. gross.

    aView->GetBounds(lrect);
    aView->GetParent(parent);

    lrect.x = -lrect.x;
    lrect.y = -lrect.y;

    ComputeParentOffsets(parent, &lrect.x, &lrect.y);
    FlattenViewTreeUp(aView, aIndex, aDamageRect, parent, aArray);
  }
#endif

  return retval;
}

PRBool nsViewManager :: AddToDisplayList(nsVoidArray *aArray, PRInt32 *aIndex, nsIView *aView, nsRect &aRect, PRUint32 aFlags)
{
  aArray->ReplaceElementAt(aView, (*aIndex)++);

  nsRect *grect = (nsRect *)aArray->ElementAt(*aIndex);

  if (!grect)
  {
    grect = new nsRect(aRect.x, aRect.y, aRect.width, aRect.height);

    if (!grect)
    {
      (*aIndex)--;
      return PR_TRUE;
    }

    aArray->ReplaceElementAt(grect, *aIndex);
  }
  else
    *grect = aRect;

  (*aIndex)++;

  aArray->ReplaceElementAt((void *)aFlags, (*aIndex)++);

  return PR_FALSE;
}

#if 0

void nsViewManager :: FlattenViewTreeUp(nsIView *aView, PRInt32 *aIndex, 
                                        const nsRect *aDamageRect, nsIView *aParView, 
                                        nsVoidArray *aArray)
{
  nsIView *nextview;
  nsRect  bounds, irect;
  PRBool  trans;
  float   opacity;
  PRBool  haswidget;
  nsViewVisibility  vis;
  nsPoint *point;

  aView->GetNextSibling(nextview);

  if (!nextview)
  {
    nextview = aParView;
    nextview->GetParent(aParView);
  }

  if (!aParView)
    return;

  aParView->GetScratchPoint(&point);

  if (nextview)
  {
    nextview->GetBounds(bounds);

    bounds.x += point->x;
    bounds.y += point->y;

    haswidget = DoesViewHaveNativeWidget(*nextview);
    nextview->GetVisibility(vis);
    nextview->HasTransparency(trans);
    nextview->GetOpacity(opacity);

    if (!haswidget && irect.IntersectRect(bounds, *aDamageRect) &&
        (nsViewVisibility_kShow == vis) && (opacity > 0.0f))
    {
      aArray->ReplaceElementAt(nextview, (*aIndex)++);

      nsRect *grect = (nsRect *)aArray->ElementAt(*aIndex);

      if (!grect)
      {
        grect = new nsRect(bounds.x, bounds.y, bounds.width, bounds.height);

        if (!grect)
        {
          (*aIndex)--;
          return;
        }

        aArray->ReplaceElementAt(grect, *aIndex);
      }
      else
        *grect = bounds;

      (*aIndex)++;

      aArray->ReplaceElementAt(nsnull, (*aIndex)++);

      //if this view completely covers the damage area and it's opaque
      //then we're done.

      if (!trans && (opacity == 1.0f) && (irect == *aDamageRect))
        return;
    }

    FlattenViewTreeUp(nextview, aIndex, aDamageRect, aParView, aArray);
  }
}

#endif

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
