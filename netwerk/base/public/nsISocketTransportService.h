/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsISocketTransportService_h___
#define nsISocketTransportService_h___

#include "nsISupports.h"

class nsITransport;

#define NS_ISOCKETTRANSPORTSERVICE_IID               \
{ /* 9610f120-ef12-11d2-92b6-00105a1b0d64 */         \
    0x9610f120,                                      \
    0xef12,                                          \
    0x11d2,                                          \
    {0x92, 0xb6, 0x00, 0x10, 0x5a, 0x1b, 0x0d, 0x64} \
}

#define NS_SOCKETTRANSPORTSERVICE_CID                \
{ /* c07e81e0-ef12-11d2-92b6-00105a1b0d64 */         \
    0xc07e81e0,                                      \
    0xef12,                                          \
    0x11d2,                                          \
    {0x92, 0xb6, 0x00, 0x10, 0x5a, 0x1b, 0x0d, 0x64} \
}


class nsISocketTransportService : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISOCKETTRANSPORTSERVICE_IID);

  NS_IMETHOD CreateTransport(const char* host, PRInt32 port,
                             nsITransport* *result) = 0;

  NS_IMETHOD Shutdown(void) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsISocketTransportService_h___ */
