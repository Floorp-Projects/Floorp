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

static const PRBool gsDebug = PR_FALSE;

#define UPDATE_QUANTUM  1000 / 40

static void vm_timer_callback(nsITimer *aTimer, void *aClosure)
{
  nsViewManager *vm = (nsViewManager *)aClosure;

  //restart the timer
  
  PRInt32 fr = vm->GetFrameRate();

  vm->mFrameRate = 0;

  vm->SetFrameRate(fr);

  if (vm->mDirtyRect.IsEmpty() == PR_FALSE)
  {
    nsIRenderingContext *cx;
    nsIPresContext      *px = vm->GetPresContext();
    nsIDeviceContext    *dx = px->GetDeviceContext();

    cx = dx->CreateRenderingContext(vm->mRootView);

    if (nsnull != cx)
    {
      vm->Refresh(vm->mRootView, cx, &vm->mDirtyRect, NS_VMREFRESH_DOUBLE_BUFFER);
      NS_RELEASE(cx);
    }

    NS_RELEASE(dx);
    NS_RELEASE(px);
  }
}

static NS_DEFINE_IID(knsViewManagerIID, NS_IVIEWMANAGER_IID);

nsViewManager :: nsViewManager()
{
}

nsViewManager :: ~nsViewManager()
{
  NS_IF_RELEASE(mTimer);
  NS_IF_RELEASE(mRootWindow);
  NS_IF_RELEASE(mRootView);

  if (nsnull != mDrawingSurface)
  {
    nsIRenderingContext *rc;

    //interesting way of killing things

    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kIRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

    nsresult rv = NSRepository::CreateInstance(kRenderingContextCID, 
                                       nsnull, 
                                       kIRenderingContextIID, 
                                       (void **)&rc);

    if (NS_OK == rv)
    {
      rc->DestroyDrawingSurface(mDrawingSurface);
      rc->Release();
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
  if (--mRefCnt == 0) {
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

  mOffset.x = mOffset.y = 0;

  mDSBounds.Empty();
  mDrawingSurface = nsnull;
  mTimer = nsnull;
  mFrameRate = 0;

  rv = SetFrameRate(UPDATE_QUANTUM);

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
  *width = 0;
  *height = 0;
}

void nsViewManager :: SetWindowDimensions(nscoord width, nscoord height)
{
  nsIPresShell* presShell = mContext->GetShell();

  // Resize the root view
  mRootView->SetDimensions(width, height);

  // Inform the presentation shell that we've been resized
  if (nsnull != presShell) {
    presShell->ResizeReflow(width, height);
    NS_RELEASE(presShell);
  }
}

void nsViewManager :: GetWindowOffsets(nscoord *xoffset, nscoord *yoffset)
{
  *xoffset = mOffset.x;
  *yoffset = mOffset.y;
}

void nsViewManager :: SetWindowOffsets(nscoord xoffset, nscoord yoffset)
{
  mOffset.x = xoffset;
  mOffset.y = yoffset;
}

void nsViewManager :: Refresh(nsIRenderingContext *aContext, nsRegion *region, PRUint32 aUpdateFlags)
{
}

static void paint_all_kids(nsIRenderingContext *context, nsIView *par, nsRect *rect)
{
  nsIView   *kid;

  if (gsDebug)
  {
    printf("ViewManager::Refresh: view=%p painting twips=(%d, %d, %d, %d) pixels: ",
           par, rect->x, rect->y, rect->XMost(), rect->YMost());
    stdout << *rect;
    printf("\n");
  }

  nsRect  parbounds;

  par->GetBounds(parbounds);

  context->PushState();
  context->Translate(parbounds.x, parbounds.y);

  par->Paint(*context, *rect);

  if (par->GetChildCount() > 0)
  {
    kid = par->GetChild(0);

    while (nsnull != kid)
    {
      paint_all_kids(context, kid, rect);
      kid = kid->GetNextSibling();
    }
  }

  context->PopState();
}

void nsViewManager :: Refresh(nsIView *aView, nsIRenderingContext *aContext, nsRect *rect, PRUint32 aUpdateFlags)
{
  nsRect              wrect;
  nsIRenderingContext *localcx = nsnull;

  if (nsnull == aContext)
  {
    localcx = CreateRenderingContext(*aView);

    //couldn't get rendering context. ack.

    if (nsnull == localcx)
      return;
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
    localcx->SetClipRect(*rect, PR_FALSE);

  localcx->Translate(-mOffset.x, -mOffset.y); 

  nsRect trect = *rect;

  if (aUpdateFlags & NS_VMREFRESH_SCREEN_RECT)
    trect.MoveBy(mOffset.x, mOffset.y);
  else
    localcx->SetClipRect(trect, PR_FALSE);

  paint_all_kids(localcx, aView, &trect);

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
    localcx->CopyOffScreenBits(wrect);

  if (localcx != aContext)
    NS_RELEASE(localcx);

  nsRect  updaterect = *rect;

  //does our dirty rect intersect the rect we just painted?

  if (updaterect.IntersectRect(updaterect, mDirtyRect))
  {
    //does the update rect fully contain the dirty rect?
    //if so, then clear the dirty rect.

    if (updaterect.Contains(mDirtyRect))
      ClearDirtyRegion();
  }
}

void nsViewManager :: Composite()
{
}

void nsViewManager :: UpdateView(nsIView *aView, nsRegion *region, PRUint32 aUpdateFlags)
{
}

void nsViewManager :: UpdateView(nsIView *aView, nsRect *rect, PRUint32 aUpdateFlags)
{
  nsRect  trect = *rect;
  nsIView *par = aView;
  nscoord x, y;

  if (gsDebug)
  {
    printf("ViewManager::UpdateView: %x, rect ", aView);
    stdout << *rect;
    printf("\n");
  }

  do
  {
    //get absolute coordinates of view

    par->GetPosition(&x, &y);

    trect.x += x;
    trect.y += y;
  }
  while (par = par->GetParent());

  if (mDirtyRect.IsEmpty())
    mDirtyRect = trect;
  else
    mDirtyRect.UnionRect(mDirtyRect, trect);

  if ((aUpdateFlags & NS_VMREFRESH_IMMEDIATE) && (nsnull != mContext))
  {
    nsIRenderingContext *cx;
    nsIDeviceContext    *dx = mContext->GetDeviceContext();

    cx = dx->CreateRenderingContext(mRootView);

    if (nsnull != cx)
    {
      Refresh(aView, cx, &mDirtyRect, aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER);
      NS_RELEASE(cx);
    }

    NS_RELEASE(dx);
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
    //XXX this is really dumb but it will at least do something

    parent->InsertChild(child, nsnull);
  }
}

void nsViewManager :: InsertChild(nsIView *parent, nsIView *child, PRInt32 zindex)
{
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  if ((nsnull != parent) && (nsnull != child))
  {
    //XXX this is really dumb but it will at least do something

    parent->InsertChild(child, nsnull);
  }
}

void nsViewManager :: RemoveChild(nsIView *parent, nsIView *child)
{
  parent->RemoveChild(child);
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
	  UpdateView(parent, &oldArea, 0);
	  nsRect newArea(aX, aY, bounds.width, bounds.height); 
	  UpdateView(parent, &newArea, 0);
  }
}

void nsViewManager :: ResizeView(nsIView *aView, nscoord width, nscoord height)
{
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

nsIPresContext * nsViewManager :: GetPresContext()
{
  NS_IF_ADDREF(mContext);
  return mContext;
}

nsDrawingSurface nsViewManager :: GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds)
{
  
  //if ((nsnull == mDrawingSurface) || (mDSBounds != aBounds))
  if((nsnull==mDrawingSurface) || ((mDSBounds.width < aBounds.width)||(mDSBounds.height<aBounds.height)))
  {
    if (nsnull != mDrawingSurface)
    {
      //destroy existing DS
      aContext.DestroyDrawingSurface(mDrawingSurface);
    }

    mDrawingSurface = aContext.CreateDrawingSurface(aBounds);
    mDSBounds = aBounds;
  }

  return mDrawingSurface;
}

void nsViewManager :: ClearDirtyRegion()
{
  mDirtyRect.x = 0;
  mDirtyRect.y = 0;
  mDirtyRect.width = 0;
  mDirtyRect.height = 0;
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

    cx->Translate(ax, ay);

    NS_RELEASE(dx);
    NS_RELEASE(win);
  }

  return cx;
}
