/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file arhandlee subject to the Netscape Public License
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

#include "nsView.h"
#include "nsIWidget.h"
#include "nsIViewManager.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIWidget.h"
#include "nsIButton.h"
#include "nsIScrollbar.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsRepository.h"
#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nsIScrollableView.h"

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

//
// Main events handler
//
nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent)
{ 
//printf(" %d %d %d (%d,%d) \n", aEvent->widget, aEvent->widgetSupports, 
//       aEvent->message, aEvent->point.x, aEvent->point.y);
  nsIView *view;
  nsEventStatus  result = nsEventStatus_eIgnore; 

  if (NS_OK == aEvent->widget->QueryInterface(kIViewIID, (void**)&view))
  {
    switch(aEvent->message)
    {
      case NS_SIZE:
      {
        nscoord width = ((nsSizeEvent*)aEvent)->windowSize->width;
        nscoord height = ((nsSizeEvent*)aEvent)->windowSize->height;

        // Inform the view manager that the root window has been resized
        nsIViewManager*  vm = view->GetViewManager();
        nsIPresContext* presContext = vm->GetPresContext();

        // The root view may not be set if this is the resize associated with
        // window creation

        nsIView* rootView = vm->GetRootView();

        if (view == rootView)
        {
          // Convert from pixels to twips
          float p2t = presContext->GetPixelsToTwips();
          vm->SetWindowDimensions(NS_TO_INT_ROUND(width * p2t),
                                  NS_TO_INT_ROUND(height * p2t));
          result = nsEventStatus_eConsumeNoDefault;
        }

        NS_IF_RELEASE(rootView);
        NS_RELEASE(presContext);
        NS_RELEASE(vm);

        break;
      }

      case NS_PAINT:
      {
        nsIViewManager    *vm = view->GetViewManager();
        nsIPresContext    *px = vm->GetPresContext();
        float             convert = px->GetPixelsToTwips();
        nsRect            vrect, trect = *((nsPaintEvent*)aEvent)->rect;
        nsIDeviceContext  *dx = px->GetDeviceContext();

        trect *= convert;

        //see if the paint region is greater than .75 the area of our root view.
        //if so, enable double buffered painting.

        view->GetBounds(vrect);

        PRBool db = PR_FALSE;

        if ((((float)trect.width * trect.height) / ((float)vrect.width * vrect.height)) >  0.75f)
          db = PR_TRUE;

        vm->Refresh(view, ((nsPaintEvent *)aEvent)->renderingContext, &trect,
                    ((db == PR_TRUE) ? NS_VMREFRESH_DOUBLE_BUFFER : 0) | NS_VMREFRESH_SCREEN_RECT);

        NS_RELEASE(dx);
        NS_RELEASE(px);
        NS_RELEASE(vm);

        result = nsEventStatus_eConsumeNoDefault;

        break;
      }

      case NS_DESTROY:
        result = nsEventStatus_eConsumeNoDefault;
        break;

      default:
        nsIViewManager *vm = view->GetViewManager();
        nsIPresContext  *cx = vm->GetPresContext();

        // pass on to view somewhere else to deal with

        aEvent->point.x = NS_TO_INT_ROUND(aEvent->point.x * cx->GetPixelsToTwips());
        aEvent->point.y = NS_TO_INT_ROUND(aEvent->point.y * cx->GetPixelsToTwips());

        result = view->HandleEvent(aEvent, NS_VIEW_FLAG_CHECK_CHILDREN | 
                                           NS_VIEW_FLAG_CHECK_PARENT |
                                           NS_VIEW_FLAG_CHECK_SIBLINGS);

        aEvent->point.x = NS_TO_INT_ROUND(aEvent->point.x * cx->GetTwipsToPixels());
        aEvent->point.y = NS_TO_INT_ROUND(aEvent->point.y * cx->GetTwipsToPixels());

        NS_RELEASE(cx);
        NS_RELEASE(vm);

        break;
    }

    NS_RELEASE(view);
  }

  return result;
}

