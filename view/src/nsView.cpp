/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsView.h"
#include "nsIWidget.h"
#include "nsViewManager.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsIWidget.h"
#include "nsIButton.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsIComponentManager.h"
#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nsIScrollableView.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsIRegion.h"


//mmptemp

static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);


//#define SHOW_VIEW_BORDERS
//#define HIDE_ALL_WIDGETS

// {34297A07-A8FD-d811-87C6-000244212BCB}
#define VIEW_WRAPPER_IID \
{ 0x34297a07, 0xa8fd, 0xd811, { 0x87, 0xc6, 0x0, 0x2, 0x44, 0x21, 0x2b, 0xcb } };


/**
 * nsISupports-derived helper class that allows to store and get a view
 */
class ViewWrapper : public nsISupports
{
  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(VIEW_WRAPPER_IID)
    NS_DECL_ISUPPORTS

    ViewWrapper(nsView* aView) : mView(aView) {}

    nsView* GetView() { return mView; }
  private:
    nsView* mView;
};

NS_IMPL_ADDREF(ViewWrapper)
NS_IMPL_RELEASE(ViewWrapper)
#ifndef DEBUG
NS_IMPL_QUERY_INTERFACE1(ViewWrapper, ViewWrapper)

#else
NS_IMETHODIMP ViewWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);

  NS_ASSERTION(!aIID.Equals(NS_GET_IID(nsIView)) &&
               !aIID.Equals(NS_GET_IID(nsIScrollableView)),
               "Someone expects a viewwrapper to be a view!");
  
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
  }
  if (aIID.Equals(NS_GET_IID(ViewWrapper))) {
    *aInstancePtr = this;
  }

  if (*aInstancePtr) {
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}
#endif


/**
 * Given a widget, returns the stored ViewWrapper on it, or NULL if no
 * ViewWrapper is there.
 */
static ViewWrapper* GetWrapperFor(nsIWidget* aWidget)
{
  // The widget's client data points back to the owning view
  if (aWidget) {
    void* clientData;
    aWidget->GetClientData(clientData);
    nsISupports* data = (nsISupports*)clientData;
    
    if (data) {
      ViewWrapper* wrapper;
      CallQueryInterface(data, &wrapper);
      // Give a weak reference to the caller. There will still be at least one
      // reference left, since the wrapper was addrefed when set on the widget.
      if (wrapper)
        wrapper->Release();
      return wrapper;
    }
  }
  return nsnull;
}

//
// Main events handler
//
nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{ 
//printf(" %d %d %d (%d,%d) \n", aEvent->widget, aEvent->widgetSupports, 
//       aEvent->message, aEvent->point.x, aEvent->point.y);
  nsEventStatus result = nsEventStatus_eIgnore;
  nsView       *view = nsView::GetViewFor(aEvent->widget);

  if (view)
  {
    view->GetViewManager()->DispatchEvent(aEvent, &result);
  }

  return result;
}

MOZ_DECL_CTOR_COUNTER(nsView)

nsView::nsView()
{
  MOZ_COUNT_CTOR(nsView);

  mVis = nsViewVisibility_kShow;
  // Views should be transparent by default. Not being transparent is
  // a promise that the view will paint all its pixels opaquely. Views
  // should make this promise explicitly by calling
  // SetViewContentTransparency.
  mVFlags = NS_VIEW_FLAG_TRANSPARENT;
  mOpacity = 1.0f;
  mViewManager = nsnull;
  mChildRemoved = PR_FALSE;
}

void nsView::DropMouseGrabbing() {
  // check to see if we are grabbing events
  if (mViewManager->GetMouseEventGrabber() == this) {
    // we are grabbing events. Move the grab to the parent if we can.
    PRBool boolResult; //not used
    // if GetParent() returns null, then we release the grab, which is the best we can do
    mViewManager->GrabMouseEvents(GetParent(), boolResult);
  }
}

