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
#include "nsIComponentManager.h"
#include "nsIRenderingContext.h"
#include "nsTransform2D.h"
#include "nsIScrollableView.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsIBlender.h"
#include "nsIRegion.h"

//mmptemp

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

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
  
  nsIView*      view = nsView::GetViewFor(aEvent->widget);
  if (nsnull != view) {
    nsIViewManager*  vm;

    view->GetViewManager(vm);
    vm->DispatchEvent(aEvent, result);
    NS_RELEASE(vm);
  }

  return result;
}

nsView :: nsView()
{
  mVis = nsViewVisibility_kShow;
  mXForm = nsnull;
  mVFlags = ~ALL_VIEW_FLAGS;
  mEventFlags = 0;
  mOpacity = 1.0f;
}

nsView :: ~nsView()
{
  mVFlags |= VIEW_FLAG_DYING;

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

  if (mXForm != nsnull)
  {
    delete mXForm;
    mXForm = nsnull;
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

    mViewManager = nsnull;
  }
  else if (nsnull != mParent)
  {
    mParent->RemoveChild(this);
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

  NS_PRECONDITION(nsnull != aWidget, "null widget ptr");
	
  // The widget's client data points back to the owning view
  if (aWidget && NS_SUCCEEDED(aWidget->GetClientData(clientData))) {
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

NS_IMETHODIMP nsView :: Init(nsIViewManager* aManager,
                             const nsRect &aBounds,
                             const nsIView *aParent,
                             const nsViewClip *aClip,
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

NS_IMETHODIMP nsView :: GetViewManager(nsIViewManager *&aViewMgr)
{
  NS_IF_ADDREF(mViewManager);
  aViewMgr = mViewManager;
  return NS_OK;
}

NS_IMETHODIMP nsView :: Paint(nsIRenderingContext& rc, const nsRect& rect,
                              PRUint32 aPaintFlags, PRBool &aResult)
{
  if (aPaintFlags & NS_VIEW_FLAG_JUST_PAINT)
  {
    //new compositor

    if (nsnull != mClientData)
    {
      nsIViewObserver *obs;

      if (NS_OK == mViewManager->GetViewObserver(obs))
      {
        if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
        {
          nsRect  crect, brect;

          GetBounds(brect);

          crect.x = mClip.mLeft + brect.x;
          crect.y = mClip.mTop + brect.y;
          crect.width = mClip.mRight - mClip.mLeft;
          crect.height = mClip.mBottom - mClip.mTop;

          rc.SetClipRect(crect, nsClipCombine_kIntersect, aResult);
        }

        obs->Paint((nsIView *)this, rc, rect);
        NS_RELEASE(obs);
      }
    }
  }
  else
  {
    nsIView *pRoot;
    PRBool  clipres = PR_FALSE;
    PRBool  clipwasset = PR_FALSE;
    float   opacity;

    mViewManager->GetRootView(pRoot);

    rc.PushState();

    GetOpacity(opacity);

    if (aPaintFlags & NS_VIEW_FLAG_CLIP_SET)
    {
      clipwasset = PR_TRUE;
      aPaintFlags &= ~NS_VIEW_FLAG_CLIP_SET;
    }
    else if (mVis == nsViewVisibility_kShow)
    {
      if (opacity == 1.0f)
      {
        nsRect brect;

        GetBounds(brect);

        if ((mClip.mLeft == mClip.mRight) || (mClip.mTop == mClip.mBottom) &&
            (this != pRoot))
        {
          rc.SetClipRect(brect, nsClipCombine_kIntersect, clipres);
        }
      }
    }

    if (clipres == PR_FALSE)
    {
      nscoord posx, posy;

      if (nsnull == mWindow)
      {
        GetPosition(&posx, &posy);
        rc.Translate(posx, posy);
      }

      if (nsnull != mXForm)
      {
        nsTransform2D *pXForm;
        rc.GetCurrentTransform(pXForm);
        pXForm->Concatenate(mXForm);
      }

      //if we are not doing a two pass render,
      //render the kids...

      if (!(aPaintFlags & (NS_VIEW_FLAG_FRONT_TO_BACK | NS_VIEW_FLAG_BACK_TO_FRONT)))
      {
        PRInt32 numkids;

        GetChildCount(numkids);

        for (PRInt32 cnt = 0; cnt < numkids; cnt++)
        {
          nsIView *kid;

          GetChild(cnt, kid);

          if (nsnull != kid)
          {
            // Don't paint child views that have widgets. They'll get their own
            // native paint requests
            nsIWidget *widget;
            PRBool     hasWidget;

            kid->GetWidget(widget);
            hasWidget = (widget != nsnull);
            if (nsnull != widget)
            {
              void *thing;
              thing = widget->GetNativeData(NS_NATIVE_WIDGET);
              NS_RELEASE(widget);

              if (nsnull == thing)
                hasWidget = PR_FALSE;
            }
            if (!hasWidget)
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
                kid->Paint(rc, kidDamageArea, aPaintFlags, clipres);
  
                if (clipres == PR_TRUE)
                  break;
              }
            }
          }
        }
      }

      if ((clipres == PR_FALSE) && (mVis == nsViewVisibility_kShow))
      {
        //don't render things that can't be seen...

        if ((opacity > 0.0f) && (mBounds.width > 1) && (mBounds.height > 1))
        {
          nsIRenderingContext *localcx = nsnull;
          nsDrawingSurface    surf = nsnull;
          nsDrawingSurface    redsurf = nsnull;
          PRBool              hasTransparency;
          PRBool              clipState;      //for when we want to throw away the clip state

          rc.PushState();

          HasTransparency(hasTransparency);

          if (opacity < 1.0f)
          {
            nsIDeviceContext  *dx = nsnull;
            nsIView           *rootview = nsnull;

            //create offscreen bitmap to render view into

            mViewManager->GetDeviceContext(dx);
            mViewManager->GetRootView(rootview);

            if ((nsnull != dx) && (nsnull != rootview))
            {
              dx->CreateRenderingContext(rootview, localcx);

              if (nsnull != localcx)
              {
                float   t2p;
                nscoord width, height;

                //create offscreen buffer for blending...

                dx->GetAppUnitsToDevUnits(t2p);

                width = NSToCoordRound(mBounds.width * t2p);
                height = NSToCoordRound(mBounds.height * t2p);

                nsRect bitrect = nsRect(0, 0, width, height);

                localcx->CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, surf);
                localcx->CreateDrawingSurface(&bitrect, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, redsurf);

                if (nsnull != surf)
                  localcx->SelectOffScreenDrawingSurface(surf);
              }

              NS_RELEASE(dx);
            }
          }

          if (nsnull == localcx)
            localcx = &rc;

          if (hasTransparency || (opacity < 1.0f))
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
            //clip is already set to this view in rendering context...

            nsIView     *curview, *preview = this;
            nsVoidArray *views = (nsVoidArray *)new nsVoidArray();
            nsVoidArray *rects = (nsVoidArray *)new nsVoidArray();
            nscoord     posx, posy;
            nsRect      damRect = rect;

            localcx->PushState();

            GetPosition(&posx, &posy);

            //we need to go back to the coordinate system that was there
            //before we came along... XXX xform not accounted for. MMP
            damRect.x += posx;
            damRect.y += posy;

            localcx->Translate(-posx, -posy);

            GetNextSibling(curview);

            if (nsnull == curview)
            {
              preview->GetParent(curview);

              if (nsnull != curview)
              {
                nsRect  prect;

                curview->GetBounds(prect);

                damRect.x += prect.x;
                damRect.y += prect.y;

                localcx->Translate(-prect.x, -prect.y);
              }
            }

            while (nsnull != curview)
            {
              nsRect kidRect;
              curview->GetBounds(kidRect);
              nsRect damageArea;
              PRBool overlap = damageArea.IntersectRect(damRect, kidRect);

              if (overlap == PR_TRUE)
              {
                // Translate damage area into kid's coordinate system
  //              nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
  //                                   damageArea.width, damageArea.height);
                nsRect kidDamageArea(damRect.x - kidRect.x, damRect.y - kidRect.y,
                                     damRect.width, damRect.height);

                //we will pop the states on the back to front pass...
                localcx->PushState();

                if (nsnull != views)
                  views->AppendElement(curview);

                rects->AppendElement(new nsRect(kidDamageArea));

                curview->Paint(*localcx, kidDamageArea, aPaintFlags | NS_VIEW_FLAG_FRONT_TO_BACK, clipres);
              }

              if (clipres == PR_TRUE)
                break;

              preview = curview;

              curview->GetNextSibling(curview);

              if (nsnull == curview)
              {
                preview->GetParent(curview);

                if (nsnull != curview)
                {
                  nsRect  prect;

                  curview->GetBounds(prect);

                  damRect.x += prect.x;
                  damRect.y += prect.y;

                  localcx->Translate(-prect.x, -prect.y);
                }
              }
            }

            //walk backwards, rendering views

            if (nsnull != views)
            {
              PRInt32 idx = views->Count();
              PRBool  isfirst = PR_TRUE;

              while (idx > 0)
              {
                if (PR_TRUE == isfirst)
                {
                  //we just rendered the last view at the
                  //end of the first pass, so there is no need
                  //to do so again.

                  nsRect *trect = (nsRect *)rects->ElementAt(--idx);
                  delete trect;

                  localcx->PopState(clipState);
                  isfirst = PR_FALSE;
                }
                else
                {
                  curview = (nsIView *)views->ElementAt(--idx);
                  nsRect *trect = (nsRect *)rects->ElementAt(idx);

                  curview->Paint(*localcx, *trect, aPaintFlags | NS_VIEW_FLAG_BACK_TO_FRONT, clipres);

                  delete trect;

                  //pop the state pushed on the front to back pass...
                  localcx->PopState(clipState);
                }
              }

              delete rects;
              delete views;
            }

            localcx->PopState(clipState);
          }

          if (nsnull != redsurf)
            localcx->SelectOffScreenDrawingSurface(redsurf);

          //now draw ourself...

          if (nsnull != mClientData)
          {
            nsIViewObserver *obs;

            if (NS_OK == mViewManager->GetViewObserver(obs))
            {
              if ((mClip.mLeft != mClip.mRight) && (mClip.mTop != mClip.mBottom))
              {
                nsRect  crect, brect;

                GetBounds(brect);

                crect.x = mClip.mLeft + brect.x;
                crect.y = mClip.mTop + brect.y;
                crect.width = mClip.mRight - mClip.mLeft;
                crect.height = mClip.mBottom - mClip.mTop;

                localcx->SetClipRect(crect, nsClipCombine_kIntersect, clipres);
              }

              obs->Paint((nsIView *)this, *localcx, rect);
              NS_RELEASE(obs);
            }
          }

          if (localcx != &rc)
          {
  //          localcx->SelectOffScreenDrawingSurface(nsnull);
            NS_RELEASE(localcx);
          }
          else
            localcx = nsnull;

          //kill offscreen buffer

          if ((nsnull != surf) && (nsnull != redsurf))
          {
            nsRect brect;

            brect.x = brect.y = 0;
            brect.width = mBounds.width;
            brect.height = mBounds.height;

            static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
            static NS_DEFINE_IID(kIBlenderIID, NS_IBLENDER_IID);

            nsIBlender  *blender = nsnull;
            nsresult rv;

            rv = nsComponentManager::CreateInstance(kBlenderCID, nsnull, kIBlenderIID, (void **)&blender);

            if (NS_OK == rv)
            {
              nsIDeviceContext  *dx;

              mViewManager->GetDeviceContext(dx);

              float   t2p;
              nscoord width, height;

              dx->GetAppUnitsToDevUnits(t2p);

              width = NSToCoordRound(mBounds.width * t2p);
              height = NSToCoordRound(mBounds.height * t2p);

              blender->Init(dx);
              blender->Blend(0, 0, width, height,redsurf,surf, 0, 0, opacity);

              NS_RELEASE(blender);
              NS_RELEASE(dx);

              rc.CopyOffScreenBits(surf, 0, 0, brect, NS_COPYBITS_XFORM_DEST_VALUES | NS_COPYBITS_TO_BACK_BUFFER);
            }

            rc.DestroyDrawingSurface(surf);
            rc.DestroyDrawingSurface(redsurf);

            surf = nsnull;
            redsurf = nsnull;
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

          rc.PopState(clipState);
        }
      }
    }

    rc.PopState(clipres);

    //now we need to exclude this view from the rest of the
    //paint process. only do this if this view is actually
    //visible and if there is no widget (like a scrollbar) here.

    if (!clipwasset && (clipres == PR_FALSE) &&
        (mVis == nsViewVisibility_kShow) && (nsnull == mWindow) &&
        (opacity > 0.0f))
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

        rc.SetClipRect(crect, nsClipCombine_kSubtract, clipres);
      }
      else if (this != pRoot)
        rc.SetClipRect(brect, nsClipCombine_kSubtract, clipres);
    }

    aResult = clipres;
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
                                    nsEventStatus &aStatus)
{
//printf(" %d %d %d %d (%d,%d) \n", this, event->widget, event->widgetSupports, 
//       event->message, event->point.x, event->point.y);
  aStatus = nsEventStatus_eIgnore;
  PRBool handledByChild = PR_FALSE;

  //see if any of this view's children can process the event
  if (aStatus == nsEventStatus_eIgnore && !(mVFlags & NS_VIEW_FLAG_DONT_CHECK_CHILDREN)) {
    PRInt32 numkids;
    nsRect  trect;
    nscoord x, y;

    GetChildCount(numkids);
    x = event->point.x;
    y = event->point.y;

    for (PRInt32 cnt = 0; cnt < numkids; cnt++)
    {
      nsIView *pKid;

      GetChild(cnt, pKid);
      pKid->GetBounds(trect);

      if (trect.Contains(x, y))
      {
        handledByChild = PR_TRUE;
        //the x, y position of the event in question
        //is inside this child view, so give it the
        //opportunity to handle the event

        event->point.x -= trect.x;
        event->point.y -= trect.y;

        pKid->HandleEvent(event, NS_VIEW_FLAG_CHECK_CHILDREN, aStatus);

        event->point.x += trect.x;
        event->point.y += trect.y;

        if (aStatus != nsEventStatus_eIgnore)
          break;
      }
    }
  }

  //if no child's bounds matched the event, check the view itself.
  if (!handledByChild && nsnull != mClientData)
  {
    nsIViewObserver *obs;

    if (NS_OK == mViewManager->GetViewObserver(obs))
    {
      obs->HandleEvent((nsIView *)this, event, aStatus);
      NS_RELEASE(obs);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetPosition(nscoord x, nscoord y)
{
  mBounds.MoveTo(x, y);

  if (nsnull != mWindow)
  {
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

NS_IMETHODIMP nsView :: GetPosition(nscoord *x, nscoord *y)
{
  nsIView *rootView;

  mViewManager->GetRootView(rootView);

  if (this == rootView)
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

NS_IMETHODIMP nsView :: GetDimensions(nscoord *width, nscoord *height)
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
  nsIView *rootView;

  mViewManager->GetRootView(rootView);
  aBounds = mBounds;

  if ((nsIView *)this == rootView)
    aBounds.x = aBounds.y = 0;

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetClip(nscoord aLeft, nscoord aTop, nscoord aRight, nscoord aBottom)
{
  mClip.mLeft = aLeft;
  mClip.mTop = aTop;
  mClip.mRight = aRight;
  mClip.mBottom = aBottom;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom,
                                PRBool &aResult)
{
  if ((mClip.mLeft == mClip.mRight) || (mClip.mTop == mClip.mBottom))
    aResult = PR_FALSE;
  else
  {
    *aLeft = mClip.mLeft;
    *aTop = mClip.mTop;
    *aRight = mClip.mRight;
    *aBottom = mClip.mBottom;
    aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetVisibility(nsViewVisibility aVisibility)
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

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetVisibility(nsViewVisibility &aVisibility)
{
  aVisibility = mVis;
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetZIndex(PRInt32 zindex)
{
  mZindex = zindex;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetZIndex(PRInt32 &aZIndex)
{
  aZIndex = mZindex;
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetParent(nsIView *aParent)
{
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetParent(nsIView *&aParent)
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

NS_IMETHODIMP nsView :: GetChildCount(PRInt32 &aCount)
{
  aCount = mNumKids;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetChild(PRInt32 index, nsIView *&aChild)
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

NS_IMETHODIMP nsView :: SetTransform(nsTransform2D &aXForm)
{
  if (nsnull == mXForm)
    mXForm = new nsTransform2D(&aXForm);
  else
    *mXForm = aXForm;

  return NS_OK;
}

NS_IMETHODIMP nsView :: GetTransform(nsTransform2D &aXForm)
{
  if (nsnull != mXForm)
    aXForm = *mXForm;
  else
    aXForm.SetToIdentity();

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetOpacity(float opacity)
{
  mOpacity = opacity;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetOpacity(float &aOpacity)
{
  aOpacity = mOpacity;
  return NS_OK;
}

NS_IMETHODIMP nsView :: HasTransparency(PRBool &aTransparent) const
{
  aTransparent = (mVFlags & VIEW_FLAG_TRANSPARENT) ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetContentTransparency(PRBool aTransparent)
{
  if (aTransparent == PR_TRUE)
    mVFlags |= VIEW_FLAG_TRANSPARENT;
  else
    mVFlags &= ~VIEW_FLAG_TRANSPARENT;

  return NS_OK;
}

NS_IMETHODIMP nsView :: SetClientData(void *aData)
{
  mClientData = aData;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetClientData(void *&aData)
{
  aData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsView :: CreateWidget(const nsIID &aWindowIID,
                                     nsWidgetInitData *aWidgetInitData,
                                     nsNativeWidget aNative)
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

NS_IMETHODIMP nsView :: GetWidget(nsIWidget *&aWidget)
{
  NS_IF_ADDREF(mWindow);
  aWidget = mWindow;
  return NS_OK;
}

//
// internal window creation functions
//
nsresult nsView :: LoadWidget(const nsCID &aClassIID)
{
  nsresult rv;

  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  rv = nsComponentManager::CreateInstance(aClassIID, nsnull, kIWidgetIID, (void**)&mWindow);

  if (NS_OK == rv)
  {
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
    fprintf(out, "(widget=%p pos={%d,%d,%d,%d}) ",
            mWindow,
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
}

NS_IMETHODIMP nsView :: SetViewFlags(PRInt32 aFlags)
{
  mVFlags |= aFlags;
  return NS_OK;
}

NS_IMETHODIMP nsView :: ClearViewFlags(PRInt32 aFlags)
{
  mVFlags &= ~aFlags;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget)
{
  nsIView   *ancestor;
  
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

  aWidget = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetDirtyRegion(nsIRegion *&aRegion)
{
  aRegion = mDirtyRegion;
  NS_IF_ADDREF(aRegion);
  return NS_OK;
}

NS_IMETHODIMP nsView :: SetDirtyRegion(nsIRegion *aRegion)
{
  NS_IF_RELEASE(mDirtyRegion);
  mDirtyRegion = aRegion;
  NS_IF_ADDREF(mDirtyRegion);
  return NS_OK;
}

NS_IMETHODIMP nsView :: GetScratchPoint(nsPoint **aPoint)
{
  NS_ASSERTION(!(!aPoint), "no point");

  *aPoint = &mScratchPoint;

  return NS_OK;
}
