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

#ifndef nsIWidgetController_h__
#define nsIWidgetController_h__

#include "nsCom.h"
#include "nsISupports.h"

/* forward declarations... */
class nsIDOMDocument;

// e5e5af70-8a38-11d2-9938-0080c7cb1080
#define NS_IWIDGETCONTROLLER_IID \
{ 0xe5e5af60, 0x8a38, 0x11d2, \
  {0x99, 0x38, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }


class nsIWidgetController : public nsISupports
{
public:

  NS_IMETHOD Initialize(nsIDOMDocument* aDocument, nsISupports* aContainer) = 0;
};


#endif /* nsIWidgetController_h__ */
