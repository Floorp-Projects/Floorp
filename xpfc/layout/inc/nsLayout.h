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

#ifndef nsLayout_h___
#define nsLayout_h___

#include "nsILayout.h"

class nsLayout : public nsILayout
{
public:
  nsLayout();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
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
  ~nsLayout();

};



#endif /* nsLayout_h___ */
