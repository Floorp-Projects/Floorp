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
#include "nsWidgetsCID.h"
#include "nsViewManager.h"
#include "nsGUIEvent.h"
#include "nsIComponentManager.h"
#include "nsGfxCIID.h"
#include "nsIInterfaceRequestor.h"

//mmptemp

static nsEventStatus HandleEvent(nsGUIEvent *aEvent);


//#define SHOW_VIEW_BORDERS

#define VIEW_WRAPPER_IID \
  { 0xbf4e1841, 0xe9ec, 0x47f2, \
    { 0xb4, 0x77, 0x0f, 0xf6, 0x0f, 0x5a, 0xac, 0xbd } }

/**
 * nsISupports-derived helper class that allows to store and get a view
 */
class ViewWrapper : public nsIInterfaceRequestor
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(VIEW_WRAPPER_IID)
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEREQUESTOR

    ViewWrapper(nsView* aView) : mView(aView) {}

    nsView* GetView() { return mView; }
  private:
    nsView* mView;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ViewWrapper, VIEW_WRAPPER_IID)

NS_IMPL_ADDREF(ViewWrapper)
NS_IMPL_RELEASE(ViewWrapper)
#ifndef DEBUG
NS_IMPL_QUERY_INTERFACE2(ViewWrapper, ViewWrapper, nsIInterfaceRequestor)

#else
NS_IMETHODIMP ViewWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);

  NS_ASSERTION(!aIID.Equals(NS_GET_IID(nsIView)),
               "Someone expects a viewwrapper to be a view!");
  
  *aInstancePtr = nsnull;
  
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = static_cast<nsISupports*>(this);
  }
  else if (aIID.Equals(NS_GET_IID(ViewWrapper))) {
    *aInstancePtr = this;
  }
  else if (aIID.Equals(NS_GET_IID(nsIInterfaceRequestor))) {
    *aInstancePtr = this;
  }


  if (*aInstancePtr) {
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}
#endif

NS_IMETHODIMP ViewWrapper::GetInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsIView))) {
    *aInstancePtr = mView;
    return NS_OK;
  }
  return QueryInterface(aIID, aInstancePtr);
}

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

// Main events handler
static nsEventStatus HandleEvent(nsGUIEvent *aEvent)
{
#if 0
  printf(" %d %d %d (%d,%d) \n", aEvent->widget, aEvent->widgetSupports, 
         aEvent->message, aEvent->point.x, aEvent->point.y);
#endif
  nsEventStatus result = nsEventStatus_eIgnore;
  nsView *view = nsView::GetViewFor(aEvent->widget);

  if (view)
  {
    nsCOMPtr<nsIViewManager> vm = view->GetViewManager();
    vm->DispatchEvent(aEvent, view, &result);
  }

  return result;
}

// Attached widget event helpers
static ViewWrapper* GetAttachedWrapperFor(nsIWidget* aWidget)
{
  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");
  return aWidget->GetAttachedViewPtr();
}

static nsView* GetAttachedViewFor(nsIWidget* aWidget)
{           
  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");

  ViewWrapper* wrapper = GetAttachedWrapperFor(aWidget);
  if (!wrapper)
    return nsnull;
  return wrapper->GetView();
}

// event handler
static nsEventStatus AttachedHandleEvent(nsGUIEvent *aEvent)
{ 
  nsEventStatus result = nsEventStatus_eIgnore;
  nsView *view = GetAttachedViewFor(aEvent->widget);

  if (view)
  {
    nsCOMPtr<nsIViewManager> vm = view->GetViewManager();
    vm->DispatchEvent(aEvent, view, &result);
  }

  return result;
}

nsView::nsView(nsViewManager* aViewManager, nsViewVisibility aVisibility)
{
  MOZ_COUNT_CTOR(nsView);

  mVis = aVisibility;
  // Views should be transparent by default. Not being transparent is
  // a promise that the view will paint all its pixels opaquely. Views
  // should make this promise explicitly by calling
  // SetViewContentTransparency.
  mVFlags = 0;
  mViewManager = aViewManager;
  mDirtyRegion = nsnull;
  mDeletionObserver = nsnull;
  mHaveInvalidationDimensions = false;
  mWidgetIsTopLevel = false;
}

