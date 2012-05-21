/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ____nsindexedtohtml___h___
#define ____nsindexedtohtml___h___

#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIStreamConverter.h"
#include "nsXPIDLString.h"
#include "nsIDirIndexListener.h"
#include "nsIDateTimeFormat.h"
#include "nsIStringBundle.h"
#include "nsIStringStream.h"
#include "nsITextToSubURI.h"
#include "nsICharsetConverterManager.h"

#define NS_NSINDEXEDTOHTMLCONVERTER_CID \
{ 0xcf0f71fd, 0xfafd, 0x4e2b, {0x9f, 0xdc, 0x13, 0x4d, 0x97, 0x2e, 0x16, 0xe2} }


class nsIndexedToHTML : public nsIStreamConverter,
                        public nsIDirIndexListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMCONVERTER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIDIRINDEXLISTENER

    nsIndexedToHTML();
    virtual ~nsIndexedToHTML();

    nsresult Init(nsIStreamListener *aListener);

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    
    void FormatSizeString(PRInt64 inSize, nsString& outSizeString);
    nsresult FormatInputStream(nsIRequest* aRequest, nsISupports *aContext, const nsAString &aBuffer);
    // Helper to properly implement OnStartRequest
    nsresult DoOnStartRequest(nsIRequest* request, nsISupports *aContext,
                              nsString& aBuffer);

protected:
    nsCOMPtr<nsIDirIndexParser>     mParser;
    nsCOMPtr<nsIStreamListener>     mListener; // final listener (consumer)

    nsCOMPtr<nsIDateTimeFormat> mDateTime;
    nsCOMPtr<nsIStringBundle> mBundle;

    nsCOMPtr<nsITextToSubURI> mTextToSubURI;
    nsCOMPtr<nsIUnicodeEncoder> mUnicodeEncoder;

private:
    // Expecting absolute locations, given by 201 lines.
    bool mExpectAbsLoc;
    nsString mEscapedEllipsis;
};

#endif

