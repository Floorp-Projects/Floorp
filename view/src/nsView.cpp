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
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

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
  
  nsIView*      view = nsView::GetViewFor(aEvent->widget);
  if (nsnull != view) {
    nsIViewManager*  vm = view->GetViewManager();
    result = vm->DispatchEvent(aEvent);
    NS_RELEASE(vm);
  }

  return result;
}

nsView :: nsView()
{
  mVis = nsViewVisibility_kShow;
  mXForm = nsnull;
  mVFlags = ~ALL_VIEW_FLAGS;
}

nsView :: ~nsView()
{
  mVFlags |= VIEW_FLAG_DYING;

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

    mViewManager = nsnull;
  }
  else if (nsnull != mParent)
  {
    mParent->RemoveChild(this);
  }

  // Destroy and release the widget
  if (nsnull != mWindow) {
    mWindow->SetClientData(nsnull);
    mWindow->Destroy();
    NS_RELEASE(mWindow);
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

  // The widget's client data points back to the owning view
  if (NS_SUCCEEDED(aWidget->GetClientData(clientData))) {
    view = (nsIView*)clientData;

    if (nsnull != view) {
#ifdef NS_DEBUG
      // Verify the pointer is really a view
      nsView* widgetView;
      NS_ASSERTION((NS_SUCCEEDED(view->QueryInterface(kIViewIID, (void **)&widgetView))) &&
                   (widgetView == view), "bad client data");
#endif
    }  
  }

  return view;
}