void nsView::DropMouseGrabbing()
{
  nsIPresShell* presShell = mViewManager->GetPresShell();
  if (presShell)
    presShell->ClearMouseCaptureOnView(this);
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

  if (mViewManager)
  {
    DropMouseGrabbing();
  
    nsView *rootView = mViewManager->GetRootViewImpl();
    
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

  // Destroy and release the widget
  DestroyWidget();

  delete mDirtyRegion;

  if (mDeletionObserver) {
    mDeletionObserver->Clear();
  }
}

void nsView::DestroyWidget()
{
  if (mWindow)
  {
    // Release memory for the view wrapper
    ViewWrapper* wrapper = GetWrapperFor(mWindow);
    NS_IF_RELEASE(wrapper);

    // If we are not attached to a base window, we're going to tear down our
    // widget here. However, if we're attached to somebody elses widget, we
    // want to leave the widget alone: don't reset the client data or call
    // Destroy. Just clear our event view ptr and free our reference to it. 
    if (mWidgetIsTopLevel) {
      ViewWrapper* wrapper = GetAttachedWrapperFor(mWindow);
      NS_IF_RELEASE(wrapper);

      mWindow->SetAttachedViewPtr(nsnull);
    }
    else {
      mWindow->SetClientData(nsnull);
      mWindow->Destroy();
    }

    NS_RELEASE(mWindow);
  }
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

nsIView* nsIView::GetViewFor(nsIWidget* aWidget)
{           
  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");

  ViewWrapper* wrapper = GetWrapperFor(aWidget);

  if (!wrapper)
    wrapper = GetAttachedWrapperFor(aWidget);

  if (wrapper)
    return wrapper->GetView();

  return nsnull;
}

void nsIView::Destroy()
{
  delete this;
}

void nsView::SetPosition(nscoord aX, nscoord aY)
{
  mDimBounds.x += aX - mPosX;
  mDimBounds.y += aY - mPosY;
  mPosX = aX;
  mPosY = aY;

  NS_ASSERTION(GetParent() || (aX == 0 && aY == 0),
               "Don't try to move the root widget to something non-zero");

  ResetWidgetBounds(true, true, false);
}

void nsIView::SetInvalidationDimensions(const nsRect* aRect)
{
  return Impl()->SetInvalidationDimensions(aRect);
}

void nsView::ResetWidgetBounds(bool aRecurse, bool aMoveOnly,
                               bool aInvalidateChangedSize) {
  if (mWindow) {
    // If our view manager has refresh disabled, then do nothing; the view
    // manager will set our position when refresh is reenabled.  Just let it
    // know that it has pending updates.
    if (!mViewManager->IsRefreshEnabled()) {
      mViewManager->PostPendingUpdate();
      return;
    }

    DoResetWidgetBounds(aMoveOnly, aInvalidateChangedSize);
  } else if (aRecurse) {
    // reposition any widgets under this view
    for (nsView* v = GetFirstChild(); v; v = v->GetNextSibling()) {
      v->ResetWidgetBounds(true, aMoveOnly, aInvalidateChangedSize);
    }
  }
}

bool nsIView::IsEffectivelyVisible()
{
  for (nsIView* v = this; v; v = v->mParent) {
    if (v->GetVisibility() == nsViewVisibility_kHide)
      return false;
  }
  return true;
}

nsIntRect nsIView::CalcWidgetBounds(nsWindowType aType)
{
  PRInt32 p2a = mViewManager->AppUnitsPerDevPixel();

  nsRect viewBounds(mDimBounds);

  nsView* parent = GetParent()->Impl();
  if (parent) {
    nsPoint offset;
    nsIWidget* parentWidget = parent->GetNearestWidget(&offset, p2a);
    // make viewBounds be relative to the parent widget, in appunits
    viewBounds += offset;

    if (parentWidget && aType == eWindowType_popup &&
        IsEffectivelyVisible()) {
      // put offset into screen coordinates. (based on client area origin)
      nsIntPoint screenPoint = parentWidget->WidgetToScreenOffset();
      viewBounds += nsPoint(NSIntPixelsToAppUnits(screenPoint.x, p2a),
                            NSIntPixelsToAppUnits(screenPoint.y, p2a));
    }
  }

  // Compute widget bounds in device pixels
  nsIntRect newBounds = viewBounds.ToNearestPixels(p2a);

  // Compute where the top-left of our widget ended up relative to the parent
  // widget, in appunits.
  nsPoint roundedOffset(NSIntPixelsToAppUnits(newBounds.x, p2a),
                        NSIntPixelsToAppUnits(newBounds.y, p2a));

  // mViewToWidgetOffset is added to coordinates relative to the view origin
  // to get coordinates relative to the widget.
  // The view origin, relative to the parent widget, is at
  // (mPosX,mPosY) - mDimBounds.TopLeft() + viewBounds.TopLeft().
  // Our widget, relative to the parent widget, is roundedOffset.
  mViewToWidgetOffset = nsPoint(mPosX, mPosY)
    - mDimBounds.TopLeft() + viewBounds.TopLeft() - roundedOffset;

  return newBounds;
}

void nsView::DoResetWidgetBounds(bool aMoveOnly,
                                 bool aInvalidateChangedSize) {
  // The geometry of a root view's widget is controlled externally,
  // NOT by sizing or positioning the view
  if (mViewManager->GetRootViewImpl() == this) {
    return;
  }
  
  nsIntRect curBounds;
  mWindow->GetBounds(curBounds);

  nsWindowType type;
  mWindow->GetWindowType(type);

  if (curBounds.IsEmpty() && mDimBounds.IsEmpty() && type == eWindowType_popup) {
    // Don't manipulate empty popup widgets. For example there's no point
    // moving hidden comboboxes around, or doing X server roundtrips
    // to compute their true screen position. This could mean that WidgetToScreen
    // operations on these widgets don't return up-to-date values, but popup
    // positions aren't reliable anyway because of correction to be on or off-screen.
    return;
  }

  NS_PRECONDITION(mWindow, "Why was this called??");

  nsIntRect newBounds = CalcWidgetBounds(type);

  bool changedPos = curBounds.TopLeft() != newBounds.TopLeft();
  bool changedSize = curBounds.Size() != newBounds.Size();

  // Child views are never attached to top level widgets, this is safe.
  if (changedPos) {
    if (changedSize && !aMoveOnly) {
      mWindow->Resize(newBounds.x, newBounds.y, newBounds.width, newBounds.height,
                      aInvalidateChangedSize);
    } else {
      mWindow->Move(newBounds.x, newBounds.y);
    }
  } else {
    if (changedSize && !aMoveOnly) {
      mWindow->Resize(newBounds.width, newBounds.height, aInvalidateChangedSize);
    } // else do nothing!
  }
}

void nsView::SetDimensions(const nsRect& aRect, bool aPaint, bool aResizeWidget)
{
  nsRect dims = aRect;
  dims.MoveBy(mPosX, mPosY);

  // Don't use nsRect's operator== here, since it returns true when
  // both rects are empty even if they have different widths and we
  // have cases where that sort of thing matters to us.
  if (mDimBounds.TopLeft() == dims.TopLeft() &&
      mDimBounds.Size() == dims.Size()) {
    return;
  }

  mDimBounds = dims;

  if (aResizeWidget) {
    ResetWidgetBounds(false, false, aPaint);
  }
}

void nsView::SetInvalidationDimensions(const nsRect* aRect)
{
  if ((mHaveInvalidationDimensions = !!aRect)) {
    mInvalidationDimensions = *aRect;
  }
}

void nsView::NotifyEffectiveVisibilityChanged(bool aEffectivelyVisible)
{
  if (!aEffectivelyVisible)
  {
    DropMouseGrabbing();
  }

  if (nsnull != mWindow)
  {
    if (aEffectivelyVisible)
    {
      DoResetWidgetBounds(false, true);
      mWindow->Show(true);
    }
    else
      mWindow->Show(false);
  }

  for (nsView* child = mFirstChild; child; child = child->mNextSibling) {
    if (child->mVis == nsViewVisibility_kHide) {
      // It was effectively hidden and still is
      continue;
    }
    // Our child is visible if we are
    child->NotifyEffectiveVisibilityChanged(aEffectivelyVisible);
  }
}

NS_IMETHODIMP nsView::SetVisibility(nsViewVisibility aVisibility)
{
  mVis = aVisibility;
  NotifyEffectiveVisibilityChanged(IsEffectivelyVisible());
  return NS_OK;
}

NS_IMETHODIMP nsView::SetFloating(bool aFloatingView)
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

void nsView::InvalidateHierarchy(nsViewManager *aViewManagerParent)
{
  if (mViewManager->GetRootViewImpl() == this)
    mViewManager->InvalidateHierarchy();

  for (nsView *child = mFirstChild; child; child = child->GetNextSibling())
    child->InvalidateHierarchy(aViewManagerParent);
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

    // If we just inserted a root view, then update the RootViewManager
    // on all view managers in the new subtree.

    nsViewManager *vm = aChild->GetViewManager();
    if (vm->GetRootViewImpl() == aChild)
    {
      aChild->InvalidateHierarchy(nsnull); // don't care about releasing grabs
    }
  }
}

void nsView::RemoveChild(nsView *child)
{
  NS_PRECONDITION(nsnull != child, "null ptr");

  if (nsnull != child)
  {
    nsView* prevKid = nsnull;
    nsView* kid = mFirstChild;
    bool found = false;
    while (nsnull != kid) {
      if (kid == child) {
        if (nsnull != prevKid) {
          prevKid->SetNextSibling(kid->GetNextSibling());
        } else {
          mFirstChild = kid->GetNextSibling();
        }
        child->SetParent(nsnull);
        found = true;
        break;
      }
      prevKid = kid;
	    kid = kid->GetNextSibling();
    }
    NS_ASSERTION(found, "tried to remove non child");

    // If we just removed a root view, then update the RootViewManager
    // on all view managers in the removed subtree.

    nsViewManager *vm = child->GetViewManager();
    if (vm->GetRootViewImpl() == child)
    {
      child->InvalidateHierarchy(GetViewManager());
    }
  }
}

// Native widgets ultimately just can't deal with the awesome power of
// CSS2 z-index. However, we set the z-index on the widget anyway
// because in many simple common cases the widgets do end up in the
// right order. We set each widget's z-index to the z-index of the
// nearest ancestor that has non-auto z-index.
static void UpdateNativeWidgetZIndexes(nsView* aView, PRInt32 aZIndex)
{
  if (aView->HasWidget()) {
    nsIWidget* widget = aView->GetWidget();
    PRInt32 curZ;
    widget->GetZIndex(&curZ);
    if (curZ != aZIndex) {
      widget->SetZIndex(aZIndex);
    }
  } else {
    for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
      if (v->GetZIndexIsAuto()) {
        UpdateNativeWidgetZIndexes(v, aZIndex);
      }
    }
  }
}

