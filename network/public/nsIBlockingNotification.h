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

#ifndef nsIBlockingCallback_h___
#define nsIBlockingCallback_h___

#include "nscore.h"
#include "nsISupports.h"
#include "prthread.h"

/* forward declarations... */
class nsIURI;

/* 7a3e0350-d724-11d2-99db-0080c7cb1080 */
#define NS_IBLOCKINGNOTIFICATION_IID   \
{ 0x7a3e0350, 0xd724, 0x11d2, \
  {0x99, 0xdb, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }


class nsIBlockingNotification : public nsISupports
{
public:
  NS_IMETHOD IsBlocked(nsIURI *aUrl, PRBool *aResult) = 0;
  NS_IMETHOD Resume(nsIURI *aUrl, void *aExtraInfo) = 0;
};



/* bd697440-d724-11d2-99db-0080c7cb1080 */
#define NS_IBLOCKINGNOTIFICATION_OBSERVER_IID   \
{ 0xbd697440, 0xd724, 0x11d2, \
  {0x99, 0xdb, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }


class nsIBlockingNotificationObserver : public nsISupports
{
public:
  NS_IMETHOD Notify(nsIBlockingNotification *aCaller,
                    nsIURI *aUrl,
                    PRThread *aThread,
                    PRInt32 aCode,
                    void  *aExtraInfo) = 0;

  NS_IMETHOD CancelNotify(nsIURI *aUrl) = 0;
};


/* Generic status codes for nsIBlockingNotificationObserver::Notify(...) */
#define NS_NOTIFY_SUCCEEDED     NS_OK
#define NS_NOTIFY_BLOCKED       NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 10)

#endif /* nsIBlockingCallback_h___ */
