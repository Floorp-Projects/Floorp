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

#ifndef nsILayout_h___
#define nsILayout_h___

#include "nsISupports.h"
#include "nsSize.h"

//914fc3a0-f73c-11d1-9244-00805f8a7ab6
#define NS_ILAYOUT_IID   \
{ 0x914fc3a0, 0xf73c, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsILayout : public nsISupports
{

public:

  NS_IMETHOD Init() = 0 ;

  NS_IMETHOD Layout() = 0 ;

  NS_IMETHOD PreferredSize(nsSize &aSize) = 0 ;
  NS_IMETHOD MinimumSize(nsSize &aSize) = 0 ;
  NS_IMETHOD MaximumSize(nsSize &aSize) = 0 ;

  NS_IMETHOD_(PRUint32) GetHorizontalGap() = 0;
  NS_IMETHOD_(PRUint32) GetVerticalGap() = 0;
  NS_IMETHOD_(void)     SetHorizontalGap(PRUint32 aGapSize) = 0;
  NS_IMETHOD_(void)     SetVerticalGap(PRUint32 aGapSize) = 0;

  NS_IMETHOD_(PRFloat64) GetHorizontalFill() = 0;
  NS_IMETHOD_(PRFloat64) GetVerticalFill() = 0;
  NS_IMETHOD_(void)      SetHorizontalFill(PRFloat64 aFillSize) = 0;
  NS_IMETHOD_(void)      SetVerticalFill(PRFloat64 aFillSize) = 0;

};

#endif /* nsILayout_h___ */