nsresult nsView :: Init(nsIViewManager* aManager,
                        const nsRect &aBounds,
                        nsIView *aParent,
                        const nsCID *aWindowCIID,
                        nsWidgetInitData *aWidgetInitData,
                        nsNativeWidget aNative,
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
  // we don't hold a reference to the view manager
  mViewManager = aManager;

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

  SetBounds(aBounds);

  // check if a real window has to be created
  if (aWindowCIID)
  {
    nsIPresContext    *cx = mViewManager->GetPresContext();
    nsIDeviceContext  *dx = cx->GetDeviceContext();
    nsRect            trect = aBounds;

    trect *= cx->GetTwipsToPixels();

    if (NS_OK == LoadWidget(*aWindowCIID))
    {
      if (aNative)
        mWindow->Create(aNative, trect, ::HandleEvent, dx, nsnull, nsnull, aWidgetInitData);
      else
      {
        nsIWidget *parent = GetOffsetFromWidget(nsnull, nsnull); 
        mWindow->Create(parent, trect, ::HandleEvent, dx, nsnull, nsnull, aWidgetInitData);
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
  PRBool  clipwasset = PR_FALSE;

  rc.PushState();

  if (aPaintFlags & NS_VIEW_FLAG_CLIP_SET)
  {
    clipwasset = PR_TRUE;
    aPaintFlags &= ~NS_VIEW_FLAG_CLIP_SET;
  }
  else if (mVis == nsViewVisibility_kShow)
  {
    nsRect brect;

    GetBounds(brect);

    if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
    {
      nsRect  crect;

      crect.x = mClip.mLeft + brect.x;
      crect.y = mClip.mTop + brect.y;
      crect.width = mClip.mRight - mClip.mLeft;
      crect.height = mClip.mBottom - mClip.mTop;

      clipres = rc.SetClipRect(crect, nsClipCombine_kIntersect);
    }
    else if (this != pRoot)
      clipres = rc.SetClipRect(brect, nsClipCombine_kIntersect);
  }

  if (nsnull != mXForm)
  {
    nsTransform2D *pXForm = rc.GetCurrentTransform();
    pXForm->Concatenate(mXForm);
  }

  if (clipres == PR_FALSE)
  {
    nscoord posx, posy;

    GetPosition(&posx, &posy);

    rc.Translate(posx, posy);

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

    if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow))
    {
      float opacity = GetOpacity();

      if (opacity > 0.0f)
      {
        rc.PushState();

        if (HasTransparency() || (opacity < 1.0f))
        {
          //overview of algorithm:
          //1. clip is set to intersection of this view and whatever is
          //   left of the damage region in the rc.
          //2. walk tree from this point down through the view list,
          //   rendering and clipping out opaque views encountered until
          //   there is nothing left in the clip area or the bottommost
          //   view is reached.
          //3. walk back up through view list restoring clips and painting
          //   or blending any non-opaque views encountered until we reach the
          //   view that started the whole process

          //walk down rendering only views within this clip

          nsIView *child = GetNextSibling(), *prevchild = this;

          while (nsnull != child)
          {
            nsRect kidRect;
            child->GetBounds(kidRect);
            nsRect damageArea;
            PRBool overlap = damageArea.IntersectRect(rect, kidRect);

            //as we tell each kid to paint, we need to mark the kid as one that was hit
            //in the front to back rendering so that when we do the back to front pass,
            //we can re-add the child's rect back into the clip.

            if (overlap == PR_TRUE)
            {
              // Translate damage area into kid's coordinate system
              nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                                   damageArea.width, damageArea.height);
              clipres = child->Paint(rc, kidDamageArea, aPaintFlags);
            }

            prevchild = child;

            child = child->GetNextSibling();

            if (nsnull == child)
              child = child->GetParent();

            if (clipres == PR_TRUE)
              break;
          }

          if ((nsnull != prevchild) && (this != prevchild))
          {
            //walk backwards, rendering views
          }
        }

        if (nsnull != mFrame)
        {
          nsIPresContext  *cx = mViewManager->GetPresContext();
          mFrame->Paint(*cx, rc, rect);
          NS_RELEASE(cx);
        }

#ifdef SHOW_VIEW_BORDERS
        {
          nscoord x, y, w, h;

          if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
          {
            x = mClip.mLeft;
            y = mClip.mTop;
            w = mClip.mRight - mClip.mLeft;
            h = mClip.mBottom - mClip.mTop;

            rc.SetColor(NS_RGB(255, 255, 0));
          }
          else
          {
            x = y = 0;

            GetDimensions(&w, &h);

            if (nsnull != mWindow)
              rc.SetColor(NS_RGB(0, 255, 0));
            else
              rc.SetColor(NS_RGB(0, 0, 255));
          }

          rc.DrawRect(x, y, w, h);
        }
#endif

        rc.PopState();
      }
    }
  }

  //XXX would be nice if we could have a version of pop that just removes the
  //state from the stack but doesn't change the state of the underlying graphics
  //context. MMP

  clipres = rc.PopState();

  //now we need to exclude this view from the rest of the
  //paint process. only do this if this view is actually
  //visible and if there is no widget (like a scrollbar) here.

//  if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow))
  if (!clipwasset && (clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow) && (nsnull == mWindow))
  {
    nsRect  brect;

    GetBounds(brect);

    if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
    {
      nsRect  crect;

      crect.x = mClip.mLeft + brect.x;
      crect.y = mClip.mTop + brect.y;
      crect.width = mClip.mRight - mClip.mLeft;
      crect.height = mClip.mBottom - mClip.mTop;

      clipres = rc.SetClipRect(crect, nsClipCombine_kSubtract);
    }
    else if (this != pRoot)
      clipres = rc.SetClipRect(brect, nsClipCombine_kSubtract);
  }

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
  nsEventStatus retval = nsEventStatus_eIgnore;

  //see if any of this view's children can process the event
  if (retval == nsEventStatus_eIgnore) {
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

  //if the view's children didn't take the event, check the view itself.
  if (retval == nsEventStatus_eIgnore && nsnull != mFrame)
  {
    nsIPresContext  *cx = mViewManager->GetPresContext();
    nscoord         xoff, yoff;

    GetScrollOffset(&xoff, &yoff);

    event->point.x += xoff;
    event->point.y += yoff;

    mFrame->HandleEvent(*cx, event, retval);

    event->point.x -= xoff;
    event->point.y -= yoff;

    NS_RELEASE(cx);
  }

  return retval;
}

void nsView :: SetPosition(nscoord x, nscoord y)
{
  mBounds.MoveTo(x, y);

  if (nsnull != mWindow)
  {
    nsIPresContext  *px = mViewManager->GetPresContext();
    nscoord         offx, offy, parx = 0, pary = 0;
    float           scale = px->GetTwipsToPixels();
    nsIWidget       *pwidget = nsnull;
  
    GetScrollOffset(&offx, &offy);

    pwidget = GetOffsetFromWidget(&parx, &pary);
    NS_IF_RELEASE(pwidget);
    
    mWindow->Move(NSTwipsToIntPixels((x + parx - offx), scale),
                  NSTwipsToIntPixels((y + pary - offy), scale));

    NS_RELEASE(px);
  }
}

void nsView :: GetPosition(nscoord *x, nscoord *y)
{
  nsIView *rootView = mViewManager->GetRootView();

  if (this == rootView)
    *x = *y = 0;
  else
  {
    *x = mBounds.x;
    *y = mBounds.y;
  }
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
    }
  }

  if (nsnull != mWindow)
  {
    nsIPresContext  *px = mViewManager->GetPresContext();
    float           t2p = px->GetTwipsToPixels();
  
    mWindow->Resize(NSTwipsToIntPixels(width, t2p), NSTwipsToIntPixels(height, t2p),
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

void nsView :: GetBounds(nsRect &aBounds) const
{
  nsIView *rootView = mViewManager->GetRootView();

  aBounds = mBounds;

  if ((nsIView *)this == rootView)
    aBounds.x = aBounds.y = 0;
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
#ifndef HIDE_ALL_WIDGETS
    if (mVis == nsViewVisibility_kShow)
      mWindow->Show(PR_TRUE);
    else
#endif
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
  return (mVFlags & VIEW_FLAG_TRANSPARENT) ? PR_TRUE : PR_FALSE;
}

void nsView :: SetContentTransparency(PRBool aTransparent)
{
  if (aTransparent == PR_TRUE)
    mVFlags |= VIEW_FLAG_TRANSPARENT;
  else
    mVFlags &= ~VIEW_FLAG_TRANSPARENT;
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
  nsISupports*  window;
  nsresult      rv;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  rv = NSRepository::CreateInstance(aClassIID, nsnull, kISupportsIID, (void**)&window);

  if (NS_OK == rv) {
    // get a pointer to the nsIWidget* interface
    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
    rv = window->QueryInterface(kIWidgetIID, (void**)&mWindow);
    window->Release();

    // Set the widget's client data
    mWindow->SetClientData((void*)this);
  }

  return rv;
}

void nsView :: List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p ", this);
  if (nsnull != mWindow) {
    nsRect windowBounds;
    mWindow->GetBounds(windowBounds);
    fprintf(out, "(widget=%p pos={%d,%d,%d,%d}) ",
            mWindow,
            windowBounds.x, windowBounds.y,
            windowBounds.width, windowBounds.height);
  }
  nsRect brect;
  GetBounds(brect);
  out << brect;
  fprintf(out, " z=%d vis=%d opc=%1.3f <\n", mZindex, mVis, mOpacity);
  nsIView* kid = mFirstChild;
  while (nsnull != kid) {
    kid->List(out, aIndent + 1);
    kid = kid->GetNextSibling();
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

nsIWidget * nsView :: GetOffsetFromWidget(nscoord *aDx, nscoord *aDy)
{
  nsIWidget *window = nsnull;
  nsIView   *ancestor = GetParent();

  while (nsnull != ancestor)
  {
	  if (nsnull != (window = ancestor->GetWidget()))
	    return window;

    if ((nsnull != aDx) && (nsnull != aDy))
    {
      nscoord offx, offy;

      ancestor->GetPosition(&offx, &offy);

      *aDx += offx;
      *aDy += offy;
    }

	  ancestor = ancestor->GetParent();
  }

  return nsnull;
}

void nsView :: GetScrollOffset(nscoord *aDx, nscoord *aDy)
{
  nsIWidget *window = nsnull;
  nsIView   *ancestor = GetParent();

  while (nsnull != ancestor)
  {
    nsIScrollableView *sview;

    static NS_DEFINE_IID(kscroller, NS_ISCROLLABLEVIEW_IID);

    if (NS_OK == ancestor->QueryInterface(kscroller, (void **)&sview))
    {
      sview->GetVisibleOffset(aDx, aDy);
      return;
    }

    ancestor = ancestor->GetParent();
  }

  *aDx = *aDy = 0;
}
