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

#include "nsView.h"
#include "nsIWidget.h"
#include "nsIViewManager.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsIButton.h"
#include "nsIScrollbar.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsIComponentManager.h"
#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nsIScrollableView.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsIRegion.h"
#include "nsIClipView.h"

static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);

//mmptemp

static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);


//#define SHOW_VIEW_BORDERS
//#define HIDE_ALL_WIDGETS

//
// Main events handler
//
nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{ 
//printf(" %d %d %d (%d,%d) \n", aEvent->widget, aEvent->widgetSupports, 
//       aEvent->message, aEvent->point.x, aEvent->point.y);
  nsEventStatus result = nsEventStatus_eIgnore;
  nsIView       *view = nsView::GetViewFor(aEvent->widget);

  if (nsnull != view)
  {
    nsIViewManager    *vm;

    view->GetViewManager(vm);
    vm->DispatchEvent(aEvent, &result);
    NS_RELEASE(vm);
  }

  return result;
}

MOZ_DECL_CTOR_COUNTER(nsView)

nsView :: nsView()
{
  MOZ_COUNT_CTOR(nsView);

  mVis = nsViewVisibility_kShow;
  mVFlags = 0;
  mOpacity = 1.0f;
  mViewManager = nsnull;
  mCompositorFlags = 0;
}

nsView :: ~nsView()
{
  MOZ_COUNT_DTOR(nsView);

  mVFlags |= NS_VIEW_PUBLIC_FLAG_DYING;

  PRInt32 numKids;
  GetChildCount(numKids);
  if (numKids > 0)
  {
    nsIView *kid;

    //nuke the kids
    do {
      GetChild(0, kid);
      if (nsnull != kid)
        kid->Destroy();
    } while (nsnull != kid);
  }

  if (nsnull != mViewManager)
  {
    nsIView *rootView;
    mViewManager->GetRootView(rootView);
    
    if (nsnull != rootView)
    {
      if (rootView == this)
      {
        // Inform the view manager that the root view has gone away...
        mViewManager->SetRootView(nsnull);
      }
      else
      {
        if (nsnull != mParent)
        {
          mViewManager->RemoveChild(mParent, this);
        }
      }
    } 
    else if (nsnull != mParent)
    {
      mParent->RemoveChild(this);
    }

    nsIView* grabbingView; //check to see if we are capturing!!!
    mViewManager->GetMouseEventGrabber(grabbingView);
    if (grabbingView == this)
    {
      PRBool boolResult;//not used
      mViewManager->GrabMouseEvents(nsnull,boolResult);
    }

    mViewManager = nsnull;
  }
  else if (nsnull != mParent)
  {
    mParent->RemoveChild(this);
  }

  if (nsnull != mZParent)
  {
    mZParent->RemoveChild(this);
    mZParent->Destroy();
  }

  // Destroy and release the widget
  if (nsnull != mWindow)
  {
    mWindow->SetClientData(nsnull);
    mWindow->Destroy();
    NS_RELEASE(mWindow);
  }
  NS_IF_RELEASE(mDirtyRegion);
}

nsresult nsView::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsIView)) || (aIID.Equals(NS_GET_IID(nsISupports)))) {
    *aInstancePtr = (void*)(nsIView*)this;
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsrefcnt nsView::AddRef() 
{
  NS_WARNING("not supported for views");
  return 1;
}

nsrefcnt nsView::Release()
{
  NS_WARNING("not supported for views");
  return 1;
}

nsIView* nsView::GetViewFor(nsIWidget* aWidget)
{           
  nsIView*  view = nsnull;
  void*     clientData;

  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");
	
  // The widget's client data points back to the owning view
  if (aWidget && NS_SUCCEEDED(aWidget->GetClientData(clientData))) {
    view = (nsIView*)clientData;
 
    nsISupports* data = (nsISupports*)clientData;
    
    if (nsnull != data) {
      if (NS_FAILED(data->QueryInterface(NS_GET_IID(nsIView), (void **)&view))) {
        return nsnull;  // return null if client data isn't a view
      }
    }
  }
  return view;
}

NS_IMETHODIMP nsView :: Init(nsIViewManager* aManager,
                             const nsRect &aBounds,
                             const nsIView *aParent,
                             nsViewVisibility aVisibilityFlag)
{
  //printf(" \n callback=%d data=%d", aWidgetCreateCallback, aCallbackData);
  NS_PRECONDITION(nsnull != aManager, "null ptr");
  if (nsnull == aManager) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mViewManager) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  // we don't hold a reference to the view manager
  mViewManager = aManager;

  mChildClip.mLeft = 0;
  mChildClip.mRight = 0;
  mChildClip.mTop = 0;
  mChildClip.mBottom = 0;

  SetBounds(aBounds);

  //temporarily set it...
  SetParent((nsIView *)aParent);

  SetVisibility(aVisibilityFlag);

  // XXX Don't clear this or we hork the scrolling view when creating the clip
  // view's widget. It needs to stay set and later the view manager will reset it
  // when the view is inserted into the view hierarchy...
#if 0
  //clear this again...
  SetParent(nsnull);
#endif

  return NS_OK;
}

