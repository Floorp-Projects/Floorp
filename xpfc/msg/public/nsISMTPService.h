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

#ifndef nsISMTPService_h___
#define nsISMTPService_h___

#include "nsISupports.h"
#include "nsString.h"

//b64f8b50-6f77-11d2-8dbc-00805f8a7ab6
#define NS_ISMTP_SERVICE_IID   \
{ 0xb64f8b50, 0x6f77, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsISMTPService : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD SendMail(nsString& aServer, nsString& aFrom, nsString& aTo, nsString& aSubject, nsString& aBody) = 0;

};

#endif /* nsISMTPService_h___ */
