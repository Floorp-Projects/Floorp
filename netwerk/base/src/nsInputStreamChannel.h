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
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsCOMPtr.h"

class nsInputStreamChannel : public nsIChannel, 
                             public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMOBSERVER
    NS_DECL_NSISTREAMLISTENER

    nsInputStreamChannel(); 
    virtual ~nsInputStreamChannel();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init(nsIURI* uri, const char* contentType, 
                  nsIInputStream* in, nsILoadGroup* group);

protected:
    nsCOMPtr<nsIURI>            mURI;
    char*                       mContentType;
    nsCOMPtr<nsIInputStream>    mInputStream;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;
    nsCOMPtr<nsISupports>       mOwner;
    nsCOMPtr<nsIChannel>        mFileTransport;
    nsCOMPtr<nsIStreamListener> mRealListener;
};

#define NS_INPUTSTREAMCHANNEL_CID                    \
{ /* ccae3fae-30f5-11d3-8cd0-0060b0fc14a3 */         \
    0xccae3fae,                                      \
    0x30f5,                                          \
    0x11d3,                                          \
    {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#endif // nsInputStreamChannel_h__