static PRInt32 FindNonAutoZIndex(nsView* aView)
{
  while (aView) {
    if (!aView->GetZIndexIsAuto()) {
      return aView->GetZIndex();
    }
    aView = aView->GetParent();
  }
  return 0;
}

nsresult nsIView::CreateWidget(nsWidgetInitData *aWidgetInitData,
                               bool aEnableDragDrop,
                               bool aResetVisibility)
{
  return Impl()->CreateWidget(aWidgetInitData,
                              aEnableDragDrop, aResetVisibility);
}

nsresult nsIView::CreateWidgetForParent(nsIWidget* aParentWidget,
                                        nsWidgetInitData *aWidgetInitData,
                                        bool aEnableDragDrop,
                                        bool aResetVisibility)
{
  return Impl()->CreateWidgetForParent(aParentWidget, aWidgetInitData,
                                       aEnableDragDrop, aResetVisibility);
}

nsresult nsIView::CreateWidgetForPopup(nsWidgetInitData *aWidgetInitData,
                                       nsIWidget* aParentWidget,
                                       bool aEnableDragDrop,
                                       bool aResetVisibility)
{
  return Impl()->CreateWidgetForPopup(aWidgetInitData, aParentWidget,
                                      aEnableDragDrop, aResetVisibility);
}

