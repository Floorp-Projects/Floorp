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

#ifndef nsIMIMEService_h___
#define nsIMIMEService_h___

#include "nsISupports.h"
#include "nsString.h"
#include "nsIMimeMessage.h"

//08d3c0f0-6f90-11d2-8dbc-00805f8a7ab6
#define NS_IMIME_SERVICE_IID   \
{ 0x08d3c0f0, 0x6f90, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }



class nsIMIMEService : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

};

#endif /* nsIMIMEService_h___ */
