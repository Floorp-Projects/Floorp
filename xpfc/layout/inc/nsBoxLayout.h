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

#ifndef nsBoxLayout_h___
#define nsBoxLayout_h___

#include "nsLayout.h"
#include "nsIXPFCCanvas.h"
#include "nsIIterator.h"


enum nsLayoutAlignment {   
  eLayoutAlignment_none,
  eLayoutAlignment_horizontal,
  eLayoutAlignment_vertical,
}; 


class nsBoxLayout : public nsLayout
{
public:
  nsBoxLayout();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD Init(nsIXPFCCanvas * aContainer);

  NS_IMETHOD Layout();

  NS_IMETHOD PreferredSize(nsSize &aSize);
  NS_IMETHOD MinimumSize(nsSize &aSize);
  NS_IMETHOD MaximumSize(nsSize &aSize);

  NS_IMETHOD_(PRUint32) GetHorizontalGap();
  NS_IMETHOD_(PRUint32) GetVerticalGap();
  NS_IMETHOD_(void)     SetHorizontalGap(PRUint32 aGapSize);
  NS_IMETHOD_(void)     SetVerticalGap(PRUint32 aGapSize);

  NS_IMETHOD_(PRFloat64) GetHorizontalFill();
  NS_IMETHOD_(PRFloat64) GetVerticalFill();
  NS_IMETHOD_(void)      SetHorizontalFill(PRFloat64 aFillSize);
  NS_IMETHOD_(void)      SetVerticalFill(PRFloat64 aFillSize);

protected:
  ~nsBoxLayout();

private:
  NS_IMETHOD  LayoutContainer();
  NS_IMETHOD  LayoutChildren();
  NS_IMETHOD  CreateIterator(nsIIterator ** aIterator) ;

private:
  nsIXPFCCanvas * mContainer;
  nsLayoutAlignment mLayoutAlignment;
  PRUint32 mVerticalGap;
  PRUint32 mHorizontalGap;
  PRFloat64 mVerticalFill;
  PRFloat64 mHorizontalFill;

// XXX This needs to be put into an interface
public:
  NS_IMETHOD_(nsLayoutAlignment)  GetLayoutAlignment();
  NS_IMETHOD_(void)               SetLayoutAlignment(nsLayoutAlignment aLayoutAlignment);

private:
  NS_IMETHOD_(PRBool)   CanLayoutUsingPreferredSizes(nsIIterator * aIterator);
  NS_IMETHOD_(PRInt32)  ComputeFreeFloatingSpace(nsIIterator * aIterator);
  NS_IMETHOD_(PRUint32) QueryNumberFloatingWidgets(nsIIterator * aIterator);

};



#endif /* nsBoxLayout_h___ */
