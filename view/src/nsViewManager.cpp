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
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScrollableView.h"
#include "nsIRegion.h"

static const PRBool gsDebug = PR_FALSE;

#define UPDATE_QUANTUM  1000 / 40

//#define USE_DIRTY_RECT
//#define NO_DOUBLE_BUFFER

static void vm_timer_callback(nsITimer *aTimer, void *aClosure)
{
  nsViewManager *vm = (nsViewManager *)aClosure;

  //restart the timer
  
  PRInt32 fr = vm->GetFrameRate();

  vm->mFrameRate = 0;
  vm->SetFrameRate(fr);
  vm->Composite();
}

static NS_DEFINE_IID(knsViewManagerIID, NS_IVIEWMANAGER_IID);

nsViewManager :: nsViewManager()
{
}

nsViewManager :: ~nsViewManager()
{
  if (nsnull != mTimer)
  {
    mTimer->Cancel();     //XXX this should not be necessary. MMP
    NS_RELEASE(mTimer);
  }

  NS_IF_RELEASE(mRootWindow);
  NS_IF_RELEASE(mRootView);
  NS_IF_RELEASE(mDirtyRegion);

  if (nsnull != mDrawingSurface)
  {
    nsIRenderingContext *rc;

    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

    nsresult rv = NSRepository::CreateInstance(kRenderingContextCID, 
                                       nsnull, 
                                       kIRenderingContextIID, 
                                       (void **)&rc);

    if (NS_OK == rv)
    {
      rc->DestroyDrawingSurface(mDrawingSurface);
      NS_RELEASE(rc);
    }
    
    mDrawingSurface = nsnull;
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

  if ((mRefCnt == 1) && (nsnull != mRootView))
  {
    nsIView *pRoot = mRootView;
    mRootView = nsnull;         //set to null so that we don't get in here again
    NS_RELEASE(pRoot);          //kill the root view
  }

  if (mRefCnt == 0)
  {
    delete this;
    return 0;
  }
  return mRefCnt;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
nsresult nsViewManager::Init(nsIPresContext* aPresContext)
{
  nsresult rv;

  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  if (nsnull == aPresContext) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mContext = aPresContext;

  mDSBounds.Empty();
  mDrawingSurface = nsnull;
  mTimer = nsnull;
  mFrameRate = 0;
  mTransCnt = 0;

  rv = SetFrameRate(UPDATE_QUANTUM);

  mLastRefresh = PR_Now();

  return rv;
}

nsIWidget * nsViewManager :: GetRootWindow()
{
  NS_IF_ADDREF(mRootWindow);
  return mRootWindow;
}

void nsViewManager :: SetRootWindow(nsIWidget *aRootWindow)
{
  NS_IF_RELEASE(mRootWindow);
  mRootWindow = aRootWindow;
  NS_IF_ADDREF(mRootWindow);
}

nsIView * nsViewManager :: GetRootView()
{
  NS_IF_ADDREF(mRootView);
  return mRootView;
}

void nsViewManager :: SetRootView(nsIView *aView)
{
  UpdateTransCnt(mRootView, aView);
  NS_IF_RELEASE(mRootView);
  mRootView = aView;
  NS_IF_ADDREF(mRootView);
}

PRUint32 nsViewManager :: GetFrameRate()
{
  return mFrameRate;
}

nsresult nsViewManager :: SetFrameRate(PRUint32 aFrameRate)
{
  nsresult  rv;

  if (aFrameRate != mFrameRate)
  {
    NS_IF_RELEASE(mTimer);

    mFrameRate = aFrameRate;

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

void nsViewManager :: GetWindowDimensions(nscoord *width, nscoord *height)
{
  if (nsnull != mRootView)
    mRootView->GetDimensions(width, height);
  else
  {
    *width = 0;
    *height = 0;
  }
}

void nsViewManager :: SetWindowDimensions(nscoord width, nscoord height)
{
  nsIPresShell* presShell = mContext->GetShell();

  // Resize the root view
  if (nsnull != mRootView)
    mRootView->SetDimensions(width, height);

  // Inform the presentation shell that we've been resized
  if (nsnull != presShell)
  {
    presShell->ResizeReflow(width, height);
    NS_RELEASE(presShell);
  }
}

void nsViewManager :: GetWindowOffsets(nscoord *xoffset, nscoord *yoffset)
{
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;

    static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

    if (NS_OK == mRootView->QueryInterface(kscroller, (void **)&scroller))
    {
      scroller->GetVisibleOffset(xoffset, yoffset);
      NS_RELEASE(scroller);
    }
    else
      *xoffset = *yoffset = 0;
  }
  else
    *xoffset = *yoffset = 0;
}

void nsViewManager :: SetWindowOffsets(nscoord xoffset, nscoord yoffset)
{
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;

    static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

    if (NS_OK == mRootView->QueryInterface(kscroller, (void **)&scroller))
    {
      scroller->SetVisibleOffset(xoffset, yoffset);
      NS_RELEASE(scroller);
    }
  }
}

void nsViewManager :: ResetScrolling(void)
{
  if (nsnull != mRootView)
  {
    nsIScrollableView *scroller;

    static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

    if (NS_OK == mRootView->QueryInterface(kscroller, (void **)&scroller))
    {
      scroller->ComputeContainerSize();
      NS_RELEASE(scroller);
    }
  }
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, nsIRegion *region, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nscoord             xoff, yoff;
  float               scale;

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
    {
      return;
    }
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    mRootWindow->GetBounds(wrect);
    nsDrawingSurface  ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  scale = mContext->GetTwipsToPixels();

  GetWindowOffsets(&xoff, &yoff);

  region->Offset(NS_TO_INT_ROUND(-xoff * scale), NS_TO_INT_ROUND(-yoff * scale));
//  localcx->SetClipRegion(*region, nsClipCombine_kIntersect);
  localcx->SetClipRegion(*region, nsClipCombine_kReplace);
  region->Offset(NS_TO_INT_ROUND(xoff * scale), NS_TO_INT_ROUND(yoff * scale));

  nsRect trect;

  region->GetBoundingBox(&trect.x, &trect.y, &trect.width, &trect.height);
  trect *= mContext->GetPixelsToTwips();

  aView->Paint(*localcx, trect, 0);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(wrect);

  if (localcx != aContext)
    NS_RELEASE(localcx);

  //is the dirty region the same as the region we just painted?

  if ((region == mDirtyRegion) || region->IsEqual(*mDirtyRegion))
    ClearDirtyRegion();
  else
    mDirtyRegion->Subtract(*region);

  mLastRefresh = PR_Now();
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, nsRect *rect, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;
  nscoord             xoff, yoff;

  //force double buffering because of non-opaque views?

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
    {
      return;
    }
  }
  else
    localcx = aContext;

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    mRootWindow->GetBounds(wrect);
    nsDrawingSurface  ds = GetDrawingSurface(*localcx, wrect);
    localcx->SelectOffScreenDrawingSurface(ds);
  }

  if (aUpdateFlags & NS_VMREFRESH_SCREEN_RECT)
    localcx->SetClipRect(*rect, nsClipCombine_kReplace);

  GetWindowOffsets(&xoff, &yoff);

  nsRect trect = *rect;

  if (aUpdateFlags & NS_VMREFRESH_SCREEN_RECT)
    trect.MoveBy(xoff, yoff);
  else
  {
    trect.MoveBy(-xoff, -yoff);
    localcx->SetClipRect(trect, nsClipCombine_kReplace);
  }

  aView->Paint(*localcx, trect, 0);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(wrect);

  if (localcx != aContext)
    NS_RELEASE(localcx);

#ifdef USE_DIRTY_RECT

  nsRect  updaterect = *rect;

  //does our dirty rect intersect the rect we just painted?

  if (updaterect.IntersectRect(updaterect, mDirtyRect))
  {
    //does the update rect fully contain the dirty rect?
    //if so, then clear the dirty rect.

    if (updaterect.Contains(mDirtyRect))
      ClearDirtyRegion();
  }

#else

  //subtract the area we just painted from the dirty region

  if ((nsnull != mDirtyRegion) && !mDirtyRegion->IsEmpty())
  {
    nsRect pixrect = trect;

    pixrect *= mContext->GetTwipsToPixels();
    mDirtyRegion->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
  }

#endif

  mLastRefresh = PR_Now();
}

