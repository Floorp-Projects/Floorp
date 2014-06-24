/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsftpdirlistingdconv__h__
#define __nsftpdirlistingdconv__h__

#include "nsIStreamConverter.h"
#include "nsString.h"

class nsIURI;

#define NS_FTPDIRLISTINGCONVERTER_CID                         \
{ /* 14C0E880-623E-11d3-A178-0050041CAF44 */         \
    0x14c0e880,                                      \
    0x623e,                                          \
    0x11d3,                                          \
    {0xa1, 0x78, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44}       \
}

class nsFTPDirListingConv : public nsIStreamConverter {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIStreamConverter methods
    NS_DECL_NSISTREAMCONVERTER

    // nsIStreamListener methods
    NS_DECL_NSISTREAMLISTENER

    // nsIRequestObserver methods
    NS_DECL_NSIREQUESTOBSERVER

    // nsFTPDirListingConv methods
    nsFTPDirListingConv();
    nsresult Init();

private:
    virtual ~nsFTPDirListingConv();

    // Get the application/http-index-format headers
    nsresult GetHeaders(nsACString& str, nsIURI* uri);
    char*    DigestBufferLines(char *aBuffer, nsCString &aString);

    // member data
    nsCString           mBuffer;            // buffered data.
    bool                mSentHeading;       // have we sent 100, 101, 200, and 300 lines yet?

    nsIStreamListener   *mFinalListener; // this guy gets the converted data via his OnDataAvailable()
};

#endif /* __nsftpdirlistingdconv__h__ */