nsView :: nsView()
{
  mVis = nsViewVisibility_kShow;
  mXForm = nsnull;
  mDying = PR_FALSE;
}

nsView :: ~nsView()
{
  mDying = PR_TRUE;

  if (GetChildCount() > 0)
  {
    nsIView *kid;

    //nuke the kids

    while (kid = GetChild(0))
      kid->Destroy();
  }

  if (mXForm != nsnull)
  {
    delete mXForm;
    mXForm = nsnull;
  }

  if (nsnull != mViewManager)
  {
    nsIView *rootView = mViewManager->GetRootView();

    if (nsnull != rootView)
    {
      if (rootView == this)
      {
        //This code should never be reached since there is a circular ref between nsView and nsViewManager
        mViewManager->SetRootView(nsnull);  // this resets our ref count to 0
      }
      else
      {
        if (nsnull != mParent)
          mViewManager->RemoveChild(mParent, this);

        NS_RELEASE(rootView);
      }
    }
    else if (nsnull != mParent)
      mParent->RemoveChild(this);

    NS_RELEASE(mViewManager);
    mViewManager = nsnull;
  }
  else if (nsnull != mParent)
    mParent->RemoveChild(this);

  if (nsnull != mFrame) {
    // Temporarily raise our refcnt because the frame is going to
    // Release us.
    mRefCnt = 99;
    mFrame->SetView(nsnull);
    mRefCnt = 0;
  }
  if (nsnull != mInnerWindow) {
	  NS_RELEASE(mInnerWindow); // this should destroy the widget and its native windows
  }
}

nsresult nsView :: QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIViewIID);

  if (aIID.Equals(kClassIID) || (aIID.Equals(kISupportsIID))) {
    *aInstancePtr = (void*)(nsIView*)this;
    AddRef();
    return NS_OK;
  }

  if (nsnull != mInnerWindow)
    return mInnerWindow->QueryInterface(aIID, aInstancePtr);

  return NS_NOINTERFACE;
}

// this should be added to nsView
nsIWidget*
GetWindowTemp(nsIView *aView)
{
  nsIWidget *window = nsnull;

  nsIView *ancestor = aView;
  while (nsnull != ancestor) {
	  if (nsnull != (window = ancestor->GetWidget())) {
	    return window;
	  }
	  ancestor = ancestor->GetParent();
  }
  return nsnull;
}

nsrefcnt nsView::AddRef() 
{
  return ++mRefCnt;
}

nsrefcnt nsView::Release()
{
  mRefCnt--;

  if (mDying == PR_FALSE)
  {
    if ((mRefCnt == 1) && (nsnull != mViewManager))
    {
      nsIView *pRoot = mViewManager->GetRootView();

      if (nsnull != pRoot)
      {
        if (pRoot == this)
        {
          nsIViewManager *vm = mViewManager;
          mViewManager = nsnull;  //set to null so that we don't get in here again
          NS_RELEASE(pRoot);      //set our ref count back to 1
          NS_RELEASE(vm);         //kill the ViewManager
        }
        else
          NS_RELEASE(pRoot);  //release root again
      }
    }

    if (mRefCnt == 0)
    {
      delete this;
      return 0;
    }
  }

  return mRefCnt;
}

