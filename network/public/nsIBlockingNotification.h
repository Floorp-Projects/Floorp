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

#ifndef nsIBlockingCallback_h___
#define nsIBlockingCallback_h___

#include "nscore.h"
#include "nsISupports.h"
#include "prthread.h"

/* forward declarations... */
class nsIURL;

/* 7a3e0350-d724-11d2-99db-0080c7cb1080 */
#define NS_IBLOCKINGNOTIFICATION_IID   \
{ 0x7a3e0350, 0xd724, 0x11d2, \
  {0x99, 0xdb, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }


class nsIBlockingNotification : public nsISupports
{
public:
  NS_IMETHOD IsBlocked(nsIURL *aUrl, PRBool *aResult) = 0;
  NS_IMETHOD Resume(nsIURL *aUrl, void *aExtraInfo) = 0;
};



/* bd697440-d724-11d2-99db-0080c7cb1080 */
#define NS_IBLOCKINGNOTIFICATION_OBSERVER_IID   \
{ 0xbd697440, 0xd724, 0x11d2, \
  {0x99, 0xdb, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }


class nsIBlockingNotificationObserver : public nsISupports
{
public:
  NS_IMETHOD Notify(nsIBlockingNotification *aCaller,
                    nsIURL *aUrl,
                    PRThread *aThread,
                    PRInt32 aCode,
                    void  *aExtraInfo) = 0;

  NS_IMETHOD CancelNotify(nsIURL *aUrl) = 0;
};


/* Generic status codes for nsIBlockingNotificationObserver::Notify(...) */
#define NS_NOTIFY_SUCCEEDED     NS_OK
#define NS_NOTIFY_BLOCKED       NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 10)

#endif /* nsIBlockingCallback_h___ */
