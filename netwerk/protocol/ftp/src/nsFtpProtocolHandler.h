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

#ifndef nsFtpProtocolHandler_h___
#define nsFtpProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "nsIConnectionCache.h"
#include "nsConnectionCacheObj.h"
#include "nsIThreadPool.h"

// {25029490-F132-11d2-9588-00805F369F95}
#define NS_FTPPROTOCOLHANDLER_CID \
    { 0x25029490, 0xf132, 0x11d2, { 0x95, 0x88, 0x0, 0x80, 0x5f, 0x36, 0x9f, 0x95 } }

class nsFtpProtocolHandler : public nsIProtocolHandler,
                             public nsIConnectionCache
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsIConnectionCache methods
    NS_DECL_NSICONNECTIONCACHE

    // nsFtpProtocolHandler methods:
    nsFtpProtocolHandler();
    virtual ~nsFtpProtocolHandler();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
protected:
    nsISupports*        mEventSinkGetter;
    nsHashtable*        mRootConnectionList;    // hash of FTP connections
    nsCOMPtr<nsIThreadPool> mPool;                  // thread pool for FTP connections
};

#define NS_FTP_CONNECTION_COUNT  6
#define NS_FTP_CONNECTION_STACK_SIZE (64 * 1024)

#endif /* nsFtpProtocolHandler_h___ */
