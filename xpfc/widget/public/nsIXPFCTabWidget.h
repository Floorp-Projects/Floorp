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

#ifndef nsIXPFCTabWidget_h___
#define nsIXPFCTabWidget_h___

#include "nsISupports.h"

//491d0190-3b99-11d2-bee0-00805f8a8dbd
#define NS_IXPFC_TABWIDGET_IID      \
 { 0x491d0190, 0x3b99, 0x11d2, \
   {0xbe, 0xe0, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }

class nsIXPFCTabWidget : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;
};

#endif /* nsIXPFCTabWidget_h___ */

