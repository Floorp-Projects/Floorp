/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
