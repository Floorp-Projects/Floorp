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

// data implementation header

#ifndef nsDataChannel_h___
#define nsDataChannel_h___

#include "nsIDataChannel.h"
#include "nsIURI.h"
#include "nsString2.h"
#include "nsIEventQueue.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"

#include "nsCOMPtr.h"

class nsIEventSinkGetter;
class nsIProgressEventSink;

class nsDataChannel : public nsIDataChannel {
public:
    NS_DECL_ISUPPORTS

    // nsIRequest methods:
    NS_DECL_NSIREQUEST

    // nsIChannel methods:
    NS_DECL_NSICHANNEL

    // nsIDataChannel methods:
    NS_DECL_NSIDATACHANNEL

    // nsFTPChannel methods:
    nsDataChannel();
    virtual ~nsDataChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                  nsIEventSinkGetter* getter);
    nsresult ParseData();

protected:
    nsIURI                  *mUrl;
    nsIInputStream          *mDataStream;
    PRUint32                mLoadAttributes;
    nsILoadGroup            *mLoadGroup;
    nsCString               mContentType;

};

#endif /* nsFTPChannel_h___ */