nsView::~nsView()
{
  MOZ_COUNT_DTOR(nsView);

  while (GetFirstChild())
  {
    nsView* child = GetFirstChild();
    if (child->GetViewManager() == mViewManager) {
      child->Destroy();
    } else {
      // just unhook it. Someone else will want to destroy this.
      RemoveChild(child);
    }
  }

  DropMouseGrabbing();
  
  if (mViewManager)
  {
    nsView *rootView = mViewManager->GetRootView();
    
    if (rootView)
    {
      // Root views can have parents!
      if (mParent)
      {
        mViewManager->RemoveChild(this);
      }

      if (rootView == this)
      {
        // Inform the view manager that the root view has gone away...
        mViewManager->SetRootView(nsnull);
      }
    }
    else if (mParent)
    {
      mParent->RemoveChild(this);
    }
    
    mViewManager = nsnull;
  }
  else if (mParent)
  {
    mParent->RemoveChild(this);
  }

  if (mZParent)
  {
    mZParent->RemoveReparentedView();
    mZParent->Destroy();
  }

  // Destroy and release the widget
  if (mWindow)
  {
    // Release memory for the view wrapper
    ViewWrapper* wrapper = GetWrapperFor(mWindow);
    NS_IF_RELEASE(wrapper);

    mWindow->SetClientData(nsnull);
    mWindow->Destroy();
    NS_RELEASE(mWindow);
  }
  NS_IF_RELEASE(mDirtyRegion);

  delete mClipRect;
}

