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

#ifndef nsIFtpProtocolConnection_h___
#define nsIFtpProtocolConnection_h___

#include "nsIProtocolConnection.h"

// {25029495-F132-11d2-9588-00805F369F95}
#define NS_IFTPPROTOCOLCONNECTION_IID               \
    { 0x25029495, 0xf132, 0x11d2, { 0x95, 0x88, 0x0, 0x80, 0x5f, 0x36, 0x9f, 0x95 } }


class nsIFtpProtocolConnection : public nsIProtocolConnection
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFTPPROTOCOLCONNECTION_IID);

    // PRE connect
    NS_IMETHOD UsePASV(PRBool aComm) = 0;

    // POST connect

    // Initiate connect
    NS_IMETHOD Get(void) = 0;

    NS_IMETHOD Put(void) = 0;
};

#endif /* nsIIFtpProtocolConnection_h___ */