NS_IMETHODIMP nsView :: Destroy()
{
  delete this;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetViewManager(nsIViewManager *&aViewMgr) const
{
  NS_IF_ADDREF(mViewManager);
  aViewMgr = mViewManager;
  return NS_OK;
}

NS_IMETHODIMP nsView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                              PRUint32 aPaintFlags, PRBool &aResult)
{
	NS_ASSERTION(aPaintFlags & NS_VIEW_FLAG_JUST_PAINT, "Only simple painting supported by nsView");
    // Just paint, assume compositor knows what it's doing.
    if (nsnull != mClientData) {
      nsCOMPtr<nsIViewObserver> observer;
      if (NS_OK == mViewManager->GetViewObserver(*getter_AddRefs(observer))) {
        observer->Paint((nsIView *)this, rc, rect);
      }
    }
	return NS_OK;
}

NS_IMETHODIMP nsView :: Paint(nsIRenderingContext& rc, const nsIRegion& region,
                              PRUint32 aPaintFlags, PRBool &aResult)
{
  // XXX apply region to rc
  // XXX get bounding rect from region
  //if (nsnull != mClientData)
  //{
  //  nsIViewObserver *obs;
  //
  //  if (NS_OK == mViewManager->GetViewObserver(obs))
  //  {
  //    obs->Paint((nsIView *)this, rc, rect, aPaintFlags);
  //    NS_RELEASE(obs);
  //  }
  //}
  aResult = PR_FALSE;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsView :: HandleEvent(nsGUIEvent *event, PRUint32 aEventFlags,
                                    nsEventStatus* aStatus, PRBool aForceHandle, PRBool& aHandled)
{
  NS_ENSURE_ARG_POINTER(aStatus);

//printf(" %d %d %d %d (%d,%d) \n", this, event->widget, event->widgetSupports, 
//       event->message, event->point.x, event->point.y);

  // Hold a refcount to the observer. The continued existence of the observer will
  // delay deletion of this view hierarchy should the event want to cause its
  // destruction in, say, some JavaScript event handler.
  nsCOMPtr<nsIViewObserver> obs;
  mViewManager->GetViewObserver(*getter_AddRefs(obs));

  // if accessible event pass directly to the view observer
  if (event->eventStructType == NS_ACCESSIBLE_EVENT || event->message == NS_CONTEXTMENU_KEY) {
    if (obs)
       obs->HandleEvent((nsIView *)this, event, aStatus, aForceHandle, aHandled);
    return NS_OK;
  }

  *aStatus = nsEventStatus_eIgnore;

  //see if any of this view's children can process the event
  if ( !(mVFlags & NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN) ) {
    PRInt32 numkids;
    nsRect  trect;
    nscoord x, y;

    GetChildCount(numkids); 
    x = event->point.x;
    y = event->point.y;

    for (PRInt32 cnt = 0; cnt < numkids && !aHandled; cnt++) 
    {
      nsIView *pKid;
      GetChild(cnt, pKid);
      if (!pKid) break;
 
      pKid->GetBounds(trect);

      if (PointIsInside(*pKid, x, y))
      {
        //the x, y position of the event in question
        //is inside this child view, so give it the
        //opportunity to handle the event

        event->point.x -= trect.x;
        event->point.y -= trect.y;

        pKid->HandleEvent(event, NS_VIEW_FLAG_CHECK_CHILDREN, aStatus, PR_FALSE, aHandled);

        event->point.x += trect.x;
        event->point.y += trect.y;
      }
    }
  }

  // if the child handled the event(Ignore) or it handled the event but still wants
  // default behavor(ConsumeDoDefault) and we are visible then pass the event down the view's
  // frame hierarchy. -EDV
  if (!aHandled && mVis == nsViewVisibility_kShow)
  {
    //if no child's bounds matched the event or we consumed but still want
    //default behavior check the view itself. -EDV
    if (nsnull != mClientData && nsnull != obs) {
      obs->HandleEvent((nsIView *)this, event, aStatus, aForceHandle, aHandled);
    }
  } 
  /* XXX Just some debug code to see what event are being thrown away because
     we are not visible. -EDV
  else if (mVis == nsViewVisibility_kHide) {
      nsIFrame* frame = (nsIFrame*)mClientData;
      printf("Throwing away=%d %d %d (%d,%d) \n", this, event->widget, 
              event->message, event->point.x, event->point.y);

  }
  */

  return NS_OK;
}

// XXX Start Temporary fix for Bug #19416
NS_IMETHODIMP nsView :: IgnoreSetPosition(PRBool aShouldIgnore)
{
  mShouldIgnoreSetPosition = aShouldIgnore;
  // resync here
  if (!mShouldIgnoreSetPosition) {
    SetPosition(mBounds.x, mBounds.y);
  }
  return NS_OK;
}
// XXX End Temporary fix for Bug #19416

NS_IMETHODIMP nsView :: SetPosition(nscoord aX, nscoord aY)
{
  nscoord x = aX;
  nscoord y = aY;
  if (IsRoot()) {
    // Add view manager's coordinate offset to the root view
    // This allows the view manager to offset it's coordinate space
    // while allowing layout to assume it's coordinate space origin is (0,0)
    nscoord offsetX;
    nscoord offsetY;
    mViewManager->GetOffset(&offsetX, &offsetY);
    x += offsetX;
    y += offsetY;
  }
  mBounds.MoveTo(x, y);

  // XXX Start Temporary fix for Bug #19416
  if (mShouldIgnoreSetPosition) {
    return NS_OK;
  }
  // XXX End Temporary fix for Bug #19416

  if (nsnull != mWindow)
  {
    // see if we are caching our widget changes. Yes? 
    // mark us as changed. Later we will actually move the 
    // widget.
    PRBool caching = PR_FALSE;
    mViewManager->IsCachingWidgetChanges(&caching);
    if (caching) {
      mVFlags |= NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED;
      return NS_OK;
    }

    nsIDeviceContext  *dx;
    float             scale;
    nsIWidget         *pwidget = nsnull;
    nscoord           parx = 0, pary = 0;
  
    mViewManager->GetDeviceContext(dx);
    dx->GetAppUnitsToDevUnits(scale);
    NS_RELEASE(dx);

    GetOffsetFromWidget(&parx, &pary, pwidget);
    NS_IF_RELEASE(pwidget);
    
    mWindow->Move(NSTwipsToIntPixels((x + parx), scale),
                  NSTwipsToIntPixels((y + pary), scale));
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: SynchWidgetSizePosition()
{
  // if the widget was moved or resized
  if (mVFlags & NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED || mVFlags & NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED)
  {
    nsIDeviceContext  *dx;
    float             t2p;

    mViewManager->GetDeviceContext(dx);
    dx->GetAppUnitsToDevUnits(t2p);
    NS_RELEASE(dx);
#if 0
    /* You would think that doing a move and resize all in one operation would
     * be faster but its not. Something is really broken here. So I'm comenting 
     * this out for now 
     */
    // if we moved and resized do it all in one shot
    if (mVFlags & NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED && mVFlags & NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED)
    {

      nscoord parx = 0, pary = 0;
      nsIWidget         *pwidget = nsnull;

      GetOffsetFromWidget(&parx, &pary, pwidget);
      NS_IF_RELEASE(pwidget);

      PRInt32 x = NSTwipsToIntPixels(mBounds.x + parx, t2p);
      PRInt32 y = NSTwipsToIntPixels(mBounds.y + pary, t2p);
      PRInt32 width = NSTwipsToIntPixels(mBounds.width, t2p);
      PRInt32 height = NSTwipsToIntPixels(mBounds.height, t2p);

      nsRect bounds;
      mWindow->GetBounds(bounds);
      if (bounds.x == x && bounds.y == y ) 
         mVFlags &= ~NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED;
      else if (bounds.width == width && bounds.height == bounds.height)
         mVFlags &= ~NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED;
      else {
         printf("%d) SetBounds(%d,%d,%d,%d)\n", this, x, y, width, height);
         mWindow->Resize(x,y,width,height, PR_TRUE);
         mVFlags &= ~NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED;
         mVFlags &= ~NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED;
         return NS_OK;
      }
    } 
#endif
    // if we just resized do it
    if (mVFlags & NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED) 
    {

      PRInt32 width = NSTwipsToIntPixels(mBounds.width, t2p);
      PRInt32 height = NSTwipsToIntPixels(mBounds.height, t2p);

      nsRect bounds;
      mWindow->GetBounds(bounds);

      if (bounds.width != width || bounds.height != bounds.height) {
#ifdef DEBUG_evaughan
        printf("%d) Resize(%d,%d)\n", this, width, height);
#endif
        mWindow->Resize(width,height, PR_TRUE);
      }

      mVFlags &= ~NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED;
    } 
    
    if (mVFlags & NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED) {
      // if we just moved do it.
      nscoord parx = 0, pary = 0;
      nsIWidget         *pwidget = nsnull;

      GetOffsetFromWidget(&parx, &pary, pwidget);
      NS_IF_RELEASE(pwidget);

      PRInt32 x = NSTwipsToIntPixels(mBounds.x + parx, t2p);
      PRInt32 y = NSTwipsToIntPixels(mBounds.y + pary, t2p);

      nsRect bounds;
      mWindow->GetBounds(bounds);
      
      if (bounds.x != x || bounds.y != y) {
#ifdef DEBUG_evaughan
         printf("%d) Move(%d,%d)\n", this, x, y);
#endif
         mWindow->Move(x,y);
      }

      mVFlags &= ~NS_VIEW_PUBLIC_FLAG_WIDGET_MOVED;
    }        
  }
  

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetPosition(nscoord *x, nscoord *y) const
{

  nsIView *rootView;

  mViewManager->GetRootView(rootView);

  if (this == ((const nsView*)rootView))
    *x = *y = 0;
  else
  {

    *x = mBounds.x;
    *y = mBounds.y;

  }


  return NS_OK;
}

NS_IMETHODIMP nsView :: SetDimensions(nscoord width, nscoord height, PRBool aPaint)
{
  if ((mBounds.width == width) &&
      (mBounds.height == height))
    return NS_OK;
  
  mBounds.SizeTo(width, height);

#if 0
  if (nsnull != mParent)
  {
    nsIScrollableView *scroller;

    static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

    // XXX The scrolled view is a child of the clip view which is a child of
    // the scrolling view. It's kind of yucky the way this works. A parent
    // notification that the child's size changed would be cleaner.
    nsIView *grandParent;
    mParent->GetParent(grandParent);
    if ((nsnull != grandParent) &&
        (NS_OK == grandParent->QueryInterface(kscroller, (void **)&scroller)))
    {
      scroller->ComputeContainerSize();
    }
  }
#endif

  if (nsnull != mWindow)
  {
    PRBool caching = PR_FALSE;
    mViewManager->IsCachingWidgetChanges(&caching);
    if (caching) {
      mVFlags |= NS_VIEW_PUBLIC_FLAG_WIDGET_RESIZED;
      return NS_OK;
    }

    nsIDeviceContext  *dx;
    float             t2p;
  
    mViewManager->GetDeviceContext(dx);
    dx->GetAppUnitsToDevUnits(t2p);

    mWindow->Resize(NSTwipsToIntPixels(width, t2p), NSTwipsToIntPixels(height, t2p),
                    aPaint);

    NS_RELEASE(dx);
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetDimensions(nscoord *width, nscoord *height) const
{
  *width = mBounds.width;
  *height = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetBounds(const nsRect &aBounds, PRBool aPaint)
{
  SetPosition(aBounds.x, aBounds.y);
  SetDimensions(aBounds.width, aBounds.height, aPaint);
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, PRBool aPaint)
{
  SetPosition(aX, aY);
  SetDimensions(aWidth, aHeight, aPaint);
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetBounds(nsRect &aBounds) const
{
  nsIView *rootView = nsnull;

  NS_ASSERTION(mViewManager, "mViewManager is null!");
  if (!mViewManager) {
    aBounds.x = aBounds.y = 0;
    return NS_ERROR_FAILURE;
  }

  mViewManager->GetRootView(rootView);
  aBounds = mBounds;

  if ((nsIView *)this == rootView)
    aBounds.x = aBounds.y = 0;

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetChildClip(nscoord aLeft, nscoord aTop, nscoord aRight, nscoord aBottom)
{
  NS_PRECONDITION(aLeft <= aRight && aTop <= aBottom, "bad clip values");
  mChildClip.mLeft = aLeft;
  mChildClip.mTop = aTop;
  mChildClip.mRight = aRight;
  mChildClip.mBottom = aBottom;

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetChildClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom) const
{
  *aLeft = mChildClip.mLeft;
  *aTop = mChildClip.mTop;
  *aRight = mChildClip.mRight;
  *aBottom = mChildClip.mBottom; 
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetVisibility(nsViewVisibility aVisibility)
{

  mVis = aVisibility;

  if (aVisibility == nsViewVisibility_kHide)
  {
    nsIView* grabbingView; //check to see if we are grabbing events
    mViewManager->GetMouseEventGrabber(grabbingView);
    if (grabbingView == this)
    {
      //if yes then we must release them before we become hidden and can't get them
      PRBool boolResult;//not used
      mViewManager->GrabMouseEvents(nsnull,boolResult);
    }
  }

  if (nsnull != mWindow)
  {
#ifndef HIDE_ALL_WIDGETS
    if (mVis == nsViewVisibility_kShow)
      mWindow->Show(PR_TRUE);
    else
#endif
      mWindow->Show(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetVisibility(nsViewVisibility &aVisibility) const
{
  aVisibility = mVis;
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetZParent(nsIView *aZParent)
{
  mZParent = aZParent;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetZParent(nsIView *&aZParent) const
{
  aZParent = mZParent;
  return NS_OK;
}

NS_IMETHODIMP nsView::SetZIndex(PRInt32 aZIndex)
{
	mZindex = aZIndex;

	if (nsnull != mWindow) {
		mWindow->SetZIndex(aZIndex);
	}

	return NS_OK;
}

NS_IMETHODIMP nsView::GetZIndex(PRInt32 &aZIndex) const
{
  aZIndex = mZindex;
  return NS_OK;
}

NS_IMETHODIMP nsView::SetAutoZIndex(PRBool aAutoZIndex)
{
	if (aAutoZIndex)
		mVFlags |= NS_VIEW_PUBLIC_FLAG_AUTO_ZINDEX;
	else
		mVFlags &= ~NS_VIEW_PUBLIC_FLAG_AUTO_ZINDEX;
	return NS_OK;
}

NS_IMETHODIMP nsView::GetAutoZIndex(PRBool &aAutoZIndex) const
{
	aAutoZIndex = ((mVFlags & NS_VIEW_PUBLIC_FLAG_AUTO_ZINDEX) != 0);
	return NS_OK;
}


NS_IMETHODIMP nsView::SetFloating(PRBool aFloatingView)
{
	if (aFloatingView)
		mVFlags |= NS_VIEW_PUBLIC_FLAG_FLOATING;
	else
		mVFlags &= ~NS_VIEW_PUBLIC_FLAG_FLOATING;

#if 0
	// recursively make all sub-views "floating" grr.
	nsIView *child = mFirstChild;
	while (nsnull != child) {
		child->SetFloating(aFloatingView);
		child->GetNextSibling(child);
	}
#endif

	return NS_OK;
}

NS_IMETHODIMP nsView::GetFloating(PRBool &aFloatingView) const
{
	aFloatingView = ((mVFlags & NS_VIEW_PUBLIC_FLAG_FLOATING) != 0);
	return NS_OK;
}

NS_IMETHODIMP nsView :: SetParent(nsIView *aParent)
{
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetParent(nsIView *&aParent) const
{
  aParent = mParent;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetNextSibling(nsIView *&aNextSibling) const
{
  aNextSibling = mNextSibling;
  return NS_OK;
}

NS_IMETHODIMP nsView::SetNextSibling(nsIView* aView)
{
  mNextSibling = aView;
  return NS_OK;
}

NS_IMETHODIMP nsView :: InsertChild(nsIView *child, nsIView *sibling)
{
  NS_PRECONDITION(nsnull != child, "null ptr");
  if (nsnull != child)
  {
    if (nsnull != sibling)
    {
#ifdef NS_DEBUG
      nsIView*  siblingParent;
      sibling->GetParent(siblingParent);
      NS_ASSERTION(siblingParent == this, "tried to insert view with invalid sibling");
#endif
      //insert after sibling
      nsIView*  siblingNextSibling;
      sibling->GetNextSibling(siblingNextSibling);
      child->SetNextSibling(siblingNextSibling);
      sibling->SetNextSibling(child);
    }
    else
    {
      child->SetNextSibling(mFirstChild);
      mFirstChild = child;
    }
    child->SetParent(this);
    mNumKids++;
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: RemoveChild(nsIView *child)
{
  NS_PRECONDITION(nsnull != child, "null ptr");

  if (nsnull != child)
  {
    nsIView* prevKid = nsnull;
    nsIView* kid = mFirstChild;
    PRBool found = PR_FALSE;
    while (nsnull != kid) {
      if (kid == child) {
        if (nsnull != prevKid) {
          nsIView*  kidNextSibling;
          kid->GetNextSibling(kidNextSibling);
          prevKid->SetNextSibling(kidNextSibling);
        } else {
          kid->GetNextSibling(mFirstChild);
        }
        child->SetParent(nsnull);
        mNumKids--;
        found = PR_TRUE;
        break;
      }
      prevKid = kid;
	    kid->GetNextSibling(kid);
    }
    NS_ASSERTION(found, "tried to remove non child");
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetChildCount(PRInt32 &aCount) const
{
  aCount = mNumKids;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetChild(PRInt32 index, nsIView *&aChild) const
{ 
  NS_PRECONDITION(!(index > mNumKids), "bad index");

  aChild = nsnull;
  if (index < mNumKids)
  {
    aChild = mFirstChild;
    for (PRInt32 cnt = 0; (cnt < index) && (nsnull != aChild); cnt++) {
      aChild->GetNextSibling(aChild);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetOpacity(float opacity)
{
  mOpacity = opacity;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetOpacity(float &aOpacity) const
{
  aOpacity = mOpacity;
  return NS_OK;
}

NS_IMETHODIMP nsView :: HasTransparency(PRBool &aTransparent) const
{
  aTransparent = (mVFlags & NS_VIEW_PUBLIC_FLAG_TRANSPARENT) ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetContentTransparency(PRBool aTransparent)
{
  if (aTransparent == PR_TRUE)
    mVFlags |= NS_VIEW_PUBLIC_FLAG_TRANSPARENT;
  else
    mVFlags &= ~NS_VIEW_PUBLIC_FLAG_TRANSPARENT;

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetClientData(void *aData)
{
  mClientData = aData;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetClientData(void *&aData) const
{
  aData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsView :: CreateWidget(const nsIID &aWindowIID,
                                     nsWidgetInitData *aWidgetInitData,
                                     nsNativeWidget aNative,
                                     PRBool aEnableDragDrop)
{
  nsIDeviceContext  *dx;
  nsRect            trect = mBounds;
  float             scale;

  NS_IF_RELEASE(mWindow);

  mViewManager->GetDeviceContext(dx);
  dx->GetAppUnitsToDevUnits(scale);

  trect *= scale;

  if (NS_OK == LoadWidget(aWindowIID))
  {
    PRBool usewidgets;

    dx->SupportsNativeWidgets(usewidgets);

    if (PR_TRUE == usewidgets)
    {
      if (aNative)
        mWindow->Create(aNative, trect, ::HandleEvent, dx, nsnull, nsnull, aWidgetInitData);
      else
      {
        nsIWidget *parent;
        GetOffsetFromWidget(nsnull, nsnull, parent);
        mWindow->Create(parent, trect, ::HandleEvent, dx, nsnull, nsnull, aWidgetInitData);
        NS_IF_RELEASE(parent);
      }
      if (aEnableDragDrop) {
        mWindow->EnableDragDrop(PR_TRUE);
      }
      
      // propagate the z-index to the widget.
      mWindow->SetZIndex(mZindex);
    }
  }

  //make sure visibility state is accurate

  nsViewVisibility vis;

  GetVisibility(vis);
  SetVisibility(vis);

  NS_RELEASE(dx);

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetWidget(nsIWidget *aWidget)
{
  NS_IF_RELEASE(mWindow);
  mWindow = aWidget;

  if (nsnull != mWindow)
  {
    NS_ADDREF(mWindow);
    mWindow->SetClientData((void *)this);
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetWidget(nsIWidget *&aWidget) const
{
  NS_IF_ADDREF(mWindow);
  aWidget = mWindow;
  return NS_OK;
}

NS_IMETHODIMP nsView::HasWidget(PRBool *aHasWidget) const
{
	*aHasWidget = (mWindow != nsnull);
	return NS_OK;
}

//
// internal window creation functions
//
nsresult nsView :: LoadWidget(const nsCID &aClassIID)
{
  nsresult rv = nsComponentManager::CreateInstance(aClassIID, nsnull, NS_GET_IID(nsIWidget), (void**)&mWindow);

  if (NS_OK == rv) {
    // Set the widget's client data
    mWindow->SetClientData((void*)this);
  }

  return rv;
}

NS_IMETHODIMP nsView::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p ", this);
  if (nsnull != mWindow) {
    nsRect windowBounds;
    nsRect nonclientBounds;
    float p2t;
    nsIDeviceContext *dx;
    mViewManager->GetDeviceContext(dx);
    dx->GetDevUnitsToAppUnits(p2t);
    NS_RELEASE(dx);
    mWindow->GetClientBounds(windowBounds);
    windowBounds *= p2t;
    mWindow->GetBounds(nonclientBounds);
    nonclientBounds *= p2t;
    nsrefcnt widgetRefCnt = mWindow->AddRef() - 1;
    mWindow->Release();
    fprintf(out, "(widget=%p[%d] pos={%d,%d,%d,%d}) ",
            mWindow, widgetRefCnt,
            nonclientBounds.x, nonclientBounds.y,
            windowBounds.width, windowBounds.height);
  }
  nsRect brect;
  GetBounds(brect);
  fprintf(out, "{%d,%d,%d,%d}",
          brect.x, brect.y, brect.width, brect.height);
  PRBool  hasTransparency;
  HasTransparency(hasTransparency);
  fprintf(out, " z=%d vis=%d opc=%1.3f tran=%d clientData=%p <\n", mZindex, mVis, mOpacity, hasTransparency, mClientData);
  nsIView* kid = mFirstChild;
  while (nsnull != kid) {
    kid->List(out, aIndent + 1);
    kid->GetNextSibling(kid);
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
  
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetViewFlags(PRUint32 aFlags)
{
  mVFlags |= aFlags;
  return NS_OK;
}

NS_IMETHODIMP nsView :: ClearViewFlags(PRUint32 aFlags)
{
  mVFlags &= ~aFlags;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetViewFlags(PRUint32 *aFlags) const
{
  *aFlags = mVFlags;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget)
{
  nsIView   *ancestor;
  aWidget = nsnull;
  
  // XXX aDx and aDy are OUT parameters and so we should initialize them
  // to 0 rather than relying on the caller to do so...
  GetParent(ancestor);
  while (nsnull != ancestor)
  {
    ancestor->GetWidget(aWidget);
	  if (nsnull != aWidget)
	    return NS_OK;

    if ((nsnull != aDx) && (nsnull != aDy))
    {
      nscoord offx, offy;

      ancestor->GetPosition(&offx, &offy);

      *aDx += offx;
      *aDy += offy;
    }

	  ancestor->GetParent(ancestor);
  }

  
  if (nsnull == aWidget) {
       // The root view doesn't have a widget
       // but maybe the view manager does.
    nsCOMPtr<nsIViewManager> vm;
    GetViewManager(*getter_AddRefs(vm));
    vm->GetWidget(&aWidget);
  }

  return NS_OK;
}

NS_IMETHODIMP nsView::GetDirtyRegion(nsIRegion *&aRegion) const
{
	if (nsnull == mDirtyRegion) {
		// The view doesn't have a dirty region so create one
		nsresult rv = nsComponentManager::CreateInstance(kRegionCID, 
		                               nsnull, 
		                               NS_GET_IID(nsIRegion), 
		                               (void**) &mDirtyRegion);

		if (NS_FAILED(rv))
			return rv;
		
		rv = mDirtyRegion->Init();
		if (NS_FAILED(rv))
			return rv;
	}

	aRegion = mDirtyRegion;
	NS_ADDREF(aRegion);
	
	return NS_OK;
}


NS_IMETHODIMP nsView::SetCompositorFlags(PRUint32 aFlags)
{
	mCompositorFlags = aFlags;
	return NS_OK;
}

NS_IMETHODIMP nsView::GetCompositorFlags(PRUint32 *aFlags)
{
	NS_ASSERTION((aFlags != nsnull), "no flags");
	*aFlags = mCompositorFlags;
	return NS_OK;
}

static void calc_extents(nsIView *view, nsRect *extents, nscoord ox, nscoord oy)
{
  nsIView     *kid;
  nsRect      bounds;
  nsIClipView *cview;

  view->GetChild(0, kid);
  while (nsnull != kid)
  {
    kid->GetBounds(bounds);

    bounds.x += ox;
    bounds.y += oy;

    extents->UnionRect(*extents, bounds);

    cview = nsnull;

    kid->QueryInterface(NS_GET_IID(nsIClipView), (void **)&cview);

    if (!cview)
      calc_extents(kid, extents, bounds.x, bounds.y);

    kid->GetNextSibling(kid);
  }
}

NS_IMETHODIMP nsView :: GetExtents(nsRect *aExtents)
{
  GetBounds(*aExtents);

  aExtents->x = 0;
  aExtents->y = 0;

  calc_extents(this, aExtents, 0, 0);

  return NS_OK;
}

PRBool nsView :: IsRoot()
{
nsIView *rootView;

  NS_ASSERTION(mViewManager != nsnull," View manager is null in nsView::IsRoot()");
  mViewManager->GetRootView(rootView);
  if (rootView == this) {
   return PR_TRUE;
  }
  
  return PR_FALSE;
}

PRBool nsView::PointIsInside(nsIView& aView, nscoord x, nscoord y) const
{
  nsRect clippedRect;
  PRBool empty;
  PRBool clipped;
  aView.GetClippedRect(clippedRect, clipped, empty);
  if (PR_TRUE == empty) {
    // Rect is completely clipped out so point can not
    // be inside it.
    return PR_FALSE;
  }

  // Check to see if the point is within the clipped rect.
  if (clippedRect.Contains(x, y)) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  } 
}


NS_IMETHODIMP nsView::GetClippedRect(nsRect& aClippedRect, PRBool& aIsClipped, PRBool& aEmpty) const
{
  // Keep track of the view's offset
  // from its ancestor.
  nscoord ancestorX = 0;
  nscoord ancestorY = 0;

  aEmpty = PR_FALSE;
  aIsClipped = PR_FALSE;
  
  GetBounds(aClippedRect);
  nsIView* parentView;
  GetParent(parentView);

  // Walk all of the way up the views to see if any
  // ancestor sets the NS_VIEW_PUBLIC_FLAG_CLIPCHILDREN
  while (parentView) {  
     PRUint32 flags;
     parentView->GetViewFlags(&flags);
     if (flags & NS_VIEW_PUBLIC_FLAG_CLIPCHILDREN) {
      aIsClipped = PR_TRUE;
      // Adjust for clip specified by ancestor
      nscoord clipLeft;
      nscoord clipTop;
      nscoord clipRight;
      nscoord clipBottom;
      parentView->GetChildClip(&clipLeft, &clipTop, &clipRight, &clipBottom);
      nsRect clipRect;
      //Offset the cliprect by the amount the child offsets from the parent
      clipRect.x = clipLeft + ancestorX;
      clipRect.y = clipTop + ancestorY;
      clipRect.width = clipRight - clipLeft;
      clipRect.height = clipBottom - clipTop;
      PRBool overlap = aClippedRect.IntersectRect(clipRect, aClippedRect);
      if (!overlap) {
        aEmpty = PR_TRUE; // Does not intersect so the rect is empty.
        return NS_OK;
      }
    }

    nsRect bounds;
    parentView->GetBounds(bounds);
    ancestorX -= bounds.x;
    ancestorY -= bounds.y;

    parentView->GetParent(parentView);
  }
 
  return NS_OK;
}


