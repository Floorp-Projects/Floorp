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

#ifndef nsIHttpProtocolConnection_h___
#define nsIHttpProtocolConnection_h___

#include "nsIProtocolConnection.h"

#define NS_IHTTPPROTOCOLCONNECTION_IID               \
{ /* bd88a750-ea35-11d2-931b-00104ba0fd40 */         \
    0xbd88a750,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIHttpProtocolConnection : public nsIProtocolConnection
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTTPPROTOCOLCONNECTION_IID);

    ////////////////////////////////////////////////////////////////////////////
    // Things that can be done at any time:

    NS_IMETHOD GetHeader(const char* header) = 0;
    
    ////////////////////////////////////////////////////////////////////////////
    // Things done before connecting:

    NS_IMETHOD AddHeader(const char* header, const char* value) = 0;

    NS_IMETHOD RemoveHeader(const char* header) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Things done to connect:

    NS_IMETHOD Get(void) = 0;

    NS_IMETHOD GetByteRange(PRUint32 from, PRUint32 to) = 0;

    NS_IMETHOD Put(void) = 0;

    NS_IMETHOD Post(void) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // Things done after connecting:

};

#endif /* nsIIHttpProtocolConnection_h___ */