void nsIView::DestroyWidget()
{
  Impl()->DestroyWidget();
}

struct DefaultWidgetInitData : public nsWidgetInitData {
  DefaultWidgetInitData() : nsWidgetInitData()
  {
    mWindowType = eWindowType_child;
    clipChildren = true;
    clipSiblings = true;
  }
};

nsresult nsView::CreateWidget(nsWidgetInitData *aWidgetInitData,
                              bool aEnableDragDrop,
                              bool aResetVisibility)
{
  AssertNoWindow();
  NS_ABORT_IF_FALSE(!aWidgetInitData ||
                    aWidgetInitData->mWindowType != eWindowType_popup,
                    "Use CreateWidgetForPopup");

  DefaultWidgetInitData defaultInitData;
  bool initDataPassedIn = !!aWidgetInitData;
  aWidgetInitData = aWidgetInitData ? aWidgetInitData : &defaultInitData;
  defaultInitData.mListenForResizes =
    (!initDataPassedIn && GetParent() &&
     GetParent()->GetViewManager() != mViewManager);

  nsIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  nsIWidget* parentWidget =
    GetParent() ? GetParent()->GetNearestWidget(nsnull) : nsnull;
  if (!parentWidget) {
    NS_ERROR("nsView::CreateWidget without suitable parent widget??");
    return NS_ERROR_FAILURE;
  }

  // XXX: using aForceUseIWidgetParent=true to preserve previous
  // semantics.  It's not clear that it's actually needed.
  mWindow = parentWidget->CreateChild(trect, ::HandleEvent,
                                      dx, aWidgetInitData,
                                      true).get();
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }
 
  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

