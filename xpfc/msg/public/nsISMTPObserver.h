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

#ifndef nsISMTPObserver_h___
#define nsISMTPObserver_h___

#include "nsISupports.h"

//fc251940-701f-11d2-8dbc-00805f8a7ab6
#define NS_ISMTP_OBSERVER_IID   \
{ 0xfc251940, 0x701f, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

typedef enum
{
  nsSMTPMethod_bdat = 0,
  nsSMTPMethod_connect = 1,
  nsSMTPMethod_data = 2,
  nsSMTPMethod_ehlo = 3,
  nsSMTPMethod_ehloComplete = 4,
  nsSMTPMethod_error = 5,
  nsSMTPMethod_expand = 6,
  nsSMTPMethod_expandComplete = 7,
  nsSMTPMethod_help = 8,
  nsSMTPMethod_helpComplete = 9,
  nsSMTPMethod_mailFrom = 10,
  nsSMTPMethod_noop = 11,
  nsSMTPMethod_quit = 12,
  nsSMTPMethod_rcpTo = 13,
  nsSMTPMethod_reset = 14,
  nsSMTPMethod_send = 15,
  nsSMTPMethod_sendCommand = 16,
  nsSMTPMethod_sendCommandComplete = 17,
  nsSMTPMethod_verify = 18,
} nsSMTPMethod;

class nsISMTPObserver : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD ServerResponse(nsSMTPMethod aMethod,
                            PRInt32 aResponseCode, 
                            nsString& aResponseString) = 0;

};

#endif /* nsISMTPObserver_h___ */