nsresult nsView :: Init(nsIViewManager* aManager,
                        const nsRect &aBounds,
                        nsIView *aParent,
                        const nsCID *aWindowCIID,
                        nsWidgetInitData *aWidgetInitData,
                        nsNativeWindow aNative,
                        PRInt32 aZIndex,
                        const nsViewClip *aClip,
                        float aOpacity,
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
  mViewManager = aManager;

  NS_ADDREF(aManager);

  mBounds = aBounds;

  if (aClip != nsnull)
    mClip = *aClip;
  else
  {
    mClip.mLeft = 0;
    mClip.mRight = 0;
    mClip.mTop = 0;
    mClip.mBottom = 0;
  }

  mOpacity = aOpacity;
  mZindex = aZIndex;

  // assign the parent view
  SetParent(aParent);

  // check if a real window has to be created
  if (aWindowCIID)
  {
    nsIPresContext    *cx = mViewManager->GetPresContext();
    nsIDeviceContext  *dx = cx->GetDeviceContext();
    nsRect            trect = aBounds;

    trect *= cx->GetTwipsToPixels();

    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
    if (NS_OK == LoadWidget(*aWindowCIID))
    {
      if (aNative)
        mWindow->Create(aNative, trect, ::HandleEvent, dx, nsnull, aWidgetInitData);
      else
      {
        nsIWidget *parent = GetWindowTemp(aParent); 
        mWindow->Create(parent, trect, ::HandleEvent, dx, nsnull, aWidgetInitData);
        NS_IF_RELEASE(parent);
      }
    }

    NS_RELEASE(dx);
    NS_RELEASE(cx);
  }

  SetVisibility(aVisibilityFlag);

  return NS_OK;
}

void nsView :: Destroy()
{
  delete this;
}

nsIViewManager * nsView :: GetViewManager()
{
  NS_IF_ADDREF(mViewManager);
  return mViewManager;
}

nsIWidget * nsView :: GetWidget()
{
  NS_IF_ADDREF(mWindow);
  return mWindow;
}

PRBool nsView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                       PRUint32 aPaintFlags, nsIView *aBackstop)
{
  nsIView *pRoot = mViewManager->GetRootView();
  PRBool  clipres = PR_FALSE;

  rc.PushState();

  if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
  {
    nsRect  crect;

    crect.x = mClip.mLeft + mBounds.x;
    crect.y = mClip.mTop + mBounds.y;
    crect.width = mClip.mRight - mClip.mLeft;
    crect.height = mClip.mBottom - mClip.mTop;

    clipres = rc.SetClipRect(crect, nsClipCombine_kIntersect);
  }
  else if (this != pRoot)
    clipres = rc.SetClipRect(mBounds, nsClipCombine_kIntersect);

  if (clipres == PR_FALSE)
  {
    rc.Translate(mBounds.x, mBounds.y);

    //XXX maybe we should set this before we set the clip? MMP

    if (nsnull != mXForm)
    {
      nsTransform2D *pXForm = rc.GetCurrentTransform();
      pXForm->Concatenate(mXForm);
    }

    PRInt32 numkids = GetChildCount();

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      nsIView *kid = GetChild(cnt);

      if (nsnull != kid)
      {
        nsRect kidRect;
        kid->GetBounds(kidRect);
        nsRect damageArea;
        PRBool overlap = damageArea.IntersectRect(rect, kidRect);

        if (overlap == PR_TRUE)
        {
          // Translate damage area into kid's coordinate system
          nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                               damageArea.width, damageArea.height);
          clipres = kid->Paint(rc, kidDamageArea, aPaintFlags);

          if (clipres == PR_TRUE)
            break;
        }
      }
    }

    if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow) && (nsnull != mFrame))
    {
      nsIPresContext  *cx = mViewManager->GetPresContext();
      rc.PushState();
      mFrame->Paint(*cx, rc, rect);
      rc.PopState();
      NS_RELEASE(cx);
    }
  }

  //XXX would be nice if we could have a version of pop that just removes the
  //state from the stack but doesn't change the state of the underlying graphics
  //context. MMP

  rc.PopState();

  //now we need to exclude this view from the rest of the
  //paint process. only do this if this view is actually
  //visible and if there is no widget (like a scrollbar) here.

  if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow) && (nsnull == mWindow))
  {
    if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
    {
      nsRect  crect;

      crect.x = mClip.mLeft + mBounds.x;
      crect.y = mClip.mTop + mBounds.y;
      crect.width = mClip.mRight - mClip.mLeft;
      crect.height = mClip.mBottom - mClip.mTop;

      clipres = rc.SetClipRect(crect, nsClipCombine_kSubtract);
    }
    else if (this != pRoot)
      clipres = rc.SetClipRect(mBounds, nsClipCombine_kSubtract);
  }

  NS_RELEASE(pRoot);

  return clipres;
}

