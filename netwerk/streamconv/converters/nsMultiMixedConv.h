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

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsString2.h"

#include "nsIFactory.h"

#define NS_MULTIMIXEDCONVERTER_CID                         \
{ /* 7584CE90-5B25-11d3-A175-0050041CAF44 */         \
    0x7584ce90,                                      \
    0x5b25,                                          \
    0x11d3,                                          \
    {0xa1, 0x75, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44}       \
}
static NS_DEFINE_CID(kMultiMixedConverterCID,          NS_MULTIMIXEDCONVERTER_CID);


class nsMultiMixedConv : public nsIStreamConverter {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIStreamConverter methods
    NS_IMETHOD Convert(nsIInputStream *aFromStream,
                       const PRUnichar *aFromType,
                       const PRUnichar *aToType, 
                       nsISupports *aCtxt, nsIInputStream **_retval);

    NS_IMETHOD AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType, 
                                nsIStreamListener *aListener, nsISupports *aCtxt);

    // nsIStreamListener methods
    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, 
                               nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count);

    // nsIStreamObserver methods
    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt);
    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, 
                             nsresult status, const PRUnichar *errorMsg);

    // nsMultiMixedConv methods
    nsMultiMixedConv();
    virtual ~nsMultiMixedConv();
    nsresult Init();
    nsresult SendData(const char *aBuffer, nsIChannel *aChannel, nsISupports *aCtxt);
    nsresult BuildURI(nsIChannel *aChannel, nsIURI **_retval);

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsMultiMixedConv* _s = new nsMultiMixedConv();
        if (_s == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(_s);
        rv = _s->Init();
        if (NS_FAILED(rv)) {
            delete _s;
            return rv;
        }
        rv = _s->QueryInterface(aIID, aResult);
        NS_RELEASE(_s);
        return rv;
    }

    // member data
    PRBool              mBoundaryStart;  // are we in the middle of a boundary?
    nsIStreamListener   *mFinalListener; // this guy gets the converted data via his OnDataAvailable()
    char                *mBoundaryCStr;
    PRInt32             mBoundaryStrLen;
    PRUint16            mPartCount;     // the number of parts we've seen so far
    nsIChannel          *mPartChannel;  // the channel for the given part we're processing.
                                        // one channel per part.
};

//////////////////////////////////////////////////
// FACTORY
class MultiMixedFactory : public nsIFactory
{
public:
    MultiMixedFactory(const nsCID &aClass, const char* className, const char* progID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~MultiMixedFactory();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};