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

#ifndef nsView_h___
#define nsView_h___

#include "nsViewManager.h"
#include "nsIView.h"
#include "nsRect.h"
#include "nsCRT.h"
#include "nsIWidget.h"
#include "nsIFactory.h"

class nsIPresContext;

class nsView : public nsIView
{
public:
  nsView();
   ~nsView();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Init(nsIViewManager* aManager,
						const nsRect &aBounds,
						nsIView *aParent,
						const nsCID *aWindowIID = nsnull,
            nsWidgetInitData *aWidgetInitData = nsnull,
						nsNativeWindow aNative = nsnull,
						PRInt32 aZIndex = 0,
						const nsViewClip *aClip = nsnull,
						float aOpacity = 1.0f,
						nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  virtual void Destroy();
  virtual nsIViewManager * GetViewManager();
  virtual nsIWidget * GetWidget();
  virtual PRBool Paint(nsIRenderingContext& rc, const nsRect& rect,
                     PRUint32 aPaintFlags, nsIView *aBackstop = nsnull);
  virtual PRBool Paint(nsIRenderingContext& rc, const nsIRegion& region, PRUint32 aPaintFlags);
  virtual nsEventStatus HandleEvent(nsGUIEvent *event, PRUint32 aEventFlags);
  virtual void SetPosition(nscoord x, nscoord y);
  virtual void GetPosition(nscoord *x, nscoord *y);
  virtual void SetDimensions(nscoord width, nscoord height);
  virtual void GetDimensions(nscoord *width, nscoord *height);
  virtual void SetBounds(const nsRect &aBounds);
  virtual void SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  virtual void GetBounds(nsRect &aBounds);
  virtual void SetClip(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  virtual PRBool GetClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom);
  virtual void SetVisibility(nsViewVisibility visibility);
  virtual nsViewVisibility GetVisibility();
  virtual void SetZIndex(PRInt32 zindex);
  virtual PRInt32 GetZIndex();
  virtual void SetParent(nsIView *aParent);
  virtual nsIView *GetParent();
  virtual nsIView* GetNextSibling() const;
  virtual void SetNextSibling(nsIView* aNextSibling);
  virtual void InsertChild(nsIView *child, nsIView *sibling);
  virtual void RemoveChild(nsIView *child);
  virtual PRInt32 GetChildCount();
  virtual nsIView * GetChild(PRInt32 index);
  virtual void SetTransform(nsTransform2D &aXForm);
  virtual void GetTransform(nsTransform2D &aXForm);
  virtual void SetOpacity(float opacity);
  virtual float GetOpacity();
  virtual PRBool HasTransparency();
  virtual void SetContentTransparency(PRBool aTransparent);
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  virtual void SetFrame(nsIFrame *aFrame);
  virtual nsIFrame * GetFrame();
  virtual nsIWidget * GetOffsetFromWidget(nscoord *aDx, nscoord *aDy);
  virtual void GetScrollOffset(nscoord *aDx, nscoord *aDy);

protected:
  //
  virtual nsresult LoadWidget(const nsCID &aClassIID);

protected:
  nsIViewManager    *mViewManager;
  nsIView           *mParent;

  // a View aggregates an nsIWidget, if any.
  // mInnerWindow is the pointer to the widget real nsISupports. QueryInterace
  // should be called on mInnerWindow if any of the widget interface wants to 
  // be exposed, 
  // mWindow is a convenience pointer to the widget functionalities.
  // mWindow is not AddRef'ed or Released otherwise we'll create a circulare
  // refcount and the view will never go away
  nsISupports		    *mInnerWindow;
  nsIWidget         *mWindow;

  //XXX should there be pointers to last child so backward walking is fast?
  nsIView           *mNextSibling;
  nsIView           *mFirstChild;
  nsIFrame          *mFrame;
  PRInt32           mZindex;
  nsViewVisibility  mVis;
  PRInt32           mNumKids;
  nsRect            mBounds;
  nsViewClip        mClip;
  nsTransform2D     *mXForm;
  float             mOpacity;
  PRInt32           mVFlags;
};

#define VIEW_FLAG_DYING       0x0001
#define VIEW_FLAG_TRANSPARENT 0x0002

#define ALL_VIEW_FLAGS        (VIEW_FLAG_DYING | VIEW_FLAG_TRANSPARENT)

#endif