nsresult nsView::CreateWidgetForParent(nsIWidget* aParentWidget,
                                       nsWidgetInitData *aWidgetInitData,
                                       bool aEnableDragDrop,
                                       bool aResetVisibility)
{
  AssertNoWindow();
  NS_ABORT_IF_FALSE(!aWidgetInitData ||
                    aWidgetInitData->mWindowType != eWindowType_popup,
                    "Use CreateWidgetForPopup");
  NS_ABORT_IF_FALSE(aParentWidget, "Parent widget required");

  DefaultWidgetInitData defaultInitData;
  aWidgetInitData = aWidgetInitData ? aWidgetInitData : &defaultInitData;

  nsIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  mWindow =
    aParentWidget->CreateChild(trect, ::HandleEvent,
                               dx, aWidgetInitData).get();
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

nsresult nsView::CreateWidgetForPopup(nsWidgetInitData *aWidgetInitData,
                                      nsIWidget* aParentWidget,
                                      bool aEnableDragDrop,
                                      bool aResetVisibility)
{
  AssertNoWindow();
  NS_ABORT_IF_FALSE(aWidgetInitData, "Widget init data required");
  NS_ABORT_IF_FALSE(aWidgetInitData->mWindowType == eWindowType_popup,
                    "Use one of the other CreateWidget methods");

  nsIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  // XXX/cjones: having these two separate creation cases seems ... um
  // ... unnecessary, but it's the way the old code did it.  Please
  // unify them by first finding a suitable parent nsIWidget, then
  // getting rid of aForceUseIWidgetParent.
  if (aParentWidget) {
    // XXX: using aForceUseIWidgetParent=true to preserve previous
    // semantics.  It's not clear that it's actually needed.
    mWindow = aParentWidget->CreateChild(trect, ::HandleEvent,
                                         dx, aWidgetInitData,
                                         true).get();
  }
  else {
    nsIWidget* nearestParent = GetParent() ? GetParent()->GetNearestWidget(nsnull)
                                           : nsnull;
    if (!nearestParent) {
      // Without a parent, we can't make a popup.  This can happen
      // when printing
      return NS_ERROR_FAILURE;
    }

    mWindow =
      nearestParent->CreateChild(trect, ::HandleEvent,
                                 dx, aWidgetInitData).get();
  }
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

void
nsView::InitializeWindow(bool aEnableDragDrop, bool aResetVisibility)
{
  NS_ABORT_IF_FALSE(mWindow, "Must have a window to initialize");

  ViewWrapper* wrapper = new ViewWrapper(this);
  NS_ADDREF(wrapper); // Will be released in ~nsView
  mWindow->SetClientData(wrapper);

  if (aEnableDragDrop) {
    mWindow->EnableDragDrop(true);
  }
      
  // propagate the z-index to the widget.
  UpdateNativeWidgetZIndexes(this, FindNonAutoZIndex(this));

  //make sure visibility state is accurate

  if (aResetVisibility) {
    SetVisibility(GetVisibility());
  }
}

// Attach to a top level widget and start receiving mirrored events.
nsresult nsIView::AttachToTopLevelWidget(nsIWidget* aWidget)
{
  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");
  /// XXXjimm This is a temporary workaround to an issue w/document
  // viewer (bug 513162).
  nsIView *oldView = GetAttachedViewFor(aWidget);
  if (oldView) {
    oldView->DetachFromTopLevelWidget();
  }

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  // Note, the previous device context will be released. Detaching
  // will not restore the old one.
  nsresult rv = aWidget->AttachViewToTopLevel(
    nsIWidget::UsePuppetWidgets() ? ::HandleEvent : ::AttachedHandleEvent, dx);
  if (NS_FAILED(rv))
    return rv;

  mWindow = aWidget;
  NS_ADDREF(mWindow);

  ViewWrapper* wrapper = new ViewWrapper(Impl());
  NS_ADDREF(wrapper);
  mWindow->SetAttachedViewPtr(wrapper);
  mWindow->EnableDragDrop(true);
  mWidgetIsTopLevel = true;

  // Refresh the view bounds
  nsWindowType type;
  mWindow->GetWindowType(type);
  CalcWidgetBounds(type);

  return NS_OK;
}

// Detach this view from an attached widget. 
nsresult nsIView::DetachFromTopLevelWidget()
{
  NS_PRECONDITION(mWidgetIsTopLevel, "Not attached currently!");
  NS_PRECONDITION(mWindow, "null mWindow for DetachFromTopLevelWidget!");

  // Release memory for the view wrapper
  ViewWrapper* wrapper = GetAttachedWrapperFor(mWindow);
  NS_IF_RELEASE(wrapper);

  mWindow->SetAttachedViewPtr(nsnull);
  NS_RELEASE(mWindow);

  mWidgetIsTopLevel = false;
  
  return NS_OK;
}

void nsView::SetZIndex(bool aAuto, PRInt32 aZIndex, bool aTopMost)
{
  bool oldIsAuto = GetZIndexIsAuto();
  mVFlags = (mVFlags & ~NS_VIEW_FLAG_AUTO_ZINDEX) | (aAuto ? NS_VIEW_FLAG_AUTO_ZINDEX : 0);
  mZIndex = aZIndex;
  SetTopMost(aTopMost);
  
  if (HasWidget() || !oldIsAuto || !aAuto) {
    UpdateNativeWidgetZIndexes(this, FindNonAutoZIndex(this));
  }
}

void nsView::AssertNoWindow()
{
  // XXX: it would be nice to make this a strong assert
  if (NS_UNLIKELY(mWindow)) {
    NS_ERROR("We already have a window for this view? BAD");
    ViewWrapper* wrapper = GetWrapperFor(mWindow);
    NS_IF_RELEASE(wrapper);
    mWindow->SetClientData(nsnull);
    mWindow->Destroy();
    NS_RELEASE(mWindow);
  }
}

//
// internal window creation functions
//
EVENT_CALLBACK nsIView::AttachWidgetEventHandler(nsIWidget* aWidget)
{
#ifdef DEBUG
  void* data = nsnull;
  aWidget->GetClientData(data);
  NS_ASSERTION(!data, "Already got client data");
#endif

  ViewWrapper* wrapper = new ViewWrapper(Impl());
  if (!wrapper)
    return nsnull;
  NS_ADDREF(wrapper); // Will be released in DetachWidgetEventHandler
  aWidget->SetClientData(wrapper);
  return ::HandleEvent;
}

void nsIView::DetachWidgetEventHandler(nsIWidget* aWidget)
{
  ViewWrapper* wrapper = GetWrapperFor(aWidget);
  NS_ASSERTION(!wrapper || wrapper->GetView() == this, "Wrong view");
  NS_IF_RELEASE(wrapper);
  aWidget->SetClientData(nsnull);
}

#ifdef DEBUG
void nsIView::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p ", (void*)this);
  if (nsnull != mWindow) {
    nscoord p2a = mViewManager->AppUnitsPerDevPixel();
    nsIntRect rect;
    mWindow->GetClientBounds(rect);
    nsRect windowBounds = rect.ToAppUnits(p2a);
    mWindow->GetBounds(rect);
    nsRect nonclientBounds = rect.ToAppUnits(p2a);
    nsrefcnt widgetRefCnt = mWindow->AddRef() - 1;
    mWindow->Release();
    PRInt32 Z;
    mWindow->GetZIndex(&Z);
    fprintf(out, "(widget=%p[%d] z=%d pos={%d,%d,%d,%d}) ",
            (void*)mWindow, widgetRefCnt, Z,
            nonclientBounds.x, nonclientBounds.y,
            windowBounds.width, windowBounds.height);
  }
  nsRect brect = GetBounds();
  fprintf(out, "{%d,%d,%d,%d}",
          brect.x, brect.y, brect.width, brect.height);
  fprintf(out, " z=%d vis=%d frame=%p <\n",
          mZIndex, mVis, mFrame);
  for (nsView* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    NS_ASSERTION(kid->GetParent() == this, "incorrect parent");
    kid->List(out, aIndent + 1);
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}
#endif // DEBUG

nsPoint nsIView::GetOffsetTo(const nsIView* aOther) const
{
  return Impl()->GetOffsetTo(static_cast<const nsView*>(aOther),
                             Impl()->GetViewManager()->AppUnitsPerDevPixel());
}

nsPoint nsView::GetOffsetTo(const nsView* aOther) const
{
  return GetOffsetTo(aOther, GetViewManager()->AppUnitsPerDevPixel());
}

nsPoint nsView::GetOffsetTo(const nsView* aOther, const PRInt32 aAPD) const
{
  NS_ABORT_IF_FALSE(GetParent() || !aOther || aOther->GetParent() ||
                    this == aOther, "caller of (outer) GetOffsetTo must not "
                    "pass unrelated views");
  // We accumulate the final result in offset
  nsPoint offset(0, 0);
  // The offset currently accumulated at the current APD
  nsPoint docOffset(0, 0);
  const nsView* v = this;
  nsViewManager* currVM = v->GetViewManager();
  PRInt32 currAPD = currVM->AppUnitsPerDevPixel();
  const nsView* root = nsnull;
  for ( ; v != aOther && v; root = v, v = v->GetParent()) {
    nsViewManager* newVM = v->GetViewManager();
    if (newVM != currVM) {
      PRInt32 newAPD = newVM->AppUnitsPerDevPixel();
      if (newAPD != currAPD) {
        offset += docOffset.ConvertAppUnits(currAPD, aAPD);
        docOffset.x = docOffset.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docOffset += v->GetPosition();
  }
  offset += docOffset.ConvertAppUnits(currAPD, aAPD);

  if (v != aOther) {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-VM-relative position of |this| in |offset|.  Get the
    // root-VM-relative position of aOther and subtract it.
    nsPoint negOffset = aOther->GetOffsetTo(root, aAPD);
    offset -= negOffset;
  }

  return offset;
}

nsPoint nsIView::GetOffsetToWidget(nsIWidget* aWidget) const
{
  nsPoint pt;
  // Get the view for widget
  nsIView* widgetIView = GetViewFor(aWidget);
  if (!widgetIView) {
    return pt;
  }
  nsView* widgetView = widgetIView->Impl();

  // Get the offset to the widget view in the widget view's APD
  // We get the offset in the widget view's APD first and then convert to our
  // APD afterwards so that we can include the widget view's ViewToWidgetOffset
  // in the sum in its native APD, and then convert the whole thing to our APD
  // so that we don't have to convert the APD of the relatively small
  // ViewToWidgetOffset by itself with a potentially large relative rounding
  // error.
  pt = -widgetView->GetOffsetTo(static_cast<const nsView*>(this));
  // Add in the offset to the widget.
  pt += widgetView->ViewToWidgetOffset();

  // Convert to our appunits.
  PRInt32 widgetAPD = widgetView->GetViewManager()->AppUnitsPerDevPixel();
  PRInt32 ourAPD = static_cast<const nsView*>(this)->
                    GetViewManager()->AppUnitsPerDevPixel();
  pt = pt.ConvertAppUnits(widgetAPD, ourAPD);
  return pt;
}

nsIWidget* nsIView::GetNearestWidget(nsPoint* aOffset) const
{
  return Impl()->GetNearestWidget(aOffset,
                                  Impl()->GetViewManager()->AppUnitsPerDevPixel());
}

nsIWidget* nsView::GetNearestWidget(nsPoint* aOffset) const
{
  return GetNearestWidget(aOffset, GetViewManager()->AppUnitsPerDevPixel());
}

nsIWidget* nsView::GetNearestWidget(nsPoint* aOffset, const PRInt32 aAPD) const
{
  // aOffset is based on the view's position, which ignores any chrome on
  // attached parent widgets.

  // We accumulate the final result in pt
  nsPoint pt(0, 0);
  // The offset currently accumulated at the current APD
  nsPoint docPt(0,0);
  const nsView* v = this;
  nsViewManager* currVM = v->GetViewManager();
  PRInt32 currAPD = currVM->AppUnitsPerDevPixel();
  for ( ; v && !v->HasWidget(); v = v->GetParent()) {
    nsViewManager* newVM = v->GetViewManager();
    if (newVM != currVM) {
      PRInt32 newAPD = newVM->AppUnitsPerDevPixel();
      if (newAPD != currAPD) {
        pt += docPt.ConvertAppUnits(currAPD, aAPD);
        docPt.x = docPt.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docPt += v->GetPosition();
  }
  if (!v) {
    if (aOffset) {
      pt += docPt.ConvertAppUnits(currAPD, aAPD);
      *aOffset = pt;
    }
    return nsnull;
  }

  // pt is now the offset from v's origin to this view's origin.
  // We add the ViewToWidgetOffset to get the offset to the widget.
  if (aOffset) {
    docPt += v->ViewToWidgetOffset();
    pt += docPt.ConvertAppUnits(currAPD, aAPD);
    *aOffset = pt;
  }
  return v->GetWidget();
}

bool nsIView::IsRoot() const
{
  NS_ASSERTION(mViewManager != nsnull," View manager is null in nsView::IsRoot()");
  return mViewManager->GetRootViewImpl() == this;
}

bool nsIView::ExternalIsRoot() const
{
  return nsIView::IsRoot();
}

void
nsIView::SetDeletionObserver(nsWeakView* aDeletionObserver)
{
  if (mDeletionObserver && aDeletionObserver) {
    aDeletionObserver->SetPrevious(mDeletionObserver);
  }
  mDeletionObserver = aDeletionObserver;
}

nsView*
nsIView::Impl()
{
  return static_cast<nsView*>(this);
}

const nsView*
nsIView::Impl() const
{
  return static_cast<const nsView*>(this);
}

nsRect
nsView::GetBoundsInParentUnits() const
{
  nsView* parent = GetParent();
  nsViewManager* VM = GetViewManager();
  if (this != VM->GetRootViewImpl() || !parent) {
    return mDimBounds;
  }
  PRInt32 ourAPD = VM->AppUnitsPerDevPixel();
  PRInt32 parentAPD = parent->GetViewManager()->AppUnitsPerDevPixel();
  return mDimBounds.ConvertAppUnitsRoundOut(ourAPD, parentAPD);
}

nsPoint
nsIView::ConvertFromParentCoords(nsPoint aPt) const
{
  const nsView* view = static_cast<const nsView*>(this);
  const nsView* parent = view->GetParent();
  if (parent) {
    aPt = aPt.ConvertAppUnits(parent->GetViewManager()->AppUnitsPerDevPixel(),
                              view->GetViewManager()->AppUnitsPerDevPixel());
  }
  aPt -= GetPosition();
  return aPt;
}
