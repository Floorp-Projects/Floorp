/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIThrobber_h___
#define nsIThrobber_h___

#include "nsweb.h"
#include "nsISupports.h"

class nsIFactory;
class nsIWidget;
class nsString;
struct nsRect;

#define NS_ITHROBBER_IID \
 { 0xa6cf905e, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

#define NS_THROBBER_CID \
 { 0xa6cf905f, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIThrobber : public nsISupports {
public:
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds) = 0;
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds, const nsString& aFileNameMask, PRInt32 aNumImages) = 0;
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY) = 0;
  NS_IMETHOD Show() = 0;
  NS_IMETHOD Hide() = 0;
  NS_IMETHOD Start() = 0;
  NS_IMETHOD Stop() = 0;
};

extern "C" NS_WEB nsresult
NS_NewThrobberFactory(nsIFactory** aFactory);

#endif /* nsIThrobber_h___ */
