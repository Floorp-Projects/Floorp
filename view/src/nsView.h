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
#include "nsIViewObserver.h"

//mmptemp

class nsIPresContext;

class nsView : public nsIView
{
public:
  nsView();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIView
  NS_IMETHOD  Init(nsIViewManager* aManager,
      						 const nsRect &aBounds,
                   const nsIView *aParent,
      						 const nsViewClip *aClip = nsnull,
      						 nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  NS_IMETHOD  Destroy();
  NS_IMETHOD  GetViewManager(nsIViewManager *&aViewMgr);
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &aResult);
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsIRegion& region,
                    PRUint32 aPaintFlags, PRBool &aResult);
  NS_IMETHOD  HandleEvent(nsGUIEvent *event, PRUint32 aEventFlags, nsEventStatus &aStatus);
  NS_IMETHOD  SetPosition(nscoord x, nscoord y);
  NS_IMETHOD  GetPosition(nscoord *x, nscoord *y);
  NS_IMETHOD  SetDimensions(nscoord width, nscoord height, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  GetDimensions(nscoord *width, nscoord *height);
  NS_IMETHOD  SetBounds(const nsRect &aBounds, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, PRBool aPaint = PR_TRUE);
  NS_IMETHOD  GetBounds(nsRect &aBounds) const;
  NS_IMETHOD  SetClip(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD  GetClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom, PRBool &aResult);
  NS_IMETHOD  SetVisibility(nsViewVisibility visibility);
  NS_IMETHOD  GetVisibility(nsViewVisibility &aVisibility);
  NS_IMETHOD  SetZIndex(PRInt32 zindex);
  NS_IMETHOD  GetZIndex(PRInt32 &aZIndex);
  NS_IMETHOD  SetParent(nsIView *aParent);
  NS_IMETHOD  GetParent(nsIView *&aParent);
  NS_IMETHOD  GetNextSibling(nsIView *&aNextSibling) const;
  NS_IMETHOD  SetNextSibling(nsIView* aNextSibling);
  NS_IMETHOD  InsertChild(nsIView *child, nsIView *sibling);
  NS_IMETHOD  RemoveChild(nsIView *child);
  NS_IMETHOD  GetChildCount(PRInt32 &aCount);
  NS_IMETHOD  GetChild(PRInt32 index, nsIView*& aChild);
  NS_IMETHOD  SetTransform(nsTransform2D &aXForm);
  NS_IMETHOD  GetTransform(nsTransform2D &aXForm);
  NS_IMETHOD  SetOpacity(float opacity);
  NS_IMETHOD  GetOpacity(float &aOpacity);
  NS_IMETHOD  HasTransparency(PRBool &aTransparent) const;
  NS_IMETHOD  SetContentTransparency(PRBool aTransparent);
  NS_IMETHOD  SetClientData(void *aData);
  NS_IMETHOD  GetClientData(void *&aData);
  NS_IMETHOD  GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget);
  NS_IMETHOD  GetDirtyRegion(nsIRegion*& aRegion);
  NS_IMETHOD  SetDirtyRegion(nsIRegion* aRegion);
  NS_IMETHOD  CreateWidget(const nsIID &aWindowIID,
                           nsWidgetInitData *aWidgetInitData = nsnull,
                           nsNativeWidget aNative = nsnull);
  NS_IMETHOD  SetWidget(nsIWidget *aWidget);
  NS_IMETHOD  GetWidget(nsIWidget *&aWidget);
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  NS_IMETHOD SetViewFlags(PRUint32 aFlags);
  NS_IMETHOD ClearViewFlags(PRUint32 aFlags);
  NS_IMETHOD GetViewFlags(PRUint32 *aFlags);
  NS_IMETHOD GetScratchPoint(nsPoint **aPoint);

  // Helper function to get the view that's associated with a widget
  static nsIView*  GetViewFor(nsIWidget* aWidget);

protected:
  virtual ~nsView();
  //
  virtual nsresult LoadWidget(const nsCID &aClassIID);

protected:
  nsIViewManager    *mViewManager;
  nsIView           *mParent;
  nsIWidget         *mWindow;

  //XXX should there be pointers to last child so backward walking is fast?
  nsIView           *mNextSibling;
  nsIView           *mFirstChild;
  void              *mClientData;
  PRInt32           mZindex;
  nsViewVisibility  mVis;
  PRInt32           mNumKids;
  nsRect            mBounds;
  nsViewClip        mClip;
  nsTransform2D     *mXForm;
  float             mOpacity;
  PRUint32          mVFlags;
  nsIRegion*        mDirtyRegion;
  nsPoint           mScratchPoint;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
};

#endif
