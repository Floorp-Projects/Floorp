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
#include "nsIRegion.h"
#include "nsView.h"
#include "nsIScrollbar.h"
#include "nsIBlender.h"

static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIScrollbarIID, NS_ISCROLLBAR_IID);

static const PRBool gsDebug = PR_FALSE;

#define UPDATE_QUANTUM  1000 / 40

//#define NO_DOUBLE_BUFFER
//#define NEW_COMPOSITOR

#define FLATVIEW_INC  3

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

    nsresult rv = nsRepository::CreateInstance(kRenderingContextCID, 
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
  }

  mObserver = nsnull;
  mContext = nsnull;

  if (nsnull != mFlatViews)
  {
    PRInt32 cnt = mFlatViews->Count(), idx;

    for (idx = 1; idx < cnt; idx += FLATVIEW_INC)
    {
      nsRect *rect = (nsRect *)mFlatViews->ElementAt(idx);

      if (nsnull != rect)
        delete rect;
    }

    delete mFlatViews;
    mFlatViews = nsnull;
  }
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
    NS_IF_RELEASE(mTimer);

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
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;
    nsresult           retval;

    retval = mRootView->QueryInterface(kIScrollableViewIID, (void **)&scroller);
    if (NS_SUCCEEDED(retval)) {
      scroller->ComputeContainerSize();
    }

    return retval;
  }

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
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  PRBool  result;
  nsRect  trect;

  if (nsnull != region)
    localcx->SetClipRegion(*region, nsClipCombine_kUnion, result);

  aView->GetBounds(trect);

  localcx->SetClipRect(trect, nsClipCombine_kIntersect, result);

  RenderViews(aView, *localcx, trect, result);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
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
  nsRect              wrect;
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
    wrect.x = wrect.y = 0;
    NS_RELEASE(widget);
    ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  nsRect trect = *rect;

  PRBool  result;

  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  RenderViews(aView, *localcx, trect, result);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
#ifdef NEW_COMPOSITOR
    localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, 0);
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
#define FRONT_TO_BACK_RENDER      1
#define FRONT_TO_BACK_ACCUMULATE  2
#define BACK_TO_FRONT_TRANS       3
#define BACK_TO_FRONT_OPACITY     4
#define COMPOSITION_DONE          5

//bit shifts
#define TRANS_PROPERTY_TRANS      0
#define TRANS_PROPERTY_OPACITY    1

//flat view flags
#define VIEW_INCLUDED             0x0001

