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

#ifndef nsInputStreamChannel_h__
#define nsInputStreamChannel_h__

#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsCRT.h"

class nsInputStreamChannel : public nsIChannel
{
public:
    NS_DECL_ISUPPORTS

    // nsIRequest methods:
    NS_IMETHOD IsPending(PRBool *result);
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);
    
    // nsIChannel methods:
    NS_IMETHOD GetURI(nsIURI * *aURI);
    NS_IMETHOD OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **_retval);
    NS_IMETHOD OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval);
    NS_IMETHOD AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt, nsIStreamListener *listener);
    NS_IMETHOD AsyncWrite(nsIInputStream *fromStream, PRUint32 startPosition,
                          PRInt32 writeCount, nsISupports *ctxt,
                          nsIStreamObserver *observer);
    NS_IMETHOD GetLoadAttributes(nsLoadFlags *aLoadAttributes);
    NS_IMETHOD SetLoadAttributes(nsLoadFlags aLoadAttributes);
    NS_IMETHOD GetContentType(char * *aContentType);
    NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup);
    NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup);

    // nsInputStreamChannel methods:
    nsInputStreamChannel(); 
    virtual ~nsInputStreamChannel();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init(nsIURI* uri, const char* contentType, nsIInputStream* in);

protected:
    nsIURI*             mURI;
    char*               mContentType;
    nsIInputStream*     mInputStream;
    nsILoadGroup*       mLoadGroup;
};

#define NS_INPUTSTREAMCHANNEL_CID                    \
{ /* ccae3fae-30f5-11d3-8cd0-0060b0fc14a3 */         \
    0xccae3fae,                                      \
    0x30f5,                                          \
    0x11d3,                                          \
    {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#endif // nsInputStreamChannel_h__