void nsViewManager :: Composite()
{
#ifdef USE_DIRTY_RECT

  if ((nsnull != mRootView) && !mDirtyRect.IsEmpty())
    Refresh(mRootView, nsnull, &mDirtyRect, NS_VMREFRESH_DOUBLE_BUFFER);

#else

  if ((nsnull != mRootView) && (nsnull != mDirtyRegion) && !mDirtyRegion->IsEmpty())
    Refresh(mRootView, nsnull, mDirtyRegion, NS_VMREFRESH_DOUBLE_BUFFER);

#endif
}

void nsViewManager :: UpdateView(nsIView *aView, nsIRegion *aRegion, PRUint32 aUpdateFlags)
{
  if (aRegion == nsnull)
  {
    nsRect  trect;

    aView->GetBounds(trect);
    trect.x = trect.y = 0;
    UpdateView(aView, trect, aUpdateFlags);
  }
}

void nsViewManager :: UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
  nsRect  trect = aRect;
  nsIView *par = aView;
  nscoord x, y;

  if (gsDebug)
  {
    printf("ViewManager::UpdateView: %x, rect ", aView);
    stdout << trect;
    printf("\n");
  }

  if (nsnull != aView)
  {
    do
    {
      //get absolute coordinates of view

      par->GetPosition(&x, &y);

      trect.x += x;
      trect.y += y;
    }
    while (par = par->GetParent());
  }