PRBool nsView :: Paint(nsIRenderingContext& rc, const nsIRegion& region, PRUint32 aPaintFlags)
{
  // XXX apply region to rc
  // XXX get bounding rect from region
  //if (nsnull != mFrame)
  //  mFrame->Paint(rc, rect, aPaintFlags);
  return PR_FALSE;
}

nsEventStatus nsView :: HandleEvent(nsGUIEvent *event, PRUint32 aEventFlags)
{
//printf(" %d %d %d %d (%d,%d) \n", this, event->widget, event->widgetSupports, 
//       event->message, event->point.x, event->point.y);
  nsIScrollbar  *scroll;
  nsEventStatus retval = nsEventStatus_eIgnore;

  if (nsnull != mWindow)
  {
    //if this is a scrollbar window that sent
    //us the event, do special processing

    static NS_DEFINE_IID(kscroller, NS_ISCROLLBAR_IID);

    if (NS_OK == mWindow->QueryInterface(kscroller, (void **)&scroll))
    {
      if (nsnull != mParent)
        retval = mParent->HandleEvent(event, NS_VIEW_FLAG_CHECK_SIBLINGS);

      NS_RELEASE(scroll);
    }
  }

  if ((retval == nsEventStatus_eIgnore) && (nsnull != mFrame))
  {
    nsIPresContext  *cx = mViewManager->GetPresContext();
    nscoord         xoff, yoff;

    mViewManager->GetWindowOffsets(&xoff, &yoff);

    event->point.x += xoff;
    event->point.y += yoff;

    mFrame->HandleEvent(*cx, event, retval);

    event->point.x -= xoff;
    event->point.y -= yoff;

    NS_RELEASE(cx);
  }

  //see if any of this view's children can process the event
  
  if ((aEventFlags & NS_VIEW_FLAG_CHECK_CHILDREN) &&
      (retval == nsEventStatus_eIgnore))
  {
    PRInt32 numkids = GetChildCount();
    nsRect  trect;
    nscoord x, y;

    x = event->point.x;
    y = event->point.y;

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      nsIView *pKid = GetChild(cnt);
      nscoord lx, ly;
      
      pKid->GetBounds(trect);

      lx = x - trect.x;
      ly = y - trect.y;

      if (trect.Contains(lx, ly))
      {
        //the x, y position of the event in question
        //is inside this child view, so give it the
        //opportunity to handle the event

        event->point.x -= trect.x;
        event->point.y -= trect.y;

        retval = pKid->HandleEvent(event, NS_VIEW_FLAG_CHECK_CHILDREN);

        event->point.x += trect.x;
        event->point.y += trect.y;

        if (retval != nsEventStatus_eIgnore)
          break;
      }
    }
  }

  //see if any of this views siblings can process this event
  //we only go from the next sibling since this is a z-ordered
  //list

  if ((aEventFlags & NS_VIEW_FLAG_CHECK_SIBLINGS) &&
      (retval == nsEventStatus_eIgnore))
  {
    nsIView *pNext = GetNextSibling();

    while (pNext)
    {
      retval = pNext->HandleEvent(event, NS_VIEW_FLAG_CHECK_CHILDREN);

      if (retval != PR_FALSE)
        break;

      pNext = pNext->GetNextSibling();
    }
  }
  
  //no-one has a clue what to do with this... so ask the
  //parents. kind of mimics life, huh?

  if ((aEventFlags & NS_VIEW_FLAG_CHECK_PARENT) && (retval == nsEventStatus_eIgnore))
  {
    nsIView *pParent = GetParent();

    while (pParent)
    {
      retval = pParent->HandleEvent(event, NS_VIEW_FLAG_CHECK_SIBLINGS);

      if (retval == nsEventStatus_eIgnore)
        break;

      pParent = pParent->GetParent();
    }
  }

  return retval;
}