void nsViewManager :: RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect, PRBool &aResult)
{
#ifdef NEW_COMPOSITOR

  PRInt32           flatlen = 0, cnt;
  PRInt32           state = FRONT_TO_BACK_RENDER;
  PRInt32           transprop;
  nsRect            backfrontrect;
  nsRect            accumrect;
  PRInt32           increment = FLATVIEW_INC;
  PRInt32           loopstart = 0;
  PRInt32           loopend;
  PRInt32           accumstart;
  float             t2p;
  nsDrawingSurface  origsurf;
  nsIRegion         *transrgn = nsnull;
  nsIRegion         *temprgn = nsnull;
  nsIRegion         *rcrgn = nsnull;

  if (nsnull != aRootView)
    FlattenViewTree(aRootView, &flatlen, &aRect);

  loopend = flatlen;

  mContext->GetAppUnitsToDevUnits(t2p);
  aRC.GetDrawingSurface(&origsurf);

#if 0
printf("flatlen %d\n", flatlen);

for (cnt = 0; cnt < flatlen; cnt += FLATVIEW_INC)
{
  nsRect *rect;
  printf("view: %x\n", mFlatViews->ElementAt(cnt));

  rect = (nsRect *)mFlatViews->ElementAt(cnt + 1);

  if (nsnull != rect)
    printf("rect: %d, %d, %d, %d\n", rect->x, rect->y, rect->width, rect->height);
  else
    printf("rect: null\n");
}
#endif

  while (state != COMPOSITION_DONE)
  {
    for (cnt = loopstart; (increment > 0) ? (cnt < loopend) : (cnt > loopend); cnt += increment)
    {
      nsIView   *curview = (nsIView *)mFlatViews->ElementAt(cnt);
      nsRect    *currect = (nsRect *)mFlatViews->ElementAt(cnt + 1);
      PRUint32  curflags = (PRUint32)mFlatViews->ElementAt(cnt + 2);
    
      if ((nsnull != curview) && (nsnull != currect))
      {
        PRBool  hasWidget = DoesViewHaveNativeWidget(*curview);

        if ((PR_FALSE == hasWidget) || ((PR_TRUE == hasWidget) && (cnt == (flatlen - FLATVIEW_INC))))
        {
          PRBool  trans;
          float   opacity;

          curview->HasTransparency(trans);
          curview->GetOpacity(opacity);

          switch (state)
          {
            default:
            case FRONT_TO_BACK_RENDER:
              if ((PR_TRUE == trans) || (opacity < 1.0f))
              {
                //need to finish using back to front till this point
                transprop = (trans << TRANS_PROPERTY_TRANS) | ((opacity < 1.0f) << TRANS_PROPERTY_OPACITY);
                backfrontrect = accumrect = *currect;
                state = FRONT_TO_BACK_ACCUMULATE;
                accumstart = cnt;
                mFlatViews->ReplaceElementAt((void *)VIEW_INCLUDED, cnt + 2);

                //get a snapshot of the current clip so that we can exclude areas
                //already excluded in it from the transparency region
                aRC.GetClipRegion(&rcrgn);

                
              }
              else
              {
                RenderView(curview, aRC, aRect, *currect, aResult);

                if (aResult == PR_FALSE)
                  aRC.SetClipRect(*currect, nsClipCombine_kSubtract, aResult);
              }

              break;

            case FRONT_TO_BACK_ACCUMULATE:
              if ((PR_TRUE == backfrontrect.Intersects(*currect)) &&
                  ((PR_TRUE == trans) || (opacity < 1.0f)))
              {
                transprop |= (trans << TRANS_PROPERTY_TRANS) | ((opacity < 1.0f) << TRANS_PROPERTY_OPACITY);
                accumrect.UnionRect(*currect, accumrect);
                mFlatViews->ReplaceElementAt((void *)VIEW_INCLUDED, cnt + 2);
              }
  
              break;

            case BACK_TO_FRONT_TRANS:
              if (PR_TRUE == accumrect.Intersects(*currect))
              {
                PRBool clipstate;

                RenderView(curview, aRC, aRect, *currect, clipstate);

                //if this view is transparent, it has been accounted
                //for, so we never have to look at it again.
                if ((PR_TRUE == trans) && (curflags & VIEW_INCLUDED))
                {
                  mFlatViews->ReplaceElementAt(nsnull, cnt);
                  mFlatViews->ReplaceElementAt(nsnull, cnt + 2);
                }
              }

              break;

            case BACK_TO_FRONT_OPACITY:
              if (PR_TRUE == accumrect.Intersects(*currect))
              {
                PRBool clipstate;

                if (opacity == 1.0f)
                  RenderView(curview, aRC, aRect, *currect, clipstate);
                else
                {
                  aRC.SelectOffScreenDrawingSurface(gRed);

                  RenderView(curview, aRC, aRect, *currect, clipstate);

                  nsRect brect;

                  brect.x = brect.y = 0;
                  brect.width = accumrect.width;
                  brect.height = accumrect.height;

                  static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
                  static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

                  nsIBlender  *blender = nsnull;
                  nsresult rv;

                  rv = nsRepository::CreateInstance(kBlenderCID, nsnull, kIBlenderIID, (void **)&blender);

                  if (NS_OK == rv)
                  {
                    nscoord width, height;

                    width = NSToCoordRound(currect->width * t2p);
                    height = NSToCoordRound(currect->height * t2p);

                    blender->Init(mContext);
                    blender->Blend(0, 0, width, height, gRed, gOffScreen, 0, 0, opacity);

                    NS_RELEASE(blender);
                  }

                  aRC.SelectOffScreenDrawingSurface(gOffScreen);
                }

                //if this view is transparent or non-opaque, it has
                //been accounted for, so we never have to look at it again.
                if (((PR_TRUE == trans) || (opacity < 1.0f)) && (curflags & VIEW_INCLUDED))
                {
                  mFlatViews->ReplaceElementAt(nsnull, cnt);
                  mFlatViews->ReplaceElementAt(nsnull, cnt + 2);
                }
              }

              break;
          }
        }
        else
          aRC.SetClipRect(*currect, nsClipCombine_kSubtract, aResult);

        if (aResult == PR_TRUE)
        {
          state = COMPOSITION_DONE;
          break;
        }
      }
    }

    switch (state)
    {
      case FRONT_TO_BACK_ACCUMULATE:
        loopstart = cnt - FLATVIEW_INC;
        loopend = accumstart - FLATVIEW_INC;
        increment = -FLATVIEW_INC;
        state = (transprop & (1 << TRANS_PROPERTY_OPACITY)) ? BACK_TO_FRONT_OPACITY : BACK_TO_FRONT_TRANS;

        if (state == BACK_TO_FRONT_OPACITY)
        {
          PRInt32 w, h;
          nsRect  bitrect;

          //prepare offscreen buffers

          w = NSToCoordRound(accumrect.width * t2p);
          h = NSToCoordRound(accumrect.height * t2p);

          if ((w > gBlendWidth) || (h > gBlendHeight))
          {
            bitrect.x = bitrect.y = 0;
            bitrect.width = w;
            bitrect.height = h;

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

            gBlendWidth = w;
            gBlendHeight = h;
printf("offscr: %d, %d (%d, %d)\n", w, h, accumrect.width, accumrect.height);
          }

          if ((nsnull == gOffScreen) || (nsnull == gRed))
            state = BACK_TO_FRONT_TRANS;
          else
          {
            aRC.Translate(-accumrect.x, -accumrect.y);
            aRC.SelectOffScreenDrawingSurface(gOffScreen);
          }
        }

        break;

      case BACK_TO_FRONT_OPACITY:
        aRC.SelectOffScreenDrawingSurface(origsurf);
        aRC.Translate(accumrect.x, accumrect.y);
        aRC.CopyOffScreenBits(gOffScreen, 0, 0, accumrect, NS_COPYBITS_XFORM_DEST_VALUES | NS_COPYBITS_TO_BACK_BUFFER);
        //falls through

      case BACK_TO_FRONT_TRANS:
        loopstart = accumstart + FLATVIEW_INC;
        loopend = flatlen;
        increment = FLATVIEW_INC;
        state = FRONT_TO_BACK_RENDER;

        //clip out what we just rendered
        aRC.SetClipRect(accumrect, nsClipCombine_kSubtract, aResult);

        //did that finish everything?
        if (aResult == PR_TRUE)
          state = COMPOSITION_DONE;

        break;

      case FRONT_TO_BACK_RENDER:
        state = COMPOSITION_DONE;
        break;
    }
  }

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

  aView->GetBounds(bounds);

  //translate parent region into child coords.

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
    printf("ViewManager::UpdateView: %x, rect ", aView);
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
        nsRect  trect = *((nsPaintEvent*)aEvent)->rect;

        float   p2t;
        mContext->GetDevUnitsToAppUnits(p2t);
        trect.ScaleRoundOut(p2t);

        // Do an immediate refresh
        if (nsnull != mContext)
        {
          nsRect  vrect;
          float   varea;

          // Check that there's actually something to paint
          view->GetBounds(vrect);
          varea = (float)vrect.width * vrect.height;

          if (varea > 0.0000001f)
          {
            nsRect     rrect = trect;
            PRUint32   updateFlags = 0;

            // Auto double buffering logic.
            // See if the paint region is greater than .25 the area of our view.
            // If so, enable double buffered painting.

            rrect.IntersectRect(rrect, vrect);
  
            if ((((float)rrect.width * rrect.height) / varea) >  0.25f)
              updateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

//printf("refreshing: view: %x, %d, %d, %d, %d\n", view, trect.x, trect.y, trect.width, trect.height);
            // Refresh the view
            Refresh(view, ((nsPaintEvent*)aEvent)->renderingContext, &trect, updateFlags);
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
    if (nsnull != mDrawingSurface)
    {
      //destroy existing DS
      aContext.DestroyDrawingSurface(mDrawingSurface);
    }

    aContext.CreateDrawingSurface(&aBounds, 0, mDrawingSurface);
    mDSBounds = aBounds;
  }

  return mDrawingSurface;
}

NS_IMETHODIMP nsViewManager :: ShowQuality(PRBool aShow)
{
  nsIScrollableView *scroller;
  nsresult          retval = NS_ERROR_FAILURE;

  if (nsnull != mRootView)
  {
    nsIView *child;

    mRootView->GetChild(0, child);

    if (nsnull != child)
    {
      retval = child->QueryInterface(kIScrollableViewIID, (void **)&scroller);

      if (NS_SUCCEEDED(retval))
        scroller->ShowQuality(aShow);
    }
  }

  return retval;
}

NS_IMETHODIMP nsViewManager :: GetShowQuality(PRBool &aResult)
{
  nsIScrollableView *scroller;
  nsresult          retval = NS_ERROR_FAILURE;

  if (nsnull != mRootView)
  {
    nsIView *child;

    mRootView->GetChild(0, child);

    if (nsnull != child)
    {
      retval = child->QueryInterface(kIScrollableViewIID, (void **)&scroller);

      if (NS_SUCCEEDED(retval))
        scroller->GetShowQuality(aResult);
    }
  }

  return retval;
}

NS_IMETHODIMP nsViewManager :: SetQuality(nsContentQuality aQuality)
{
  nsIScrollableView *scroller;
  nsresult          retval = NS_ERROR_FAILURE;

  if (nsnull != mRootView)
  {
    nsIView *child;

    mRootView->GetChild(0, child);

    if (nsnull != child)
    {
      retval = child->QueryInterface(kIScrollableViewIID, (void **)&scroller);

      if (NS_SUCCEEDED(retval))
        scroller->SetQuality(aQuality);
    }
  }

  return retval;
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
    nsresult rv = nsRepository::CreateInstance(kRegionCID, 
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

void nsViewManager :: FlattenViewTree(nsIView *aView, PRInt32 *aIndex, const nsRect *aDamageRect, nsIView *aTopView, nsVoidArray *aArray, nscoord aX, nscoord aY)
{
  PRInt32 numkids, cnt;

  NS_ASSERTION(!(nsnull == aView), "no view");
  NS_ASSERTION(!(nsnull == aIndex), "no index");

  if (nsnull == aArray)
  {
    if (nsnull == mFlatViews)
      mFlatViews = new nsVoidArray(8);

    aArray = mFlatViews;

    if (nsnull == aArray)
      return;
  }

  if (nsnull == aTopView)
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

  aView->GetChildCount(numkids);

  if (numkids > 0)
  {
    PRBool hasWidget = DoesViewHaveNativeWidget(*aView);

    if ((PR_FALSE == hasWidget) || ((PR_TRUE == hasWidget) && (aView == aTopView)))
    {
      for (cnt = 0; cnt < numkids; cnt++)
      {
        nsIView *child;

        aView->GetChild(cnt, child);
        FlattenViewTree(child, aIndex, aDamageRect, aTopView, aArray, lrect.x, lrect.y);
      }
    }
  }

  nsViewVisibility  vis;
  float             opacity;
  PRBool            overlap;

  aView->GetVisibility(vis);
  aView->GetOpacity(opacity);

  if (nsnull != aDamageRect)
    overlap = lrect.Intersects(*aDamageRect);
  else
    overlap = PR_TRUE;

  if ((nsViewVisibility_kShow == vis) && (opacity > 0.0f) && (PR_TRUE == overlap))
  {
    aArray->ReplaceElementAt(aView, (*aIndex)++);

    nsRect *grect = (nsRect *)aArray->ElementAt(*aIndex);

    if (nsnull == grect)
    {
      grect = new nsRect(lrect.x, lrect.y, lrect.width, lrect.height);

      if (nsnull == grect)
      {
        (*aIndex)--;
        return;
      }

      aArray->ReplaceElementAt(grect, *aIndex);
    }
    else
      *grect = lrect;

    (*aIndex)++;

    aArray->ReplaceElementAt(nsnull, (*aIndex)++);
  }
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
