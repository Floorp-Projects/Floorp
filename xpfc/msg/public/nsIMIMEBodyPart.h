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

#ifndef nsIMIMEBodyPart_h___
#define nsIMIMEBodyPart_h___

#include "nsISupports.h"
#include "nsString.h"

//4804d230-703a-11d2-8dbc-00805f8a7ab6
#define NS_IMIME_BODY_PART_IID   \
{ 0x4804d230, 0x703a, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIMIMEBodyPart : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

};

#endif /* nsIMIMEBodyPart_h___ */