void nsView :: SetPosition(nscoord x, nscoord y)
{
  mBounds.MoveTo(x, y);

  if (nsnull != mWindow)
  {
    nsIPresContext  *px = mViewManager->GetPresContext();
    nscoord         offx, offy;
    float           scale = px->GetTwipsToPixels();

    mViewManager->GetWindowOffsets(&offx, &offy);
    
    mWindow->Move(NS_TO_INT_ROUND((x + offx) * scale), NS_TO_INT_ROUND((y + offy) * scale));

    NS_RELEASE(px);
  }
}

void nsView :: GetPosition(nscoord *x, nscoord *y)
{
  *x = mBounds.x;
  *y = mBounds.y;
}

void nsView :: SetDimensions(nscoord width, nscoord height)
{
  mBounds.SizeTo(width, height);

  if (nsnull != mParent)
  {
    nsIScrollableView *scroller;

    static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

    if (NS_OK == mParent->QueryInterface(kscroller, (void **)&scroller))
    {
      scroller->ComputeContainerSize();
      NS_RELEASE(scroller);
    }
  }

  if (nsnull != mWindow)
  {
    nsIPresContext  *px = mViewManager->GetPresContext();
    float           t2p = px->GetTwipsToPixels();
  
    mWindow->Resize(NS_TO_INT_ROUND(t2p * width), NS_TO_INT_ROUND(t2p * height),
                    PR_TRUE);

    NS_RELEASE(px);
  }
}

void nsView :: GetDimensions(nscoord *width, nscoord *height)
{
  *width = mBounds.width;
  *height = mBounds.height;
}

void nsView :: SetBounds(const nsRect &aBounds)
{
  SetPosition(aBounds.x, aBounds.y);
  SetDimensions(aBounds.width, aBounds.height);
}

void nsView :: SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  SetPosition(aX, aY);
  SetDimensions(aWidth, aHeight);
}

void nsView :: GetBounds(nsRect &aBounds)
{
  aBounds = mBounds;
}

void nsView :: SetClip(nscoord aLeft, nscoord aTop, nscoord aRight, nscoord aBottom)
{
  mClip.mLeft = aLeft;
  mClip.mTop = aTop;
  mClip.mRight = aRight;
  mClip.mBottom = aBottom;
}

PRBool nsView :: GetClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom)
{
  if ((mClip.mLeft == mClip.mRight) || (mClip.mTop == mClip.mBottom))
    return PR_FALSE;
  else
  {
    *aLeft = mClip.mLeft;
    *aTop = mClip.mTop;
    *aRight = mClip.mRight;
    *aBottom = mClip.mBottom;

    return PR_TRUE;
  }
}

void nsView :: SetVisibility(nsViewVisibility aVisibility)
{
  mVis = aVisibility;

  if (nsnull != mWindow)
  {
    if (mVis == nsViewVisibility_kShow)
      mWindow->Show(PR_TRUE);
    else
      mWindow->Show(PR_FALSE);
  }
}

nsViewVisibility nsView :: GetVisibility()
{
  return mVis;
}

void nsView :: SetZIndex(PRInt32 zindex)
{
  mZindex = zindex;
}

PRInt32 nsView :: GetZIndex()
{
  return mZindex;
}

void nsView :: SetParent(nsIView *aParent)
{
  mParent = aParent;
}

nsIView * nsView :: GetParent()
{
  return mParent;
}

nsIView * nsView :: GetNextSibling() const
{
  return mNextSibling;
}

void nsView::SetNextSibling(nsIView* aView)
{
  mNextSibling = aView;
}

