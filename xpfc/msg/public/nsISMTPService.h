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

#ifndef nsISMTPService_h___
#define nsISMTPService_h___

#include "nsISupports.h"
#include "nsString.h"
#include "nsIMessage.h"
#include "nsIMimeMessage.h"
#include "nsISMTPObserver.h"

//b64f8b50-6f77-11d2-8dbc-00805f8a7ab6
#define NS_ISMTP_SERVICE_IID   \
{ 0xb64f8b50, 0x6f77, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsISMTPService : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD SendMail(nsString& aServer, 
                      nsString& aFrom, 
                      nsString& aTo, 
                      nsString& aSubject, 
                      nsString& aBody,
                      nsISMTPObserver * aObserver = nsnull) = 0;

  NS_IMETHOD SendMail(nsString& aServer, 
                      nsIMessage& aMessage,
                      nsISMTPObserver * aObserver = nsnull) = 0;

  NS_IMETHOD SendMail(nsString& aServer, 
                      nsIMIMEMessage& aMIMEMessage,
                      nsISMTPObserver * aObserver = nsnull) = 0;

};

#endif /* nsISMTPService_h___ */