nsresult nsView::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  NS_ASSERTION(!aIID.Equals(NS_GET_IID(nsISupports)),
               "Someone expects views to be ISupports-derived!");
  
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsIView))) {
    *aInstancePtr = (void*)(nsIView*)this;
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsView* nsView::GetViewFor(nsIWidget* aWidget)
{           
  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");

  ViewWrapper* wrapper = GetWrapperFor(aWidget);
  if (wrapper)
    return wrapper->GetView();  
  return nsnull;
}

nsresult nsIView::Init(nsIViewManager* aManager,
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
  mViewManager = NS_STATIC_CAST(nsViewManager*, aManager);

  nsView* v = NS_STATIC_CAST(nsView*, this);
  v->SetPosition(aBounds.x, aBounds.y);
  nsRect dim(0, 0, aBounds.width, aBounds.height);
  v->SetDimensions(dim, PR_FALSE);
  v->SetVisibility(aVisibilityFlag);

  // We shouldn't set the parent here. It should be set when we put this view
  // into the view hierarchy.
  v->SetParent(NS_STATIC_CAST(nsView*, NS_CONST_CAST(nsIView*, aParent)));
  return NS_OK;
}

void nsIView::Destroy()
{
  delete this;
}

NS_IMETHODIMP nsView::Paint(nsIRenderingContext& rc, const nsRect& rect,
                              PRUint32 aPaintFlags, PRBool &aResult)
{
    // Just paint, assume compositor knows what it's doing.
    if (nsnull != mClientData) {
      nsCOMPtr<nsIViewObserver> observer;
      if (NS_OK == mViewManager->GetViewObserver(*getter_AddRefs(observer))) {
        observer->Paint((nsIView *)this, rc, rect);
      }
    }
        return NS_OK;
}

NS_IMETHODIMP nsView::Paint(nsIRenderingContext& rc, const nsIRegion& region,
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

void nsView::SetPosition(nscoord aX, nscoord aY)
{
  mDimBounds.x += aX - mPosX;
  mDimBounds.y += aY - mPosY;
  mPosX = aX;
  mPosY = aY;

  NS_ASSERTION(GetParent() || (aX == 0 && aY == 0),
               "Don't try to move the root widget to something non-zero");

  ResetWidgetPosition(PR_TRUE);
}

void nsView::SetPositionIgnoringChildWidgets(nscoord aX, nscoord aY)
{
  mDimBounds.x += aX - mPosX;
  mDimBounds.y += aY - mPosY;
  mPosX = aX;
  mPosY = aY;

  ResetWidgetPosition(PR_FALSE);
}

void nsView::ResetWidgetPosition(PRBool aRecurse) {
  if (mWindow)
  {
    // see if we are caching our widget changes. Yes? 
    // mark us as changed. Later we will actually move the 
    // widget.
    PRBool caching = PR_FALSE;
    mViewManager->IsCachingWidgetChanges(&caching);
    if (caching) {
      mVFlags |= NS_VIEW_FLAG_WIDGET_MOVED;
      return;
    }

    nsIDeviceContext  *dx;
    float             scale;
  
    mViewManager->GetDeviceContext(dx);
    scale = dx->AppUnitsToDevUnits();
    NS_RELEASE(dx);

    nsPoint offset(0, 0);
    if (GetParent()) {
      GetParent()->GetNearestWidget(&offset);
    }

    mWindow->Move(NSTwipsToIntPixels((mDimBounds.x + offset.x), scale),
                  NSTwipsToIntPixels((mDimBounds.y + offset.y), scale));
  } else if (aRecurse) {
    // reposition any widgets under this view
    for (nsView* v = GetFirstChild(); v; v = v->GetNextSibling()) {
      v->ResetWidgetPosition(aRecurse);
    }
  }
}

NS_IMETHODIMP nsView::SynchWidgetSizePosition()
{
  // if the widget was moved or resized
  if (mVFlags & NS_VIEW_FLAG_WIDGET_MOVED || mVFlags & NS_VIEW_FLAG_WIDGET_RESIZED)
  {
    nsIDeviceContext  *dx;
    float             t2p;

    mViewManager->GetDeviceContext(dx);
    t2p = dx->AppUnitsToDevUnits();
    NS_RELEASE(dx);
    // if we just resized do it
    if (mVFlags & NS_VIEW_FLAG_WIDGET_RESIZED) 
    {

      PRInt32 width = NSTwipsToIntPixels(mDimBounds.width, t2p);
      PRInt32 height = NSTwipsToIntPixels(mDimBounds.height, t2p);

      nsRect bounds;
      mWindow->GetBounds(bounds);

      if (bounds.width != width || bounds.height != bounds.height) {
        mWindow->Resize(width,height, PR_TRUE);
      }

      mVFlags &= ~NS_VIEW_FLAG_WIDGET_RESIZED;
    } 
    
    if (mVFlags & NS_VIEW_FLAG_WIDGET_MOVED) {
      // if we just moved do it.
      nsPoint offset;
      GetParent()->GetNearestWidget(&offset);

      PRInt32 x = NSTwipsToIntPixels(mDimBounds.x + offset.x, t2p);
      PRInt32 y = NSTwipsToIntPixels(mDimBounds.y + offset.y, t2p);

      nsRect bounds;
      mWindow->GetBounds(bounds);
      
      if (bounds.x != x || bounds.y != y) {
         mWindow->Move(x,y);
      }

      mVFlags &= ~NS_VIEW_FLAG_WIDGET_MOVED;
    }        
  }
  

  return NS_OK;
}

void nsView::SetDimensions(const nsRect& aRect, PRBool aPaint)
{
  nsRect dims = aRect;
  dims.MoveBy(mPosX, mPosY);

  if (mDimBounds.x == dims.x && mDimBounds.y == dims.y && mDimBounds.width == dims.width
      && mDimBounds.height == dims.height) {
    return;
  }
  
  if (nsnull == mWindow)
  {
    mDimBounds = dims;
  }
  else
  {
    PRBool needToMoveWidget = mDimBounds.x != dims.x || mDimBounds.y != dims.y;

    mDimBounds = dims;

    PRBool caching = PR_FALSE;
    mViewManager->IsCachingWidgetChanges(&caching);
    if (caching) {
      mVFlags |= NS_VIEW_FLAG_WIDGET_RESIZED | (needToMoveWidget ? NS_VIEW_FLAG_WIDGET_MOVED : 0);
      return;
    }

    nsIDeviceContext  *dx;
    float             t2p;
  
    mViewManager->GetDeviceContext(dx);
    t2p = dx->AppUnitsToDevUnits();

    if (needToMoveWidget) {
      NS_ASSERTION(GetParent(), "Don't try to move the root widget, dude");

      nsPoint offset;
      GetParent()->GetNearestWidget(&offset);
    
      mWindow->Move(NSTwipsToIntPixels((mDimBounds.x + offset.x), t2p),
                    NSTwipsToIntPixels((mDimBounds.y + offset.y), t2p));
    }
    mWindow->Resize(NSTwipsToIntPixels(mDimBounds.width, t2p), NSTwipsToIntPixels(mDimBounds.height, t2p),
                    aPaint);

    NS_RELEASE(dx);
  }
}

NS_IMETHODIMP nsView::SetVisibility(nsViewVisibility aVisibility)
{

  mVis = aVisibility;

  if (aVisibility == nsViewVisibility_kHide)
  {
    DropMouseGrabbing();
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

NS_IMETHODIMP nsView::SetFloating(PRBool aFloatingView)
{
	if (aFloatingView)
		mVFlags |= NS_VIEW_FLAG_FLOATING;
	else
		mVFlags &= ~NS_VIEW_FLAG_FLOATING;

#if 0
	// recursively make all sub-views "floating" grr.
	for (nsView* child = mFirstChild; chlid; child = child->GetNextSibling()) {
		child->SetFloating(aFloatingView);
	}
#endif

	return NS_OK;
}

void nsView::InsertChild(nsView *aChild, nsView *aSibling)
{
  NS_PRECONDITION(nsnull != aChild, "null ptr");

  if (nsnull != aChild)
  {
    if (nsnull != aSibling)
    {
#ifdef NS_DEBUG
      NS_ASSERTION(aSibling->GetParent() == this, "tried to insert view with invalid sibling");
#endif
      //insert after sibling
      aChild->SetNextSibling(aSibling->GetNextSibling());
      aSibling->SetNextSibling(aChild);
    }
    else
    {
      aChild->SetNextSibling(mFirstChild);
      mFirstChild = aChild;
    }
    aChild->SetParent(this);
  }
}

void nsView::RemoveChild(nsView *child)
{
  NS_PRECONDITION(nsnull != child, "null ptr");

  if (nsnull != child)
  {
    nsView* prevKid = nsnull;
    nsView* kid = mFirstChild;
    PRBool found = PR_FALSE;
    while (nsnull != kid) {
      if (kid == child) {
        if (nsnull != prevKid) {
          prevKid->SetNextSibling(kid->GetNextSibling());
        } else {
          mFirstChild = kid->GetNextSibling();
        }
        child->SetParent(nsnull);
        found = PR_TRUE;
        break;
      }
      prevKid = kid;
      mChildRemoved = PR_TRUE;
	    kid = kid->GetNextSibling();
    }
    NS_ASSERTION(found, "tried to remove non child");
  }
}

NS_IMETHODIMP nsView::SetOpacity(float opacity)
{
  mOpacity = opacity;
  return NS_OK;
}

NS_IMETHODIMP nsView::SetContentTransparency(PRBool aTransparent)
{
  if (aTransparent == PR_TRUE)
    mVFlags |= NS_VIEW_FLAG_TRANSPARENT;
  else
    mVFlags &= ~NS_VIEW_FLAG_TRANSPARENT;

  return NS_OK;
}

nsresult nsIView::CreateWidget(const nsIID &aWindowIID,
                                   nsWidgetInitData *aWidgetInitData,
                                   nsNativeWidget aNative,
                                   PRBool aEnableDragDrop,
                                   PRBool aResetVisibility,
                                   nsContentType aContentType)
{
  nsIDeviceContext  *dx;
  nsRect            trect = mDimBounds;
  float             scale;

  NS_IF_RELEASE(mWindow);

  mViewManager->GetDeviceContext(dx);
  scale = dx->AppUnitsToDevUnits();

  trect *= scale;

  nsView* v = NS_STATIC_CAST(nsView*, this);
  if (NS_OK == v->LoadWidget(aWindowIID))
  {
    PRBool usewidgets;

    dx->SupportsNativeWidgets(usewidgets);

    if (PR_TRUE == usewidgets)
    {
      PRBool initDataPassedIn = PR_TRUE;
      nsWidgetInitData initData;
      if (!aWidgetInitData) {
        // No initData, we're a child window
        // Create initData to pass in params
        initDataPassedIn = PR_FALSE;
        initData.clipChildren = PR_TRUE; // Clip child window's children
        aWidgetInitData = &initData;
      }
      aWidgetInitData->mContentType = aContentType;

      if (aNative)
        mWindow->Create(aNative, trect, ::HandleEvent, dx, nsnull, nsnull, aWidgetInitData);
      else
      {
        if (!initDataPassedIn && GetParent() && 
          GetParent()->GetViewManager() != mViewManager)
          initData.mListenForResizes = PR_TRUE;

        nsIWidget* parentWidget = GetParent() ? GetParent()->GetNearestWidget(nsnull)
          : nsnull;
        mWindow->Create(parentWidget, trect,
                        ::HandleEvent, dx, nsnull, nsnull, aWidgetInitData);
      }
      if (aEnableDragDrop) {
        mWindow->EnableDragDrop(PR_TRUE);
      }
      
      // propagate the z-index to the widget.
      mWindow->SetZIndex(mZIndex);
    }
  }

  //make sure visibility state is accurate
  
  if (aResetVisibility) {
    v->SetVisibility(GetVisibility());
  }

  NS_RELEASE(dx);

  return NS_OK;
}

void nsView::SetZIndex(PRBool aAuto, PRInt32 aZIndex, PRBool aTopMost)
{
  mVFlags = (mVFlags & ~NS_VIEW_FLAG_AUTO_ZINDEX) | (aAuto ? NS_VIEW_FLAG_AUTO_ZINDEX : 0);
  mZIndex = aZIndex;
  SetTopMost(aTopMost);
  
  if (nsnull != mWindow) {
    mWindow->SetZIndex(aZIndex);
  }
}

NS_IMETHODIMP nsView::SetWidget(nsIWidget *aWidget)
{
  ViewWrapper* wrapper = new ViewWrapper(this);
  if (!wrapper)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(wrapper); // Will be released in ~nsView or upon setting a new widget

  // Destroy any old wrappers if there are any
  ViewWrapper* oldWrapper = GetWrapperFor(aWidget);
  NS_IF_RELEASE(oldWrapper);


  NS_IF_RELEASE(mWindow);
  mWindow = aWidget;

  if (nsnull != mWindow)
  {
    NS_ADDREF(mWindow);
    mWindow->SetClientData(wrapper);
  }

  return NS_OK;
}

//
// internal window creation functions
//
nsresult nsView::LoadWidget(const nsCID &aClassIID)
{
  ViewWrapper* wrapper = new ViewWrapper(this);
  if (!wrapper)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(wrapper); // Will be released in ~nsView

  nsresult rv = CallCreateInstance(aClassIID, &mWindow);

  if (NS_SUCCEEDED(rv)) {
    // Set the widget's client data
    mWindow->SetClientData(wrapper);
  } else {
    delete wrapper;
  }

  return rv;
}

#ifdef DEBUG
void nsIView::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p ", (void*)this);
  if (nsnull != mWindow) {
    nsRect windowBounds;
    nsRect nonclientBounds;
    float p2t;
    nsIDeviceContext *dx;
    mViewManager->GetDeviceContext(dx);
    p2t = dx->DevUnitsToAppUnits();
    NS_RELEASE(dx);
    mWindow->GetClientBounds(windowBounds);
    windowBounds *= p2t;
    mWindow->GetBounds(nonclientBounds);
    nonclientBounds *= p2t;
    nsrefcnt widgetRefCnt = mWindow->AddRef() - 1;
    mWindow->Release();
    fprintf(out, "(widget=%p[%d] pos={%d,%d,%d,%d}) ",
            (void*)mWindow, widgetRefCnt,
            nonclientBounds.x, nonclientBounds.y,
            windowBounds.width, windowBounds.height);
  }
  nsRect brect = GetBounds();
  fprintf(out, "{%d,%d,%d,%d}",
          brect.x, brect.y, brect.width, brect.height);
  const nsView* v = NS_STATIC_CAST(const nsView*, this);
  if (v->IsZPlaceholderView()) {
    fprintf(out, " z-placeholder(%p)",
            (void*)NS_STATIC_CAST(const nsZPlaceholderView*, this)->GetReparentedView());
  }
  if (v->GetZParent()) {
    fprintf(out, " zparent=%p", (void*)v->GetZParent());
  }
  fprintf(out, " z=%d vis=%d opc=%1.3f tran=%d clientData=%p <\n",
          mZIndex, mVis, mOpacity, IsTransparent(), mClientData);
  for (nsView* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    NS_ASSERTION(kid->GetParent() == this, "incorrect parent");
    kid->List(out, aIndent + 1);
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}
#endif // DEBUG

nsIWidget* nsIView::GetNearestWidget(nsPoint* aOffset)
{
  nsPoint pt(0, 0);
  nsView* v;
  for (v = NS_STATIC_CAST(nsView*, this);
       v && !v->HasWidget(); v = v->GetParent()) {
    pt += v->GetPosition();
  }
  if (!v) {
    if (aOffset) {
      *aOffset = pt;
    }
    return NS_STATIC_CAST(nsView*, this)->GetViewManager()->GetWidget();
  }

  // pt is now the offset from v's origin to this's origin
  // The widget's origin is the top left corner of v's bounds, which may
  // not coincide with v's origin
  if (aOffset) {
    nsRect vBounds = v->GetBounds();
    *aOffset = pt + v->GetPosition() -  nsPoint(vBounds.x, vBounds.y);
  }
  return v->GetWidget();
}

nsresult nsView::GetDirtyRegion(nsIRegion*& aRegion)
{
  if (nsnull == mDirtyRegion) {
    nsresult rv = GetViewManager()->CreateRegion(&mDirtyRegion);
    if (NS_FAILED(rv))
      return rv;
  }

  aRegion = mDirtyRegion;
  NS_ADDREF(aRegion);
  return NS_OK;
}

PRBool nsIView::IsRoot() const
{
  NS_ASSERTION(mViewManager != nsnull," View manager is null in nsView::IsRoot()");
  return mViewManager->GetRootView() == this;
}

PRBool nsIView::ExternalIsRoot() const
{
  return nsIView::IsRoot();
}

// Returns PR_TRUE iff we found a placeholder parent edge on the way
static PRBool ApplyClipRect(const nsView* aView, nsRect* aRect, PRBool aFollowPlaceholders)
{
  // this records the offset from the origin of the current aView
  // to the origin of the initial aView
  nsPoint offset(0, 0);
  PRBool lastViewIsFloating = aView->GetFloating();
  PRBool foundPlaceholders = PR_FALSE;
  while (PR_TRUE) {
    const nsView* parentView = aView->GetParent();
    nsPoint offsetFromParent = aView->GetPosition();

    const nsView* zParent = aView->GetZParent();
    if (zParent) {
      foundPlaceholders = PR_TRUE;
      if (aFollowPlaceholders) {
        // correct offsetFromParent to account for the fact that we're
        // switching parentView to ZParent
        // Note tha the common case is that parentView is an ancestor of
        // ZParent.
        const nsView* zParentChain;
        for (zParentChain = zParent;
             zParentChain != parentView && zParentChain;
             zParentChain = zParentChain->GetParent()) {
          offsetFromParent -= zParentChain->GetPosition();
        }
        if (!zParentChain) {
          // uh oh. As we walked up the tree, we missed zParent. This can
          // happen because of odd (but legal) containing block hierarchies.
          // Just compute the required offset from scratch; this is slow,
          // but hopefully rarely executed.
          offsetFromParent = nsViewManager::ComputeViewOffset(aView)
            - nsViewManager::ComputeViewOffset(zParent);
        }
        parentView = zParent;
      }
    }

    if (!parentView) {
      break;
    }

    PRBool parentIsFloating = parentView->GetFloating();
    if (lastViewIsFloating && !parentIsFloating) {
      break;
    }

    // now make offset be the offset from parentView's origin to the initial
    // aView's origin
    offset += offsetFromParent;
    if (parentView->GetClipChildrenToBounds(aFollowPlaceholders)) {
      // Get the parent's clip rect (which is just the dimensions in this
      // case) into the initial aView's coordinates
      nsRect clipRect = parentView->GetDimensions();
      clipRect -= offset;
      PRBool overlap = aRect->IntersectRect(clipRect, *aRect);
      if (!overlap) {
        break;
      }
    }
    const nsRect* r = parentView->GetClipChildrenToRect();
    if (r && !aFollowPlaceholders) {
      // Get the parent's clip rect into the initial aView's coordinates
      nsRect clipRect = *r;
      clipRect -= offset;
      PRBool overlap = aRect->IntersectRect(clipRect, *aRect);
      if (!overlap) {
        break;
      }
    }

    aView = parentView;
    lastViewIsFloating = parentIsFloating;
  }

  return foundPlaceholders;
}

nsRect nsView::GetClippedRect()
{
  nsRect clip = GetDimensions();
  PRBool foundPlaceholders = ApplyClipRect(this, &clip, PR_FALSE);
  if (foundPlaceholders && !clip.IsEmpty()) {
    ApplyClipRect(this, &clip, PR_TRUE);
  }
  return clip;
}