void nsView :: InsertChild(nsIView *child, nsIView *sibling)
{
  NS_PRECONDITION(nsnull != child, "null ptr");
  if (nsnull != child)
  {
    if (nsnull != sibling)
    {
      NS_ASSERTION(!(sibling->GetParent() != this), "tried to insert view with invalid sibling");
      //insert after sibling
      child->SetNextSibling(sibling->GetNextSibling());
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
}

void nsView :: RemoveChild(nsIView *child)
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
          prevKid->SetNextSibling(kid->GetNextSibling());
        } else {
          mFirstChild = kid->GetNextSibling();
        }
        child->SetParent(nsnull);
        mNumKids--;
        found = PR_TRUE;
        break;
      }
      prevKid = kid;
	    kid = kid->GetNextSibling();
    }
    NS_ASSERTION(found, "tried to remove non child");
  }
}

PRInt32 nsView :: GetChildCount()
{
  return mNumKids;
}

nsIView * nsView :: GetChild(PRInt32 index)
{ 
  NS_PRECONDITION(!(index > mNumKids), "bad index");

  if (index < mNumKids)
  {
    nsIView *kid = mFirstChild;
    for (PRInt32 cnt = 0; (cnt < index) && (nsnull != kid); cnt++) {
      kid = kid->GetNextSibling();
    }
    return kid;
  }
  return nsnull;
}

void nsView :: SetTransform(nsTransform2D &aXForm)
{
  if (nsnull == mXForm)
    mXForm = new nsTransform2D(&aXForm);
  else
    *mXForm = aXForm;
}

void nsView :: GetTransform(nsTransform2D &aXForm)
{
  if (nsnull != mXForm)
    aXForm = *mXForm;
  else
    aXForm.SetToIdentity();
}

void nsView :: SetOpacity(float opacity)
{
  mOpacity = opacity;
}

float nsView :: GetOpacity()
{
  return mOpacity;
}

PRBool nsView :: HasTransparency()
{
  return PR_FALSE;
}

// Frames have a pointer to the view, so don't AddRef the frame.
void nsView :: SetFrame(nsIFrame *aFrame)
{
  mFrame = aFrame;
}

nsIFrame * nsView :: GetFrame()
{
  return mFrame;
}

//
// internal window creation functions
//
nsresult nsView :: LoadWidget(const nsCID &aClassIID)
{
  nsresult rv;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  rv = NSRepository::CreateInstance(aClassIID, this, kISupportsIID, (void**)&mInnerWindow);

  if (NS_OK == rv) {
    // load the convenience nsIWidget pointer.
    // NOTE: mWindow is released so not to create a circulare refcount
    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
    rv = mInnerWindow->QueryInterface(kIWidgetIID, (void**)&mWindow);
    if (NS_OK != rv) {
	    mInnerWindow->Release();
	    mInnerWindow = NULL;
    }
    else {
      mWindow->Release();
    }
  }

  return rv;
}

void nsView :: List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p win=%p ", this, mWindow);
  out << mBounds;
  fprintf(out, " z=%d vis=%d opc=%1.3f <\n", mZindex, mVis, mOpacity);
  nsIView* kid = mFirstChild;
  while (nsnull != kid) {
    kid->List(out, aIndent + 1);
    kid = kid->GetNextSibling();
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

void nsView :: AdjustChildWidgets(nscoord aDx, nscoord aDy)
{
  PRInt32 numkids = GetChildCount();

  for (PRInt32 cnt = 0; cnt < numkids; cnt++)
  {
    nsIView   *kid = GetChild(cnt);
    nsIWidget *win = kid->GetWidget();

    if (nsnull != win)
    {
      nsRect  bounds;

      win->BeginResizingChildren();
      win->GetBounds(bounds);
      win->Move(bounds.x + aDx, bounds.y + aDy);
    }

    kid->AdjustChildWidgets(aDx, aDy);

    if (nsnull != win)
    {
      win->EndResizingChildren();
      NS_RELEASE(win);
    }
  }
}