#ifdef USE_DIRTY_RECT

  if (mDirtyRect.IsEmpty())
    mDirtyRect = trect;
  else
    mDirtyRect.UnionRect(mDirtyRect, trect);

#else

  AddRectToDirtyRegion(trect);

#endif

  if (nsnull != mContext)
  {
    if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE)
#ifdef USE_DIRTY_RECT
      Refresh(mRootView, nsnull, &mDirtyRect, aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER);
#else
      Refresh(mRootView, nsnull, mDirtyRegion, aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER);
#endif
    else if ((mFrameRate > 0) && !(aUpdateFlags & NS_VMREFRESH_NO_SYNC))
    {
      PRTime now = PR_Now();
      PRTime conversion, ustoms;
      PRInt32 deltams;

      LL_I2L(ustoms, 1000);
      LL_SUB(conversion, now, mLastRefresh);
      LL_DIV(conversion, conversion, ustoms);
      LL_L2I(deltams, conversion);

      if (deltams > (1000 / (PRInt32)mFrameRate))
        Composite();
    }
  }
}

PRBool nsViewManager :: DispatchEvent(nsIEvent *event)
{
  return PR_TRUE;
}

PRBool nsViewManager :: GrabMouseEvents(nsIView *aView)
{
  return PR_TRUE;
}

PRBool nsViewManager :: GrabKeyEvents(nsIView *aView)
{
  return PR_TRUE;
}

nsIView * nsViewManager :: GetMouseEventGrabber()
{
  return nsnull;
}

nsIView * nsViewManager :: GetKeyEventGrabber()
{
  return nsnull;
}

void nsViewManager :: InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                                  PRBool above)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    PRInt32 numkids = parent->GetChildCount();
    nsIView *kid = nsnull, *prev = nsnull;

    //verify that the sibling exists...

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      kid = parent->GetChild(cnt);

      if (kid == sibling)
        break;

      prev = kid;
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

    if (child->GetVisibility() != nsViewVisibility_kHide)
      UpdateView(child, nsnull, 0);
  }
}

void nsViewManager :: InsertChild(nsIView *parent, nsIView *child, PRInt32 zindex)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    PRInt32 numkids = parent->GetChildCount();
    nsIView *kid = nsnull, *prev = nsnull;

    //find the right insertion point...

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      PRInt32 idx;
    
      kid = parent->GetChild(cnt);

      idx = kid->GetZIndex();

      if (zindex < idx)
        break;

      prev = kid;
    }

    //in case this hasn't been set yet... maybe we should not do this? MMP

    child->SetZIndex(zindex);
    parent->InsertChild(child, prev);

    UpdateTransCnt(nsnull, child);

    //and mark this area as dirty if the view is visible...

    if (child->GetVisibility() != nsViewVisibility_kHide)
      UpdateView(child, nsnull, 0);
  }
}

void nsViewManager :: RemoveChild(nsIView *parent, nsIView *child)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    UpdateTransCnt(child, nsnull);
    UpdateView(child, nsnull, NS_VMREFRESH_NO_SYNC);
    parent->RemoveChild(child);
  }
}

void nsViewManager :: MoveViewBy(nsIView *aView, nscoord aX, nscoord aY)
{
  nscoord x, y;

  aView->GetPosition(&x, &y);
  MoveViewTo(aView, aX + x, aY + y);
}

void nsViewManager :: MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
  nscoord oldX;
  nscoord oldY;

  aView->GetPosition(&oldX, &oldY);
  aView->SetPosition(aX, aY);

  // only do damage control if the view is visible

  if (nsViewVisibility_kHide != aView->GetVisibility())
  {
    nsRect  bounds;
    aView->GetBounds(bounds);
  	nsRect oldArea(oldX, oldY, bounds.width, bounds.height);
  	nsIView* parent = aView->GetParent();  // no addref
	  UpdateView(parent, oldArea, 0);
	  nsRect newArea(aX, aY, bounds.width, bounds.height); 
	  UpdateView(parent, newArea, 0);
  }
}

