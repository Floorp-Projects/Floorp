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

#ifndef nsScrollingView_h___
#define nsScrollingView_h___

#include "nsView.h"
#include "nsIScrollableView.h"

class nsScrollingView : public nsView, public nsIScrollableView
{
public:
  nsScrollingView();
  ~nsScrollingView();

  NS_IMETHOD QueryInterface(REFNSIID aIID,
                            void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  virtual nsresult Init(nsIViewManager* aManager,
					const nsRect &aBounds,
					nsIView *aParent,
					const nsIID *aWindowIID = nsnull,
					nsNativeWindow aNative = nsnull,
					PRInt32 aZIndex = 0,
					const nsRect *aClipRect = nsnull,
					float aOpacity = 1.0f,
					nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  //overrides
  virtual void SetPosition(nscoord x, nscoord y);
  virtual void SetDimensions(nscoord width, nscoord height);
  virtual nsEventStatus HandleEvent(nsGUIEvent *aEvent, PRBool aCheckParent = PR_TRUE, PRBool aCheckChildren = PR_TRUE);
  virtual void AdjustChildWidgets(nscoord aDx, nscoord aDy);

  //nsIScrollableView interface
  virtual void SetContainerSize(PRInt32 aSize);
  virtual PRInt32 GetContainerSize();

  virtual void SetVisibleOffset(PRInt32 aOffset);
  virtual PRInt32 GetVisibleOffset();

protected:
  PRInt32 mSize;
  PRInt32 mOffset;
  nsIView *mScrollBarView;
};

#endif