void nsViewManager :: ResizeView(nsIView *aView, nscoord width, nscoord height)
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

	nsIView *parent = aView->GetParent();  // no addref

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

  UpdateView(parent, trect, 0);

  //bottom edge...

  trect.x = x;
  trect.y = top;
  trect.width = right - x;
  trect.height = bottom - top;

  UpdateView(parent, trect, 0);
}

void nsViewManager :: SetViewClip(nsIView *aView, nsRect *rect)
{
}

void nsViewManager :: SetViewVisibility(nsIView *aView, nsViewVisibility visible)
{
}

void nsViewManager :: SetViewZindex(nsIView *aView, PRInt32 zindex)
{
}

void nsViewManager :: MoveViewAbove(nsIView *aView, nsIView *other)
{
}

void nsViewManager :: MoveViewBelow(nsIView *aView, nsIView *other)
{
}

PRBool nsViewManager :: IsViewShown(nsIView *aView)
{
  return PR_TRUE;
}

PRBool nsViewManager :: GetViewClipAbsolute(nsIView *aView, nsRect *rect)
{
  return PR_TRUE;
}

void nsViewManager :: SetViewContentTransparency(nsIView *aView, PRBool aTransparent)
{
  if (aTransparent != aView->HasTransparency())
  {
    if (aTransparent == PR_FALSE)
    {
      //going opaque
      mTransCnt--;
    }
    else
    {
      //going transparent
      mTransCnt++;
    }

    aView->SetContentTransparency(aTransparent);
  }
}

void nsViewManager :: SetViewOpacity(nsIView *aView, float aOpacity)
{
  PRBool  newopaque, oldopaque;
  float   oldopacity;

  if ((aOpacity == 1.0f) || (aOpacity == 0.0f))
    newopaque = PR_TRUE;
  else
    newopaque = PR_FALSE;

  oldopacity = aView->GetOpacity();

  if ((oldopacity == 1.0f) || (oldopacity == 0.0f))
    oldopaque = PR_TRUE;
  else
    oldopaque = PR_FALSE;

  if (newopaque != oldopaque)
  {
    if (newopaque == PR_FALSE)
    {
      //going transparent
      mTransCnt++;
    }
    else
    {
      //going opaque
      mTransCnt--;
    }
  }

  aView->SetOpacity(aOpacity);
}

nsIPresContext * nsViewManager :: GetPresContext()
{
  NS_IF_ADDREF(mContext);
  return mContext;
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

    mDrawingSurface = aContext.CreateDrawingSurface(&aBounds);
    mDSBounds = aBounds;
  }

  return mDrawingSurface;
}

void nsViewManager :: ClearDirtyRegion()
{
#ifdef USE_DIRTY_RECT

  mDirtyRect.x = 0;
  mDirtyRect.y = 0;
  mDirtyRect.width = 0;
  mDirtyRect.height = 0;

#else

  if (nsnull != mDirtyRegion)
    mDirtyRegion->SetTo(0, 0, 0, 0);

#endif
}

nsIRenderingContext * nsViewManager :: CreateRenderingContext(nsIView &aView)
{
  nsIView             *par = &aView;
  nsIWidget           *win;
  nsIRenderingContext *cx = nsnull;
  nsIDeviceContext    *dx;
  nscoord             x, y, ax = 0, ay = 0;

  do
  {
    win = par->GetWidget();

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
  }
  while (par = par->GetParent());

  if (nsnull != win)
  {
    dx = mContext->GetDeviceContext();
    cx = dx->CreateRenderingContext(&aView);

    if (nsnull != cx)
      cx->Translate(ax, ay);

    NS_RELEASE(dx);
    NS_RELEASE(win);
  }

  return cx;
}

void nsViewManager :: AddRectToDirtyRegion(nsRect &aRect)
{
  if (nsnull == mDirtyRegion)
  {
    static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
    static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);

    nsresult rv = NSRepository::CreateInstance(kRegionCID, 
                                       nsnull, 
                                       kIRegionIID, 
                                       (void **)&mDirtyRegion);

    if (NS_OK != rv)
      return;
    else
      mDirtyRegion->Init();
  }

  nsRect  trect = aRect;

  trect *= mContext->GetTwipsToPixels();
  mDirtyRegion->Union(trect.x, trect.y, trect.width, trect.height);
}

void nsViewManager :: UpdateTransCnt(nsIView *oldview, nsIView *newview)
{
  if ((nsnull != oldview) && (oldview->HasTransparency() ||
      (oldview->GetOpacity() != 1.0f)))
    mTransCnt--;

  if ((nsnull != newview) && (newview->HasTransparency() ||
      (newview->GetOpacity() != 1.0f)))
    mTransCnt++;
}
